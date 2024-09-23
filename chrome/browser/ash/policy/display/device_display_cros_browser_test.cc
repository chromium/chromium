// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/display/device_display_cros_browser_test.h"

#include "ash/display/display_configuration_controller.h"
#include "ash/shell.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "ui/display/display.h"

namespace em = enterprise_management;

namespace policy {

gfx::Size DeviceDisplayCrosTestHelper::GetResolutionOfDisplay(
    int64_t display_id) const {
  display::ManagedDisplayMode display_mode;
  if (GetDisplayManager()->GetSelectedModeForDisplayId(display_id,
                                                       &display_mode)) {
    return display_mode.size();
  }
  const display::Display& display =
      GetDisplayManager()->GetDisplayForId(display_id);
  return display.GetSizeInPixel();
}

int DeviceDisplayCrosTestHelper::GetScaleOfDisplay(int64_t display_id) const {
  // Converting scale to percents.
  display::ManagedDisplayMode display_mode;
  const display::Display& display =
      GetDisplayManager()->GetDisplayForId(display_id);
  return floor(display.device_scale_factor() * 100.0 + 0.5);
}

display::DisplayManager* DeviceDisplayCrosTestHelper::GetDisplayManager()
    const {
  return ash::Shell::Get()->display_manager();
}

int64_t DeviceDisplayCrosTestHelper::GetFirstDisplayId() const {
  return GetDisplayManager()->first_display_id();
}

int64_t DeviceDisplayCrosTestHelper::GetSecondDisplayId() const {
  if (GetDisplayManager()->GetNumDisplays() < 2) {
    ADD_FAILURE() << "The second display is not connected.";
    return 0;
  }
  return GetDisplayManager()->GetConnectedDisplayIdList()[1];
}

const display::Display& DeviceDisplayCrosTestHelper::GetFirstDisplay() const {
  return GetDisplayManager()->GetDisplayForId(GetFirstDisplayId());
}

const display::Display& DeviceDisplayCrosTestHelper::GetSecondDisplay() const {
  return GetDisplayManager()->GetDisplayForId(GetSecondDisplayId());
}

display::Display::Rotation
DeviceDisplayCrosTestHelper::GetRotationOfFirstDisplay() const {
  return GetFirstDisplay().rotation();
}

// Fails the test and returns ROTATE_0 if there is no second display.
display::Display::Rotation
DeviceDisplayCrosTestHelper::GetRotationOfSecondDisplay() const {
  return GetSecondDisplay().rotation();
}

double DeviceDisplayCrosTestHelper::GetScaleOfFirstDisplay() const {
  return GetScaleOfDisplay(GetFirstDisplayId());
}

double DeviceDisplayCrosTestHelper::GetScaleOfSecondDisplay() const {
  return GetScaleOfDisplay(GetSecondDisplayId());
}

gfx::Size DeviceDisplayCrosTestHelper::GetResolutionOfFirstDisplay() const {
  return GetResolutionOfDisplay(GetFirstDisplayId());
}

gfx::Size DeviceDisplayCrosTestHelper::GetResolutionOfSecondDisplay() const {
  return GetResolutionOfDisplay(GetSecondDisplayId());
}

void DeviceDisplayCrosTestHelper::ToggleSecondDisplay() {
  GetDisplayManager()->AddRemoveDisplay();
  base::RunLoop().RunUntilIdle();
}

void DeviceDisplayPolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture() {
  ash::SessionManagerClient::InitializeFakeInMemory();
  ash::DisplayConfigurationController::DisableAnimatorForTest();
  DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
}

void DeviceDisplayPolicyCrosBrowserTest::TearDownOnMainThread() {
  // If the login display is still showing, exit gracefully.
  if (ash::LoginDisplayHost::default_host()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::AttemptExit));
    RunUntilBrowserProcessQuits();
  }
  DevicePolicyCrosBrowserTest::TearDownOnMainThread();
}

}  // namespace policy
