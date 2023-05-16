// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/drive_pinning_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash {

namespace {

using drivefs::pinning::Progress;
using ::testing::ElementsAre;

constexpr char kDrivePinningId[] = "drive-pinning";

const test::UIPath kDrivePinningDialoguePath = {kDrivePinningId,
                                                "drivePinningDialogue"};
const test::UIPath kSpaceInformationPath = {kDrivePinningId,
                                            "spaceInformation"};
const test::UIPath kAcceptButtonPath = {kDrivePinningId, "acceptButton"};
const test::UIPath kDeclineButtonPath = {kDrivePinningId, "declineButton"};

}  // namespace

class DrivePinningScreenTest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<drivefs::pinning::Stage> {
 public:
  DrivePinningScreenTest() {
    feature_list_.InitWithFeatures(
        {ash::features::kOobeChoobe, ash::features::kOobeDrivePinning,
         ash::features::kDriveFsBulkPinning,
         ash::features::kFeatureManagementDriveFsBulkPinning},
        {});
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    DrivePinningScreen* drive_pining_screen =
        WizardController::default_controller()->GetScreen<DrivePinningScreen>();

    original_callback_ = drive_pining_screen->get_exit_callback_for_testing();
    drive_pining_screen->set_exit_callback_for_testing(base::BindRepeating(
        &DrivePinningScreenTest::HandleScreenExit, base::Unretained(this)));
  }

  void SetPinManagerProgress(Progress progress) {
    WizardController::default_controller()
        ->GetScreen<DrivePinningScreen>()
        ->OnProgressForTest(progress);
  }

  void ShowDrivePinningScreen() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    WizardController::default_controller()->AdvanceToScreen(
        DrivePinningScreenView::kScreenId);
  }

  void WaitForScreenExit() {
    if (result_.has_value()) {
      return;
    }
    base::test::TestFuture<void> waiter;
    quit_closure_ = waiter.GetCallback();
    EXPECT_TRUE(waiter.Wait());
  }

  DrivePinningScreen::ScreenExitCallback original_callback_;
  absl::optional<DrivePinningScreen::Result> result_;

 protected:
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

 private:
  void HandleScreenExit(DrivePinningScreen::Result result) {
    result_ = result;
    original_callback_.Run(result);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(DrivePinningScreenTest, Accept) {
  Progress current_progress = Progress();
  current_progress.stage = drivefs::pinning::Stage::kSuccess;
  // Expect the free space to be 100 GB (107,374,182,400  bytes), the required
  // space to be 512 MB.
  current_progress.free_space = 100LL * 1024LL * 1024LL * 1024LL;
  current_progress.required_space = 512 * 1024 * 1024;

  SetPinManagerProgress(current_progress);
  ShowDrivePinningScreen();

  test::OobeJS().ExpectVisiblePath(kDrivePinningDialoguePath);
  test::OobeJS().ExpectElementText(
      l10n_util::GetStringFUTF8(
          IDS_OOBE_DRIVE_PINNING_ADDITIONAL_SUBTITLE,
          ui::FormatBytes(current_progress.required_space),
          ui::FormatBytes(current_progress.free_space)),
      kSpaceInformationPath);
  test::OobeJS().TapOnPath(kAcceptButtonPath);

  WaitForScreenExit();

  EXPECT_TRUE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      prefs::kOobeDrivePinningEnabledDeferred));
  EXPECT_EQ(result_.value(), DrivePinningScreen::Result::ACCEPT);
}

IN_PROC_BROWSER_TEST_F(DrivePinningScreenTest, Decline) {
  Progress current_progress = Progress();
  current_progress.stage = drivefs::pinning::Stage::kSuccess;
  // Expect the free space to be 100 GB (107,374,182,400  bytes), the required
  // space to be 512 MB.
  current_progress.free_space = 100LL * 1024LL * 1024LL * 1024LL;
  current_progress.required_space = 512 * 1024 * 1024;

  SetPinManagerProgress(current_progress);
  ShowDrivePinningScreen();

  test::OobeJS().ExpectVisiblePath(kDrivePinningDialoguePath);
  test::OobeJS().TapOnPath(kDeclineButtonPath);

  WaitForScreenExit();

  EXPECT_FALSE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      prefs::kOobeDrivePinningEnabledDeferred));
  EXPECT_EQ(result_.value(), DrivePinningScreen::Result::DECLINE);
}

IN_PROC_BROWSER_TEST_P(DrivePinningScreenTest, ScreenSkippedOnError) {
  Progress current_progress = Progress();
  current_progress.stage = GetParam();

  SetPinManagerProgress(current_progress);
  ShowDrivePinningScreen();

  WaitForScreenExit();

  EXPECT_FALSE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      prefs::kOobeDrivePinningEnabledDeferred));
  EXPECT_EQ(result_.value(), DrivePinningScreen::Result::NOT_APPLICABLE);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DrivePinningScreenTest,
    ::testing::Values(drivefs::pinning::Stage::kCannotGetFreeSpace,
                      drivefs::pinning::Stage::kCannotListFiles,
                      drivefs::pinning::Stage::kNotEnoughSpace,
                      drivefs::pinning::Stage::kCannotEnableDocsOffline));

}  // namespace ash
