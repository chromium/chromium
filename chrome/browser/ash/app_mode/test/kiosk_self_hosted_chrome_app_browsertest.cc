// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/fake_cws_mixin.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/app_mode/test/test_app_data_load_waiter.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::LaunchAppManually;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;

namespace {

// The chrome app used to configure `KioskMixin`.`
KioskMixin::CwsChromeAppOption ChromeAppInConfig() {
  return KioskMixin::SimpleChromeAppOption();
}

}  // namespace

// Verifies that self hosted Chrome apps work in Kiosk.
class KioskSelfHostedChromeAppTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskSelfHostedChromeAppTest() = default;
  KioskSelfHostedChromeAppTest(const KioskSelfHostedChromeAppTest&) = delete;
  KioskSelfHostedChromeAppTest& operator=(const KioskSelfHostedChromeAppTest&) =
      delete;
  ~KioskSelfHostedChromeAppTest() override = default;

  FakeCWS& private_cws() { return private_cws_.fake_cws(); }

 protected:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    private_cws().SetUpdateCrx(ChromeAppInConfig().app_id,
                               ChromeAppInConfig().crx_filename,
                               ChromeAppInConfig().crx_version);
  }

  FakeCwsMixin private_cws_{&mixin_host_,
                            FakeCwsMixin::CwsInstanceType::kPrivate};

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/KioskMixin::Config{
                        /*name=*/{},
                        /*auto_launch_account_id=*/{},
                        {KioskMixin::SelfHostedChromeAppOption(
                            /*account_id=*/ChromeAppInConfig().account_id,
                            /*app_id=*/ChromeAppInConfig().app_id,
                            /*update_url=*/private_cws_.UpdateUrl())}}};
};

IN_PROC_BROWSER_TEST_F(KioskSelfHostedChromeAppTest,
                       AppManagerExtractsCrxMetadata) {
  auto& manager = CHECK_DEREF(KioskChromeAppManager::Get());
  TestAppDataLoadWaiter waiter(&manager, ChromeAppInConfig().app_id,
                               /*version=*/std::string());
  waiter.WaitForAppData();
  ASSERT_TRUE(waiter.loaded());
}

IN_PROC_BROWSER_TEST_F(KioskSelfHostedChromeAppTest, LaunchesSelfHostedApp) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());

  // Update checks should be made to the private store instead of CWS.
  EXPECT_GT(private_cws().GetUpdateCheckCountAndReset(), 0);
}

}  // namespace ash
