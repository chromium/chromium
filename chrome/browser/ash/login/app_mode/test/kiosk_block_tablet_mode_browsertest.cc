// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// UI should always be in the clamshell mode during the kiosk session. But if
// the device is in a physical tablet mode (e.g. the lid is flipped) the
// internal events like keyboard inputs should be blocked.
class KioskBlockTabletModeTest : public WebKioskBaseTest {
 public:
  KioskBlockTabletModeTest() = default;
  KioskBlockTabletModeTest(const KioskBlockTabletModeTest&) = delete;
  KioskBlockTabletModeTest& operator=(const KioskBlockTabletModeTest&) = delete;
  ~KioskBlockTabletModeTest() override = default;

  void SetUpOnMainThread() override {
    WebKioskBaseTest::SetUpOnMainThread();
    tablet_test_api_ = std::make_unique<TabletModeControllerTestApi>();
  }

  void TearDownOnMainThread() override {
    tablet_test_api_.reset();
    WebKioskBaseTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<TabletModeControllerTestApi> tablet_test_api_;
};

IN_PROC_BROWSER_TEST_F(KioskBlockTabletModeTest, TabletModeIsBlocked) {
  InitializeRegularOnlineKiosk();

  tablet_test_api_->EnterTabletMode();

  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(tablet_test_api_->IsInPhysicalTabletState());
  EXPECT_TRUE(tablet_test_api_->AreEventsBlocked());
}

IN_PROC_BROWSER_TEST_F(KioskBlockTabletModeTest,
                       SwitchToClamshellModeWhenKioskStarts) {
  tablet_test_api_->EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(tablet_test_api_->IsInPhysicalTabletState());
  EXPECT_TRUE(tablet_test_api_->AreEventsBlocked());

  InitializeRegularOnlineKiosk();

  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(tablet_test_api_->IsInPhysicalTabletState());
  EXPECT_TRUE(tablet_test_api_->AreEventsBlocked());
}

}  // namespace ash
