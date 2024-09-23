// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/ash_notification_drag_controller.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/metrics/histogram_functions.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/view.h"

namespace ash {

AshNotificationDragController::AshNotificationDragController() = default;

AshNotificationDragController::~AshNotificationDragController() = default;

void AshNotificationDragController::OnDragStarted() {
  if (drag_in_progress_) {
    // A drag-and-drop session could start before an async drop finishes. In
    // this case, neither `OnDropCompleted()` nor `OnDragCancelled()` is called.
    // Therefore, clean up the active notification drag handling.
    CleanUp(DragEndState::kInterruptedByNewDrag);
  } else {
    drag_in_progress_ = true;
  }
}

void AshNotificationDragController::OnDragCancelled() {
  CleanUp(DragEndState::kCancelled);
}

void AshNotificationDragController::OnDropCompleted(
    ui::mojom::DragOperation drag_operation) {
  // Remove the dragged notification from the message center if drag-and-drop
  // ends with copy. `MessageCenter::RemoveNotification()` guarantees that only
  // unpinned notifications are removable to users.
  DragEndState state = DragEndState::kCompletedWithoutDrop;
  if (drag_operation == ui::mojom::DragOperation::kCopy) {
    message_center::MessageCenter::Get()->RemoveNotification(
        *dragged_notification_id_, /*by_user=*/true);
    state = DragEndState::kCompletedWithDrop;
  }

  CleanUp(state);
}

void AshNotificationDragController::WriteDragDataForView(
    views::View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {
  // Sets the image to show during drag.
  // TODO(b/308814203): clean the static_cast checks by replacing
  // `AshNotificationView*` with a base class.
  if (!message_center_utils::IsAshNotificationView(sender)) {
    return;
  }

  AshNotificationView* notification_view =
      static_cast<AshNotificationView*>(sender);
  const std::optional<gfx::ImageSkia> drag_image =
      notification_view->GetDragImage();
  DCHECK(drag_image);

  // The drag point is at the top left corner, or top right corner under RTL.
  data->provider().SetDragImage(
      *drag_image, base::i18n::IsRTL()
                       ? gfx::Vector2d(drag_image->size().width(), /*y=*/0)
                       : gfx::Vector2d());

  notification_view->AttachDropData(data);
}

int AshNotificationDragController::GetDragOperationsForView(
    views::View* sender,
    const gfx::Point& p) {
  // TODO(b/308814203): clean the static_cast checks by replacing
  // `AshNotificationView*` with a base class.
  if (!message_center_utils::IsAshNotificationView(sender)) {
    return ui::DragDropTypes::DRAG_NONE;
  }

  const std::optional<gfx::Rect> drag_area =
      static_cast<AshNotificationView*>(sender)->GetDragAreaBounds();

  // Use `DRAG_COPY` if:
  // 1. `sender` is draggable; and
  // 2. `drag_area` contains `p`.
  if (drag_area && drag_area->Contains(p)) {
    return ui::DragDropTypes::DRAG_COPY;
  }

  return ui::DragDropTypes::DRAG_NONE;
}

bool AshNotificationDragController::CanStartDragForView(
    views::View* sender,
    const gfx::Point& press_pt,
    const gfx::Point& p) {
  // TODO(b/308814203): clean the static_cast checks by replacing
  // `AshNotificationView*` with a base class.
  if (!message_center_utils::IsAshNotificationView(sender)) {
    return false;
  }

  const AshNotificationView* const notification_view =
      static_cast<AshNotificationView*>(sender);
  const std::optional<gfx::Rect> drag_area =
      notification_view->GetDragAreaBounds();

  // Enable dragging `notification_view_` if:
  // 1. `notification_view` is backed by a notification; and
  // 2. `notification_view` is draggable; and
  // 3. `drag_area` contains the initial press point.
  const bool can_start_drag =
      (!!message_center::MessageCenter::Get()->FindNotificationById(
           notification_view->notification_id()) &&
       drag_area && drag_area->Contains(press_pt));

  // Assume that the drag on `sender` will start when `can_start_drag` is true.
  // TODO(crbug.com/40254274): in some edge cases, the view drag does not
  // start when `CanStartDragForView()` returns true. We should come up with a
  // general solution to observe drag start.
  if (can_start_drag) {
    // A drag-and-drop session could start before an async drop finishes. In
    // this case, neither `OnDropCompleted()` nor `OnDragCancelled()` is called.
    // Therefore, clean up the active notification drag handling.
    if (drag_in_progress_) {
      CleanUp(DragEndState::kInterruptedByNewDrag);
    }
  }

  return can_start_drag;
}

void AshNotificationDragController::OnWillStartDragForView(
    views::View* dragged_view) {
  // TODO(b/308814203): clean the static_cast checks by replacing
  // `AshNotificationView*` with a base class.
  if (!message_center_utils::IsAshNotificationView(dragged_view)) {
    return;
  }
  OnNotificationDragWillStart(static_cast<AshNotificationView*>(dragged_view));
}

void AshNotificationDragController::OnNotificationDragWillStart(
    AshNotificationView* dragged_view) {
  DCHECK(!drag_in_progress_);
  dragged_notification_id_ = dragged_view->notification_id();

  // The drag drop client in Ash, i.e. `DragDropController`, is a singleton.
  // Hence, always use the primary root window to access the drag drop client.
  drag_drop_client_observer_.Observe(
      aura::client::GetDragDropClient(Shell::GetPrimaryRootWindow()));

  message_center::MessageCenter* message_center_ptr =
      message_center::MessageCenter::Get();
  message_center::Notification* notification =
      message_center_ptr->FindNotificationById(*dragged_notification_id_);
  base::UmaHistogramEnumeration("Ash.NotificationView.ImageDrag.Start",
                                notification->notifier_id().catalog_name);

  // Hide the message center bubble if it is open.
  if (message_center_ptr->IsMessageCenterVisible()) {
    StatusAreaWidget* status_area_widget =
        RootWindowController::ForWindow(
            dragged_view->GetWidget()->GetNativeView())
            ->GetStatusAreaWidget();
    TrayBackgroundView* message_center_bubble = nullptr;
    message_center_bubble = status_area_widget->notification_center_tray();

    // We cannot destroy the message center bubble instantly. Otherwise, if
    // `dragged_view` is under gesture drag, the gesture state will be reset
    // when the bubble is closed. Therefore, post a task to close the bubble
    // asynchronously.
    DCHECK(message_center_bubble);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](const base::WeakPtr<TrayBackgroundView>& weak_ptr) {
                         if (weak_ptr) {
                           weak_ptr->CloseBubble();
                         }
                       },
                       message_center_bubble->GetWeakPtr()));

    return;
  }

  // Hide the dragged notification popup if any. Assume that the notification
  // popup only shows when the message center is hidden.
  // NOTE: if the dragged notification is a child of a notification group, hide
  // the group notification popup.
  message_center_ptr->MarkSinglePopupAsShown(
      notification->group_child()
          ? message_center_ptr->FindParentNotification(notification)->id()
          : *dragged_notification_id_,
      /*mark_notification_as_read=*/true);
}

void AshNotificationDragController::CleanUp(DragEndState state) {
  DCHECK(drag_in_progress_);
  drag_in_progress_ = false;
  dragged_notification_id_.reset();
  drag_drop_client_observer_.Reset();

  base::UmaHistogramEnumeration("Ash.NotificationView.ImageDrag.EndState",
                                state);
}

}  // namespace ash
