// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_TEST_API_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_TEST_API_H_

#include <memory>

#include "ash/wm/tablet_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"

namespace ash {

class ScopedDisableInternalMouseAndKeyboard;
class TabletModeController;
class TabletModeWindowManager;

// Use the api in this class to test TabletModeController.
class TabletModeControllerTestApi {
 public:
  static constexpr float kDegreesToRadians = 3.1415926f / 180.0f;
  static constexpr float kMeanGravity = 9.8066f;

  TabletModeControllerTestApi();
  ~TabletModeControllerTestApi();

  // Enters or exits tablet mode. Use these instead when stuff such as tray
  // visibilty depends on the event blocker instead of the actual tablet mode.
  void EnterTabletMode();
  void LeaveTabletMode();

  // Called to attach an external mouse. If we're currently in tablet mode,
  // tablet mode will be ended because of this.
  void AttachExternalMouse();

  void TriggerLidUpdate(const gfx::Vector3dF& lid);
  void TriggerBaseAndLidUpdate(const gfx::Vector3dF& base,
                               const gfx::Vector3dF& lid);

  void OpenLidToAngle(float degrees);
  void HoldDeviceVertical();
  void OpenLid();
  void CloseLid();
  void SetTabletMode(bool on);

  // Sets the event blocker on the tablet mode controller.
  void set_event_blocker(
      std::unique_ptr<ScopedDisableInternalMouseAndKeyboard> blocker) {
    tablet_mode_controller_->event_blocker_ = std::move(blocker);
  }

  TabletModeWindowManager* tablet_mode_window_manager() {
    return tablet_mode_controller_->tablet_mode_window_manager_.get();
  }

  // Set the TickClock. This is only to be used by tests that need to
  // artificially and deterministically control the current time.
  // This does not take the ownership of the tick_clock. |tick_clock| must
  // outlive the TabletModeController instance.
  void set_tick_clock(const base::TickClock* tick_clock) {
    DCHECK(tick_clock);
    tablet_mode_controller_->tick_clock_ = tick_clock;
  }
  const base::TickClock* tick_clock() {
    return tablet_mode_controller_->tick_clock_;
  }

  bool CanUseUnstableLidAngle() const {
    return tablet_mode_controller_->CanUseUnstableLidAngle();
  }

  TabletModeController::UiMode force_ui_mode() const {
    return tablet_mode_controller_->force_ui_mode_;
  }

  bool IsTabletModeStarted() const {
    return tablet_mode_controller_->IsTabletModeWindowManagerEnabled();
  }

  bool AreEventsBlocked() const {
    return tablet_mode_controller_->AreInternalInputDeviceEventsBlocked();
  }

 private:
  TabletModeController* tablet_mode_controller_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeControllerTestApi);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_TEST_API_H_
