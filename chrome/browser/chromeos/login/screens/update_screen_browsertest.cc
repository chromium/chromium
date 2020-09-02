// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_screen.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/version_updater/version_updater.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

const char kStubWifiGuid[] = "wlan0";
const test::UIPath kCheckingDownloadingUpdate = {"oobe-update",
                                                 "checking-downloading-update"};
const test::UIPath kCheckingForUpdatesDialog = {"oobe-update",
                                                "checking-downloading-update",
                                                "checking-for-updates-dialog"};
const test::UIPath kUpdatingDialog = {
    "oobe-update", "checking-downloading-update", "updating-dialog"};
const test::UIPath kUpdatingProgress = {
    "oobe-update", "checking-downloading-update", "updating-progress"};
const test::UIPath kProgressMessage = {
    "oobe-update", "checking-downloading-update", "progress-message"};
const test::UIPath kUpdateCompletedDialog = {
    "oobe-update", "checking-downloading-update", "update-complete-dialog"};
const test::UIPath kCellularPermissionDialog = {"oobe-update",
                                                "cellular-permission-dialog"};
const test::UIPath kCellularPermissionNext = {"oobe-update",
                                              "cellular-permission-next"};
const test::UIPath kCellularPermissionBack = {"oobe-update",
                                              "cellular-permission-back"};
const test::UIPath kLowBatteryWarningMessage = {"oobe-update",
                                                "battery-warning"};
const test::UIPath kErrorMessage = {"error-message"};

// Paths for better update screen https://crbug.com/1101317
const test::UIPath kBetterUpdateCheckingForUpdatesDialog = {
    "oobe-update", "checking-for-updates-dialog"};
const test::UIPath kUpdateInProgressDialog = {"oobe-update",
                                              "update-in-progress-dialog"};
const test::UIPath kRestartingDialog = {"oobe-update", "restarting-dialog"};
const test::UIPath kBetterUpdateCompletedDialog = {
    "oobe-update", "better-update-complete-dialog"};

// UMA names for better test reading.
const char kTimeCheck[] = "OOBE.UpdateScreen.StageTime.Check";
const char kTimeDownload[] = "OOBE.UpdateScreen.StageTime.Download";
const char kTimeFinalize[] = "OOBE.UpdateScreen.StageTime.Finalize";
const char kTimeVerify[] = "OOBE.UpdateScreen.StageTime.Verify";

// These values should be kept in sync with the progress bar values in
// chrome/browser/chromeos/login/version_updater/version_updater.cc.
const int kUpdateCheckProgress = 14;
const int kVerifyingProgress = 74;
const int kFinalizingProgress = 81;
const int kUpdateCompleteProgress = 100;

// Defines what part of update progress does download part takes.
const int kDownloadProgressIncrement = 60;

constexpr base::TimeDelta kTimeAdvanceSeconds10 =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kTimeAdvanceSeconds60 =
    base::TimeDelta::FromSeconds(60);
constexpr base::TimeDelta kTimeDefaultWaiting =
    base::TimeDelta::FromSeconds(10);

std::string GetDownloadingString(int status_resource_id) {
  return l10n_util::GetStringFUTF8(
      IDS_DOWNLOADING, l10n_util::GetStringUTF16(status_resource_id));
}

int GetDownloadingProgress(double progress) {
  return kUpdateCheckProgress +
         static_cast<int>(progress * kDownloadProgressIncrement);
}

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

}  // namespace

class UpdateScreenTest : public OobeBaseTest {
 public:
  UpdateScreenTest() {
    feature_list_.InitWithFeatures({},
                                   {chromeos::features::kBetterUpdateScreen});
  }
  ~UpdateScreenTest() override = default;

  void CheckPathVisiblity(std::initializer_list<base::StringPiece> element_ids,
                          bool visibility);
  void CheckUpdatingDialogComponents(const int updating_progress_value,
                                     const std::string& progress_message_value);

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    tick_clock_.Advance(kTimeAdvanceSeconds60);

    error_screen_ = GetOobeUI()->GetErrorScreen();
    update_screen_ = UpdateScreen::Get(
        WizardController::default_controller()->screen_manager());
    update_screen_->set_exit_callback_for_testing(base::BindRepeating(
        &UpdateScreenTest::HandleScreenExit, base::Unretained(this)));
    version_updater_ = update_screen_->GetVersionUpdaterForTesting();
    version_updater_->set_tick_clock_for_testing(&tick_clock_);
    update_screen_->set_tick_clock_for_testing(&tick_clock_);
  }

 protected:
  void WaitForScreenResult() {
    if (last_screen_result_.has_value())
      return;

    base::RunLoop run_loop;
    screen_result_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ShowUpdateScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        UpdateView::kScreenId);
  }

  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

  UpdateScreen* update_screen_ = nullptr;
  // Version updater - owned by |update_screen_|.
  VersionUpdater* version_updater_ = nullptr;
  // Error screen - owned by OobeUI.
  ErrorScreen* error_screen_ = nullptr;

  base::SimpleTestTickClock tick_clock_;

  base::HistogramTester histogram_tester_;

  base::Optional<UpdateScreen::Result> last_screen_result_;

 private:
  void HandleScreenExit(UpdateScreen::Result result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = result;

    if (screen_result_callback_)
      std::move(screen_result_callback_).Run();
  }

  base::OnceClosure screen_result_callback_;

  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreenTest);
};

class BetterUpdateScreenTest : public UpdateScreenTest {
 public:
  BetterUpdateScreenTest() {
    feature_list_.InitWithFeatures({chromeos::features::kBetterUpdateScreen},
                                   {});
  }
  ~BetterUpdateScreenTest() override = default;

  void SetTickClockAndDefaultDelaysForTesting(
      const base::TickClock* tick_clock) {
    version_updater_->set_tick_clock_for_testing(tick_clock);
    update_screen_->set_tick_clock_for_testing(tick_clock);
    // Set time for waiting in the test to not update constants manually, if
    // they change.
    version_updater_->set_wait_for_reboot_time_for_testing(kTimeDefaultWaiting);
    update_screen_->set_wait_before_reboot_time_for_testing(
        kTimeDefaultWaiting);
  }

 protected:
  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

void UpdateScreenTest::CheckPathVisiblity(
    std::initializer_list<base::StringPiece> element_ids,
    bool visibility) {
  if (visibility)
    test::OobeJS().ExpectVisiblePath(element_ids);
  else
    test::OobeJS().ExpectHiddenPath(element_ids);
}

void UpdateScreenTest::CheckUpdatingDialogComponents(
    const int updating_progress_value,
    const std::string& progress_message_value) {
  CheckPathVisiblity(kUpdatingDialog, true);
  test::OobeJS().ExpectEQ(
      test::GetOobeElementPath(kUpdatingProgress) + ".value",
      updating_progress_value);
  test::OobeJS().ExpectElementText(progress_message_value, kProgressMessage);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestUpdateCheckDoneBeforeShow) {
  ShowUpdateScreen();
  // For this test, the show timer is expected not to fire - cancel it
  // immediately.
  EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());
  update_screen_->GetShowTimerForTesting()->Stop();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::IDLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  status.set_current_operation(update_engine::Operation::IDLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());

  ASSERT_NE(GetOobeUI()->current_screen(), UpdateView::kScreenId);

  // Show another screen, and verify the Update screen in not shown before it.
  GetOobeUI()->GetView<NetworkScreenHandler>()->Show();
  OobeScreenWaiter network_screen_waiter(NetworkScreenView::kScreenId);
  network_screen_waiter.set_assert_next_screen();
  network_screen_waiter.Wait();
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 1);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestUpdateNotFoundAfterScreenShow) {
  ShowUpdateScreen();
  EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::IDLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  update_screen_->GetShowTimerForTesting()->FireNow();

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update");
  test::OobeJS().ExpectVisiblePath(kCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdatingDialog);

  status.set_current_operation(update_engine::Operation::IDLE);
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 1);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestUpdateAvailable) {
  update_screen_->set_ignore_update_deadlines_for_testing(true);
  ShowUpdateScreen();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  update_screen_->GetShowTimerForTesting()->FireNow();

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update");
  test::OobeJS().ExpectVisiblePath(kCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kUpdatingDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateCompletedDialog);

  // Duplicate CHECKING status to test correctness of time recording.
  tick_clock_.Advance(kTimeAdvanceSeconds10);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  tick_clock_.Advance(kTimeAdvanceSeconds60);
  status.set_current_operation(update_engine::Operation::UPDATE_AVAILABLE);
  status.set_progress(0.0);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  // Duplicate UPDATE_AVAILABLE status to test correctness of time recording.
  tick_clock_.Advance(kTimeAdvanceSeconds10);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  status.set_progress(0.0);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdatingDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateCompletedDialog);

  CheckUpdatingDialogComponents(
      kUpdateCheckProgress, l10n_util::GetStringUTF8(IDS_INSTALLING_UPDATE));

  tick_clock_.Advance(kTimeAdvanceSeconds60);
  status.set_progress(0.01);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  CheckUpdatingDialogComponents(
      kUpdateCheckProgress,
      GetDownloadingString(IDS_DOWNLOADING_TIME_LEFT_LONG));

  tick_clock_.Advance(kTimeAdvanceSeconds60);
  status.set_progress(0.08);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  CheckUpdatingDialogComponents(
      GetDownloadingProgress(0.08),
      GetDownloadingString(IDS_DOWNLOADING_TIME_LEFT_STATUS_ONE_HOUR));

  tick_clock_.Advance(kTimeAdvanceSeconds10);
  status.set_progress(0.7);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  CheckUpdatingDialogComponents(
      GetDownloadingProgress(0.7),
      GetDownloadingString(IDS_DOWNLOADING_TIME_LEFT_SMALL));

  tick_clock_.Advance(kTimeAdvanceSeconds10);
  status.set_progress(0.9);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  CheckUpdatingDialogComponents(
      GetDownloadingProgress(0.9),
      GetDownloadingString(IDS_DOWNLOADING_TIME_LEFT_SMALL));

  tick_clock_.Advance(kTimeAdvanceSeconds10);
  status.set_current_operation(update_engine::Operation::VERIFYING);
  status.set_progress(1.0);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  // Duplicate VERIFYING status to test correctness of time recording.
  tick_clock_.Advance(kTimeAdvanceSeconds10);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  CheckUpdatingDialogComponents(kVerifyingProgress,
                                l10n_util::GetStringUTF8(IDS_UPDATE_VERIFYING));

  tick_clock_.Advance(kTimeAdvanceSeconds10);
  status.set_current_operation(update_engine::Operation::FINALIZING);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  // Duplicate FINALIZING status to test correctness of time recording.
  tick_clock_.Advance(kTimeAdvanceSeconds10);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  CheckUpdatingDialogComponents(
      kFinalizingProgress, l10n_util::GetStringUTF8(IDS_UPDATE_FINALIZING));

  tick_clock_.Advance(kTimeAdvanceSeconds10);
  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  CheckUpdatingDialogComponents(
      kUpdateCompleteProgress, l10n_util::GetStringUTF8(IDS_UPDATE_FINALIZING));

  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());

  // Expect proper metric recorded.
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     1);
  histogram_tester_.ExpectTimeBucketCount(
      "OOBE.UpdateScreen.UpdateDownloadingTime",
      2 * kTimeAdvanceSeconds60 + 7 * kTimeAdvanceSeconds10, 1);

  histogram_tester_.ExpectTotalCount(kTimeCheck, 1);
  histogram_tester_.ExpectTimeBucketCount(
      kTimeCheck, kTimeAdvanceSeconds60 + 2 * kTimeAdvanceSeconds10, 1);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 1);
  histogram_tester_.ExpectTimeBucketCount(
      kTimeDownload, 2 * kTimeAdvanceSeconds60 + 3 * kTimeAdvanceSeconds10, 1);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 1);
  histogram_tester_.ExpectTimeBucketCount(kTimeVerify,
                                          2 * kTimeAdvanceSeconds10, 1);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 1);
  histogram_tester_.ExpectTimeBucketCount(kTimeFinalize,
                                          2 * kTimeAdvanceSeconds10, 1);

  // Simulate the situation where reboot does not happen in time.
  ASSERT_TRUE(version_updater_->GetRebootTimerForTesting()->IsRunning());
  version_updater_->GetRebootTimerForTesting()->FireNow();

  test::OobeJS().ExpectHiddenPath(kUpdatingDialog);
  test::OobeJS().ExpectVisiblePath(kUpdateCompletedDialog);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorIssuingUpdateCheck) {
  update_engine_client()->set_update_check_result(
      chromeos::UpdateEngineClient::UPDATE_RESULT_FAILED);
  ShowUpdateScreen();

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorCheckingForUpdate) {
  ShowUpdateScreen();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::ERROR);
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  update_engine_client()->set_default_status(status);
  version_updater_->UpdateStatusChangedForTesting(status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 0);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestErrorUpdating) {
  ShowUpdateScreen();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::ERROR);
  status.set_new_version("latest and greatest");

  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 0);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestTemporaryPortalNetwork) {
  // Change ethernet state to offline.
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);

  ShowUpdateScreen();

  // If the network is a captive portal network, error message is shown with a
  // delay.
  EXPECT_TRUE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());
  EXPECT_EQ(OobeScreen::SCREEN_UNKNOWN.AsId(),
            error_screen_->GetParentScreen());

  // If network goes back online, the error message timer should be canceled.
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  EXPECT_FALSE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());

  status.set_current_operation(update_engine::Operation::UPDATE_AVAILABLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  // Verify that update screen is showing checking for update UI.
  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update");
  test::OobeJS().ExpectVisiblePath(kCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdatingDialog);

  status.set_current_operation(update_engine::Operation::IDLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  ASSERT_TRUE(last_screen_result_.has_value());
  EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
            last_screen_result_.value());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 1);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestTwoOfflineNetworks) {
  // Change ethernet state to portal.
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  ShowUpdateScreen();

  // Update screen will delay error message about portal state because
  // ethernet is behind captive portal. Simulate the delay timing out.
  EXPECT_TRUE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());
  update_screen_->GetErrorMessageTimerForTesting()->FireNow();
  EXPECT_FALSE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());

  ASSERT_EQ(UpdateView::kScreenId.AsId(), error_screen_->GetParentScreen());

  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  error_screen_waiter.set_assert_next_screen();
  error_screen_waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kErrorMessage);
  test::OobeJS().ExpectVisible("error-message-md");
  test::OobeJS().ExpectHasClass("ui-state-update", kErrorMessage);
  test::OobeJS().ExpectHasClass("error-state-portal", kErrorMessage);

  // Change active network to the wifi behind proxy.
  network_portal_detector_.SetDefaultNetwork(
      kStubWifiGuid,
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED);

  test::OobeJS()
      .CreateHasClassWaiter(true, "error-state-proxy", kErrorMessage)
      ->Wait();

  EXPECT_FALSE(last_screen_result_.has_value());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 0);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestVoidNetwork) {
  network_portal_detector_.SimulateNoNetwork();

  // First portal detection attempt returns NULL network and undefined
  // results, so detection is restarted.
  ShowUpdateScreen();

  EXPECT_FALSE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());

  network_portal_detector_.WaitForPortalDetectionRequest();
  network_portal_detector_.SimulateNoNetwork();

  EXPECT_FALSE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());
  ASSERT_EQ(UpdateView::kScreenId.AsId(), error_screen_->GetParentScreen());
  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  // Second portal detection also returns NULL network and undefined
  // results.  In this case, offline message should be displayed.
  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  error_screen_waiter.set_assert_next_screen();
  error_screen_waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kErrorMessage);
  test::OobeJS().ExpectVisible("error-message-md");

  test::OobeJS().ExpectHasClass("ui-state-update", kErrorMessage);
  test::OobeJS().ExpectHasClass("error-state-offline", kErrorMessage);

  EXPECT_FALSE(last_screen_result_.has_value());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 0);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, TestAPReselection) {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);

  ShowUpdateScreen();

  // Force timer expiration.
  EXPECT_TRUE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());
  update_screen_->GetErrorMessageTimerForTesting()->FireNow();
  ASSERT_EQ(UpdateView::kScreenId.AsId(), error_screen_->GetParentScreen());
  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  error_screen_waiter.set_assert_next_screen();
  error_screen_waiter.Wait();

  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      "fake_path", base::DoNothing(), base::DoNothing(),
      false /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);

  ASSERT_EQ(OobeScreen::SCREEN_UNKNOWN.AsId(),
            error_screen_->GetParentScreen());
  EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());
  update_screen_->GetShowTimerForTesting()->FireNow();

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  ASSERT_FALSE(last_screen_result_.has_value());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 0);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, UpdateOverCellularAccepted) {
  update_screen_->set_ignore_update_deadlines_for_testing(true);

  update_engine::StatusResult status;
  status.set_current_operation(
      update_engine::Operation::NEED_PERMISSION_TO_UPDATE);
  status.set_new_version("latest and greatest");

  ShowUpdateScreen();

  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update");
  test::OobeJS().ExpectVisiblePath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kCheckingDownloadingUpdate);

  test::OobeJS().TapOnPath(kCellularPermissionNext);

  test::OobeJS()
      .CreateVisibilityWaiter(true, kCheckingDownloadingUpdate)
      ->Wait();

  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kCheckingForUpdatesDialog);
  test::OobeJS().ExpectVisiblePath(kUpdatingDialog);

  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  version_updater_->UpdateStatusChangedForTesting(status);

  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     1);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 1);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 1);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 1);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 1);
  ASSERT_FALSE(last_screen_result_.has_value());
}

IN_PROC_BROWSER_TEST_F(UpdateScreenTest, UpdateOverCellularRejected) {
  update_screen_->set_ignore_update_deadlines_for_testing(true);

  update_engine::StatusResult status;
  status.set_current_operation(
      update_engine::Operation::NEED_PERMISSION_TO_UPDATE);
  status.set_new_version("latest and greatest");

  ShowUpdateScreen();

  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update");
  test::OobeJS().ExpectVisiblePath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kCheckingDownloadingUpdate);

  test::OobeJS().ClickOnPath(kCellularPermissionBack);

  WaitForScreenResult();
  EXPECT_EQ(UpdateScreen::Result::UPDATE_ERROR, last_screen_result_.value());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 0);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_F(BetterUpdateScreenTest, TestInitialLowBatteryStatus) {
  update_screen_->set_ignore_update_deadlines_for_testing(true);
  // Set low battery and discharging status before oobe-update screen is shown.
  power_manager::PowerSupplyProperties props;
  props.set_battery_percent(49);
  props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  power_manager_client()->UpdatePowerProperties(props);

  ShowUpdateScreen();
  EXPECT_TRUE(power_manager_client()->HasObserver(update_screen_));
  test::OobeJS().ExpectHiddenPath(kLowBatteryWarningMessage);

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  status.set_progress(0.0);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  // Warning message is shown while not charging and battery is low.
  test::OobeJS().ExpectVisiblePath(kLowBatteryWarningMessage);
}

IN_PROC_BROWSER_TEST_F(BetterUpdateScreenTest,
                       TestBatteryWarningDuringUpdateStages) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  SetTickClockAndDefaultDelaysForTesting(
      mocked_task_runner->GetMockTickClock());
  update_screen_->set_ignore_update_deadlines_for_testing(true);
  ShowUpdateScreen();
  EXPECT_TRUE(power_manager_client()->HasObserver(update_screen_));

  power_manager::PowerSupplyProperties props;
  props.set_battery_percent(49);
  props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  power_manager_client()->UpdatePowerProperties(props);

  test::OobeJS().ExpectHiddenPath(kLowBatteryWarningMessage);

  update_engine::StatusResult status;
  // Warning message is hidden before DOWNLOADING stage.
  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectHiddenPath(kLowBatteryWarningMessage);

  status.set_current_operation(update_engine::Operation::UPDATE_AVAILABLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectHiddenPath(kLowBatteryWarningMessage);

  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  status.set_progress(0.0);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath(kLowBatteryWarningMessage);

  // Warning message remains on the screen during next update stages, iff the
  // battery is low and discharging.
  status.set_current_operation(update_engine::Operation::VERIFYING);
  status.set_progress(1.0);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath(kLowBatteryWarningMessage);

  status.set_current_operation(update_engine::Operation::FINALIZING);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath(kLowBatteryWarningMessage);

  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  // Show waiting for reboot screen for several seconds.
  ASSERT_TRUE(update_screen_->GetWaitRebootTimerForTesting()->IsRunning());
  mocked_task_runner->FastForwardBy(kTimeDefaultWaiting);

  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(update_engine_client()->reboot_after_update_call_count(), 1);
  test::OobeJS().ExpectVisiblePath(kLowBatteryWarningMessage);
}

IN_PROC_BROWSER_TEST_F(BetterUpdateScreenTest,
                       TestBatteryWarningOnDifferentBatteryStatus) {
  update_screen_->set_ignore_update_deadlines_for_testing(true);
  ShowUpdateScreen();
  EXPECT_TRUE(power_manager_client()->HasObserver(update_screen_));

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  status.set_progress(0.0);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  power_manager::PowerSupplyProperties props;

  // Warning message is hidden while not charging, but enough battery.
  props.set_battery_percent(100);
  props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_FULL);
  power_manager_client()->UpdatePowerProperties(props);

  test::OobeJS().ExpectVisible("oobe-update");
  test::OobeJS().ExpectHiddenPath(kLowBatteryWarningMessage);

  props.set_battery_percent(85);
  props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  power_manager_client()->UpdatePowerProperties(props);

  test::OobeJS().ExpectHiddenPath(kLowBatteryWarningMessage);

  // Warning message is shown while not charging and battery is low.
  props.set_battery_percent(48);
  power_manager_client()->UpdatePowerProperties(props);

  test::OobeJS().ExpectVisiblePath(kLowBatteryWarningMessage);

  // Warning message is hidden while charging.
  props.set_battery_percent(49);
  props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_CHARGING);
  power_manager_client()->UpdatePowerProperties(props);

  test::OobeJS().ExpectHiddenPath(kLowBatteryWarningMessage);
}

IN_PROC_BROWSER_TEST_F(BetterUpdateScreenTest,
                       TestUpdateCompletedRebootNeeded) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  SetTickClockAndDefaultDelaysForTesting(
      mocked_task_runner->GetMockTickClock());
  update_screen_->set_ignore_update_deadlines_for_testing(true);
  ShowUpdateScreen();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().CreateVisibilityWaiter(true, kRestartingDialog)->Wait();

  // Make sure that after the screen is shown waiting timer starts.
  mocked_task_runner->RunUntilIdle();
  // Show waiting for reboot screen for several seconds.
  ASSERT_TRUE(update_screen_->GetWaitRebootTimerForTesting()->IsRunning());
  mocked_task_runner->FastForwardBy(kTimeDefaultWaiting);

  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  ASSERT_EQ(update_engine_client()->reboot_after_update_call_count(), 1);

  // Simulate the situation where reboot does not happen in time.
  ASSERT_TRUE(version_updater_->GetRebootTimerForTesting()->IsRunning());
  mocked_task_runner->FastForwardBy(kTimeDefaultWaiting);

  test::OobeJS().ExpectHiddenPath(kRestartingDialog);
  test::OobeJS().ExpectVisiblePath(kBetterUpdateCompletedDialog);
}

IN_PROC_BROWSER_TEST_F(BetterUpdateScreenTest, UpdateScreenSteps) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  SetTickClockAndDefaultDelaysForTesting(
      mocked_task_runner->GetMockTickClock());
  update_screen_->set_ignore_update_deadlines_for_testing(true);
  ShowUpdateScreen();

  update_engine::StatusResult status;
  // CHECKING_FOR_UPDATE:
  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS()
      .CreateVisibilityWaiter(true, kBetterUpdateCheckingForUpdatesDialog)
      ->Wait();
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kRestartingDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCompletedDialog);

  // UPDATE_AVAILABLE:
  status.set_current_operation(update_engine::Operation::UPDATE_AVAILABLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath(kBetterUpdateCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kRestartingDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCompletedDialog);

  // DOWNLOADING:
  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdateInProgressDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kRestartingDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCompletedDialog);

  // VERIFYING:
  status.set_current_operation(update_engine::Operation::VERIFYING);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kRestartingDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCompletedDialog);

  // FINALIZING:
  status.set_current_operation(update_engine::Operation::FINALIZING);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().ExpectVisiblePath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kRestartingDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCompletedDialog);

  // UPDATED_NEED_REBOOT:
  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS().CreateVisibilityWaiter(true, kRestartingDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCompletedDialog);

  // Make sure that after the screen is shown waiting timer starts.
  mocked_task_runner->RunUntilIdle();
  // Show waiting for reboot screen for several seconds.
  ASSERT_TRUE(update_screen_->GetWaitRebootTimerForTesting()->IsRunning());
  mocked_task_runner->FastForwardBy(kTimeDefaultWaiting);

  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  ASSERT_EQ(update_engine_client()->reboot_after_update_call_count(), 1);

  // Simulate the situation where reboot does not happen in time.
  ASSERT_TRUE(version_updater_->GetRebootTimerForTesting()->IsRunning());
  mocked_task_runner->FastForwardBy(kTimeDefaultWaiting);

  test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kRestartingDialog);
  test::OobeJS().ExpectVisiblePath(kBetterUpdateCompletedDialog);
}

IN_PROC_BROWSER_TEST_F(BetterUpdateScreenTest, UpdateOverCellularShown) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  SetTickClockAndDefaultDelaysForTesting(
      mocked_task_runner->GetMockTickClock());
  update_screen_->set_ignore_update_deadlines_for_testing(true);
  ShowUpdateScreen();

  update_engine::StatusResult status;
  status.set_current_operation(
      update_engine::Operation::NEED_PERMISSION_TO_UPDATE);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kCellularPermissionDialog)
      ->Wait();
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);

  test::OobeJS().TapOnPath(kCellularPermissionNext);

  test::OobeJS()
      .CreateVisibilityWaiter(true, kBetterUpdateCheckingForUpdatesDialog)
      ->Wait();
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
}

}  // namespace chromeos
