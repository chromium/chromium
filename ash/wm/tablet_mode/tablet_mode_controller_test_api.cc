// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/numerics/math_constants.h"
#include "base/run_loop.h"
#include "base/time/default_tick_clock.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchpad_device.h"

namespace ash {

namespace {

bool IsTabletModeControllerInitialized() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableTabletMode);
}

}  // namespace

TabletModeControllerTestApi::TabletModeControllerTestApi()
    : tablet_mode_controller_(Shell::Get()->tablet_mode_controller()) {
  tablet_mode_controller_->OnDeviceListsComplete();
}

TabletModeControllerTestApi::~TabletModeControllerTestApi() = default;

void TabletModeControllerTestApi::EnterTabletMode() {
  tablet_mode_controller_->SetEnabledForTest(true);
}

void TabletModeControllerTestApi::LeaveTabletMode() {
  tablet_mode_controller_->SetEnabledForTest(false);
}

void TabletModeControllerTestApi::AttachExternalMouse() {
  // Calling RunUntilIdle() here is necessary before setting the mouse devices
  // to prevent the callback from evdev thread to overwrite whatever we set
  // here below. See `InputDeviceFactoryEvdevProxy::OnStartupScanComplete()`.
  base::RunLoop().RunUntilIdle();
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
  if (!IsTabletModeControllerInitialized()) {
    // The controller is not observing the DeviceDataManager, hence we need to
    // notify it ourselves.
    tablet_mode_controller_->OnInputDeviceConfigurationChanged(
        ui::InputDeviceEventObserver::kMouse);
  }
}

void TabletModeControllerTestApi::AttachExternalTouchpad() {
  // Similar to |AttachExternalMouse|.
  base::RunLoop().RunUntilIdle();
  ui::DeviceDataManagerTestApi().SetTouchpadDevices({ui::TouchpadDevice(
      4, ui::InputDeviceType::INPUT_DEVICE_USB, "touchpad")});
  if (!IsTabletModeControllerInitialized()) {
    tablet_mode_controller_->OnInputDeviceConfigurationChanged(
        ui::InputDeviceEventObserver::kTouchpad);
  }
}

void TabletModeControllerTestApi::DetachAllMice() {
  // See comment in |AttachExternalMouse| for why we need to call
  // |base::RunLoop::RunUntilIdle|.
  base::RunLoop().RunUntilIdle();
  ui::DeviceDataManagerTestApi().SetMouseDevices({});
  if (!IsTabletModeControllerInitialized()) {
    // The controller is not observing the DeviceDataManager, hence we need to
    // notify it ourselves.
    tablet_mode_controller_->OnInputDeviceConfigurationChanged(
        ui::InputDeviceEventObserver::kMouse);
  }
}

void TabletModeControllerTestApi::DetachAllTouchpads() {
  // Similar to |DetachAllMice|.
  base::RunLoop().RunUntilIdle();
  ui::DeviceDataManagerTestApi().SetTouchpadDevices({});
  if (!IsTabletModeControllerInitialized()) {
    tablet_mode_controller_->OnInputDeviceConfigurationChanged(
        ui::InputDeviceEventObserver::kTouchpad);
  }
}

void TabletModeControllerTestApi::TriggerLidUpdate(const gfx::Vector3dF& lid) {
  AccelerometerUpdate update;
  update.Set(ACCELEROMETER_SOURCE_SCREEN, lid.x(), lid.y(), lid.z());
  tablet_mode_controller_->OnAccelerometerUpdated(update);
}

void TabletModeControllerTestApi::TriggerBaseAndLidUpdate(
    const gfx::Vector3dF& base,
    const gfx::Vector3dF& lid) {
  AccelerometerUpdate update;
  update.Set(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, base.x(), base.y(),
             base.z());
  update.Set(ACCELEROMETER_SOURCE_SCREEN, lid.x(), lid.y(), lid.z());
  tablet_mode_controller_->OnAccelerometerUpdated(update);
}

void TabletModeControllerTestApi::OpenLidToAngle(float degrees) {
  DCHECK(degrees >= 0.0f);
  DCHECK(degrees <= 360.0f);

  float radians = degrees * kDegreesToRadians;
  gfx::Vector3dF base_vector(0.0f, -base::kMeanGravityFloat, 0.0f);
  gfx::Vector3dF lid_vector(0.0f, base::kMeanGravityFloat * cos(radians),
                            base::kMeanGravityFloat * sin(radians));
  TriggerBaseAndLidUpdate(base_vector, lid_vector);
}

void TabletModeControllerTestApi::HoldDeviceVertical() {
  gfx::Vector3dF base_vector(9.8f, 0.0f, 0.0f);
  gfx::Vector3dF lid_vector(9.8f, 0.0f, 0.0f);
  TriggerBaseAndLidUpdate(base_vector, lid_vector);
}

void TabletModeControllerTestApi::OpenLid() {
  tablet_mode_controller_->LidEventReceived(
      chromeos::PowerManagerClient::LidState::OPEN, tick_clock()->NowTicks());
}

void TabletModeControllerTestApi::CloseLid() {
  tablet_mode_controller_->LidEventReceived(
      chromeos::PowerManagerClient::LidState::CLOSED, tick_clock()->NowTicks());
}

void TabletModeControllerTestApi::SetTabletMode(bool on) {
  tablet_mode_controller_->TabletModeEventReceived(
      on ? chromeos::PowerManagerClient::TabletMode::ON
         : chromeos::PowerManagerClient::TabletMode::OFF,
      tick_clock()->NowTicks());
}

void TabletModeControllerTestApi::SuspendImminent() {
  tablet_mode_controller_->SuspendImminent(
      power_manager::SuspendImminent::Reason::SuspendImminent_Reason_IDLE);
}

void TabletModeControllerTestApi::SuspendDone(base::TimeDelta sleep_duration) {
  tablet_mode_controller_->SuspendDone(sleep_duration);
}

}  // namespace ash
