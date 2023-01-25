// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_DRAG_CONTROLLER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_DRAG_CONTROLLER_H_

#include "ui/views/drag_controller.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {
class OSExchangeData;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

// Handles drag on Ash notification views.
class AshNotificationDragController : public views::DragController {
 public:
  AshNotificationDragController();
  AshNotificationDragController(const AshNotificationDragController&) = delete;
  AshNotificationDragController& operator=(
      const AshNotificationDragController&) = delete;
  ~AshNotificationDragController() override;

 private:
  // views::DragController:
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& p) override;
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_DRAG_CONTROLLER_H_
