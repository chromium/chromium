// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/consumer_update_screen.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
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
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_state.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {
constexpr const char kUserActionAcceptUpdateOverCellular[] =
    "consumer-update-accept-cellular";
constexpr const char kUserActionRejectUpdateOverCellular[] =
    "consumer-update-reject-cellular";
constexpr const char kUserActionSkipUpdate[] = "skip-consumer-update";
constexpr const char kUserActionBackButton[] = "back";

// Time in seconds after which we initiate reboot.
constexpr const base::TimeDelta kWaitBeforeRebootTime = base::Seconds(2);
// When battery percent is lower and DISCHARGING warn user about it.
const double kInsufficientBatteryPercent = 50;

constexpr base::TimeDelta kUmaMinUpdateTime = base::Milliseconds(1);
constexpr base::TimeDelta kUmaMaxUpdateTime = base::Hours(2);
constexpr int kUmaUpdateTimeBuckets = 50;

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

void RecordIsOptionalUpdateSkipped(bool skipped) {
  base::UmaHistogramBoolean("OOBE.ConsumerUpdateScreen.IsOptionalUpdateSkipped",
                            skipped);
}

}  // namespace

// static
std::string ConsumerUpdateScreen::GetResultString(Result result) {
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
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

ConsumerUpdateScreen::ConsumerUpdateScreen(
    base::WeakPtr<ConsumerUpdateScreenView> view,
    ErrorScreen* error_screen,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ConsumerUpdateScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      error_screen_(error_screen),
      exit_callback_(exit_callback),
      histogram_helper_(std::make_unique<ErrorScreensHistogramHelper>(
          ErrorScreensHistogramHelper::ErrorParentScreen::kConsumerUpdate)),
      version_updater_(std::make_unique<VersionUpdater>(this)),
      wait_before_reboot_time_(kWaitBeforeRebootTime) {}

ConsumerUpdateScreen::~ConsumerUpdateScreen() = default;

bool ConsumerUpdateScreen::MaybeSkip(WizardContext& context) {
  CHECK(!g_browser_process->platform_part()
             ->browser_policy_connector_ash()
             ->IsDeviceEnterpriseManaged());
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
      view_) {
    view_->SetUpdateState(
        ConsumerUpdateScreenView::UIState::kCellularPermission);
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
  if (view_ && !is_mandatory_update_.has_value()) {
    base::TimeDelta time_left = version_updater_->update_info().total_time_left;
    is_mandatory_update_ = time_left < maximum_time_force_update_;
    base::UmaHistogramBoolean("OOBE.ConsumerUpdateScreen.IsMandatory",
                              is_mandatory_update_.value());
    view_->SetIsUpdateMandatory(is_mandatory_update_.value());
  }
}

void ConsumerUpdateScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionAcceptUpdateOverCellular) {
    version_updater_->SetUpdateOverCellularOneTimePermission();
  } else if (action_id == kUserActionRejectUpdateOverCellular) {
    version_updater_->RejectUpdateOverCellular();
    RecordOobeConsumerUpdateScreenSkippedReasonHistogram(
        OobeConsumerUpdateScreenSkippedReason::kDeclineCellular);
    exit_callback_.Run(Result::DECLINE_CELLULAR);
  } else if (action_id == kUserActionSkipUpdate) {
    RecordIsOptionalUpdateSkipped(/*skipped=*/true);
    exit_callback_.Run(Result::SKIPPED);
  } else if (action_id == kUserActionBackButton) {
    version_updater_->RejectUpdateOverCellular();
    exit_callback_.Run(Result::BACK);
  } else {
    BaseScreen::OnUserAction(args);
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
    case VersionUpdater::Result::UPDATE_OPT_OUT_INFO_SHOWN:
      // the opt_out_info_shown is displayed only for FAU
      NOTREACHED();
      break;
  }
}

void ConsumerUpdateScreen::ExitUpdate(VersionUpdater::Result result) {
  version_updater_->StartExitUpdate(result);
}

void ConsumerUpdateScreen::OnWaitForRebootTimeElapsed() {
  LOG(ERROR) << "Unable to reboot - asking user for a manual reboot.";
  if (!view_) {
    return;
  }
  view_->SetUpdateState(ConsumerUpdateScreenView::UIState::kManualReboot);
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
  if (view_) {
    view_->SetUpdateState(
        ConsumerUpdateScreenView::UIState::kRestartInProgress);
  }
}

void ConsumerUpdateScreen::SetUpdateStatusMessage(int percent,
                                                  base::TimeDelta time_left) {
  if (!view_) {
    return;
  }
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

void ConsumerUpdateScreen::UpdateBatteryWarningVisibility() {
  if (!view_) {
    return;
  }
  const absl::optional<power_manager::PowerSupplyProperties>& proto =
      chromeos::PowerManagerClient::Get()->GetLastStatus();
  if (!proto.has_value()) {
    return;
  }
  view_->ShowLowBatteryWarningMessage(
      proto->battery_state() ==
          power_manager::PowerSupplyProperties_BatteryState_DISCHARGING &&
      proto->battery_percent() < kInsufficientBatteryPercent);
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
  if (view_ && AccessibilityManager::Get()) {
    view_->SetAutoTransition(
        !AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  }
}

void ConsumerUpdateScreen::OnErrorScreenHidden() {
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  Show(context());
}

void ConsumerUpdateScreen::UpdateInfoChanged(
    const VersionUpdater::UpdateInfo& update_info) {
  if (!view_) {
    return;
  }
  const update_engine::StatusResult& status = update_info.status;
  if (update_info.requires_permission_for_cellular) {
    view_->SetUpdateState(
        ConsumerUpdateScreenView::UIState::kCellularPermission);
    return;
  }
  switch (status.current_operation()) {
    case update_engine::Operation::CHECKING_FOR_UPDATE:
      view_->SetUpdateState(
          ConsumerUpdateScreenView::UIState::kCheckingForUpdate);
      break;
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
    case update_engine::Operation::CLEANUP_PREVIOUS_UPDATE:
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::IDLE:
    case update_engine::Operation::UPDATED_BUT_DEFERRED:
      break;
    case update_engine::Operation::UPDATE_AVAILABLE:
      view_->SetUpdateState(
          ConsumerUpdateScreenView::UIState::kCheckingForUpdate);
      update_available = true;
      break;
    case update_engine::Operation::DOWNLOADING:
    case update_engine::Operation::VERIFYING:
    case update_engine::Operation::FINALIZING:
      view_->SetUpdateState(
          ConsumerUpdateScreenView::UIState::kUpdateInProgress);
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
      NOTREACHED();
  }
  UpdateBatteryWarningVisibility();
}

void ConsumerUpdateScreen::RecordOobeConsumerUpdateScreenSkippedReasonHistogram(
    OobeConsumerUpdateScreenSkippedReason reason) {
  base::UmaHistogramEnumeration("OOBE.ConsumerUpdateScreen.SkipReason", reason);
}

}  // namespace ash
