// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_drag_controller.h"

#include "ash/system/message_center/ash_notification_view.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace ash {

AshNotificationDragController::AshNotificationDragController() = default;

AshNotificationDragController::~AshNotificationDragController() = default;

void AshNotificationDragController::WriteDragDataForView(
    views::View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {
  AshNotificationView* notification_view =
      static_cast<AshNotificationView*>(sender);
  const absl::optional<gfx::Rect> drag_area =
      notification_view->GetDragAreaBounds();
  DCHECK(drag_area);

  const absl::optional<gfx::ImageSkia> drag_image =
      notification_view->GetDragImage();
  DCHECK(drag_image);
  data->provider().SetDragImage(*drag_image, press_pt - drag_area->origin());
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
  const absl::optional<gfx::Rect> drag_area =
      static_cast<AshNotificationView*>(sender)->GetDragAreaBounds();

  // Enable dragging `notification_view_` if:
  // 1. `notification_view_` is draggable; and
  // 2. `drag_area` contains the initial press point.
  return drag_area && drag_area->Contains(press_pt);
}

}  // namespace ash
