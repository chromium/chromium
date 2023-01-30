// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_DRAG_CONTROLLER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_DRAG_CONTROLLER_H_

#include <string>

#include "base/scoped_observation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/views/drag_controller.h"

namespace aura::client {
class DragDropClient;
}  // namespace aura::client

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
class AshNotificationView;

// Handles drag on Ash notification views.
class AshNotificationDragController
    : public aura::client::DragDropClientObserver,
      public views::DragController {
 public:
  AshNotificationDragController();
  AshNotificationDragController(const AshNotificationDragController&) = delete;
  AshNotificationDragController& operator=(
      const AshNotificationDragController&) = delete;
  ~AshNotificationDragController() override;

 private:
  // aura::client::DragDropClientObserver:
  void OnDragCompleted(const ui::DropTargetEvent& event) override;
  void OnDragCancelled() override;

  // views::DragController:
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& p) override;
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  void OnNotificationViewDragStarted(AshNotificationView* dragged_view);
  void OnNotificationViewDragEnded();

  // Corresponds to the notification view under drag. Set/reset when the drag on
  // a notification view starts/ends.
  absl::optional<std::string> dragged_notification_id_;

  // Helps to track drag-and-drop events. Set/reset when the drag on a
  // notification view starts/ends.
  base::ScopedObservation<aura::client::DragDropClient,
                          aura::client::DragDropClientObserver>
      drag_drop_client_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_DRAG_CONTROLLER_H_
