// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains interactive UI tests for the system permission settings
// integration. Specifically, it tests the cross-platform caching mechanism
// (`SystemMediaPermissionCache`) for OS-level camera and microphone
// permissions. The tests verify that the cached permission state correctly
// updates when the browser window regains focus, and that `IsDeniedFresh` can
// successfully bypass the cache to retrieve real-time state from the OS.
#include "chrome/browser/permissions/system/system_permission_settings.h"

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/permissions/system/system_media_permission_cache_mac_test_helper.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/browser/permissions/system/system_media_source_win.h"
#endif

namespace system_permission_settings {

namespace {

class SystemPermissionSettingsBrowserTest : public InProcessBrowserTest {
 public:
  SystemPermissionSettingsBrowserTest() {
#if BUILDFLAG(IS_MAC)
    SetUseFakeMediaStreamDevices(false);
#elif BUILDFLAG(IS_WIN)
    SystemMediaSourceWin::GetInstance().SetMockStatus(
        ContentSettingsType::MEDIASTREAM_CAMERA,
        SystemMediaSourceWin::Status::kNotDetermined);
    SystemMediaSourceWin::GetInstance().SetMockStatus(
        ContentSettingsType::MEDIASTREAM_MIC,
        SystemMediaSourceWin::Status::kNotDetermined);
#endif
  }
  ~SystemPermissionSettingsBrowserTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    mac_test_helper_ =
        std::make_unique<SystemMediaPermissionCacheMacTestHelper>();
#endif
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
#if BUILDFLAG(IS_MAC)
    mac_test_helper_.reset();
    InProcessBrowserTest::TearDown();
#elif BUILDFLAG(IS_WIN)
    SystemMediaSourceWin::GetInstance().SetMockStatus(
        ContentSettingsType::MEDIASTREAM_CAMERA, std::nullopt);
    SystemMediaSourceWin::GetInstance().SetMockStatus(
        ContentSettingsType::MEDIASTREAM_MIC, std::nullopt);
    InProcessBrowserTest::TearDown();
#else
    InProcessBrowserTest::TearDown();
#endif
  }

  void SetMockStatus(bool is_denied) {
#if BUILDFLAG(IS_MAC)
    mac_test_helper_->SetCameraStatus(is_denied);
    mac_test_helper_->SetMicStatus(is_denied);
#elif BUILDFLAG(IS_WIN)
    SystemMediaSourceWin::GetInstance().SetMockStatus(
        ContentSettingsType::MEDIASTREAM_CAMERA,
        is_denied ? SystemMediaSourceWin::Status::kDenied
                  : SystemMediaSourceWin::Status::kAllowed);
    SystemMediaSourceWin::GetInstance().SetMockStatus(
        ContentSettingsType::MEDIASTREAM_MIC,
        is_denied ? SystemMediaSourceWin::Status::kDenied
                  : SystemMediaSourceWin::Status::kAllowed);
#endif
  }

 protected:
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<SystemMediaPermissionCacheMacTestHelper> mac_test_helper_;
#endif
};

}  // namespace

// Verifies that when the browser window regains focus (is activated), the
// system permission cache is refreshed in the background. It tests this by
// mutating the mock system permission, forcing a window deactivation and
// reactivation, and then validating that the cached values eventually match
// the new mock state.
IN_PROC_BROWSER_TEST_F(SystemPermissionSettingsBrowserTest,
                       TestWindowActivationRefresh) {
  // Initial state is NotDetermined. Cache will be updated asynchronously.
  // Wait for the initial background task to populate the cache.
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

  // 1. Change mock status to Denied.
  SetMockStatus(true);

  // The cache is still Stale.
  EXPECT_FALSE(system_permission_settings::IsDenied(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  // 2. Simulate deactivation and reactivation of the browser window.
  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
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
}

// Verifies that calling `IsDeniedFresh` will query the underlying OS permission
// directly (bypassing any stale cached values) and that the cache is
// subsequently updated with the fresh result.
IN_PROC_BROWSER_TEST_F(SystemPermissionSettingsBrowserTest,
                       TestIsDeniedFreshBypassesCache) {
  // Wait for the initial background task to populate the cache.
  base::RunLoop run_loop_init;
  system_permission_settings::IsDeniedFresh(
      ContentSettingsType::MEDIASTREAM_CAMERA,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool is_denied) {
            EXPECT_FALSE(is_denied);
            run_loop->Quit();
          },
          &run_loop_init));
  run_loop_init.Run();

  // 1. Change mock status to Allowed.
  SetMockStatus(false);

  // The cached value is still NotDetermined or Denied from earlier.
  // Since we don't know exactly what state it's in (we started in
  // NotDetermined), we just ensure IsDenied returns false.
  EXPECT_FALSE(system_permission_settings::IsDenied(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  // 2. Test IsDeniedFresh - it should return the mock value directly and update
  // the cache.
  base::RunLoop run_loop_fresh;
  system_permission_settings::IsDeniedFresh(
      ContentSettingsType::MEDIASTREAM_CAMERA,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool is_denied) {
            EXPECT_FALSE(is_denied);
            run_loop->Quit();
          },
          &run_loop_fresh));
  run_loop_fresh.Run();

  // The cached value must have been updated to Allowed (not denied).
  EXPECT_FALSE(system_permission_settings::IsDenied(
      ContentSettingsType::MEDIASTREAM_CAMERA));
}

}  // namespace system_permission_settings
