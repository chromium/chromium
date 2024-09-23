// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/update_screen.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/version_updater/version_updater.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

const char kStubWifiGuid[] = "wlan0";
const test::UIPath kCellularPermissionDialog = {"oobe-update",
                                                "cellular-permission-dialog"};
const test::UIPath kCellularPermissionNext = {"oobe-update",
                                              "cellular-permission-next"};
const test::UIPath kCellularPermissionBack = {"oobe-update",
                                              "cellular-permission-back"};
const test::UIPath kLowBatteryWarningMessage = {"oobe-update",
                                                "battery-warning"};
const test::UIPath kErrorMessage = {"error-message"};
const test::UIPath kBetterUpdateCheckingForUpdatesDialog = {"oobe-update",
                                                            "checking-update"};
const test::UIPath kUpdateInProgressDialog = {"oobe-update",
                                              "update-in-progress-dialog"};
const test::UIPath kRestartingDialog = {"oobe-update", "restarting-dialog"};
const test::UIPath kBetterUpdateCompletedDialog = {
    "oobe-update", "better-update-complete-dialog"};
const test::UIPath kOptOutStep = {"oobe-update", "opt-out-info-dialog"};
const test::UIPath kOptOutInfoNext = {"oobe-update", "opt-out-info-next"};

// UMA names for better test reading.
const char kTimeCheck[] = "OOBE.UpdateScreen.StageTime.Check";
const char kTimeDownload[] = "OOBE.UpdateScreen.StageTime.Download";
const char kTimeFinalize[] = "OOBE.UpdateScreen.StageTime.Finalize";
const char kTimeVerify[] = "OOBE.UpdateScreen.StageTime.Verify";

// These values should be kept in sync with the progress bar values in
// chrome/browser/ash/login/version_updater/version_updater.cc.
const int kUpdateCheckProgress = 14;
const int kVerifyingProgress = 74;
const int kFinalizingProgress = 81;
const int kUpdateCompleteProgress = 100;

// Defines what part of update progress does download part takes.
const int kDownloadProgressIncrement = 60;

constexpr base::TimeDelta kTimeAdvanceMicroseconds200 = base::Microseconds(200);
constexpr base::TimeDelta kTimeAdvanceSeconds10 = base::Seconds(10);
constexpr base::TimeDelta kTimeAdvanceSeconds60 = base::Seconds(60);
constexpr base::TimeDelta kTimeDefaultWaiting = base::Seconds(10);

// Parameter to be used in tests.
struct RegionToCodeMap {
  const char* region;
  const char* country_code;
  bool is_eu;
  bool is_opt_out_feature_enabled;
};

const RegionToCodeMap kParamSet[]{{"unknown", "", false, false},
                                  {"unknown", "", false, true},
                                  {"Europe/Berlin", "de", true, false},
                                  {"Europe/Berlin", "de", true, true},
                                  {"Europe/Zurich", "ch", false, false},
                                  {"Europe/Zurich", "ch", false, true}};

std::string GetDownloadingString(int status_resource_id) {
  // TODO(https://crbug.com/1161276) Adapt for BetterUpdate version.
  return l10n_util::GetStringFUTF8(
      IDS_DOWNLOADING, l10n_util::GetStringUTF16(status_resource_id));
}

int GetDownloadingProgress(double progress) {
  // TODO(https://crbug.com/1161276) Adapt for BetterUpdate version.
  return kUpdateCheckProgress +
         static_cast<int>(progress * kDownloadProgressIncrement);
}

OobeUI* GetOobeUI() {
  auto* host = LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

class UpdateScreenTest : public OobeBaseTest,
                         public LocalStateMixin::Delegate,
                         public ::testing::WithParamInterface<RegionToCodeMap> {
 public:
  UpdateScreenTest() {
    if (GetParam().is_opt_out_feature_enabled) {
      feature_list_.InitAndEnableFeature(
          features::kConsumerAutoUpdateToggleAllowed);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kConsumerAutoUpdateToggleAllowed);
    }
  }

  UpdateScreenTest(const UpdateScreenTest&) = delete;
  UpdateScreenTest& operator=(const UpdateScreenTest&) = delete;

  ~UpdateScreenTest() override = default;

  void CheckUpdatingDialogComponents(
      const int /*updating_progress_value*/,
      const std::string& /*progress_message_value*/) {
    // TODO(https://crbug.com/1161276) Adapt for BetterUpdate version.
  }

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    tick_clock_.Advance(kTimeAdvanceSeconds60);

    error_screen_ = GetOobeUI()->GetErrorScreen();
    update_screen_ =
        WizardController::default_controller()->GetScreen<UpdateScreen>();
    update_screen_->set_exit_callback_for_testing(base::BindRepeating(
        &UpdateScreenTest::HandleScreenExit, base::Unretained(this)));
    version_updater_ = update_screen_->GetVersionUpdaterForTesting();
    version_updater_->set_tick_clock_for_testing(&tick_clock_);
    update_screen_->set_tick_clock_for_testing(&tick_clock_);

    // Waiting for update screen to be shown might take a long time on some test
    // build and the timer might be fired already. Increase the delay and call
    // fire from the test instead.
    update_screen_->set_delay_for_delayed_timer_for_testing(
        base::TimeDelta::Max());

    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->is_branded_build = true;
  }

  void SetUpLocalState() override {
    RegionToCodeMap param = GetParam();
    g_browser_process->local_state()->SetString(::prefs::kSigninScreenTimezone,
                                                param.region);
  }

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

  void WaitForOptOutStepAndClickNext() {
    test::OobeJS().CreateVisibilityWaiter(true, kOptOutStep)->Wait();
    test::OobeJS().TapOnPath(kOptOutInfoNext);
    WaitForScreenResult();
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(UpdateScreen::Result::UPDATE_OPT_OUT_INFO_SHOWN,
              last_screen_result_.value());
  }

 protected:
  void WaitForScreenResult() {
    if (last_screen_result_.has_value()) {
      return;
    }
    base::test::TestFuture<void> waiter;
    screen_result_callback_ = waiter.GetCallback();
    EXPECT_TRUE(waiter.Wait());
  }

  void ShowUpdateScreen() {
    WaitForOobeUI();
    WizardController::default_controller()->AdvanceToScreen(
        UpdateView::kScreenId);
    // When opt out option is not available we try to wait and don't show the
    // screen if the check for update happens fast enough. When we have an
    // opt out option we still need to show an additional step so we start
    // showing the spinner from the start.
    if (GetParam().is_eu && features::IsConsumerAutoUpdateToggleAllowed()) {
      OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
      update_screen_waiter.set_assert_next_screen();
      update_screen_waiter.Wait();
    }
  }

  // Preconditions:
  // - `UpdateScreen` is shown;
  // - Network is in a portal state.
  // Postconditions:
  // - Timer to delay showing the `ErrorScreen` is started.
  void WaitForDelayedErrorTimerToStart() {
    LOG(INFO) << "Waiting for delayed error timer to start";
    // Wait for the delayed timer to start running.
    auto* delayed_error_timer =
        update_screen_->GetErrorMessageTimerForTesting();
    test::TestPredicateWaiter(
        base::BindRepeating(&base::OneShotTimer::IsRunning,
                            base::Unretained(delayed_error_timer)))
        .Wait();
  }

  // Preconditions:
  // - `UpdateScreen` is shown;
  // - Network is in a portal state.
  // Postconditions:
  // - Timer to delay showing the `ErrorScreen` is fired;
  // - `ErrorScreen` is shown.
  void WaitForDelayedErrorTimerToFire() {
    auto* delayed_error_timer =
        update_screen_->GetErrorMessageTimerForTesting();
    WaitForDelayedErrorTimerToStart();
    // Fire the timer.
    delayed_error_timer->FireNow();
    ASSERT_EQ(UpdateView::kScreenId.AsId(), error_screen_->GetParentScreen());
    EXPECT_FALSE(delayed_error_timer->IsRunning());

    // Wait for `ErrorScreen` to be shown.
    OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
    error_screen_waiter.set_assert_next_screen();
    error_screen_waiter.Wait();
  }

  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

  raw_ptr<UpdateScreen, DanglingUntriaged> update_screen_ = nullptr;
  // Version updater - owned by `update_screen_`.
  raw_ptr<VersionUpdater, DanglingUntriaged> version_updater_ = nullptr;
  // Error screen - owned by OobeUI.
  raw_ptr<ErrorScreen, DanglingUntriaged> error_screen_ = nullptr;

  base::SimpleTestTickClock tick_clock_;

  base::HistogramTester histogram_tester_;

  std::optional<UpdateScreen::Result> last_screen_result_;

 private:
  void HandleScreenExit(UpdateScreen::Result result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = result;

    if (screen_result_callback_)
      std::move(screen_result_callback_).Run();
  }

  base::OnceClosure screen_result_callback_;

  base::test::ScopedFeatureList feature_list_;
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestUpdateCheckDoneBeforeShow) {
  ShowUpdateScreen();
  // For this test, the show timer is expected not to fire - cancel it
  // immediately.
  if (!GetParam().is_eu || !features::IsConsumerAutoUpdateToggleAllowed()) {
    EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());
    update_screen_->GetShowTimerForTesting()->Stop();
  }

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::IDLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);
  tick_clock_.Advance(kTimeAdvanceMicroseconds200);

  status.set_current_operation(update_engine::Operation::IDLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  if (GetParam().is_eu && features::IsConsumerAutoUpdateToggleAllowed()) {
    WaitForOptOutStepAndClickNext();
  } else {
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
              last_screen_result_.value());
  }

  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 1);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestUpdateNotFoundAfterScreenShow) {
  ShowUpdateScreen();
  if (!GetParam().is_eu || !features::IsConsumerAutoUpdateToggleAllowed())
    EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::IDLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);
  tick_clock_.Advance(kTimeAdvanceMicroseconds200);

  if (!GetParam().is_eu || !features::IsConsumerAutoUpdateToggleAllowed())
    update_screen_->GetShowTimerForTesting()->FireNow();

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update");
  test::OobeJS().ExpectVisiblePath(kBetterUpdateCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);

  status.set_current_operation(update_engine::Operation::IDLE);
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  if (GetParam().is_eu && features::IsConsumerAutoUpdateToggleAllowed()) {
    WaitForOptOutStepAndClickNext();
  } else {
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
              last_screen_result_.value());
  }
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 1);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestUpdateAvailable) {
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::CRITICAL);
  ShowUpdateScreen();

  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  if (!GetParam().is_eu || !features::IsConsumerAutoUpdateToggleAllowed())
    update_screen_->GetShowTimerForTesting()->FireNow();

  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisible("oobe-update");
  test::OobeJS().ExpectVisiblePath(kBetterUpdateCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCompletedDialog);

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

  test::OobeJS().CreateVisibilityWaiter(true, kUpdateInProgressDialog)->Wait();
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCompletedDialog);

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
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestErrorIssuingUpdateCheck) {
  update_engine_client()->set_update_check_result(
      UpdateEngineClient::UPDATE_RESULT_FAILED);
  ShowUpdateScreen();

  if (GetParam().is_eu && features::IsConsumerAutoUpdateToggleAllowed()) {
    WaitForOptOutStepAndClickNext();
  } else {
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
              last_screen_result_.value());
  }

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestErrorCheckingForUpdate) {
  ShowUpdateScreen();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::ERROR);
  // GetLastStatus() will be called via ExitUpdate() called from
  // UpdateStatusChanged().
  update_engine_client()->set_default_status(status);
  version_updater_->UpdateStatusChangedForTesting(status);

  if (GetParam().is_eu && features::IsConsumerAutoUpdateToggleAllowed()) {
    WaitForOptOutStepAndClickNext();
  } else {
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
              last_screen_result_.value());
  }

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 0);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestErrorUpdating) {
  ShowUpdateScreen();

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::ERROR);
  status.set_new_version("latest and greatest");

  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  if (GetParam().is_eu && features::IsConsumerAutoUpdateToggleAllowed()) {
    WaitForOptOutStepAndClickNext();
  } else {
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
              last_screen_result_.value());
  }

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 0);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestTemporaryPortalNetwork) {
  update_screen_->set_show_delay_for_testing(kTimeAdvanceSeconds10);

  // Change ethernet state to offline.
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetectorMixin::NetworkStatus::kPortal);

  ShowUpdateScreen();

  // If the network is a captive portal network, error message is shown with a
  // delay.
  WaitForDelayedErrorTimerToStart();
  EXPECT_EQ(OOBE_SCREEN_UNKNOWN.AsId(), error_screen_->GetParentScreen());

  // If network goes back online, the error message timer should be canceled.
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetectorMixin::NetworkStatus::kOnline);
  EXPECT_FALSE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());

  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);
  tick_clock_.Advance(kTimeAdvanceSeconds10);

  if (!GetParam().is_eu || !features::IsConsumerAutoUpdateToggleAllowed())
    EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());

  // Update available, but it is not critical in test.
  status.set_current_operation(update_engine::Operation::UPDATE_AVAILABLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  // Verify that update screen is showing checking for update UI.
  OobeScreenWaiter update_screen_waiter(UpdateView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  // As update is not critical in test if opt out option is available we proceed
  // to opt-out-info step instead.
  test::OobeJS().ExpectVisible("oobe-update");
  if (!GetParam().is_eu || !features::IsConsumerAutoUpdateToggleAllowed()) {
    test::OobeJS().ExpectVisiblePath(kBetterUpdateCheckingForUpdatesDialog);
  }
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);

  status.set_current_operation(update_engine::Operation::IDLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  if (GetParam().is_eu && features::IsConsumerAutoUpdateToggleAllowed()) {
    WaitForOptOutStepAndClickNext();
  } else {
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
              last_screen_result_.value());
  }
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 1);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestTwoOfflineNetworks) {
  // Change ethernet state to portal.
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetectorMixin::NetworkStatus::kPortal);
  ShowUpdateScreen();

  WaitForDelayedErrorTimerToFire();

  test::OobeJS().ExpectVisiblePath(kErrorMessage);
  test::OobeJS().ExpectVisiblePath(
      {"error-message", "captive-portal-message-text"});
  test::OobeJS().ExpectVisiblePath(
      {"error-message", "captive-portal-proxy-message-text"});

  network_portal_detector_.SetDefaultNetwork(
      kStubWifiGuid, shill::kTypeWifi,
      NetworkPortalDetectorMixin::NetworkStatus::kOffline);

  test::OobeJS().ExpectVisiblePath(
      {"error-message", "update-proxy-message-text"});

  EXPECT_FALSE(last_screen_result_.has_value());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 0);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestVoidNetwork) {
  network_portal_detector_.SimulateNoNetwork();
  ShowUpdateScreen();

  EXPECT_FALSE(update_screen_->GetErrorMessageTimerForTesting()->IsRunning());
  ASSERT_EQ(UpdateView::kScreenId.AsId(), error_screen_->GetParentScreen());
  EXPECT_FALSE(update_screen_->GetShowTimerForTesting()->IsRunning());

  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  error_screen_waiter.set_assert_next_screen();
  error_screen_waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kErrorMessage);
  test::OobeJS().ExpectVisiblePath({"error-message", "offlineMessageBody"});

  EXPECT_FALSE(last_screen_result_.has_value());
  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     0);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 0);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 0);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 0);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 0);
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestAPReselection) {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetectorMixin::NetworkStatus::kPortal);

  ShowUpdateScreen();

  WaitForDelayedErrorTimerToFire();

  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      "fake_path", base::DoNothing(), base::DoNothing(),
      false /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);

  ASSERT_EQ(OOBE_SCREEN_UNKNOWN.AsId(), error_screen_->GetParentScreen());
  if (!GetParam().is_eu || !features::IsConsumerAutoUpdateToggleAllowed()) {
    EXPECT_TRUE(update_screen_->GetShowTimerForTesting()->IsRunning());
    update_screen_->GetShowTimerForTesting()->FireNow();
  }

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

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, UpdateOverCellularAccepted) {
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::CRITICAL);
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
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);

  test::OobeJS().TapOnPath(kCellularPermissionNext);
  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS()
      .CreateVisibilityWaiter(true, kBetterUpdateCheckingForUpdatesDialog)
      ->Wait();

  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
  test::OobeJS().ExpectVisiblePath(kBetterUpdateCheckingForUpdatesDialog);

  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  version_updater_->UpdateStatusChangedForTesting(status);

  histogram_tester_.ExpectTotalCount("OOBE.UpdateScreen.UpdateDownloadingTime",
                                     1);
  histogram_tester_.ExpectTotalCount(kTimeCheck, 1);
  histogram_tester_.ExpectTotalCount(kTimeDownload, 1);
  histogram_tester_.ExpectTotalCount(kTimeVerify, 1);
  histogram_tester_.ExpectTotalCount(kTimeFinalize, 1);
  ASSERT_FALSE(last_screen_result_.has_value());
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, UpdateOverCellularRejected) {
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::CRITICAL);
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
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);

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

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestInitialLowBatteryStatus) {
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::CRITICAL);
  // Set low battery and discharging status before oobe-update screen is shown.
  power_manager::PowerSupplyProperties props;
  props.set_battery_percent(49);
  props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  power_manager_client()->UpdatePowerProperties(props);

  ShowUpdateScreen();
  EXPECT_TRUE(power_manager_client()->HasObserver(update_screen_));
  test::OobeJS().ExpectHiddenPath(kLowBatteryWarningMessage);

  status.set_current_operation(update_engine::Operation::DOWNLOADING);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  status.set_progress(0.0);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  // Warning message is shown while not charging and battery is low.
  test::OobeJS().ExpectVisiblePath(kLowBatteryWarningMessage);
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestBatteryWarningDuringUpdateStages) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  SetTickClockAndDefaultDelaysForTesting(
      mocked_task_runner->GetMockTickClock());
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::CRITICAL);
  ShowUpdateScreen();
  EXPECT_TRUE(power_manager_client()->HasObserver(update_screen_));

  power_manager::PowerSupplyProperties props;
  props.set_battery_percent(49);
  props.set_battery_state(
      power_manager::PowerSupplyProperties_BatteryState_DISCHARGING);
  power_manager_client()->UpdatePowerProperties(props);

  test::OobeJS().ExpectHiddenPath(kLowBatteryWarningMessage);

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

IN_PROC_BROWSER_TEST_P(UpdateScreenTest,
                       TestBatteryWarningOnDifferentBatteryStatus) {
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::CRITICAL);
  ShowUpdateScreen();
  EXPECT_TRUE(power_manager_client()->HasObserver(update_screen_));

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

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, TestUpdateCompletedRebootNeeded) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  SetTickClockAndDefaultDelaysForTesting(
      mocked_task_runner->GetMockTickClock());
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::CRITICAL);
  ShowUpdateScreen();

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
  ASSERT_TRUE(version_updater_->get_reboot_timer_for_testing()->IsRunning());
  mocked_task_runner->FastForwardBy(kTimeDefaultWaiting);

  test::OobeJS().ExpectHiddenPath(kRestartingDialog);
  test::OobeJS().ExpectVisiblePath(kBetterUpdateCompletedDialog);
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, UpdateScreenSteps) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  SetTickClockAndDefaultDelaysForTesting(
      mocked_task_runner->GetMockTickClock());
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::CRITICAL);
  ShowUpdateScreen();

  // CHECKING_FOR_UPDATE:
  status.set_current_operation(update_engine::Operation::CHECKING_FOR_UPDATE);
  status.set_new_version("latest and greatest");
  status.set_new_size(1'000'000'000);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  if (GetParam().is_eu && features::IsConsumerAutoUpdateToggleAllowed()) {
    test::OobeJS().ExpectVisiblePath(kBetterUpdateCheckingForUpdatesDialog);
  } else {
    test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);
  }
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kRestartingDialog);
  test::OobeJS().ExpectHiddenPath(kBetterUpdateCompletedDialog);

  if (!GetParam().is_eu || !features::IsConsumerAutoUpdateToggleAllowed())
    update_screen_->GetShowTimerForTesting()->FireNow();

  test::OobeJS().ExpectVisiblePath(kBetterUpdateCheckingForUpdatesDialog);
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
  ASSERT_TRUE(version_updater_->get_reboot_timer_for_testing()->IsRunning());
  mocked_task_runner->FastForwardBy(kTimeDefaultWaiting);

  test::OobeJS().ExpectHiddenPath(kBetterUpdateCheckingForUpdatesDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateInProgressDialog);
  test::OobeJS().ExpectHiddenPath(kRestartingDialog);
  test::OobeJS().ExpectVisiblePath(kBetterUpdateCompletedDialog);
}

IN_PROC_BROWSER_TEST_P(UpdateScreenTest, UpdateOverCellularShown) {
  base::ScopedMockTimeMessageLoopTaskRunner mocked_task_runner;
  SetTickClockAndDefaultDelaysForTesting(
      mocked_task_runner->GetMockTickClock());
  update_engine::StatusResult status;
  status.set_update_urgency(update_engine::UpdateUrgency::CRITICAL);
  ShowUpdateScreen();

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
  status.set_current_operation(update_engine::Operation::UPDATE_AVAILABLE);
  update_engine_client()->set_default_status(status);
  update_engine_client()->NotifyObserversThatStatusChanged(status);

  test::OobeJS()
      .CreateVisibilityWaiter(true, kBetterUpdateCheckingForUpdatesDialog)
      ->Wait();
  test::OobeJS().ExpectHiddenPath(kCellularPermissionDialog);
}

INSTANTIATE_TEST_SUITE_P(All, UpdateScreenTest, testing::ValuesIn(kParamSet));

}  // namespace
}  // namespace ash
