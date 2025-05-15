// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::LaunchAppManually;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;

namespace {

bool LaunchKioskAppAndWaitSession() {
  return LaunchAppManually(TheKioskApp()) && WaitKioskLaunched();
}

}  // namespace

// UI should always be in the clamshell mode during the kiosk session. But if
// the device is in a physical tablet mode (e.g. the lid is flipped) the
// internal events like keyboard inputs should be blocked.
class KioskBlockTabletModeTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskBlockTabletModeTest() {
    // Force allow Chrome Apps in Kiosk, since they are default disabled since
    // M138.
    scoped_feature_list_.InitFromCommandLine("AllowChromeAppsInKioskSessions",
                                             "");
  }

  KioskBlockTabletModeTest(const KioskBlockTabletModeTest&) = delete;
  KioskBlockTabletModeTest& operator=(const KioskBlockTabletModeTest&) = delete;
  ~KioskBlockTabletModeTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    tablet_test_api_ = std::make_unique<TabletModeControllerTestApi>();
  }

  void TearDownOnMainThread() override {
    tablet_test_api_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<TabletModeControllerTestApi> tablet_test_api_;
  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/GetParam()};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(KioskBlockTabletModeTest, TabletModeIsBlocked) {
  ASSERT_TRUE(LaunchKioskAppAndWaitSession());

  tablet_test_api_->EnterTabletMode();

  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(tablet_test_api_->IsInPhysicalTabletState());
  EXPECT_TRUE(tablet_test_api_->AreEventsBlocked());
}

IN_PROC_BROWSER_TEST_P(KioskBlockTabletModeTest,
                       SwitchToClamshellModeWhenKioskStarts) {
  tablet_test_api_->EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(tablet_test_api_->IsInPhysicalTabletState());
  EXPECT_TRUE(tablet_test_api_->AreEventsBlocked());

  ASSERT_TRUE(LaunchKioskAppAndWaitSession());

  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(tablet_test_api_->IsInPhysicalTabletState());
  EXPECT_TRUE(tablet_test_api_->AreEventsBlocked());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskBlockTabletModeTest,
    testing::Values(KioskMixin::Config{/*name=*/"WebApp",
                                       /*auto_launch_account_id=*/{},
                                       {KioskMixin::SimpleWebAppOption()}},
                    KioskMixin::Config{/*name=*/"ChromeApp",
                                       /*auto_launch_account_id=*/{},
                                       {KioskMixin::SimpleChromeAppOption()}}),
    KioskMixin::ConfigName);

}  // namespace ash
