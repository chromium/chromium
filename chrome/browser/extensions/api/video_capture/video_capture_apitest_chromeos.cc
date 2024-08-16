// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_mode/consumer_kiosk_test_helper.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kTestingAppId[] = "lplgglkemdojicmnmlcfoaokgcobcpei";
}  // namespace

namespace extensions {
namespace {

// This class contains API tests related to the "videoCapture" permission.
class VideoCaptureApiTestChromeOs : public PlatformAppBrowserTest {
 public:
  VideoCaptureApiTestChromeOs() : settings_helper_(false) {}
  ~VideoCaptureApiTestChromeOs() override = default;

  void SetUpOnMainThread() override {
    PlatformAppBrowserTest::SetUpOnMainThread();
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    owner_settings_service_ =
        settings_helper_.CreateOwnerSettingsService(browser()->profile());
    // Verify fake devices are enabled. This is necessary to make sure there is
    // at least one device in the system. Otherwise, this test would fail on
    // machines without physical media devices since getUserMedia fails early in
    // those cases.
    EXPECT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
  }

  void TearDownOnMainThread() override {
    owner_settings_service_.reset();
    settings_helper_.RestoreRealDeviceSettingsProvider();
    user_manager_.Reset();
    PlatformAppBrowserTest::TearDownOnMainThread();
  }

 protected:
  void EnterKioskSession() {
    user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    chromeos::SetUpFakeKioskSession();
  }

  void SetAutoLaunchApp() {
    AddConsumerKioskChromeAppForTesting(
        CHECK_DEREF(owner_settings_service_.get()), kTestingAppId);
    SetConsumerKioskAutoLaunchChromeAppForTesting(
        CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()),
        kTestingAppId);
    manager()->SetAppWasAutoLaunchedWithZeroDelay(kTestingAppId);
  }

  ash::KioskChromeAppManager* manager() const {
    return ash::KioskChromeAppManager::Get();
  }

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_;

  ash::ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<ash::FakeOwnerSettingsService> owner_settings_service_;
};

IN_PROC_BROWSER_TEST_F(VideoCaptureApiTestChromeOs,
                       CameraPanTiltZoom_NoKioskSession) {
  ASSERT_TRUE(RunExtensionTest(
      "api_test/video_capture/camera_pan_tilt_zoom_no_kiosk_session",
      {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(VideoCaptureApiTestChromeOs,
                       CameraPanTiltZoom_KioskSessionOnly) {
  EnterKioskSession();
  ASSERT_TRUE(
      RunExtensionTest("api_test/video_capture/"
                       "camera_pan_tilt_zoom_kiosk_session_only",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(VideoCaptureApiTestChromeOs, CameraPanTiltZoom) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(RunExtensionTest("api_test/video_capture/camera_pan_tilt_zoom",
                               {.launch_as_platform_app = true}))
      << message_;
}

}  // namespace
}  // namespace extensions
