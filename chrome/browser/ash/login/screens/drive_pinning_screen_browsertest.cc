// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/login/drive_pinning_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash {

namespace {

using base::test::RunClosure;
using base::test::TestFuture;
using drivefs::pinning::Progress;
using drivefs::pinning::Stage;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::Field;

constexpr char kDrivePinningId[] = "drive-pinning";

const test::UIPath kDrivePinningDialoguePath = {kDrivePinningId,
                                                "drivePinningDialogue"};
const test::UIPath kSpaceInformationPath = {kDrivePinningId,
                                            "spaceInformation"};
const test::UIPath kToggleButtonPath = {kDrivePinningId, "drivePinningToggle"};
const test::UIPath kNextButtonPath = {kDrivePinningId, "nextButton"};

}  // namespace

class DrivePinningBaseScreenTest : public OobeBaseTest {
 public:
  DrivePinningBaseScreenTest() {
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
    drive_pining_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
    drive_pining_screen->set_ignore_choobe_controller_state_for_testing(true);
  }

  void SetPinningManagerProgress(Progress progress) {
    WizardController::default_controller()
        ->GetScreen<DrivePinningScreen>()
        ->OnProgressForTest(progress);
  }

  void ShowDrivePinningScreen() {
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->skip_choobe_for_tests = true;

    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    WizardController::default_controller()->AdvanceToScreen(
        DrivePinningScreenView::kScreenId);
  }

  DrivePinningScreen::Result WaitForScreenExitResult() {
    DrivePinningScreen::Result result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

 private:
  base::test::TestFuture<DrivePinningScreen::Result> screen_result_waiter_;
  DrivePinningScreen::ScreenExitCallback original_callback_;
};

class DrivePinningScreenTest
    : public DrivePinningBaseScreenTest,
      public ::testing::WithParamInterface<drivefs::pinning::Stage> {};

IN_PROC_BROWSER_TEST_F(DrivePinningScreenTest, Accept) {
  Progress current_progress = Progress();
  current_progress.stage = drivefs::pinning::Stage::kSuccess;
  // Expect the free space to be 100 GB (107,374,182,400  bytes), the required
  // space to be 512 MB.
  current_progress.free_space = 100LL * 1024LL * 1024LL * 1024LL;
  current_progress.required_space = 512 * 1024 * 1024;

  SetPinningManagerProgress(current_progress);
  ShowDrivePinningScreen();

  test::OobeJS().ExpectVisiblePath(kDrivePinningDialoguePath);
  test::OobeJS().ExpectElementText(
      l10n_util::GetStringFUTF8(
          IDS_OOBE_DRIVE_PINNING_TOGGLE_SUBTITLE,
          ui::FormatBytes(current_progress.required_space),
          ui::FormatBytes(current_progress.free_space)),
      kSpaceInformationPath);
  test::OobeJS().TapOnPath(kNextButtonPath);

  DrivePinningScreen::Result result = WaitForScreenExitResult();

  EXPECT_TRUE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      prefs::kOobeDrivePinningEnabledDeferred));
  EXPECT_EQ(result, DrivePinningScreen::Result::NEXT);
}

IN_PROC_BROWSER_TEST_F(DrivePinningScreenTest, Decline) {
  Progress current_progress = Progress();
  current_progress.stage = Stage::kSuccess;
  // Expect the free space to be 100 GB (107,374,182,400  bytes), the required
  // space to be 512 MB.
  current_progress.free_space = 100LL * 1024LL * 1024LL * 1024LL;
  current_progress.required_space = 512 * 1024 * 1024;

  SetPinningManagerProgress(current_progress);
  ShowDrivePinningScreen();

  test::OobeJS().ExpectVisiblePath(kDrivePinningDialoguePath);
  test::OobeJS().TapOnPath(kToggleButtonPath);
  test::OobeJS().TapOnPath(kNextButtonPath);

  DrivePinningScreen::Result result = WaitForScreenExitResult();

  EXPECT_FALSE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      prefs::kOobeDrivePinningEnabledDeferred));
  EXPECT_EQ(result, DrivePinningScreen::Result::NEXT);
}

IN_PROC_BROWSER_TEST_P(DrivePinningScreenTest, ScreenSkippedOnError) {
  Progress current_progress = Progress();
  current_progress.stage = GetParam();

  SetPinningManagerProgress(current_progress);
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->skip_choobe_for_tests = true;

  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  ShowDrivePinningScreen();
  DrivePinningScreen::Result result = WaitForScreenExitResult();

  EXPECT_FALSE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      prefs::kOobeDrivePinningEnabledDeferred));
  EXPECT_EQ(result, DrivePinningScreen::Result::NOT_APPLICABLE);
}

INSTANTIATE_TEST_SUITE_P(All,
                         DrivePinningScreenTest,
                         ::testing::Values(Stage::kCannotGetFreeSpace,
                                           Stage::kCannotListFiles,
                                           Stage::kNotEnoughSpace,
                                           Stage::kCannotEnableDocsOffline));

class DrivePinningMockObserver
    : public drive::DriveIntegrationService::Observer {
 public:
  MOCK_METHOD(void, OnBulkPinProgress, (const Progress& progress), (override));
};

class DrivePinningIntegrationServiceTest : public DrivePinningBaseScreenTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    create_drive_integration_service_ = base::BindRepeating(
        &DrivePinningIntegrationServiceTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath mount_path = profile->GetPath().Append("drivefs");
    fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, std::string(), mount_path,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

  drive::DriveIntegrationService* GetDriveServiceForActiveProfile() {
    auto* drive_service = drive::DriveIntegrationServiceFactory::FindForProfile(
        ProfileManager::GetActiveUserProfile());
    EXPECT_NE(drive_service, nullptr);
    return drive_service;
  }

  void WaitForSuccessStateChange() {
    auto* drive_service = GetDriveServiceForActiveProfile();
    auto* const pinning_manager = drive_service->GetPinningManager();
    if (pinning_manager &&
        pinning_manager->GetProgress().stage == Stage::kSuccess) {
      return;
    }

    observer_.Observe(drive_service);

    base::RunLoop run_loop;
    EXPECT_CALL(observer_, OnBulkPinProgress(_)).Times(AnyNumber());
    EXPECT_CALL(observer_,
                OnBulkPinProgress(Field(&Progress::stage, Stage::kSuccess)))
        .Times(1)
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&observer_);
  }

 private:
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;
  DrivePinningMockObserver observer_;
};

IN_PROC_BROWSER_TEST_F(DrivePinningIntegrationServiceTest,
                       UnmountRestartsCalculation) {
  base::HistogramTester histogram_tester;

  ash::FakeSpacedClient::Get()->set_free_disk_space(int64_t(3) << 30);
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->skip_choobe_for_tests = true;

  // Login as a user and bypass the consolidated consent screen. This is
  // required as the bulk pinning required space is kicked off on exit of this
  // screen.
  login_manager_mixin_.LoginAsNewRegularUser();
  ash::test::WaitForConsolidatedConsentScreen();
  ash::test::TapConsolidatedConsentAccept();

  // Wait for bulk pinning to get to `kSuccess` before clearing the cache and
  // remounting the file system, this simulates a restart by DriveFS
  WaitForSuccessStateChange();
  TestFuture<bool> future;
  GetDriveServiceForActiveProfile()->ClearCacheAndRemountFileSystem(
      future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // Wait for the `kSuccess` again as after a successful remount the
  // `DrivePinningScreen` should kick off another space calculation if it was
  // previously started.
  WaitForSuccessStateChange();

  // Advance directly to the drive pinning screen and then click "Next".
  WizardController::default_controller()->AdvanceToScreen(
      DrivePinningScreenView::kScreenId);
  test::OobeJS().ExpectVisiblePath(kDrivePinningDialoguePath);
  test::OobeJS().TapOnPath(kNextButtonPath);

  DrivePinningScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, DrivePinningScreen::Result::NEXT);
  histogram_tester.ExpectUniqueSample(
      "FileBrowser.GoogleDrive.BulkPinning.CHOOBEScreenInitializations", 1, 2);
}

}  // namespace ash
