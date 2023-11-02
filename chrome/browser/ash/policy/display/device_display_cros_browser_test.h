// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DISPLAY_DEVICE_DISPLAY_CROS_BROWSER_TEST_H_
#define CHROME_BROWSER_ASH_POLICY_DISPLAY_DEVICE_DISPLAY_CROS_BROWSER_TEST_H_

#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"

namespace policy {

// A helper class to be used in the different tests that deal with policy driven
// display setting changes.
class DeviceDisplayCrosTestHelper {
 public:
  DeviceDisplayCrosTestHelper() = default;
  DeviceDisplayCrosTestHelper(const DeviceDisplayCrosTestHelper&) = delete;
  DeviceDisplayCrosTestHelper& operator=(const DeviceDisplayCrosTestHelper&) =
      delete;
  ~DeviceDisplayCrosTestHelper() = default;

  display::DisplayManager* GetDisplayManager() const;
  int64_t GetFirstDisplayId() const;
  int64_t GetSecondDisplayId() const;
  const display::Display& GetFirstDisplay() const;
  const display::Display& GetSecondDisplay() const;
  display::Display::Rotation GetRotationOfFirstDisplay() const;
  display::Display::Rotation GetRotationOfSecondDisplay() const;
  double GetScaleOfFirstDisplay() const;
  double GetScaleOfSecondDisplay() const;
  gfx::Size GetResolutionOfFirstDisplay() const;
  gfx::Size GetResolutionOfSecondDisplay() const;
  // Creates second display if there is none yet, or removes it if there is one.
  void ToggleSecondDisplay();

 protected:
  gfx::Size GetResolutionOfDisplay(int64_t display_id) const;
  int GetScaleOfDisplay(int64_t display_id) const;
};

class DeviceDisplayPolicyCrosBrowserTest : public DevicePolicyCrosBrowserTest {
 public:
  DeviceDisplayPolicyCrosBrowserTest() = default;
  DeviceDisplayPolicyCrosBrowserTest(
      const DeviceDisplayPolicyCrosBrowserTest&) = delete;
  DeviceDisplayPolicyCrosBrowserTest& operator=(
      const DeviceDisplayPolicyCrosBrowserTest&) = delete;
  ~DeviceDisplayPolicyCrosBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override;
  void TearDownOnMainThread() override;

  DeviceDisplayCrosTestHelper* display_helper() { return &display_helper_; }

  void UnsetPolicy();

 private:
  DeviceDisplayCrosTestHelper display_helper_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DISPLAY_DEVICE_DISPLAY_CROS_BROWSER_TEST_H_
