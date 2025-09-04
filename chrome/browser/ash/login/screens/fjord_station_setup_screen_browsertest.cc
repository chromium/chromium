// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_station_setup_screen.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/fjord_station_setup_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

constexpr char kStationSetupId[] = "fjord-station-setup";
constexpr char kSrcAttribute[] = "src";
constexpr char kTextKeyAttribute[] = "textKey";
constexpr char kExpectedStationSetupUrl[] =
    "http://codec.localhost:27702/oobe/station-setup";
constexpr char kExpectedFinishSetupUrl[] =
    "http://codec.localhost:27702/oobe/finish-setup";
constexpr char kExpectedNextButtonTextKey[] = "fjordStationSetupNextButton";
constexpr char kExpectedDoneButtonTextKey[] = "fjordStationSetupDoneButton";

const test::UIPath kStationSetupFramePath = {kStationSetupId,
                                             "fjordStationSetupFrame"};
const test::UIPath kActionButtonPath = {kStationSetupId, "primaryButton"};

class FjordStationSetupScreenTest : public OobeBaseTest {
 public:
  FjordStationSetupScreenTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kFjordOobeForceEnabled);
  }
  ~FjordStationSetupScreenTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    FjordStationSetupScreen* station_setup_screen =
        WizardController::default_controller()
            ->GetScreen<FjordStationSetupScreen>();
    station_setup_screen->set_exit_callback_for_testing(
        screen_exit_waiter_.GetRepeatingCallback());
  }

  void Login() {
    login_manager_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
  }

  void ShowStationSetupScreen() {
    LoginDisplayHost::default_host()->StartWizard(
        FjordStationSetupScreenView::kScreenId);
    OobeScreenWaiter(FjordStationSetupScreenView::kScreenId).Wait();
  }

  void WaitForScreenExit() { EXPECT_TRUE(screen_exit_waiter_.Wait()); }

 private:
  base::test::TestFuture<void> screen_exit_waiter_;

  LoginManagerMixin login_manager_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FjordStationSetupScreenTest,
                       ClickActionButtonTwiceExitsScreen) {
  Login();
  ShowStationSetupScreen();

  // Check initial button text and webview URL are correct.
  test::OobeJS().ExpectVisiblePath(kStationSetupFramePath);
  EXPECT_EQ(
      test::OobeJS().GetAttributeString(kSrcAttribute, kStationSetupFramePath),
      kExpectedStationSetupUrl);

  test::OobeJS().CreateVisibilityWaiter(true, kActionButtonPath)->Wait();
  EXPECT_EQ(
      test::OobeJS().GetAttributeString(kTextKeyAttribute, kActionButtonPath),
      kExpectedNextButtonTextKey);

  test::OobeJS().TapOnPath(kActionButtonPath);

  // Verify after clicking Next the button text and webview frame URL are
  // updated.
  EXPECT_EQ(
      test::OobeJS().GetAttributeString(kSrcAttribute, kStationSetupFramePath),
      kExpectedFinishSetupUrl);
  EXPECT_EQ(
      test::OobeJS().GetAttributeString(kTextKeyAttribute, kActionButtonPath),
      kExpectedDoneButtonTextKey);
  test::OobeJS().TapOnPath(kActionButtonPath);

  WaitForScreenExit();
}

}  // namespace
}  // namespace ash
