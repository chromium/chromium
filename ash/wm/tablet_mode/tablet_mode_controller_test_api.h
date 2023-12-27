// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_TEST_API_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_TEST_API_H_

#include <memory>

#include "ash/wm/tablet_mode/internal_input_devices_event_blocker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class InternalInputDevicesEventBlocker;
class TabletModeController;
class TabletModeWindowManager;

// Use the api in this class to test TabletModeController.
class TabletModeControllerTestApi {
 public:
  static constexpr float kDegreesToRadians = 3.1415926f / 180.0f;

  TabletModeControllerTestApi();

  TabletModeControllerTestApi(const TabletModeControllerTestApi&) = delete;
  TabletModeControllerTestApi& operator=(const TabletModeControllerTestApi&) =
      delete;

  ~TabletModeControllerTestApi();

  // Enters or exits tablet mode. Use these instead when stuff such as tray
  // visibilty depends on the event blocker instead of the actual tablet mode.
  void EnterTabletMode();
  void LeaveTabletMode();

  // Called to attach an external mouse/touchpad. If we're currently in tablet
  // mode, tablet mode will be ended because of this.
  void AttachExternalMouse();
  void AttachExternalTouchpad();

  // Called in association with the above to remove all mice/touchpads.
  void DetachAllMice();
  void DetachAllTouchpads();

  void TriggerLidUpdate(const gfx::Vector3dF& lid);
  void TriggerBaseAndLidUpdate(const gfx::Vector3dF& base,
                               const gfx::Vector3dF& lid);

  void OpenLidToAngle(float degrees);
  void HoldDeviceVertical();
  void OpenLid();
  void CloseLid();
  void SetTabletMode(bool on);

  // Called to simulate the device suspend and resume.
  void SuspendImminent();
  void SuspendDone(base::TimeDelta sleep_duration);

  // Sets the event blocker on the tablet mode controller.
  void set_event_blocker(
      std::unique_ptr<InternalInputDevicesEventBlocker> blocker) {
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

  bool AreEventsBlocked() const {
    return tablet_mode_controller_->AreInternalInputDeviceEventsBlocked();
  }

  bool IsScreenshotShown() const {
    return !!tablet_mode_controller_->screenshot_layer_;
  }

  bool IsInPhysicalTabletState() const {
    return tablet_mode_controller_->is_in_tablet_physical_state();
  }

  float GetLidAngle() const { return tablet_mode_controller_->lid_angle(); }

 private:
  raw_ptr<TabletModeController> tablet_mode_controller_;
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_TEST_API_H_
