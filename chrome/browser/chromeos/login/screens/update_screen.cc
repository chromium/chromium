// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_screen.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/chromeos/login/configuration_keys.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/wizard_context.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/network/network_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

namespace {

constexpr const char kUserActionAcceptUpdateOverCellular[] =
    "update-accept-cellular";
constexpr const char kUserActionRejectUpdateOverCellular[] =
    "update-reject-cellular";

constexpr const char kUserActionCancelUpdateShortcut[] = "cancel-update";

const char kUpdateDeadlineFile[] = "/tmp/update-check-response-deadline";

// Time in seconds after which we initiate reboot.
constexpr const base::TimeDelta kWaitBeforeRebootTime =
    base::TimeDelta::FromSeconds(2);

// Delay before showing error message if captive portal is detected.
// We wait for this delay to let captive portal to perform redirect and show
// its login page before error message appears.
constexpr const base::TimeDelta kDelayErrorMessage =
    base::TimeDelta::FromSeconds(10);

constexpr const base::TimeDelta kShowDelay =
    base::TimeDelta::FromMicroseconds(400);

// When battery percent is lower and DISCHARGING warn user about it.
const double kInsufficientBatteryPercent = 50;

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

}  // anonymous namespace

// static
std::string UpdateScreen::GetResultString(Result result) {
  switch (result) {
    case Result::UPDATE_NOT_REQUIRED:
      return "UpdateNotRequired";
    case Result::UPDATE_ERROR:
      return "UpdateError";
    case Result::UPDATE_SKIPPED:
      return chromeos::BaseScreen::kNotApplicable;
  }
}

// static
UpdateScreen* UpdateScreen::Get(ScreenManager* manager) {
  return static_cast<UpdateScreen*>(manager->GetScreen(UpdateView::kScreenId));
}

UpdateScreen::UpdateScreen(UpdateView* view,
                           ErrorScreen* error_screen,
                           const ScreenExitCallback& exit_callback)
    : BaseScreen(UpdateView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      error_screen_(error_screen),
      exit_callback_(exit_callback),
      histogram_helper_(
          std::make_unique<ErrorScreensHistogramHelper>("Update")),
      version_updater_(std::make_unique<VersionUpdater>(this)),
      wait_before_reboot_time_(kWaitBeforeRebootTime),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  if (view_)
    view_->Bind(this);
}

UpdateScreen::~UpdateScreen() {
  if (view_)
    view_->Unbind();
}

void UpdateScreen::OnViewDestroyed(UpdateView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool UpdateScreen::MaybeSkip(WizardContext* context) {
  if (context->enrollment_triggered_early) {
    LOG(WARNING) << "Skip OOBE Update because of enrollment request.";
    exit_callback_.Run(VersionUpdater::Result::UPDATE_SKIPPED);
    return true;
  }

  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
    LOG(WARNING) << "Skip OOBE Update for remora devices.";
    exit_callback_.Run(VersionUpdater::Result::UPDATE_SKIPPED);
    return true;
  }

  const auto* skip_screen_key = context->configuration.FindKeyOfType(
      configuration::kUpdateSkipUpdate, base::Value::Type::BOOLEAN);
  const bool skip_screen = skip_screen_key && skip_screen_key->GetBool();

  if (skip_screen) {
    LOG(WARNING) << "Skip OOBE Update because of configuration.";
    exit_callback_.Run(VersionUpdater::Result::UPDATE_SKIPPED);
    return true;
  }
  return false;
}

void UpdateScreen::ShowImpl() {
  if (chromeos::features::IsBetterUpdateEnabled()) {
    DCHECK(!power_manager_subscription_);
    power_manager_subscription_ = std::make_unique<
        ScopedObserver<PowerManagerClient, PowerManagerClient::Observer>>(this);
    power_manager_subscription_->Add(PowerManagerClient::Get());
    PowerManagerClient::Get()->RequestStatusUpdate();
  }
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (view_) {
    view_->SetCancelUpdateShortcutEnabled(true);
  }
#endif
  RefreshView(version_updater_->update_info());

  show_timer_.Start(FROM_HERE, kShowDelay,
                    base::BindOnce(&UpdateScreen::MakeSureScreenIsShown,
                                   weak_factory_.GetWeakPtr()));

  version_updater_->StartNetworkCheck();
}

void UpdateScreen::HideImpl() {
  power_manager_subscription_.reset();
  show_timer_.Stop();
  if (view_)
    view_->Hide();
  is_shown_ = false;
}

void UpdateScreen::OnUserAction(const std::string& action_id) {
  bool is_chrome_branded_build = false;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  is_chrome_branded_build = true;
#endif

  if (!is_chrome_branded_build &&
      action_id == kUserActionCancelUpdateShortcut) {
    // Skip update UI, usually used only in debug builds/tests.
    VLOG(1) << "Forced update cancel";
    ExitUpdate(Result::UPDATE_NOT_REQUIRED);
  } else if (action_id == kUserActionAcceptUpdateOverCellular) {
    version_updater_->SetUpdateOverCellularOneTimePermission();
  } else if (action_id == kUserActionRejectUpdateOverCellular) {
    version_updater_->RejectUpdateOverCellular();
    ExitUpdate(Result::UPDATE_ERROR);
  } else {
    BaseScreen::OnUserAction(action_id);
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
  if (chromeos::features::IsBetterUpdateEnabled()) {
    view_->SetUIState(UpdateView::UIState::kManualReboot);
  } else {
    view_->SetUpdateCompleted(true);
  }
}

void UpdateScreen::PrepareForUpdateCheck() {
  error_message_timer_.Stop();
  error_screen_->HideCaptivePortal();

  connect_request_subscription_.reset();
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

void UpdateScreen::UpdateErrorMessage(
    const NetworkPortalDetector::CaptivePortalStatus status,
    const NetworkError::ErrorState& error_state,
    const std::string& network_name) {
  error_screen_->SetErrorState(error_state, network_name);
  if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL) {
    if (is_first_portal_notification_) {
      is_first_portal_notification_ = false;
      error_screen_->FixCaptivePortal();
    }
  }
}

void UpdateScreen::DelayErrorMessage() {
  if (error_message_timer_.IsRunning())
    return;

  error_message_timer_.Start(FROM_HERE, kDelayErrorMessage, this,
                             &UpdateScreen::ShowErrorMessage);
}

void UpdateScreen::UpdateInfoChanged(
    const VersionUpdater::UpdateInfo& update_info) {
  const update_engine::StatusResult& status = update_info.status;
  hide_progress_on_exit_ = false;
  bool need_refresh_view = true;
  switch (status.current_operation()) {
    case update_engine::Operation::CHECKING_FOR_UPDATE:
      if (view_)
        view_->SetUIState(UpdateView::UIState::kCheckingForUpdate);
      if (start_update_stage_.is_null())
        start_update_stage_ = tick_clock_->NowTicks();
      need_refresh_view = false;
      break;
      // Do nothing in these cases, we don't want to notify the user of the
      // check unless there is an update.
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::IDLE:
      need_refresh_view = false;
      break;
    case update_engine::Operation::UPDATE_AVAILABLE:
      if (view_)
        view_->SetUIState(UpdateView::UIState::kCheckingForUpdate);
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
        view_->SetUIState(UpdateView::UIState::KUpdateInProgress);
      SetUpdateStatusMessage(update_info.better_update_progress,
                             update_info.total_time_left);
      MakeSureScreenIsShown();
      if (!is_critical_checked_) {
        // Because update engine doesn't send UPDATE_STATUS_UPDATE_AVAILABLE we
        // need to check if update is critical on first downloading
        // notification.
        is_critical_checked_ = true;
        if (!HasCriticalUpdate()) {
          VLOG(1) << "Non-critical update available: " << status.new_version();
          hide_progress_on_exit_ = true;
          ExitUpdate(Result::UPDATE_NOT_REQUIRED);
        } else {
          check_time_ = tick_clock_->NowTicks() - start_update_stage_;
          start_update_stage_ = start_update_downloading_ =
              tick_clock_->NowTicks();
          VLOG(1) << "Critical update available: " << status.new_version();
        }
      }
      break;
    case update_engine::Operation::VERIFYING:
      if (view_)
        view_->SetUIState(UpdateView::UIState::KUpdateInProgress);
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
        view_->SetUIState(UpdateView::UIState::KUpdateInProgress);
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
        if (chromeos::features::IsBetterUpdateEnabled()) {
          ShowRebootInProgress();
          wait_reboot_timer_.Start(FROM_HERE, wait_before_reboot_time_,
                                   version_updater_.get(),
                                   &VersionUpdater::RebootAfterUpdate);
        } else {
          version_updater_->RebootAfterUpdate();
        }
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
      need_refresh_view = false;
      break;
    default:
      NOTREACHED();
  }
  if (chromeos::features::IsBetterUpdateEnabled())
    UpdateBatteryWarningVisibility();
  if (need_refresh_view)
    RefreshView(update_info);
}

void UpdateScreen::FinishExitUpdate(Result result) {
  if (!start_update_stage_.is_null()) {
    check_time_ = (check_time_.is_zero())
                      ? tick_clock_->NowTicks() - start_update_stage_
                      : check_time_;
    RecordCheckTime(check_time_);
  }
  show_timer_.Stop();
  exit_callback_.Run(result);
}

void UpdateScreen::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  UpdateBatteryWarningVisibility();
}

void UpdateScreen::ShowRebootInProgress() {
  MakeSureScreenIsShown();
  if (view_) {
    if (chromeos::features::IsBetterUpdateEnabled()) {
      view_->SetUIState(UpdateView::UIState::kRestartInProgress);
    } else {
      view_->SetUpdateCompleted(true);
    }
  }
}

void UpdateScreen::SetUpdateStatusMessage(int percent,
                                          base::TimeDelta time_left) {
  if (!view_)
    return;
  base::string16 time_left_message;
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
  const base::Optional<power_manager::PowerSupplyProperties>& proto =
      PowerManagerClient::Get()->GetLastStatus();
  if (!proto.has_value())
    return;
  view_->ShowLowBatteryWarningMessage(
      is_critical_checked_ && HasCriticalUpdate() &&
      proto->battery_state() ==
          power_manager::PowerSupplyProperties_BatteryState_DISCHARGING &&
      proto->battery_percent() < kInsufficientBatteryPercent);
}

void UpdateScreen::RefreshView(const VersionUpdater::UpdateInfo& update_info) {
  if (view_) {
    view_->SetProgress(update_info.progress);
    view_->SetProgressMessage(update_info.progress_message);
    view_->SetEstimatedTimeLeft(update_info.estimated_time_left_in_secs);
    view_->SetShowEstimatedTimeLeft(update_info.show_estimated_time_left);
    view_->SetShowCurtain(update_info.progress_unavailable ||
                          hide_progress_on_exit_);
    view_->SetRequiresPermissionForCellular(
        update_info.requires_permission_for_cellular);
  }
}

bool UpdateScreen::HasCriticalUpdate() {
  if (ignore_update_deadlines_)
    return true;
  if (has_critical_update_.has_value())
    return has_critical_update_.value();
  has_critical_update_ = true;

  std::string deadline;
  // Checking for update flag file causes us to do blocking IO on UI thread.
  // Temporarily allow it until we fix http://crosbug.com/11106
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath update_deadline_file_path(kUpdateDeadlineFile);
  if (!base::ReadFileToString(update_deadline_file_path, &deadline) ||
      deadline.empty()) {
    has_critical_update_ = false;
    return false;
  }

  // TODO(dpolukhin): Analyze file content. Now we can just assume that
  // if the file exists and not empty, there is critical update.
  return true;
}

void UpdateScreen::MakeSureScreenIsShown() {
  show_timer_.Stop();

  if (is_shown_ || !view_)
    return;

  is_shown_ = true;
  histogram_helper_->OnScreenShow();

  view_->Show();
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

void UpdateScreen::OnErrorScreenHidden() {
  error_screen_->SetParentScreen(OobeScreen::SCREEN_UNKNOWN);
  Show(context());
}

}  // namespace chromeos
