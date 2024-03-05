// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_ASH_NOTIFICATION_DRAG_CONTROLLER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_ASH_NOTIFICATION_DRAG_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
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
  FRIEND_TEST_ALL_PREFIXES(AshNotificationViewDragTest, Basics);

  // Lists notification drag end states.
  // NOTE: used by metrics. Therefore, current values should not be renumbered
  // or removed. This should be kept in sync with the enum
  // `NotificationImageDragEndState` in
  // tools/metrics/histograms/metadata/ash/enums.xml.
  enum class DragEndState {
    // Interrupted by a new drag session before the current one finishes.
    kInterruptedByNewDrag = 0,

    // Cancelled by users.
    kCancelled = 1,

    // Drag completes and the notification image is dropped to the target.
    kCompletedWithDrop = 2,

    // Drag completes and the notification image is NOT dropped to the target.
    kCompletedWithoutDrop = 3,

    kMaxValue = kCompletedWithoutDrop,
  };

  // aura::client::DragDropClientObserver:
  void OnDragStarted() override;
  void OnDragCancelled() override;
  void OnDropCompleted(ui::mojom::DragOperation drag_operation) override;

  // views::DragController:
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& p) override;
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;
  void OnWillStartDragForView(views::View* dragged_view) override;

  void OnNotificationDragWillStart(AshNotificationView* dragged_view);

  // Cleans up the data members for the current drag-and-drop session. This
  // method gets called when:
  // 1. Drag is cancelled; or
  // 2. Drop is completed; or
  // 3. A new drag-and-drop session starts without waiting for the current
  // async drop to finish.
  void CleanUp(DragEndState state);

  // True if there is a notification drag being handled.
  bool drag_in_progress_ = false;

  // Corresponds to the notification view under drag. Set/reset when the drag on
  // a notification view starts/ends.
  std::optional<std::string> dragged_notification_id_;

  // Helps to track drag-and-drop events. Set/reset when the drag on a
  // notification view starts/ends.
  base::ScopedObservation<aura::client::DragDropClient,
                          aura::client::DragDropClientObserver>
      drag_drop_client_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_ASH_NOTIFICATION_DRAG_CONTROLLER_H_
