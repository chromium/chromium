// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/update_screen.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_state.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

constexpr const char kUserActionAcceptUpdateOverCellular[] =
    "update-accept-cellular";
constexpr const char kUserActionRejectUpdateOverCellular[] =
    "update-reject-cellular";

constexpr const char kUserActionCancelUpdateShortcut[] = "cancel-update";

constexpr const char kUserActionOptOutInfoNext[] = "opt-out-info-next";

// Time in seconds after which we initiate reboot.
constexpr const base::TimeDelta kWaitBeforeRebootTime = base::Seconds(2);

constexpr const base::TimeDelta kDefaultShowDelay = base::Microseconds(400);

// When battery percent is lower and DISCHARGING warn user about it.
const double kInsufficientBatteryPercent = 50;

// Passing "--quick-start-test-forced-update" on the command line will simulate
// the "Forced Update" flow after the wifi credentials transfer is complete.
// This is for testing only and will not install an actual update. If this
// switch is present, the Chromebook reboots and attempts to automatically
// resume the Quick Start connection after reboot.
constexpr char kQuickStartTestForcedUpdateSwitch[] =
    "quick-start-test-forced-update";

void RecordDownloadingTime(base::TimeDelta duration) {
  base::UmaHistogramLongTimes("OOBE.UpdateScreen.UpdateDownloadingTime",
                              duration);
}

void RecordCheckTime(const base::TimeDelta duration) {
  base::UmaHistogramLongTimes("OOBE.UpdateScreen.StageTime.Check", duration);
}

void RecordDownloadTime(const base::TimeDelta duration) {
  base::UmaHistogramLongTimes("OOBE.UpdateScreen.StageTime.Download", duration);
}

void RecordVerifyTime(const base::TimeDelta duration) {
  base::UmaHistogramLongTimes("OOBE.UpdateScreen.StageTime.Verify", duration);
}

void RecordFinalizeTime(const base::TimeDelta duration) {
  base::UmaHistogramLongTimes("OOBE.UpdateScreen.StageTime.Finalize", duration);
}

void RecordUpdateStages(const base::TimeDelta check_time,
                        const base::TimeDelta download_time,
                        const base::TimeDelta verify_time,
                        const base::TimeDelta finalize_time) {
  RecordCheckTime(check_time);
  RecordDownloadTime(download_time);
  RecordVerifyTime(verify_time);
  RecordFinalizeTime(finalize_time);
}

void RecordUpdateCheckTimeout(bool timeout) {
  base::UmaHistogramBoolean("OOBE.UpdateScreen.CheckTimeout", timeout);
}

}  // namespace

// static
std::string UpdateScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::UPDATE_NOT_REQUIRED:
      return "UpdateNotRequired";
    case Result::UPDATE_ERROR:
      return "UpdateError";
    case Result::UPDATE_SKIPPED:
      return BaseScreen::kNotApplicable;
    case Result::UPDATE_OPT_OUT_INFO_SHOWN:
      return "UpdateNotRequired_OptOutInfo";
    case Result::UPDATE_CHECK_TIMEOUT:
      return "UpdateCheckTimeout";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

UpdateScreen::UpdateScreen(base::WeakPtr<UpdateView> view,
                           ErrorScreen* error_screen,
                           const ScreenExitCallback& exit_callback)
    : BaseScreen(UpdateView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      error_screen_(error_screen),
      exit_callback_(exit_callback),
      histogram_helper_(std::make_unique<ErrorScreensHistogramHelper>(
          ErrorScreensHistogramHelper::ErrorParentScreen::kUpdate)),
      version_updater_(std::make_unique<VersionUpdater>(this)),
      wait_before_reboot_time_(kWaitBeforeRebootTime),
      show_delay_(kDefaultShowDelay),
      tick_clock_(base::DefaultTickClock::GetInstance()) {}

UpdateScreen::~UpdateScreen() = default;

bool UpdateScreen::MaybeSkip(WizardContext& context) {
  if (context.enrollment_triggered_early) {
    LOG(WARNING) << "Skip OOBE Update because of enrollment request.";
    exit_callback_.Run(VersionUpdater::Result::UPDATE_SKIPPED);
    return true;
  }

  if (IsRollbackFlow(context)) {
    LOG(WARNING)
        << "Skip OOBE Update because enterprise rollback just happened.";
    exit_callback_.Run(VersionUpdater::Result::UPDATE_SKIPPED);
    return true;
  }

  if (!context.is_branded_build) {
    LOG(WARNING) << "Skip OOBE Update because of not branded build.";
    exit_callback_.Run(VersionUpdater::Result::UPDATE_SKIPPED);
    return true;
  }
  return false;
}

void UpdateScreen::ShowImpl() {
  is_opt_out_enabled_ = CheckIfOptOutIsEnabled();
  // AccessibilityManager::Get() can be nullptr in unittests.
  if (AccessibilityManager::Get()) {
    AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
    accessibility_subscription_ = accessibility_manager->RegisterCallback(
        base::BindRepeating(&UpdateScreen::OnAccessibilityStatusChanged,
                            weak_factory_.GetWeakPtr()));
  }
  if (!power_manager_subscription_.IsObserving()) {
    power_manager_subscription_.Observe(chromeos::PowerManagerClient::Get());
  }
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (view_) {
    view_->SetCancelUpdateShortcutEnabled(true);
  }
#endif
  if (version_updater_->update_info().requires_permission_for_cellular &&
      view_) {
    view_->SetUpdateState(UpdateView::UIState::kCellularPermission);
  }
  // If opt out is enabled for the region don't try to skip the screen
  // without showing it, as we will need to show additional step to user anyway.
  if (is_opt_out_enabled_) {
    MakeSureScreenIsShown();
  } else {
    show_timer_.Start(FROM_HERE, show_delay_,
                      base::BindOnce(&UpdateScreen::MakeSureScreenIsShown,
                                     weak_factory_.GetWeakPtr()));
  }
  version_updater_->StartNetworkCheck();
}

void UpdateScreen::HideImpl() {
  accessibility_subscription_ = {};
  power_manager_subscription_.Reset();
  show_timer_.Stop();
  is_shown_ = false;
}

void UpdateScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  bool is_chrome_branded_build = false;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  is_chrome_branded_build = true;
#endif

  if (action_id == kUserActionCancelUpdateShortcut) {
    if (is_chrome_branded_build) {
      VLOG(1) << "Ignore update cancel in branded build";
      return;
    }
    // Skip update UI, usually used only in debug builds/tests.
    VLOG(1) << "Forced update cancel";
    ExitUpdate(Result::UPDATE_NOT_REQUIRED);
  } else if (action_id == kUserActionAcceptUpdateOverCellular) {
    version_updater_->SetUpdateOverCellularOneTimePermission();
  } else if (action_id == kUserActionRejectUpdateOverCellular) {
    version_updater_->RejectUpdateOverCellular();
    ExitUpdate(Result::UPDATE_ERROR);
  } else if (action_id == kUserActionOptOutInfoNext) {
    FinishExitUpdate(Result::UPDATE_OPT_OUT_INFO_SHOWN);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

base::OneShotTimer* UpdateScreen::GetShowTimerForTesting() {
  return &show_timer_;
}

base::OneShotTimer* UpdateScreen::GetErrorMessageTimerForTesting() {
  return &error_message_timer_;
}

VersionUpdater* UpdateScreen::GetVersionUpdaterForTesting() {
  return version_updater_.get();
}

void UpdateScreen::ExitUpdate(Result result) {
  version_updater_->StartExitUpdate(result);
}

void UpdateScreen::OnWaitForRebootTimeElapsed() {
  LOG(ERROR) << "Unable to reboot - asking user for a manual reboot.";
  MakeSureScreenIsShown();
  if (!view_)
    return;
  view_->SetUpdateState(UpdateView::UIState::kManualReboot);
}

void UpdateScreen::PrepareForUpdateCheck() {
  error_message_timer_.Stop();
  error_screen_->HideCaptivePortal();

  connect_request_subscription_ = {};
  if (version_updater_->update_info().state ==
      VersionUpdater::State::STATE_ERROR)
    HideErrorMessage();
}

void UpdateScreen::ShowErrorMessage() {
  LOG(WARNING) << "UpdateScreen::ShowErrorMessage()";

  error_message_timer_.Stop();

  is_shown_ = false;
  show_timer_.Stop();

  connect_request_subscription_ =
      error_screen_->RegisterConnectRequestCallback(base::BindRepeating(
          &UpdateScreen::OnConnectRequested, weak_factory_.GetWeakPtr()));
  error_screen_->SetUIState(NetworkError::UI_STATE_UPDATE);
  error_screen_->SetParentScreen(UpdateView::kScreenId);
  error_screen_->SetHideCallback(base::BindOnce(
      &UpdateScreen::OnErrorScreenHidden, weak_factory_.GetWeakPtr()));
  error_screen_->Show(nullptr);
  histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
}

void UpdateScreen::UpdateErrorMessage(NetworkState::PortalState state,
                                      NetworkError::ErrorState error_state,
                                      const std::string& network_name) {
  error_screen_->SetErrorState(error_state, network_name);
  if (state == NetworkState::PortalState::kPortal ||
      state == NetworkState::PortalState::kPortalSuspected) {
    if (is_first_portal_notification_) {
      is_first_portal_notification_ = false;
      error_screen_->FixCaptivePortal();
    }
  }
}

void UpdateScreen::DelayErrorMessage() {
  if (error_message_timer_.IsRunning())
    return;

  error_message_timer_.Start(FROM_HERE, delay_error_message_, this,
                             &UpdateScreen::ShowErrorMessage);
}

void UpdateScreen::UpdateInfoChanged(
    const VersionUpdater::UpdateInfo& update_info) {
  if (is_hidden()) {
    return;
  }
  const update_engine::StatusResult& status = update_info.status;
  hide_progress_on_exit_ = false;
  has_critical_update_ =
      status.update_urgency() == update_engine::UpdateUrgency::CRITICAL;
  if (update_info.requires_permission_for_cellular && view_) {
    view_->SetUpdateState(UpdateView::UIState::kCellularPermission);
    MakeSureScreenIsShown();
    return;
  }

  // For testing resuming Quick Start after an update with the
  // kQuickStartTestForcedUpdateSwitch only.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kQuickStartTestForcedUpdateSwitch) &&
      context()->quick_start_setup_ongoing) {
    WizardController::default_controller()
        ->quick_start_controller()
        ->PrepareForUpdate(/*is_forced=*/true);
    did_prepare_quick_start_for_update_ = true;
    view_->SetUpdateState(UpdateView::UIState::kUpdateInProgress);
    // Set that critical update applied in OOBE.
    g_browser_process->local_state()->SetBoolean(
        prefs::kOobeCriticalUpdateCompleted, true);
    wait_reboot_timer_.Start(FROM_HERE, wait_before_reboot_time_,
                             version_updater_.get(),
                             &VersionUpdater::RebootAfterUpdate);
    return;
  }

  switch (status.current_operation()) {
    case update_engine::Operation::CHECKING_FOR_UPDATE:
      if (view_)
        view_->SetUpdateState(UpdateView::UIState::kCheckingForUpdate);
      if (start_update_stage_.is_null())
        start_update_stage_ = tick_clock_->NowTicks();
      break;
      // Do nothing in these cases, we don't want to notify the user of the
      // check unless there is an update.
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
    case update_engine::Operation::CLEANUP_PREVIOUS_UPDATE:
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::IDLE:
    case update_engine::Operation::UPDATED_BUT_DEFERRED:
      break;
    case update_engine::Operation::UPDATE_AVAILABLE:
      if (view_)
        view_->SetUpdateState(UpdateView::UIState::kCheckingForUpdate);
      if (start_update_stage_.is_null())
        start_update_stage_ = tick_clock_->NowTicks();
      MakeSureScreenIsShown();
      if (!HasCriticalUpdate()) {
        VLOG(1) << "Non-critical update available: " << status.new_version();
        hide_progress_on_exit_ = true;
        ExitUpdate(Result::UPDATE_NOT_REQUIRED);
      }
      break;
    case update_engine::Operation::DOWNLOADING:
      if (view_)
        view_->SetUpdateState(UpdateView::UIState::kUpdateInProgress);
      SetUpdateStatusMessage(update_info.better_update_progress,
                             update_info.total_time_left);
      MakeSureScreenIsShown();

      if (is_critical_checked_) {
        break;
      }

      // Because update engine doesn't send UPDATE_STATUS_UPDATE_AVAILABLE we
      // need to check if update is critical on first downloading
      // notification.
      is_critical_checked_ = true;

      if (!HasCriticalUpdate()) {
        VLOG(1) << "Non-critical update available: " << status.new_version();
        hide_progress_on_exit_ = true;
        ExitUpdate(Result::UPDATE_NOT_REQUIRED);
        break;
      }

      check_time_ = tick_clock_->NowTicks() - start_update_stage_;
      start_update_stage_ = start_update_downloading_ = tick_clock_->NowTicks();
      VLOG(1) << "Critical update available: " << status.new_version();

      if (context()->quick_start_setup_ongoing) {
        WizardController::default_controller()
            ->quick_start_controller()
            ->PrepareForUpdate(/*is_forced=*/true);
        did_prepare_quick_start_for_update_ = true;
      }
      break;
    case update_engine::Operation::VERIFYING:
      if (view_)
        view_->SetUpdateState(UpdateView::UIState::kUpdateInProgress);
      SetUpdateStatusMessage(update_info.better_update_progress,
                             update_info.total_time_left);
      // Make sure that VERIFYING and DOWNLOADING stages are recorded correctly.
      if (download_time_.is_zero()) {
        download_time_ = tick_clock_->NowTicks() - start_update_stage_;
        start_update_stage_ = tick_clock_->NowTicks();
      }
      MakeSureScreenIsShown();
      break;
    case update_engine::Operation::FINALIZING:
      if (view_)
        view_->SetUpdateState(UpdateView::UIState::kUpdateInProgress);
      // set that critical update applied in OOBE.
      g_browser_process->local_state()->SetBoolean(
          prefs::kOobeCriticalUpdateCompleted, true);
      SetUpdateStatusMessage(update_info.better_update_progress,
                             update_info.total_time_left);
      // Make sure that VERIFYING and FINALIZING stages are recorded correctly.
      if (verify_time_.is_zero()) {
        verify_time_ = tick_clock_->NowTicks() - start_update_stage_;
        start_update_stage_ = tick_clock_->NowTicks();
      }
      MakeSureScreenIsShown();
      break;
    case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
      MakeSureScreenIsShown();
      break;
    case update_engine::Operation::UPDATED_NEED_REBOOT:
      MakeSureScreenIsShown();
      if (HasCriticalUpdate()) {
        finalize_time_ = tick_clock_->NowTicks() - start_update_stage_;
        RecordUpdateStages(check_time_, download_time_, verify_time_,
                           finalize_time_);
        RecordDownloadingTime(tick_clock_->NowTicks() -
                              start_update_downloading_);
        ShowRebootInProgress();
        wait_reboot_timer_.Start(FROM_HERE, wait_before_reboot_time_,
                                 version_updater_.get(),
                                 &VersionUpdater::RebootAfterUpdate);
      } else {
        hide_progress_on_exit_ = true;
        ExitUpdate(Result::UPDATE_NOT_REQUIRED);
      }
      break;
    case update_engine::Operation::ERROR:
    case update_engine::Operation::REPORTING_ERROR_EVENT:
      // Ignore update errors for non-critical updates to prevent blocking the
      // user from getting to login screen during OOBE if the pending update is
      // not critical.
      if (update_info.is_checking_for_update || !HasCriticalUpdate()) {
        ExitUpdate(Result::UPDATE_NOT_REQUIRED);
      } else {
        ExitUpdate(Result::UPDATE_ERROR);
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  UpdateBatteryWarningVisibility();
}

void UpdateScreen::FinishExitUpdate(Result result) {
  if (did_prepare_quick_start_for_update_) {
    WizardController::default_controller()
        ->quick_start_controller()
        ->ResumeSessionAfterCancelledUpdate();
  }

  RecordUpdateCheckTimeout(result == Result::UPDATE_CHECK_TIMEOUT);

  if (!start_update_stage_.is_null() && check_time_.is_zero()) {
    check_time_ = tick_clock_->NowTicks() - start_update_stage_;
    RecordCheckTime(check_time_);
  }
  show_timer_.Stop();
  if (is_opt_out_enabled_ && result == Result::UPDATE_NOT_REQUIRED) {
    if (view_)
      view_->SetUpdateState(UpdateView::UIState::kOptOutInfo);
    return;
  }
  exit_callback_.Run(result);
}

void UpdateScreen::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  UpdateBatteryWarningVisibility();
}

void UpdateScreen::ShowRebootInProgress() {
  MakeSureScreenIsShown();
  if (view_)
    view_->SetUpdateState(UpdateView::UIState::kRestartInProgress);
}

void UpdateScreen::SetUpdateStatusMessage(int percent,
                                          base::TimeDelta time_left) {
  if (!view_)
    return;
  std::u16string time_left_message;
  if (time_left.InMinutes() == 0) {
    time_left_message = l10n_util::GetStringFUTF16(
        IDS_UPDATE_STATUS_SUBTITLE_TIME_LEFT,
        l10n_util::GetPluralStringFUTF16(IDS_TIME_LONG_SECS,
                                         time_left.InSeconds()));
  } else {
    time_left_message = l10n_util::GetStringFUTF16(
        IDS_UPDATE_STATUS_SUBTITLE_TIME_LEFT,
        l10n_util::GetPluralStringFUTF16(IDS_TIME_LONG_MINS,
                                         time_left.InMinutes()));
  }
  view_->SetUpdateStatus(
      percent,
      l10n_util::GetStringFUTF16(IDS_UPDATE_STATUS_SUBTITLE_PERCENT,
                                 base::FormatPercent(percent)),
      time_left_message);
}

void UpdateScreen::UpdateBatteryWarningVisibility() {
  if (!view_)
    return;
  const std::optional<power_manager::PowerSupplyProperties>& proto =
      chromeos::PowerManagerClient::Get()->GetLastStatus();
  if (!proto.has_value())
    return;
  view_->ShowLowBatteryWarningMessage(
      is_critical_checked_ && HasCriticalUpdate() &&
      proto->battery_state() ==
          power_manager::PowerSupplyProperties_BatteryState_DISCHARGING &&
      proto->battery_percent() < kInsufficientBatteryPercent);
}

bool UpdateScreen::HasCriticalUpdate() {
  return has_critical_update_.has_value() && has_critical_update_.value();
}

void UpdateScreen::MakeSureScreenIsShown() {
  show_timer_.Stop();

  if (is_shown_ || !view_)
    return;

  is_shown_ = true;
  histogram_helper_->OnScreenShow();

  // AccessibilityManager::Get() can be nullptr in unittests.
  if (AccessibilityManager::Get()) {
    view_->SetAutoTransition(
        !AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  }
  // `is_opt_out_enabled_` can be true only if the feature is enabled.
  DCHECK(!is_opt_out_enabled_ || features::IsConsumerAutoUpdateToggleAllowed());
  view_->Show(is_opt_out_enabled_);
}

void UpdateScreen::HideErrorMessage() {
  LOG(WARNING) << "UpdateScreen::HideErrorMessage()";
  error_screen_->Hide();
  histogram_helper_->OnErrorHide();
}

void UpdateScreen::OnConnectRequested() {
  if (version_updater_->update_info().state ==
      VersionUpdater::State::STATE_ERROR) {
    LOG(WARNING) << "Hiding error message since AP was reselected";
    version_updater_->StartUpdateCheck();
  }
}

void UpdateScreen::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
      AccessibilityNotificationType::kManagerShutdown) {
    accessibility_subscription_ = {};
    return;
  }
  // AccessibilityManager::Get() can be nullptr in unittests.
  if (view_ && AccessibilityManager::Get()) {
    view_->SetAutoTransition(
        !AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  }
}

void UpdateScreen::OnErrorScreenHidden() {
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  Show(context());
}

// static
bool UpdateScreen::CheckIfOptOutIsEnabled() {
  if (!features::IsConsumerAutoUpdateToggleAllowed())
    return false;
  auto country = system::GetCountryCodeFromTimezoneIfAvailable(
      g_browser_process->local_state()->GetString(
          ::prefs::kSigninScreenTimezone));
  if (!country.has_value()) {
    return false;
  }
  return base::Contains(kEUCountriesSet, country.value());
}

}  // namespace ash
