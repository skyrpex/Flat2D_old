#include "View.hpp"
#include "Bone.hpp"
#include "Attachment.hpp"
#include "Application.hpp"
#include <QKeyEvent>
#include <QGraphicsItem>
#include <QTimer>
#include <QStack>
#include <QDebug>
#include <QUrl>
#include <QFileInfo>
#include <QMimeData>
#include <QMenu>
#include "commands/RotateCommand.hpp"
#include "commands/ScaleCommand.hpp"
#include "commands/TranslateCommand.hpp"
#include "commands/CreateCommand.hpp"
#include "commands/SelectionCommand.hpp"

// Update interval: 60 fps
static const int UpdateInterval = 1000 / 60;

static const char * const SelectEditModeText = "Select mode";
static const char * const TranslateEditModeText = "Translate mode";
static const char * const RotateEditModeText = "Rotate mode";
static const char * const ScaleEditModeText = "Scale mode";

static const int TransformKey = Qt::Key_1;
static const int CreateKey = Qt::Key_2;

static const int TranslateKey = Qt::Key_T;
static const int RotateKey = Qt::Key_R;
static const int ScaleKey = Qt::Key_S;

View::View(QWidget *parent) :
    QGraphicsView(new QGraphicsScene, parent),
    m_root(new Bone("Root")),
    m_targetMode(BoneTargetMode),
    m_ellipseItem(new QGraphicsEllipseItem(-10, -10, 20, 20)),
    m_thickEllipseItem(new QGraphicsEllipseItem(-10, -10, 20, 20)),
    m_lineItem(new QGraphicsLineItem),
    m_solidLineItem(new QGraphicsLineItem)
{
    setSceneRect(-512, -400, 1024, 800);
    setRenderHints(QPainter::Antialiasing);
    setBackgroundBrush(Qt::darkGray);
    setStyleSheet("QGraphicsView { border: 0; }");
    setAcceptDrops(true);

    Bone *bone = new Bone("Bone 2", m_root);
    bone->setScaleFromLength(100);

    Attachment *attachment1 = new Attachment(QPixmap(":/images/head.png"));
    m_root->addAttachment(attachment1);

    Attachment *attachment2 = new Attachment(QPixmap(":/images/torso.png"));
    bone->addAttachment(attachment2);

    scene()->addItem(m_root);
    scene()->addItem(attachment1);
    scene()->addItem(attachment2);

    setBoneTargetMode();
    setTransformEditMode();
    setSelectTransformMode();

    m_circleItem = new QGraphicsEllipseItem(-10, -10, 20, 20);
    m_circleItem->setVisible(false);
    scene()->addItem(m_circleItem);

    m_ellipseItem->setVisible(false);
    scene()->addItem(m_ellipseItem);

    m_thickEllipseItem->setVisible(false);
    m_thickEllipseItem->setPen(QPen(Qt::black, 2));
    scene()->addItem(m_thickEllipseItem);

    m_lineItem->setPen(QPen(Qt::black, 0, Qt::DashLine));
    m_lineItem->setVisible(false);
    scene()->addItem(m_lineItem);

    m_solidLineItem->setPen(QPen(Qt::black, 0, Qt::SolidLine));
    m_solidLineItem->setVisible(false);
    scene()->addItem(m_solidLineItem);

    //
    setViewportUpdateMode(NoViewportUpdate);

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), viewport(), SLOT(update()));
    timer->start(UpdateInterval);
}

void View::setBoneTargetMode()
{
    m_targetMode = BoneTargetMode;

    m_root->setOpacity(1);
    foreach(Bone *bone, bones()) {
        bone->setFlag(QGraphicsItem::ItemIsSelectable, true);
        bone->setFlag(QGraphicsItem::ItemIsMovable, true);
    }

    m_root->mapAttachmentsFromScene();
    foreach(Attachment *attachment, attachments()) {
        attachment->setOpacity(0.6);
        attachment->setEnabled(false);
    }
}

void View::setAttachmentTargetMode()
{
    m_targetMode = AttachmentTargetMode;

    m_root->setOpacity(0.6);
    foreach(Bone *bone, bones()) {
        bone->setFlag(QGraphicsItem::ItemIsSelectable, false);
        bone->setFlag(QGraphicsItem::ItemIsMovable, false);
    }

    foreach(Attachment *attachment, attachments()) {
        attachment->setOpacity(1);
        attachment->setEnabled(true);
    }
}

void View::setTransformEditMode()
{
    m_editMode = TransformEditMode;

    setDragMode(RubberBandDrag);

    m_ellipseItem->setVisible(false);
    m_solidLineItem->setVisible(false);
}

void View::setCreateEditMode()
{
    setSelectTransformMode();

    m_editMode = CreateEditMode;

    setBoneTargetMode();
    setDragMode(NoDrag);
    m_targetBone = NULL;

    if(scene()->selectedItems().count() > 1) {
        scene()->clearSelection();
    }


    m_ellipseItem->setVisible(false);
    m_solidLineItem->setVisible(false);
}

void View::setParentEditMode()
{
    m_editMode = ParentEditMode;
    m_targetItem = NULL;

    m_ellipseItem->setVisible(false);
    m_solidLineItem->setVisible(false);
}

void View::setSelectTransformMode()
{
    if(m_transformMode == SelectTransformMode) {
        return;
    }

    setTransformEditMode();

    m_transformMode = SelectTransformMode;

    m_lineItem->setVisible(false);
    m_ellipseItem->setVisible(false);
    m_solidLineItem->setVisible(false);
}

void View::setRotateTransformMode()
{
    if(m_transformMode != SelectTransformMode) {
        return;
    }

    setTransformEditMode();

    m_transformMode = RotateTransformMode;

    m_targetItem = targetItem(scene()->selectedItems());
    if(m_targetItem) {
        m_ellipseItem->setPos(m_targetItem->scenePos());

        QLineF line(m_targetItem->scenePos(), cursorScenePos());
        m_lineItem->setLine(line);

        m_oldRotation = line.angle();
    }

//    m_ellipseItem->setVisible(m_targetItem);
    m_lineItem->setVisible(m_targetItem);

    m_rotationBackup.clear();
    foreach(QGraphicsItem *item, scene()->selectedItems()) {
        m_rotationBackup.insert(item, item->rotation());
    }
}

void View::setScaleTransformMode()
{
    if(m_transformMode != SelectTransformMode) {
        return;
    }

    setTransformEditMode();

    m_transformMode = ScaleTransformMode;

    m_targetItem = targetItem(scene()->selectedItems());
    if(m_targetItem) {
        m_thickEllipseItem->setPos(m_targetItem->scenePos());
        m_ellipseItem->setPos(m_targetItem->scenePos());

        QLineF line(m_targetItem->scenePos(), cursorScenePos());
        m_lineItem->setLine(line);
        m_oldLength = line.length();

        qreal size = m_oldLength*2;
        m_thickEllipseItem->setRect(-m_oldLength, -m_oldLength, size, size);
    }

//    m_ellipseItem->setVisible(false);
    m_lineItem->setVisible(m_targetItem);
//    m_thickEllipseItem->setVisible(m_targetItem);

    m_scaleBackup.clear();
    foreach(QGraphicsItem *item, scene()->selectedItems()) {
        m_scaleBackup.insert(item, item->scale());
    }
}

void View::keyPressEvent(QKeyEvent *event)
{
    if(!event->isAutoRepeat() && m_editMode == TransformEditMode) {
        switch(event->key()) {
        case RotateKey:
            setRotateTransformMode();
            break;

        case ScaleKey:
            setScaleTransformMode();
            break;

        default:
            break;
        }
    }

    QGraphicsView::keyPressEvent(event);
}

void View::mousePressEvent(QMouseEvent *event)
{
    if(m_editMode == TransformEditMode) {
        switch(m_transformMode) {
        case SelectTransformMode: {
            // Switch target mode?
            QGraphicsItem *item = itemAt(event->pos());
            if(m_targetMode == BoneTargetMode && dynamic_cast<Attachment *>(item)) {
                setAttachmentTargetMode();
            }
            else if(m_targetMode == AttachmentTargetMode && dynamic_cast<Bone *>(item)) {
                setBoneTargetMode();
            }
            QGraphicsView::mousePressEvent(event);

            m_hasTranslated = false;
            m_translationBackup.clear();
            foreach(QGraphicsItem *item, scene()->selectedItems()) {
                m_translationBackup.insert(item, item->pos());
            }

            break;
        }

        case RotateTransformMode:
            if(event->buttons() & Qt::LeftButton) {
                commitRotation();
            }
            else if(event->buttons() & Qt::RightButton) {
                cancelRotation();
            }
            break;

        case ScaleTransformMode:
            if(event->buttons() & Qt::LeftButton) {
                commitScale();
            }
            else if(event->buttons() & Qt::RightButton) {
                cancelScale();
            }
            break;
        }
    }
    else if(m_editMode == CreateEditMode) {
        if(event->buttons() & Qt::LeftButton) {
            qDebug() << m_targetBone;
            if(m_targetBone) {
                commitBoneCreation();
            }
            else {
                QGraphicsItem *item = itemAt(event->pos());
                Bone *bone = dynamic_cast<Bone *>(item);
                if(bone) {
                    scene()->clearSelection();
                    bone->setSelected(true);
                    m_targetBone = 0;
                }
                else {
                    Bone *parent = 0;
                    QList<QGraphicsItem *> selectedItems = scene()->selectedItems();
                    if(selectedItems.count() == 1) {
                        parent = dynamic_cast<Bone *>(selectedItems.first());
                    }

                    if(parent) {
                        m_targetBone = new Bone("New", parent);

                        QPointF scenePos = mapToScene(event->pos());
                        m_targetBone->setScenePos(scenePos);
                        m_targetBone->setJoint(true);
                        m_targetBone->setSceneScale(1);

                        scene()->clearSelection();
                        m_targetBone->setSelected(true);
                    }
                    else {
                        m_targetBone = NULL;
                    }
                }
            }
        }
        else if(event->buttons() & Qt::RightButton) {
//            cancelScale();
        }
    }
    else if(m_editMode == ParentEditMode) {
//        foreach(QGraphicsItem *item, scene()->items()) {
//            item->setOpacity(0.5);
//        }

        QGraphicsItem *itemAtCursor = itemAt(event->pos());
        Bone *bone = dynamic_cast<Bone *>(itemAtCursor);
        Attachment *attachment = dynamic_cast<Attachment *>(itemAtCursor);
        if(bone || attachment) {
            m_targetItem = itemAtCursor;
            m_hotSpot = mapToScene(event->pos());
        }
    }
}

void View::mouseMoveEvent(QMouseEvent *event)
{
    if(event->buttons() & Qt::RightButton) {
        event->ignore();
        return;
    }

    if(m_editMode == TransformEditMode) {
        switch(m_transformMode) {
        case SelectTransformMode: {
            QGraphicsView::mouseMoveEvent(event);
            m_hasTranslated = true;
            break;
        }

        case RotateTransformMode: {
            if(m_targetItem) {
                QPointF scenePos = mapToScene(event->pos());
                QLineF line(m_targetItem->scenePos(), scenePos);
                m_lineItem->setLine(line);

                qreal offset = m_oldRotation - line.angle();

                QList<QGraphicsItem*> selectedItems = scene()->selectedItems();
                foreach(QGraphicsItem *item, selectedItems) {
                    item->setRotation(item->rotation() + offset);
                }

                m_oldRotation = line.angle();
            }
            break;
        }

        case ScaleTransformMode: {
            if(m_targetItem) {
                QPointF scenePos = mapToScene(event->pos());
                QLineF line(m_targetItem->scenePos(), scenePos);
                m_lineItem->setLine(line);

                qreal factor = line.length() / m_oldLength;
                foreach(QGraphicsItem *item, scene()->selectedItems()) {
                    item->setScale(item->scale() * factor);
                }

                m_oldLength = line.length();
            }
            break;
        }
        }
    }
    else if(m_editMode == CreateEditMode) {
        if(m_targetBone) {
            QPointF scenePos = mapToScene(event->pos());
            QLineF line(m_targetBone->scenePos(), scenePos);

            m_targetBone->setSceneRotation(-line.angle());

            m_targetBone->setBoneSceneLength(line.length());
        }
    }
    else if(m_editMode == ParentEditMode) {
//        if(event->buttons() == 0) {
//            foreach(QGraphicsItem *item, scene()->items()) {
//                item->setOpacity(0.5);
//            }

//            QGraphicsItem *item = itemAt(event->pos());
//            if(item) {
//                item->setOpacity(1);
//            }
//        }
    }
}

void View::mouseReleaseEvent(QMouseEvent *event)
{
    m_lineItem->setVisible(false);
    m_thickEllipseItem->setVisible(false);
    QGraphicsView::mouseReleaseEvent(event);

    if(m_editMode == TransformEditMode) {
        commitTranslation();
    }
//    else if(m_editMode == CreateEditMode) {
//        if(m_targetBone) {
//            commitBoneCreation();
//        }
//    }
    else if(m_editMode == ParentEditMode) {
        if(m_targetItem) {
            QGraphicsItem *itemAtCursor = itemAt(event->pos());
            Bone *bone = dynamic_cast<Bone *>(itemAtCursor);
            if(bone && bone != m_targetItem) {
                Bone *targetBone = dynamic_cast<Bone *>(m_targetItem);
                if(targetBone) {
                    targetBone->setParentItem(bone);
                }
                else {
                    Attachment *targetAttachment = dynamic_cast<Attachment *>(m_targetItem);
                    if(targetAttachment) {
                        bone->addAttachment(targetAttachment);
                    }
                }
            }
        }

        m_targetItem = NULL;
    }
}

void View::contextMenuEvent(QContextMenuEvent *event)
{
    QGraphicsItem *itemAtCursor = itemAt(event->pos());
    Bone *bone = dynamic_cast<Bone *>(itemAtCursor);
    if(bone) {
        QMenu menu;
        QAction *removeAction = menu.addAction(tr("Remove"));
        QAction *removeChildrenAction = menu.addAction(tr("Remove Children"));

        bool hasParent = bone->parentBone();
        bool hasChildren = !bone->childBones().isEmpty();

        removeAction->setEnabled(hasParent);
        removeChildrenAction->setVisible(hasChildren);

        QAction *selectedAction = menu.exec(event->globalPos());
        if(selectedAction == removeAction) {
            delete bone;
        }
        else if(selectedAction == removeChildrenAction) {
            qDeleteAll(bone->childBones());
        }
        event->accept();
    }
    else {
        event->ignore();
    }
}

void View::paintEvent(QPaintEvent *event)
{
    if(m_targetMode == BoneTargetMode) {
        m_root->mapAttachmentsToScene();
    }
    QGraphicsView::paintEvent(event);
}

void View::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);

    // Draw parental lines
    foreach(QGraphicsItem *item, scene()->items()) {
        Bone *bone = dynamic_cast<Bone *>(item);
        if(bone) {
            // Draw line to parent bone
            if(bone->parentBone()) {
                painter->setPen(QPen(Qt::black, 0));
                painter->drawLine(bone->scenePos(), bone->parentBone()->scenePeakPos());
            }

            // Draw lines to attachments
            foreach(Attachment *attachment, bone->attachments()) {
                painter->setPen(QPen(Qt::darkRed, 0));
                painter->drawLine(bone->scenePos(), attachment->scenePos());
            }
        }
    }

    // Draw parental edit line
    if(m_editMode == ParentEditMode) {
        if(m_targetItem) {
            QPen pen(Qt::green, 2);
            pen.setStyle(Qt::DashLine);
            pen.setCosmetic(true);
            painter->setPen(pen);

            QPointF cursorPos = mapToScene(mapFromGlobal(QCursor::pos()));
            painter->drawLine(m_hotSpot, cursorPos);
        }
    }
}

void View::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasUrls()) {
        event->accept();
    }
}

void View::dragMoveEvent(QDragMoveEvent *event)
{
    if(event->mimeData()->hasUrls() /*&& dynamic_cast<Bone *>(itemAt(event->pos()))*/) {
        event->accept();
    }
    else {
        event->ignore();
    }
}

//void View::dragLeaveEvent(QDragLeaveEvent *event)
//{
//    if(event->mimeData()->hasUrls() && dynamic_cast<Bone *>(itemAt(event->pos()))) {
//        event->accept();
//    }
//}

void View::dropEvent(QDropEvent *event)
{
    Q_ASSERT(event->mimeData()->hasUrls());

//    Bone *bone = dynamic_cast<Bone *>(itemAt(event->pos()));
//    Q_ASSERT(bone);

    foreach(QUrl url, event->mimeData()->urls()) {
        QString filePath = url.path().mid(1);
        QPixmap pixmap(filePath);
        Attachment *attachment = new Attachment(pixmap);

        QPointF scenePos = mapToScene(event->pos());
        attachment->setPos(scenePos - QPointF(pixmap.width(), pixmap.height())/2.0);
//        attachment->setLocalRotation(-bone->sceneRotation());

//        bone->addAttachment(attachment);
        scene()->addItem(attachment);
    }

    event->acceptProposedAction();
}

Bone *View::topLevelBone(const QList<QGraphicsItem *> &items) const
{
    Bone *topBone = 0;
    int topLevel = -1;

    foreach(QGraphicsItem *item, items) {
        Bone *bone = dynamic_cast<Bone *>(item);
        if(!bone) {
            continue;
        }

        int level = 0;
        Bone *parent = bone->parentBone();
        while(parent) {
            ++level;
            parent = parent->parentBone();
        }

        if((topBone && level < topLevel) || !topBone) {
            topBone = bone;
            topLevel = level;
        }
    }

    return topBone;
}

QGraphicsItem *View::nearestItem(const QPointF &scenePos, const QList<QGraphicsItem *> &items) const
{
    QGraphicsItem *nearestItem = 0;
    qreal nearestDistance = 0;
    foreach(QGraphicsItem *item, items) {
        qreal distance = QLineF(scenePos, item->scenePos()).length();
        if((nearestItem && distance < nearestDistance) || !nearestItem) {
            nearestItem = item;
            nearestDistance = distance;
        }
    }
    return nearestItem;
}

QGraphicsItem *View::targetItem(const QList<QGraphicsItem *> &items) const
{
    QGraphicsItem *item = topLevelBone(items);
    if(!item) {
        item = nearestItem(cursorScenePos(), items);
    }

    return item;
}

QPointF View::cursorScenePos() const
{
    return mapToScene(mapFromGlobal(QCursor::pos()));
}

QList<Bone *> View::bones() const
{
    QList<Bone *> bones;

    QStack<Bone *> stack;
    stack << m_root;
    while(!stack.isEmpty()) {
        Bone *bone = stack.pop();

        bones << bone;

        foreach(Bone *child, bone->childBones()) {
            stack.push(child);
        }
    }

    return bones;
}


QList<Attachment *> View::attachments() const
{
    QList<Attachment *> attachments;

    QStack<Bone *> bones;
    bones << m_root;
    while(!bones.isEmpty()) {
        Bone *bone = bones.pop();

        attachments << bone->attachments();

        foreach(Bone *child, bone->childBones()) {
            bones.push(child);
        }
    }

    return attachments;
}

void View::commitRotation()
{
    qApp->undoStack()->beginMacro("Rotate");

    qApp->undoStack()->push(new SelectionCommand(scene(), scene()->selectedItems()));

    QMapIterator<QGraphicsItem *, qreal> it(m_rotationBackup);
    while(it.hasNext()) {
        it.next();
        QGraphicsItem *item = it.key();
        qreal oldRotation = it.value();
        qreal newRotation = item->rotation();
        qApp->undoStack()->push(new RotateCommand(item, oldRotation, newRotation));
    }
    qApp->undoStack()->endMacro();
    m_rotationBackup.clear();

    setSelectTransformMode();
}

void View::commitScale()
{
    qApp->undoStack()->beginMacro("Scale");

    qApp->undoStack()->push(new SelectionCommand(scene(), scene()->selectedItems()));

    QMapIterator<QGraphicsItem *, qreal> it(m_scaleBackup);
    while(it.hasNext()) {
        it.next();
        QGraphicsItem *item = it.key();
        qreal oldScale = it.value();
        qreal newScale = item->scale();
        qApp->undoStack()->push(new ScaleCommand(item, oldScale, newScale));
    }
    qApp->undoStack()->endMacro();
    m_scaleBackup.clear();

    setSelectTransformMode();
}

void View::commitTranslation()
{
    if(m_translationBackup.isEmpty() || !m_hasTranslated) {
        return;
    }

    qApp->undoStack()->beginMacro("Translate");

    qApp->undoStack()->push(new SelectionCommand(scene(), scene()->selectedItems()));

    QMapIterator<QGraphicsItem *, QPointF> it(m_translationBackup);
    while(it.hasNext()) {
        it.next();
        QGraphicsItem *item = it.key();
        QPointF oldPos = it.value();
        QPointF newPos = item->pos();
        qApp->undoStack()->push(new TranslateCommand(item, oldPos, newPos));
    }
    qApp->undoStack()->endMacro();
    m_translationBackup.clear();
}

void View::commitBoneCreation()
{
    Q_ASSERT(m_targetBone);

//    qApp->undoStack()->beginMacro("Create");
    qApp->undoStack()->push(new CreateCommand(m_targetBone, m_targetBone->parentBone()));
//    qApp->undoStack()->push(new SelectionCommand(scene(), scene()->selectedItems()));
//    qApp->undoStack()->endMacro();

    m_targetBone = NULL;
}

void View::cancelRotation()
{
    QMapIterator<QGraphicsItem *, qreal> it(m_rotationBackup);
    while(it.hasNext()) {
        it.next();
        QGraphicsItem *item = it.key();
        qreal oldRotation = it.value();
        item->setRotation(oldRotation);
    }
    m_rotationBackup.clear();

    setSelectTransformMode();
}

void View::cancelScale()
{
    QMapIterator<QGraphicsItem *, qreal> it(m_scaleBackup);
    while(it.hasNext()) {
        it.next();
        QGraphicsItem *item = it.key();
        qreal oldScale = it.value();
        item->setScale(oldScale);
    }
    m_scaleBackup.clear();

    setSelectTransformMode();
}
