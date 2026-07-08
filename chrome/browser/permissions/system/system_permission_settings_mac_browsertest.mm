// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#import <AVFoundation/AVFoundation.h>

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "chrome/browser/permissions/system/media_authorization_wrapper_mac.h"
#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_permission_settings {

namespace {

class MockMediaAuthorizationWrapper : public MediaAuthorizationWrapper {
 public:
  MockMediaAuthorizationWrapper() = default;
  ~MockMediaAuthorizationWrapper() override = default;

  AVAuthorizationStatus AuthorizationStatusForMediaType(
      AVMediaType media_type) override {
    if ([media_type isEqualToString:AVMediaTypeVideo]) {
      return camera_status;
    } else if ([media_type isEqualToString:AVMediaTypeAudio]) {
      return mic_status;
    }
    return AVAuthorizationStatusNotDetermined;
  }

  void RequestAccessForMediaType(AVMediaType media_type,
                                 base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  AVAuthorizationStatus camera_status = AVAuthorizationStatusNotDetermined;
  AVAuthorizationStatus mic_status = AVAuthorizationStatusNotDetermined;
};

class SystemPermissionSettingsMacTest : public InProcessBrowserTest {
 public:
  SystemPermissionSettingsMacTest() { SetUseFakeMediaStreamDevices(false); }
  ~SystemPermissionSettingsMacTest() override = default;

  void SetUp() override {
    system_permission_settings::SetMediaAuthorizationWrapperForTesting(
        &mock_wrapper_);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    system_permission_settings::SetMediaAuthorizationWrapperForTesting(nullptr);
  }

 protected:
  MockMediaAuthorizationWrapper mock_wrapper_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SystemPermissionSettingsMacTest,
                       TestCachingAndActivation) {
  // Initial state is NotDetermined. Cache will be updated asynchronously.
  // Wait for the initial background task to populate the cache.
  ASSERT_TRUE(base::test::RunUntil([]() {
    return !system_permission_settings::IsAllowed(
               ContentSettingsType::MEDIASTREAM_CAMERA) &&
           !system_permission_settings::IsDenied(
               ContentSettingsType::MEDIASTREAM_CAMERA) &&
           system_permission_settings::CanPrompt(
               ContentSettingsType::MEDIASTREAM_CAMERA);
  }));

  // 1. Change mock status to Denied.
  mock_wrapper_.camera_status = AVAuthorizationStatusDenied;
  mock_wrapper_.mic_status = AVAuthorizationStatusDenied;

  // The cache is still Stale (NotDetermined / allowed=false / denied=false).
  EXPECT_FALSE(system_permission_settings::IsDenied(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  // 2. Simulate deactivation and reactivation of the browser window.
  // We can do this by creating a second browser window, activating it, and then
  // activating the first browser window.
  Browser* second_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second_browser));
  ASSERT_FALSE(browser()->GetWindow()->IsActive());

  ui_test_utils::BrowserActivationWaiter waiter(browser());
  browser()->GetWindow()->Activate();
  waiter.WaitForActivation();

  // The activation should trigger a background task to refresh the cache.
  // Wait until the cache reflects the denied state.
  ASSERT_TRUE(base::test::RunUntil([]() {
    return system_permission_settings::IsDenied(
               ContentSettingsType::MEDIASTREAM_CAMERA) &&
           system_permission_settings::IsDenied(
               ContentSettingsType::MEDIASTREAM_MIC);
  }));

  // 3. Test IsDeniedFresh - it should return the mock value directly and update
  // the cache.
  mock_wrapper_.camera_status = AVAuthorizationStatusAuthorized;

  // The cached value is still Denied.
  EXPECT_TRUE(system_permission_settings::IsDenied(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  base::RunLoop run_loop;
  system_permission_settings::IsDeniedFresh(
      ContentSettingsType::MEDIASTREAM_CAMERA,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool is_denied) {
            EXPECT_FALSE(is_denied);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  // The cached value must have been updated to Allowed (not denied) by
  // IsDeniedFresh.
  EXPECT_FALSE(system_permission_settings::IsDenied(
      ContentSettingsType::MEDIASTREAM_CAMERA));
}

}  // namespace system_permission_settings
