// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/version_updater/version_updater.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/version_updater/update_time_estimator.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Time in seconds after which we decide that the device has not rebooted
// automatically. If reboot didn't happen during this interval, ask user to
// reboot device manually.
constexpr const base::TimeDelta kWaitForRebootTime = base::Seconds(3);

// Progress bar stages. Each represents progress bar value
// at the beginning of each stage.
// TODO(nkostylev): Base stage progress values on approximate time.
// TODO(nkostylev): Animate progress during each state.
const int kBeforeUpdateCheckProgress = 7;
const int kBeforeDownloadProgress = 14;
const int kBeforeVerifyingProgress = 74;
const int kBeforeFinalizingProgress = 81;
const int kProgressComplete = 100;

// Defines what part of update progress does download part takes.
const int kDownloadProgressIncrement = 60;

// Period of time between planned updates.
constexpr const base::TimeDelta kUpdateTime = base::Seconds(1);

void RecordUpdateCheckRetryCountForVersionUpdater(int num_retries) {
  base::UmaHistogramCounts100("OOBE.VersionUpdater.UpdateCheckRetriesCount",
                              num_retries);
}

void RecordVersionUpdaterUpdateEngineOperation(
    const update_engine::StatusResult& status) {
  if (status.is_install()) {
    base::UmaHistogramExactLinear(
        "OOBE.VersionUpdater.UpdateEngineOperation.DLCInstall",
        static_cast<int>(status.current_operation()),
        update_engine::Operation_ARRAYSIZE);
  } else {
    base::UmaHistogramExactLinear(
        "OOBE.VersionUpdater.UpdateEngineOperation.Update",
        static_cast<int>(status.current_operation()),
        update_engine::Operation_ARRAYSIZE);
  }
}

void RecordCheckingForUpdateTime(const base::TimeDelta duration) {
  base::UmaHistogramLongTimes("OOBE.VersionUpdater.UpdateCheckDuration",
                              duration);
}

}  // anonymous namespace

VersionUpdater::UpdateInfo::UpdateInfo() {}

VersionUpdater::VersionUpdater(VersionUpdater::Delegate* delegate)
    : delegate_(delegate),
      wait_for_reboot_time_(kWaitForRebootTime),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  Init();
}

VersionUpdater::~VersionUpdater() {
  UpdateEngineClient::Get()->RemoveObserver(this);
  if (NetworkHandler::IsInitialized())
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

void VersionUpdater::Init() {
  time_estimator_ = UpdateTimeEstimator();
  update_info_ = UpdateInfo();
}

void VersionUpdater::StartNetworkCheck() {
  // If portal detector is enabled and portal detection before AU is
  // allowed, initiate network state check. Otherwise, directly
  // proceed to update.
  if (!network_portal_detector::GetInstance()->IsEnabled()) {
    StartUpdateCheck();
    return;
  }

  delegate_->UpdateInfoChanged(update_info_);

  if (NetworkHandler::IsInitialized()) {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    if (!handler->HasObserver(this))
      handler->AddObserver(this);
    const NetworkState* default_network = handler->DefaultNetwork();
    PortalStateChanged(default_network,
                       default_network ? default_network->GetPortalState()
                                       : NetworkState::PortalState::kUnknown);
  }
}

void VersionUpdater::StartUpdateCheck() {
  delegate_->PrepareForUpdateCheck();
  retry_check_timer_.Start(FROM_HERE, retry_check_timeout_,
                           base::BindOnce(&VersionUpdater::OnRetryCheckElapsed,
                                          weak_ptr_factory_.GetWeakPtr()));
  RequestUpdateCheck();
}

void VersionUpdater::SetUpdateOverCellularOneTimePermission() {
  UpdateEngineClient::Get()->SetUpdateOverCellularOneTimePermission(
      update_info_.update_version, update_info_.update_size,
      base::BindOnce(&VersionUpdater::OnSetUpdateOverCellularOneTimePermission,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VersionUpdater::StopObserving() {
  UpdateEngineClient::Get()->RemoveObserver(this);
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
  }
}

void VersionUpdater::RejectUpdateOverCellular() {
  // Reset UI context to show curtain again when the user goes back to the
  // screen.
  update_info_.progress_unavailable = true;
  update_info_.requires_permission_for_cellular = false;
  delegate_->UpdateInfoChanged(update_info_);
}

void VersionUpdater::RebootAfterUpdate() {
  VLOG(1) << "Initiate reboot after update";
  UpdateEngineClient::Get()->RebootAfterUpdate();
  if (wait_for_reboot_time_.is_zero())  // Primarily for testing.
    OnWaitForRebootTimeElapsed();
  else
    reboot_timer_.Start(FROM_HERE, wait_for_reboot_time_, this,
                        &VersionUpdater::OnWaitForRebootTimeElapsed);
}

void VersionUpdater::StartExitUpdate(Result result) {
  UpdateEngineClient::Get()->RemoveObserver(this);
  if (NetworkHandler::IsInitialized())
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
  delegate_->FinishExitUpdate(result);
  // Reset internal state, because in case of error user may make another
  // update attempt.
  Init();
  RecordUpdateCheckRetryCountForVersionUpdater(num_retries_);
}

void VersionUpdater::GetEolInfo(EolInfoCallback callback) {
  // Request the End of Life (Auto Update Expiration) status. Bind to a weak_ptr
  // bound method rather than passing `callback` directly so that `callback`
  // does not outlive `this`.
  UpdateEngineClient::Get()->GetEolInfo(
      base::BindOnce(&VersionUpdater::OnGetEolInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void VersionUpdater::OnGetEolInfo(EolInfoCallback callback,
                                  UpdateEngineClient::EolInfo info) {
  std::move(callback).Run(std::move(info));
}

void VersionUpdater::UpdateStatusChangedForTesting(
    const update_engine::StatusResult& status) {
  UpdateStatusChanged(status);
}

void VersionUpdater::RequestUpdateCheck() {
  update_info_.state = State::STATE_UPDATE;
  update_info_.progress = kBeforeUpdateCheckProgress;
  update_info_.update_version = std::string();
  update_info_.update_size = 0;
  delegate_->UpdateInfoChanged(update_info_);

  if (!UpdateEngineClient::Get()->HasObserver(this)) {
    UpdateEngineClient::Get()->AddObserver(this);
  }
  VLOG(1) << "Initiate update check";
  checking_for_update_start_ = tick_clock_->NowTicks();
  TriggerUpdateCheck();
}

void VersionUpdater::TriggerUpdateCheck() {
  num_retries_++;
  UpdateEngineClient::Get()->RequestUpdateCheck(base::BindOnce(
      &VersionUpdater::OnUpdateCheckStarted, weak_ptr_factory_.GetWeakPtr()));
}

void VersionUpdater::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  RecordVersionUpdaterUpdateEngineOperation(status);
  // If the status change is for an installation, this means that DLCs are being
  // installed and has nothing to do with the OS. Ignore this status change.
  // Do not ignore update_engine::Operation::IDLE even if is_install is true,
  // because install stays true on status changes after a DLC install, even if
  // no DLC install is in progress anymore.
  if (status.is_install() &&
      status.current_operation() != update_engine::Operation::IDLE) {
    LOG(WARNING) << "Ignoring update status change related to DLC install.";
    return;
  }

  update_info_.status = status;

  if (update_info_.is_checking_for_update &&
      status.current_operation() >
          update_engine::Operation::CHECKING_FOR_UPDATE &&
      status.current_operation() != update_engine::Operation::ERROR &&
      status.current_operation() !=
          update_engine::Operation::REPORTING_ERROR_EVENT) {
    update_info_.is_checking_for_update = false;
  }
  if (!non_idle_status_received_ &&
      status.current_operation() > update_engine::Operation::IDLE) {
    non_idle_status_received_ = true;
    RecordCheckingForUpdateTime(tick_clock_->NowTicks() -
                                checking_for_update_start_);
  }

  time_estimator_.Update(status);

  bool exit_update = false;
  switch (status.current_operation()) {
    case update_engine::Operation::CHECKING_FOR_UPDATE:
      break;
    case update_engine::Operation::UPDATE_AVAILABLE:
      update_info_.progress = kBeforeDownloadProgress;
      update_info_.progress_message =
          l10n_util::GetStringUTF16(IDS_UPDATE_AVAILABLE);
      update_info_.show_estimated_time_left = false;
      update_info_.progress_unavailable = false;
      break;
    case update_engine::Operation::DOWNLOADING:
      update_info_.progress_message =
          l10n_util::GetStringUTF16(IDS_INSTALLING_UPDATE);
      update_info_.progress_unavailable = false;
      update_info_.progress =
          kBeforeDownloadProgress +
          static_cast<int>(status.progress() * kDownloadProgressIncrement);
      update_info_.show_estimated_time_left = time_estimator_.HasDownloadTime();
      update_info_.estimated_time_left_in_secs =
          time_estimator_.GetDownloadTimeLeft().InSeconds();
      break;
    case update_engine::Operation::VERIFYING:
      update_info_.progress = kBeforeVerifyingProgress;
      update_info_.progress_message =
          l10n_util::GetStringUTF16(IDS_UPDATE_VERIFYING);
      update_info_.show_estimated_time_left = false;
      break;
    case update_engine::Operation::FINALIZING:
      update_info_.progress = kBeforeFinalizingProgress;
      update_info_.progress_message =
          l10n_util::GetStringUTF16(IDS_UPDATE_FINALIZING);
      update_info_.show_estimated_time_left = false;
      break;
    case update_engine::Operation::UPDATED_NEED_REBOOT:
      update_info_.progress = kProgressComplete;
      update_info_.show_estimated_time_left = false;
      update_info_.progress_unavailable = false;
      break;
    case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
      VLOG(1) << "Update requires user permission to proceed.";
      update_info_.state = State::STATE_REQUESTING_USER_PERMISSION;
      update_info_.update_version = status.new_version();
      update_info_.update_size = status.new_size();
      update_info_.requires_permission_for_cellular = true;
      update_info_.progress_unavailable = false;

      UpdateEngineClient::Get()->RemoveObserver(this);
      break;
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
      VLOG(1) << "Attempting rollback";
      break;
    case update_engine::Operation::IDLE:
      // Exit update only if update engine was in non-idle status before.
      // Otherwise resend the update which may have been ignored due to busy.
      if (non_idle_status_received_) {
        exit_update = true;
      } else {
        TriggerUpdateCheck();
      }
      break;
    case update_engine::Operation::CLEANUP_PREVIOUS_UPDATE:
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::ERROR:
    case update_engine::Operation::REPORTING_ERROR_EVENT:
    case update_engine::Operation::UPDATED_BUT_DEFERRED:
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (time_estimator_.HasTotalTime(status.current_operation())) {
    const auto update_status = time_estimator_.GetUpdateStatus();
    update_info_.total_time_left = update_status.time_left;
    update_info_.better_update_progress = update_status.progress;
    if (!refresh_timer_) {
      refresh_timer_ = std::make_unique<base::RepeatingTimer>(tick_clock_);
      refresh_timer_->Start(FROM_HERE, kUpdateTime, this,
                            &VersionUpdater::RefreshTimeLeftEstimation);
    }
  } else {
    if (refresh_timer_) {
      refresh_timer_->Stop();
      refresh_timer_.reset();
    }
  }

  delegate_->UpdateInfoChanged(update_info_);
  if (exit_update)
    StartExitUpdate(Result::UPDATE_NOT_REQUIRED);
}

void VersionUpdater::RefreshTimeLeftEstimation() {
  const auto update_status = time_estimator_.GetUpdateStatus();
  update_info_.total_time_left = update_status.time_left;
  update_info_.better_update_progress = update_status.progress;
  delegate_->UpdateInfoChanged(update_info_);
}

void VersionUpdater::PortalStateChanged(const NetworkState* network,
                                        const NetworkState::PortalState state) {
  VLOG(1) << "VersionUpdater::PortalStateChanged(): "
          << "network=" << (network ? network->path() : "")
          << ", portal state=" << state;

  // Wait for sane detection results.
  if (network && state == NetworkState::PortalState::kUnknown) {
    return;
  }

  if (update_info_.state == State::STATE_ERROR) {
    // In the case of online state hide error message and proceed to
    // the update stage. Otherwise, update error message content.
    if (state == NetworkState::PortalState::kOnline)
      StartUpdateCheck();
    else
      UpdateErrorMessage(network, state);
  } else {
    // In the case of online state immediately proceed to the update
    // stage. Otherwise, prepare and show error message.
    if (state == NetworkState::PortalState::kOnline) {
      StartUpdateCheck();
    } else {
      UpdateErrorMessage(network, state);

      // StartUpdateCheck, which gets called when the error clears up, will add
      // the update engine observer back.
      UpdateEngineClient::Get()->RemoveObserver(this);

      update_info_.state = State::STATE_ERROR;
      delegate_->UpdateInfoChanged(update_info_);
      if (state == NetworkState::PortalState::kPortal ||
          state == NetworkState::PortalState::kPortalSuspected) {
        delegate_->DelayErrorMessage();
      } else {
        delegate_->ShowErrorMessage();
      }
    }
  }
}

void VersionUpdater::OnRetryCheckElapsed() {
  // If update_engine didn't handle our request, exit with update_not_requiered.
  if (!non_idle_status_received_) {
    LOG(WARNING) << "Exiting update after retry check timout.";
    StartExitUpdate(Result::UPDATE_CHECK_TIMEOUT);
  }
}

void VersionUpdater::OnShuttingDown() {
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this);
}

void VersionUpdater::OnWaitForRebootTimeElapsed() {
  delegate_->OnWaitForRebootTimeElapsed();
}

void VersionUpdater::UpdateErrorMessage(const NetworkState* network,
                                        NetworkState::PortalState state) {
  std::string network_name = std::string();
  NetworkError::ErrorState error_state;
  switch (state) {
    case NetworkState::PortalState::kUnknown:
      [[fallthrough]];
    case NetworkState::PortalState::kNoInternet:
      error_state = NetworkError::ERROR_STATE_OFFLINE;
      break;
    case NetworkState::PortalState::kPortal:
      [[fallthrough]];
    case NetworkState::PortalState::kPortalSuspected:
      DCHECK(network);
      error_state = NetworkError::ERROR_STATE_PORTAL;
      network_name = network->name();
      break;
    case NetworkState::PortalState::kOnline:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  delegate_->UpdateErrorMessage(state, error_state, network_name);
}

void VersionUpdater::OnSetUpdateOverCellularOneTimePermission(bool success) {
  update_info_.requires_permission_for_cellular = false;
  if (!success) {
    // Reset UI context to show curtain again when the user goes back to the
    // screen.
    update_info_.progress_unavailable = true;
  }
  delegate_->UpdateInfoChanged(update_info_);

  if (success) {
    StartUpdateCheck();
  } else {
    StartExitUpdate(Result::UPDATE_ERROR);
  }
}

void VersionUpdater::OnUpdateCheckStarted(
    UpdateEngineClient::UpdateCheckResult result) {
  VLOG(1) << "Callback from RequestUpdateCheck, result " << result;
  if (result != UpdateEngineClient::UPDATE_RESULT_SUCCESS)
    StartExitUpdate(Result::UPDATE_NOT_REQUIRED);
}

}  // namespace ash
