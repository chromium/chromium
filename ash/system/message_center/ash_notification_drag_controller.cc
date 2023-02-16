// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_drag_controller.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_notification_view.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/view.h"

namespace ash {

AshNotificationDragController::AshNotificationDragController() = default;

AshNotificationDragController::~AshNotificationDragController() = default;

void AshNotificationDragController::OnDragCompleted(
    const ui::DropTargetEvent& event) {
  OnNotificationViewDragEnded();
}

void AshNotificationDragController::OnDragCancelled() {
  OnNotificationViewDragEnded();
}

void AshNotificationDragController::WriteDragDataForView(
    views::View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {
  AshNotificationView* notification_view =
      static_cast<AshNotificationView*>(sender);
  const absl::optional<gfx::Rect> drag_area =
      notification_view->GetDragAreaBounds();
  DCHECK(drag_area);

  // Set the image to show during drag.
  const absl::optional<gfx::ImageSkia> drag_image =
      notification_view->GetDragImage();
  DCHECK(drag_image);
  data->provider().SetDragImage(*drag_image, press_pt - drag_area->origin());

  notification_view->AttachDropData(data);
}

int AshNotificationDragController::GetDragOperationsForView(
    views::View* sender,
    const gfx::Point& p) {
  const absl::optional<gfx::Rect> drag_area =
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
  AshNotificationView* notification_view =
      static_cast<AshNotificationView*>(sender);
  const absl::optional<gfx::Rect> drag_area =
      notification_view->GetDragAreaBounds();

  // Enable dragging `notification_view_` if:
  // 1. `notification_view_` is draggable; and
  // 2. `drag_area` contains the initial press point.
  const bool can_start_drag = (drag_area && drag_area->Contains(press_pt));

  // Assume that the drag on `sender` will start when `can_start_drag` is true.
  // TODO(https://crbug.com/1410276): in some edge cases, the view drag does not
  // start when `CanStartDragForView()` returns true. We should come up with a
  // general solution to observe drag start.
  if (can_start_drag) {
    OnNotificationViewDragStarted(notification_view);
  }

  return can_start_drag;
}

void AshNotificationDragController::OnNotificationViewDragStarted(
    AshNotificationView* dragged_view) {
  DCHECK(dragged_view);
  DCHECK(!dragged_notification_id_);
  dragged_notification_id_ = dragged_view->notification_id();

  // The drag drop client in Ash, i.e. `DragDropController`, is a singleton.
  // Hence, always use the primary root window to access the drag drop client.
  drag_drop_client_observer_.Observe(
      aura::client::GetDragDropClient(Shell::GetPrimaryRootWindow()));

  // Hide the message center bubble if it is open.
  message_center::MessageCenter* message_center_ptr =
      message_center::MessageCenter::Get();
  if (message_center_ptr->IsMessageCenterVisible()) {
    StatusAreaWidget* status_area_widget =
        RootWindowController::ForWindow(
            dragged_view->GetWidget()->GetNativeView())
            ->GetStatusAreaWidget();
    TrayBackgroundView* message_center_bubble = nullptr;
    if (features::IsQsRevampEnabled()) {
      message_center_bubble = status_area_widget->notification_center_tray();
    } else {
      // If the quick setting revamp feature is not enabled, we should hide the
      // unified system tray bubble.
      message_center_bubble = status_area_widget->unified_system_tray();
    }

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
  message_center::Notification* notification =
      message_center_ptr->FindNotificationById(*dragged_notification_id_);
  message_center_ptr->MarkSinglePopupAsShown(
      notification->group_child()
          ? message_center_ptr->FindParentNotification(notification)->id()
          : *dragged_notification_id_,
      /*mark_notification_as_read=*/true);
}

void AshNotificationDragController::OnNotificationViewDragEnded() {
  DCHECK(dragged_notification_id_);
  dragged_notification_id_.reset();
  drag_drop_client_observer_.Reset();
}

}  // namespace ash
