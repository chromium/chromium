// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/consumer_update_screen.h"

#include <algorithm>
#include <optional>

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
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_oobe.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/network_state.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

// Time in seconds after which we initiate reboot.
constexpr const base::TimeDelta kWaitBeforeRebootTime = base::Seconds(2);
// When battery percent is lower and DISCHARGING warn user about it.
const double kInsufficientBatteryPercent = 50;

constexpr base::TimeDelta kUmaMinUpdateTime = base::Milliseconds(1);
constexpr base::TimeDelta kUmaMaxUpdateTime = base::Hours(2);
constexpr int kUmaUpdateTimeBuckets = 50;

// Passing "--quick-start-test-consumer-update" on the command line will
// simulate the "Consumer Update" flow. This is for testing only and will not
// install an actual update. If this switch is present, the Chromebook reboots
// and attempts to automatically resume the Quick Start connection after reboot.
constexpr char kQuickStartTestConsumerUpdateSwitch[] =
    "quick-start-test-consumer-update";

void RecordUpdateTime(base::TimeDelta update_time, bool is_mandatory) {
  if (is_mandatory) {
    base::UmaHistogramCustomTimes(
        "OOBE.ConsumerUpdateScreen.UpdateTime.Mandatory", update_time,
        kUmaMinUpdateTime, kUmaMaxUpdateTime, kUmaUpdateTimeBuckets);
  } else {
    base::UmaHistogramCustomTimes(
        "OOBE.ConsumerUpdateScreen.UpdateTime.Optional", update_time,
        kUmaMinUpdateTime, kUmaMaxUpdateTime, kUmaUpdateTimeBuckets);
  }
}
void RecordUpdateEstimatorTime(base::TimeDelta update_time,
                               base::TimeDelta estimate_update_time) {
  base::UmaHistogramCustomTimes("OOBE.ConsumerUpdateScreen.EstimatorTimeLeft",
                                estimate_update_time, kUmaMinUpdateTime,
                                kUmaMaxUpdateTime, kUmaUpdateTimeBuckets);
  if (update_time > estimate_update_time) {
    base::UmaHistogramCustomTimes(
        "OOBE.ConsumerUpdateScreen.EstimatorErrorShort",
        update_time - estimate_update_time, kUmaMinUpdateTime,
        kUmaMaxUpdateTime, kUmaUpdateTimeBuckets);
  } else {
    base::UmaHistogramCustomTimes(
        "OOBE.ConsumerUpdateScreen.EstimatorErrorExceed",
        estimate_update_time - update_time, kUmaMinUpdateTime,
        kUmaMaxUpdateTime, kUmaUpdateTimeBuckets);
  }
}

void RecordIsOptionalUpdateSkipped(bool skipped) {
  base::UmaHistogramBoolean("OOBE.ConsumerUpdateScreen.IsOptionalUpdateSkipped",
                            skipped);
}

void RecordOobeConsumerUpdateAvailableHistogram() {
  base::UmaHistogramBoolean("OOBE.ConsumerUpdateScreen.UpdateAvailable", true);
}

}  // namespace

// static
std::string ConsumerUpdateScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::BACK:
      return "Back";
    case Result::UPDATED:
      return "Updated";
    case Result::UPDATE_NOT_REQUIRED:
      return "UpdateNotRequired";
    case Result::UPDATE_ERROR:
      return "UpdateError";
    case Result::SKIPPED:
      return "UpdateSkipped";
    case Result::DECLINE_CELLULAR:
      return "UpdateDeclineCellular";
    case Result::CHECK_TIMEOUT:
      return "UpdateCheckTimeout";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

ConsumerUpdateScreen::ConsumerUpdateScreen(
    base::WeakPtr<ConsumerUpdateScreenView> view,
    ErrorScreen* error_screen,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ConsumerUpdateScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      view_(std::move(view)),
      error_screen_(error_screen),
      exit_callback_(exit_callback),
      histogram_helper_(std::make_unique<ErrorScreensHistogramHelper>(
          ErrorScreensHistogramHelper::ErrorParentScreen::kConsumerUpdate)),
      version_updater_(std::make_unique<VersionUpdater>(this)),
      wait_before_reboot_time_(kWaitBeforeRebootTime) {}

ConsumerUpdateScreen::~ConsumerUpdateScreen() = default;

bool ConsumerUpdateScreen::MaybeSkip(WizardContext& context) {
  CHECK(!ash::InstallAttributes::Get()->IsEnterpriseManaged());
  if (context.skip_to_login_for_tests || context.is_add_person_flow) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  // skip if consumer update applied in OOBE before restarting.
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kOobeConsumerUpdateCompleted)) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  // skip if critical update applied in OOBE.
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kOobeCriticalUpdateCompleted)) {
    LOG(WARNING) << "Skip OOBE Consumer Update because a critical update was "
                    "applied during OOBE.";
    RecordOobeConsumerUpdateScreenSkippedReasonHistogram(
        OobeConsumerUpdateScreenSkippedReason::kCriticalUpdateCompleted);
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  if (!context.is_branded_build) {
    LOG(WARNING) << "Skip OOBE Consumer Update because of not branded build.";
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void ConsumerUpdateScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  if (AccessibilityManager::Get()) {
    AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
    accessibility_subscription_ = accessibility_manager->RegisterCallback(
        base::BindRepeating(&ConsumerUpdateScreen::OnAccessibilityStatusChanged,
                            weak_factory_.GetWeakPtr()));
  }

  if (!power_manager_subscription_.IsObserving()) {
    power_manager_subscription_.Observe(chromeos::PowerManagerClient::Get());
  }

  if (version_updater_->update_info().requires_permission_for_cellular &&
      GetRemote()->is_bound()) {
    (*GetRemote())
        ->SetScreenStep(screens_oobe::mojom::ConsumerUpdatePage::
                            ConsumerUpdateStep::kCellularPermission);
  }

  screen_shown_time_ = base::TimeTicks::Now();
  view_->Show();
  version_updater_->StartNetworkCheck();
}

void ConsumerUpdateScreen::HideImpl() {
  accessibility_subscription_ = {};
  power_manager_subscription_.Reset();
}

void ConsumerUpdateScreen::DelaySkipButton() {
  if (delay_skip_button_timer_.IsRunning()) {
    return;
  }

  delay_skip_button_timer_.Start(FROM_HERE, delay_skip_button_time_, this,
                                 &ConsumerUpdateScreen::SetSkipButton);
}

void ConsumerUpdateScreen::SetSkipButton() {
  if (!is_mandatory_update_.has_value()) {
    estimate_update_time_left_ =
        version_updater_->update_info().total_time_left;
    is_mandatory_update_ =
        estimate_update_time_left_ < maximum_time_force_update_;
    base::UmaHistogramBoolean("OOBE.ConsumerUpdateScreen.IsMandatory",
                              is_mandatory_update_.value());
    if (GetRemote()->is_bound()) {
      (*GetRemote())->ShowSkipButton();
    }
  }
}

void ConsumerUpdateScreen::DelayExitNoUpdate() {
  exit_callback_.Run(Result::UPDATE_NOT_REQUIRED);
}

void ConsumerUpdateScreen::FinishExitUpdate(VersionUpdater::Result result) {
  switch (result) {
    case VersionUpdater::Result::UPDATE_NOT_REQUIRED:
      RecordOobeConsumerUpdateScreenSkippedReasonHistogram(
          OobeConsumerUpdateScreenSkippedReason::kUpdateNotRequired);
      wait_exit_timer_.Start(FROM_HERE, exit_delay_, this,
                             &ConsumerUpdateScreen::DelayExitNoUpdate);
      break;
    case VersionUpdater::Result::UPDATE_ERROR:
      RecordOobeConsumerUpdateScreenSkippedReasonHistogram(
          OobeConsumerUpdateScreenSkippedReason::kUpdateError);
      exit_callback_.Run(Result::UPDATE_ERROR);
      break;
    case VersionUpdater::Result::UPDATE_SKIPPED:
      exit_callback_.Run(Result::NOT_APPLICABLE);
      break;
    case VersionUpdater::Result::UPDATE_CHECK_TIMEOUT:
      exit_callback_.Run(Result::CHECK_TIMEOUT);
      break;
    case VersionUpdater::Result::UPDATE_OPT_OUT_INFO_SHOWN:
      // the opt_out_info_shown is displayed only for FAU
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void ConsumerUpdateScreen::ExitUpdate(VersionUpdater::Result result) {
  version_updater_->StartExitUpdate(result);
}

void ConsumerUpdateScreen::OnDeclineCellularClicked() {
  version_updater_->RejectUpdateOverCellular();
  RecordOobeConsumerUpdateScreenSkippedReasonHistogram(
      OobeConsumerUpdateScreenSkippedReason::kDeclineCellular);
  version_updater_->StopObserving();
  exit_callback_.Run(Result::DECLINE_CELLULAR);
}

void ConsumerUpdateScreen::OnAcceptCellularClicked() {
  version_updater_->SetUpdateOverCellularOneTimePermission();
}

void ConsumerUpdateScreen::OnSkipClicked() {
  RecordIsOptionalUpdateSkipped(/*skipped=*/true);
  version_updater_->StopObserving();
  if (did_prepare_quick_start_for_update_) {
    WizardController::default_controller()
        ->quick_start_controller()
        ->ResumeSessionAfterCancelledUpdate();
  }
  exit_callback_.Run(Result::SKIPPED);
}

void ConsumerUpdateScreen::OnBackClicked() {
  version_updater_->RejectUpdateOverCellular();
  version_updater_->StopObserving();
  exit_callback_.Run(Result::BACK);
}

void ConsumerUpdateScreen::OnWaitForRebootTimeElapsed() {
  LOG(ERROR) << "Unable to reboot - asking user for a manual reboot.";
  if (GetRemote()->is_bound()) {
    (*GetRemote())
        ->SetScreenStep(screens_oobe::mojom::ConsumerUpdatePage::
                            ConsumerUpdateStep::kManualReboot);
  }
}

void ConsumerUpdateScreen::PrepareForUpdateCheck() {
  error_message_timer_.Stop();
  error_screen_->HideCaptivePortal();

  if (version_updater_->update_info().state ==
      VersionUpdater::State::STATE_ERROR) {
    HideErrorMessage();
  }
}

void ConsumerUpdateScreen::ShowErrorMessage() {
  LOG(WARNING) << "ConsumerUpdateScreen::ShowErrorMessage()";

  error_message_timer_.Stop();

  error_screen_->SetUIState(NetworkError::UI_STATE_UPDATE);
  error_screen_->SetParentScreen(ConsumerUpdateScreenView::kScreenId);
  error_screen_->SetHideCallback(base::BindOnce(
      &ConsumerUpdateScreen::OnErrorScreenHidden, weak_factory_.GetWeakPtr()));
  error_screen_->Show(nullptr);
  histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
}

void ConsumerUpdateScreen::UpdateErrorMessage(
    NetworkState::PortalState state,
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

void ConsumerUpdateScreen::DelayErrorMessage() {
  if (error_message_timer_.IsRunning()) {
    return;
  }

  error_message_timer_.Start(FROM_HERE, delay_error_message_, this,
                             &ConsumerUpdateScreen::ShowErrorMessage);
}

void ConsumerUpdateScreen::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  UpdateBatteryWarningVisibility();
}

void ConsumerUpdateScreen::ShowRebootInProgress() {
  if (GetRemote()->is_bound()) {
    (*GetRemote())
        ->SetScreenStep(screens_oobe::mojom::ConsumerUpdatePage::
                            ConsumerUpdateStep::kRestartInProgress);
  }
}

void ConsumerUpdateScreen::SetUpdateStatusMessage(int percent,
                                                  base::TimeDelta time_left) {
  std::string time_left_message;
  if (time_left.InMinutes() == 0) {
    time_left_message = l10n_util::GetStringFUTF8(
        IDS_UPDATE_STATUS_SUBTITLE_TIME_LEFT,
        l10n_util::GetPluralStringFUTF16(IDS_TIME_LONG_SECS,
                                         time_left.InSeconds()));
  } else {
    time_left_message = l10n_util::GetStringFUTF8(
        IDS_UPDATE_STATUS_SUBTITLE_TIME_LEFT,
        l10n_util::GetPluralStringFUTF16(IDS_TIME_LONG_MINS,
                                         time_left.InMinutes()));
  }
  if (GetRemote()->is_bound()) {
    (*GetRemote())
        ->SetUpdateStatusMessage(
            percent,
            l10n_util::GetStringFUTF8(IDS_UPDATE_STATUS_SUBTITLE_PERCENT,
                                      base::FormatPercent(percent)),
            time_left_message);
  }
}

void ConsumerUpdateScreen::UpdateBatteryWarningVisibility() {
  const std::optional<power_manager::PowerSupplyProperties>& proto =
      chromeos::PowerManagerClient::Get()->GetLastStatus();
  if (!proto.has_value()) {
    return;
  }
  if (GetRemote()->is_bound()) {
    (*GetRemote())
        ->SetLowBatteryWarningVisible(
            proto->battery_state() ==
                power_manager::PowerSupplyProperties_BatteryState_DISCHARGING &&
            proto->battery_percent() < kInsufficientBatteryPercent);
  }
}

void ConsumerUpdateScreen::HideErrorMessage() {
  LOG(WARNING) << "ConsumerUpdateScreen::HideErrorMessage()";
  error_screen_->Hide();
  histogram_helper_->OnErrorHide();
}

void ConsumerUpdateScreen::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
      AccessibilityNotificationType::kManagerShutdown) {
    accessibility_subscription_ = {};
    return;
  }
  // AccessibilityManager::Get() can be nullptr in unittests.
  if (GetRemote()->is_bound() && AccessibilityManager::Get()) {
    (*GetRemote())
        ->SetAutoTransition(
            !AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  }
}

void ConsumerUpdateScreen::OnErrorScreenHidden() {
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  Show(context());
}

void ConsumerUpdateScreen::UpdateInfoChanged(
    const VersionUpdater::UpdateInfo& update_info) {
  if (is_hidden()) {
    return;
  }

  const update_engine::StatusResult& status = update_info.status;
  if (update_info.requires_permission_for_cellular && GetRemote()->is_bound()) {
    (*GetRemote())
        ->SetScreenStep(screens_oobe::mojom::ConsumerUpdatePage::
                            ConsumerUpdateStep::kCellularPermission);
    return;
  }

  // For testing resuming Quick Start after an update with the
  // kQuickStartTestConsumerUpdateSwitch only.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kQuickStartTestConsumerUpdateSwitch) &&
      context()->quick_start_setup_ongoing) {
    // Remove switch to avoid update loop.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        kQuickStartTestConsumerUpdateSwitch);
    WizardController::default_controller()
        ->quick_start_controller()
        ->PrepareForUpdate(/*is_forced=*/false);
    did_prepare_quick_start_for_update_ = true;
    if (GetRemote()->is_bound()) {
      (*GetRemote())
          ->SetScreenStep(screens_oobe::mojom::ConsumerUpdatePage::
                              ConsumerUpdateStep::kUpdateInProgress);
    }
    // Set consumer update complete for next reboot.
    g_browser_process->local_state()->SetBoolean(
        prefs::kOobeConsumerUpdateCompleted, true);
    wait_reboot_timer_.Start(FROM_HERE, wait_before_reboot_time_,
                             version_updater_.get(),
                             &VersionUpdater::RebootAfterUpdate);
    return;
  }

  switch (status.current_operation()) {
    case update_engine::Operation::CHECKING_FOR_UPDATE:
      if (GetRemote()->is_bound()) {
        (*GetRemote())
            ->SetScreenStep(screens_oobe::mojom::ConsumerUpdatePage::
                                ConsumerUpdateStep::kCheckingForUpdate);
      }
      break;
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
    case update_engine::Operation::CLEANUP_PREVIOUS_UPDATE:
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::IDLE:
    case update_engine::Operation::UPDATED_BUT_DEFERRED:
      break;
    case update_engine::Operation::UPDATE_AVAILABLE:
      if (GetRemote()->is_bound()) {
        (*GetRemote())
            ->SetScreenStep(screens_oobe::mojom::ConsumerUpdatePage::
                                ConsumerUpdateStep::kCheckingForUpdate);
      }
      update_available = true;
      RecordOobeConsumerUpdateAvailableHistogram();
      break;
    case update_engine::Operation::DOWNLOADING:
      if (context()->quick_start_setup_ongoing &&
          !did_prepare_quick_start_for_update_) {
        WizardController::default_controller()
            ->quick_start_controller()
            ->PrepareForUpdate(/*is_forced=*/false);
        did_prepare_quick_start_for_update_ = true;
      }
      [[fallthrough]];
    case update_engine::Operation::VERIFYING:
    case update_engine::Operation::FINALIZING:
      if (GetRemote()->is_bound()) {
        (*GetRemote())
            ->SetScreenStep(screens_oobe::mojom::ConsumerUpdatePage::
                                ConsumerUpdateStep::kUpdateInProgress);
      }
      DelaySkipButton();
      SetUpdateStatusMessage(update_info.better_update_progress,
                             update_info.total_time_left);
      break;
    case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
      break;
    case update_engine::Operation::UPDATED_NEED_REBOOT: {
      g_browser_process->local_state()->SetBoolean(
          prefs::kOobeConsumerUpdateCompleted, true);

      base::TimeDelta update_time = base::TimeTicks::Now() - screen_shown_time_;
      RecordUpdateEstimatorTime(update_time, estimate_update_time_left_);
      RecordUpdateTime(update_time, is_mandatory_update_.value_or(true));
      if (!is_mandatory_update_.value_or(true)) {
        RecordIsOptionalUpdateSkipped(/*skipped=*/false);
      }

      ShowRebootInProgress();
      wait_reboot_timer_.Start(FROM_HERE, wait_before_reboot_time_,
                               version_updater_.get(),
                               &VersionUpdater::RebootAfterUpdate);
      break;
    }
    case update_engine::Operation::ERROR:
    case update_engine::Operation::REPORTING_ERROR_EVENT:
      if (!update_available) {
        ExitUpdate(VersionUpdater::Result::UPDATE_NOT_REQUIRED);
      } else {
        ExitUpdate(VersionUpdater::Result::UPDATE_ERROR);
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  UpdateBatteryWarningVisibility();
}

void ConsumerUpdateScreen::RecordOobeConsumerUpdateScreenSkippedReasonHistogram(
    OobeConsumerUpdateScreenSkippedReason reason) {
  base::UmaHistogramEnumeration("OOBE.ConsumerUpdateScreen.SkipReason", reason);
}

}  // namespace ash
