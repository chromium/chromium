// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_screen.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/screens/update_view.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using pairing_chromeos::HostPairingController;

namespace chromeos {

namespace {

constexpr const char kContextKeyEstimatedTimeLeftSec[] = "time-left-sec";
constexpr const char kContextKeyShowEstimatedTimeLeft[] = "show-time-left";
constexpr const char kContextKeyUpdateCompleted[] = "update-completed";
constexpr const char kContextKeyShowCurtain[] = "show-curtain";
constexpr const char kContextKeyShowProgressMessage[] = "show-progress-msg";
constexpr const char kContextKeyProgress[] = "progress";
constexpr const char kContextKeyProgressMessage[] = "progress-msg";
constexpr const char kContextKeyRequiresPermissionForCelluar[] =
    "requires-permission-for-cellular";

constexpr const char kUserActionAcceptUpdateOverCellular[] =
    "update-accept-cellular";
constexpr const char kUserActionRejectUpdateOverCellular[] =
    "update-reject-cellular";

#if !defined(OFFICIAL_BUILD)
constexpr const char kUserActionCancelUpdateShortcut[] = "cancel-update";
constexpr const char kContextKeyCancelUpdateShortcutEnabled[] =
    "cancel-update-enabled";
#endif

// If reboot didn't happen, ask user to reboot device manually.
const int kWaitForRebootTimeSec = 3;

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

const char kUpdateDeadlineFile[] = "/tmp/update-check-response-deadline";

// Minimum timestep between two consecutive measurements for the download rates.
const int kMinTimeStepInSeconds = 1;

// Smooth factor that is used for the average downloading speed
// estimation.
// avg_speed = smooth_factor * cur_speed + (1.0 - smooth_factor) *
// avg_speed.
const double kDownloadSpeedSmoothFactor = 0.1;

// Minumum allowed value for the average downloading speed.
const double kDownloadAverageSpeedDropBound = 1e-8;

// An upper bound for possible downloading time left estimations.
const double kMaxTimeLeft = 24 * 60 * 60;

// Delay before showing error message if captive portal is detected.
// We wait for this delay to let captive portal to perform redirect and show
// its login page before error message appears.
const int kDelayErrorMessageSec = 10;

// The delay in milliseconds at which we will send the host status to the Master
// device periodically during the updating process.
const int kHostStatusReportDelay = 5 * 60 * 1000;

// Invoked from call to RequestUpdateCheck upon completion of the DBus call.
void StartUpdateCallback(UpdateScreen* screen,
                         UpdateEngineClient::UpdateCheckResult result) {
  VLOG(1) << "Callback from RequestUpdateCheck, result " << result;
  if (UpdateScreen::HasInstance(screen) &&
      result != UpdateEngineClient::UPDATE_RESULT_SUCCESS) {
    screen->ExitUpdate(UpdateScreen::REASON_UPDATE_INIT_FAILED);
  }
}

}  // anonymous namespace

// static
UpdateScreen::InstanceSet& UpdateScreen::GetInstanceSet() {
  static base::NoDestructor<std::set<UpdateScreen*>> instance_set;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);  // not threadsafe.
  return *instance_set;
}

// static
bool UpdateScreen::HasInstance(UpdateScreen* inst) {
  InstanceSet& instance_set = GetInstanceSet();
  InstanceSet::iterator found = instance_set.find(inst);
  return (found != instance_set.end());
}

// static
UpdateScreen* UpdateScreen::Get(ScreenManager* manager) {
  return static_cast<UpdateScreen*>(
      manager->GetScreen(OobeScreen::SCREEN_OOBE_UPDATE));
}

UpdateScreen::UpdateScreen(BaseScreenDelegate* base_screen_delegate,
                           UpdateView* view,
                           HostPairingController* remora_controller)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_OOBE_UPDATE),
      reboot_check_delay_(kWaitForRebootTimeSec),
      view_(view),
      remora_controller_(remora_controller),
      histogram_helper_(new ErrorScreensHistogramHelper("Update")),
      weak_factory_(this) {
  if (view_)
    view_->Bind(this);

  GetInstanceSet().insert(this);
}

UpdateScreen::~UpdateScreen() {
  if (view_)
    view_->Unbind();

  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
  network_portal_detector::GetInstance()->RemoveObserver(this);
  GetInstanceSet().erase(this);
}

void UpdateScreen::OnViewDestroyed(UpdateView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void UpdateScreen::StartNetworkCheck() {
  // If portal detector is enabled and portal detection before AU is
  // allowed, initiate network state check. Otherwise, directly
  // proceed to update.
  if (!network_portal_detector::GetInstance()->IsEnabled()) {
    StartUpdateCheck();
    return;
  }
  state_ = State::STATE_FIRST_PORTAL_CHECK;
  is_first_detection_notification_ = true;
  is_first_portal_notification_ = true;
  network_portal_detector::GetInstance()->AddAndFireObserver(this);
}

void UpdateScreen::SetIgnoreIdleStatus(bool ignore_idle_status) {
  ignore_idle_status_ = ignore_idle_status;
}

void UpdateScreen::ExitUpdate(UpdateScreen::ExitReason reason) {
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
  network_portal_detector::GetInstance()->RemoveObserver(this);
  SetHostPairingControllerStatus(HostPairingController::UPDATE_STATUS_UPDATED);

  switch (reason) {
    case REASON_UPDATE_CANCELED:
      Finish(ScreenExitCode::UPDATE_NOUPDATE);
      break;
    case REASON_UPDATE_INIT_FAILED:
      Finish(ScreenExitCode::UPDATE_ERROR_CHECKING_FOR_UPDATE);
      break;
    case REASON_UPDATE_OVER_CELLULAR_REJECTED:
      Finish(ScreenExitCode::UPDATE_REJECT_OVER_CELLULAR);
      break;
    case REASON_UPDATE_NON_CRITICAL:
    case REASON_UPDATE_ENDED: {
      UpdateEngineClient* update_engine_client =
          DBusThreadManager::Get()->GetUpdateEngineClient();
      switch (update_engine_client->GetLastStatus().status) {
        case UpdateEngineClient::UPDATE_STATUS_ATTEMPTING_ROLLBACK:
          break;
        case UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE:
        case UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT:
        case UpdateEngineClient::UPDATE_STATUS_DOWNLOADING:
        case UpdateEngineClient::UPDATE_STATUS_FINALIZING:
        case UpdateEngineClient::UPDATE_STATUS_VERIFYING:
        case UpdateEngineClient::UPDATE_STATUS_NEED_PERMISSION_TO_UPDATE:
          DCHECK(!HasCriticalUpdate());
          // Noncritical update, just exit screen as if there is no update.
          FALLTHROUGH;
        case UpdateEngineClient::UPDATE_STATUS_IDLE:
          Finish(ScreenExitCode::UPDATE_NOUPDATE);
          break;
        case UpdateEngineClient::UPDATE_STATUS_ERROR:
        case UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT:
          if (is_checking_for_update_) {
            Finish(ScreenExitCode::UPDATE_ERROR_CHECKING_FOR_UPDATE);
          } else if (HasCriticalUpdate()) {
            Finish(ScreenExitCode::UPDATE_ERROR_UPDATING_CRITICAL_UPDATE);
          } else {
            Finish(ScreenExitCode::UPDATE_ERROR_UPDATING);
          }
          break;
        default:
          NOTREACHED();
      }
    } break;
    default:
      NOTREACHED();
  }
}

void UpdateScreen::UpdateStatusChanged(
    const UpdateEngineClient::Status& status) {
  if (is_checking_for_update_ &&
      status.status > UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE) {
    is_checking_for_update_ = false;
  }
  if (ignore_idle_status_ &&
      status.status > UpdateEngineClient::UPDATE_STATUS_IDLE) {
    ignore_idle_status_ = false;
  }

  switch (status.status) {
    case UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      // Do nothing in these cases, we don't want to notify the user of the
      // check unless there is an update.
      SetHostPairingControllerStatus(
          HostPairingController::UPDATE_STATUS_UPDATING);
      break;
    case UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE:
      MakeSureScreenIsShown();
      GetContextEditor()
          .SetInteger(kContextKeyProgress, kBeforeDownloadProgress)
          .SetBoolean(kContextKeyShowEstimatedTimeLeft, false);
      if (!HasCriticalUpdate()) {
        VLOG(1) << "Noncritical update available: " << status.new_version;
        ExitUpdate(REASON_UPDATE_NON_CRITICAL);
      } else {
        VLOG(1) << "Critical update available: " << status.new_version;
        GetContextEditor()
            .SetString(kContextKeyProgressMessage,
                       l10n_util::GetStringUTF16(IDS_UPDATE_AVAILABLE))
            .SetBoolean(kContextKeyShowProgressMessage, true)
            .SetBoolean(kContextKeyShowCurtain, false);
      }
      break;
    case UpdateEngineClient::UPDATE_STATUS_DOWNLOADING:
      MakeSureScreenIsShown();
      if (!is_downloading_update_) {
        // Because update engine doesn't send UPDATE_STATUS_UPDATE_AVAILABLE
        // we need to is update critical on first downloading notification.
        is_downloading_update_ = true;
        download_start_time_ = download_last_time_ = base::Time::Now();
        download_start_progress_ = status.download_progress;
        download_last_progress_ = status.download_progress;
        is_download_average_speed_computed_ = false;
        download_average_speed_ = 0.0;
        if (!HasCriticalUpdate()) {
          VLOG(1) << "Non-critical update available: " << status.new_version;
          ExitUpdate(REASON_UPDATE_NON_CRITICAL);
        } else {
          VLOG(1) << "Critical update available: " << status.new_version;
          GetContextEditor()
              .SetString(kContextKeyProgressMessage,
                         l10n_util::GetStringUTF16(IDS_INSTALLING_UPDATE))
              .SetBoolean(kContextKeyShowProgressMessage, true)
              .SetBoolean(kContextKeyShowCurtain, false);
        }
      }
      UpdateDownloadingStats(status);
      break;
    case UpdateEngineClient::UPDATE_STATUS_VERIFYING:
      MakeSureScreenIsShown();
      GetContextEditor()
          .SetInteger(kContextKeyProgress, kBeforeVerifyingProgress)
          .SetString(kContextKeyProgressMessage,
                     l10n_util::GetStringUTF16(IDS_UPDATE_VERIFYING))
          .SetBoolean(kContextKeyShowProgressMessage, true);
      break;
    case UpdateEngineClient::UPDATE_STATUS_FINALIZING:
      MakeSureScreenIsShown();
      GetContextEditor()
          .SetInteger(kContextKeyProgress, kBeforeFinalizingProgress)
          .SetString(kContextKeyProgressMessage,
                     l10n_util::GetStringUTF16(IDS_UPDATE_FINALIZING))
          .SetBoolean(kContextKeyShowProgressMessage, true);
      break;
    case UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      MakeSureScreenIsShown();
      GetContextEditor()
          .SetInteger(kContextKeyProgress, kProgressComplete)
          .SetBoolean(kContextKeyShowEstimatedTimeLeft, false);
      if (HasCriticalUpdate()) {
        GetContextEditor().SetBoolean(kContextKeyShowCurtain, false);
        VLOG(1) << "Initiate reboot after update";
        SetHostPairingControllerStatus(
            HostPairingController::UPDATE_STATUS_REBOOTING);
        DBusThreadManager::Get()->GetUpdateEngineClient()->RebootAfterUpdate();
        reboot_timer_.Start(FROM_HERE,
                            base::TimeDelta::FromSeconds(reboot_check_delay_),
                            this, &UpdateScreen::OnWaitForRebootTimeElapsed);
      } else {
        ExitUpdate(REASON_UPDATE_NON_CRITICAL);
      }
      break;
    case UpdateEngineClient::UPDATE_STATUS_NEED_PERMISSION_TO_UPDATE:
      VLOG(1) << "Update requires user permission to proceed.";
      state_ = State::STATE_REQUESTING_USER_PERMISSION;
      pending_update_version_ = status.new_version;
      pending_update_size_ = status.new_size;

      DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);

      MakeSureScreenIsShown();
      GetContextEditor()
          .SetBoolean(kContextKeyRequiresPermissionForCelluar, true)
          .SetBoolean(kContextKeyShowCurtain, false);
      break;
    case UpdateEngineClient::UPDATE_STATUS_ATTEMPTING_ROLLBACK:
      VLOG(1) << "Attempting rollback";
      break;
    case UpdateEngineClient::UPDATE_STATUS_IDLE:
      if (ignore_idle_status_) {
        // It is first IDLE status that is sent before we initiated the check.
        break;
      }
      FALLTHROUGH;
    case UpdateEngineClient::UPDATE_STATUS_ERROR:
    case UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT:
      ExitUpdate(REASON_UPDATE_ENDED);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void UpdateScreen::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  VLOG(1) << "UpdateScreen::OnPortalDetectionCompleted(): "
          << "network=" << (network ? network->path() : "") << ", "
          << "state.status=" << state.status << ", "
          << "state.response_code=" << state.response_code;

  // Wait for sane detection results.
  if (network &&
      state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN) {
    return;
  }

  // Restart portal detection for the first notification about offline state.
  if ((!network ||
       state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE) &&
      is_first_detection_notification_) {
    is_first_detection_notification_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce([]() {
          network_portal_detector::GetInstance()->StartPortalDetection(
              false /* force */);
        }));
    return;
  }
  is_first_detection_notification_ = false;

  NetworkPortalDetector::CaptivePortalStatus status = state.status;
  if (state_ == State::STATE_ERROR) {
    // In the case of online state hide error message and proceed to
    // the update stage. Otherwise, update error message content.
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE)
      StartUpdateCheck();
    else
      UpdateErrorMessage(network, status);
  } else if (state_ == State::STATE_FIRST_PORTAL_CHECK) {
    // In the case of online state immediately proceed to the update
    // stage. Otherwise, prepare and show error message.
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
      StartUpdateCheck();
    } else {
      UpdateErrorMessage(network, status);

      if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL)
        DelayErrorMessage();
      else
        ShowErrorMessage();
    }
  }
}

void UpdateScreen::CancelUpdate() {
  VLOG(1) << "Forced update cancel";
  ExitUpdate(REASON_UPDATE_CANCELED);
}

// TODO(jdufault): This should return a pointer. See crbug.com/672142.
base::OneShotTimer& UpdateScreen::GetErrorMessageTimerForTesting() {
  return error_message_timer_;
}

void UpdateScreen::Show() {
  is_shown_ = true;
  histogram_helper_->OnScreenShow();

#if !defined(OFFICIAL_BUILD)
  GetContextEditor().SetBoolean(kContextKeyCancelUpdateShortcutEnabled, true);
#endif
  GetContextEditor()
      .SetInteger(kContextKeyProgress, kBeforeUpdateCheckProgress)
      .SetBoolean(kContextKeyRequiresPermissionForCelluar, false);

  if (view_)
    view_->Show();
}

void UpdateScreen::Hide() {
  if (view_)
    view_->Hide();
  is_shown_ = false;
}

void UpdateScreen::OnUserAction(const std::string& action_id) {
#if !defined(OFFICIAL_BUILD)
  if (action_id == kUserActionCancelUpdateShortcut)
    CancelUpdate();
  else
#endif
      if (action_id == kUserActionAcceptUpdateOverCellular) {
    DBusThreadManager::Get()
        ->GetUpdateEngineClient()
        ->SetUpdateOverCellularOneTimePermission(
            pending_update_version_, pending_update_size_,
            base::BindRepeating(
                &UpdateScreen::RetryUpdateWithUpdateOverCellularPermissionSet,
                weak_factory_.GetWeakPtr()));
  } else if (action_id == kUserActionRejectUpdateOverCellular) {
    // Reset UI context to show curtain again when the user goes back to the
    // update screen.
    GetContextEditor()
        .SetBoolean(kContextKeyShowCurtain, true)
        .SetBoolean(kContextKeyRequiresPermissionForCelluar, false);
    ExitUpdate(REASON_UPDATE_OVER_CELLULAR_REJECTED);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void UpdateScreen::RetryUpdateWithUpdateOverCellularPermissionSet(
    bool success) {
  if (success) {
    GetContextEditor().SetBoolean(kContextKeyRequiresPermissionForCelluar,
                                  false);
    StartUpdateCheck();
  } else {
    // Reset UI context to show curtain again when the user goes back to the
    // update screen.
    GetContextEditor()
        .SetBoolean(kContextKeyShowCurtain, true)
        .SetBoolean(kContextKeyRequiresPermissionForCelluar, false);
    ExitUpdate(REASON_UPDATE_OVER_CELLULAR_REJECTED);
  }
}

void UpdateScreen::UpdateDownloadingStats(
    const UpdateEngineClient::Status& status) {
  base::Time download_current_time = base::Time::Now();
  if (download_current_time >=
      download_last_time_ +
          base::TimeDelta::FromSeconds(kMinTimeStepInSeconds)) {
    // Estimate downloading rate.
    double progress_delta =
        std::max(status.download_progress - download_last_progress_, 0.0);
    double time_delta =
        (download_current_time - download_last_time_).InSecondsF();
    double download_rate = status.new_size * progress_delta / time_delta;

    download_last_time_ = download_current_time;
    download_last_progress_ = status.download_progress;

    // Estimate time left.
    double progress_left = std::max(1.0 - status.download_progress, 0.0);
    if (!is_download_average_speed_computed_) {
      download_average_speed_ = download_rate;
      is_download_average_speed_computed_ = true;
    }
    download_average_speed_ =
        kDownloadSpeedSmoothFactor * download_rate +
        (1.0 - kDownloadSpeedSmoothFactor) * download_average_speed_;
    if (download_average_speed_ < kDownloadAverageSpeedDropBound) {
      time_delta = (download_current_time - download_start_time_).InSecondsF();
      download_average_speed_ =
          status.new_size *
          (status.download_progress - download_start_progress_) / time_delta;
    }
    double work_left = progress_left * status.new_size;
    double time_left = work_left / download_average_speed_;
    // |time_left| may be large enough or even +infinity. So we must
    // |bound possible estimations.
    time_left = std::min(time_left, kMaxTimeLeft);

    GetContextEditor()
        .SetBoolean(kContextKeyShowEstimatedTimeLeft, true)
        .SetInteger(kContextKeyEstimatedTimeLeftSec,
                    static_cast<int>(time_left));
  }

  int download_progress =
      static_cast<int>(status.download_progress * kDownloadProgressIncrement);
  GetContextEditor().SetInteger(kContextKeyProgress,
                                kBeforeDownloadProgress + download_progress);
}

bool UpdateScreen::HasCriticalUpdate() {
  if (is_ignore_update_deadlines_)
    return true;

  std::string deadline;
  // Checking for update flag file causes us to do blocking IO on UI thread.
  // Temporarily allow it until we fix http://crosbug.com/11106
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath update_deadline_file_path(kUpdateDeadlineFile);
  if (!base::ReadFileToString(update_deadline_file_path, &deadline) ||
      deadline.empty()) {
    return false;
  }

  // TODO(dpolukhin): Analyze file content. Now we can just assume that
  // if the file exists and not empty, there is critical update.
  return true;
}

void UpdateScreen::OnWaitForRebootTimeElapsed() {
  LOG(ERROR) << "Unable to reboot - asking user for a manual reboot.";
  MakeSureScreenIsShown();
  GetContextEditor().SetBoolean(kContextKeyUpdateCompleted, true);
}

void UpdateScreen::MakeSureScreenIsShown() {
  if (!is_shown_)
    get_base_screen_delegate()->ShowCurrentScreen();
}

void UpdateScreen::SetHostPairingControllerStatus(
    HostPairingController::UpdateStatus update_status) {
  if (!remora_controller_)
    return;

  static bool is_update_in_progress = true;

  if (update_status > HostPairingController::UPDATE_STATUS_UPDATING) {
    // Set |is_update_in_progress| to false to prevent sending the scheduled
    // UPDATE_STATUS_UPDATING message after UPDATE_STATUS_UPDATED or
    // UPDATE_STATUS_REBOOTING is received.
    is_update_in_progress = false;
    remora_controller_->OnUpdateStatusChanged(update_status);
    return;
  }

  if (is_update_in_progress) {
    DCHECK_EQ(update_status, HostPairingController::UPDATE_STATUS_UPDATING);
    remora_controller_->OnUpdateStatusChanged(update_status);

    // Send UPDATE_STATUS_UPDATING message every |kHostStatusReportDelay|ms.
    base::SequencedTaskRunnerHandle::Get()->PostNonNestableDelayedTask(
        FROM_HERE,
        base::BindOnce(&UpdateScreen::SetHostPairingControllerStatus,
                       weak_factory_.GetWeakPtr(), update_status),
        base::TimeDelta::FromMilliseconds(kHostStatusReportDelay));
  }
}

ErrorScreen* UpdateScreen::GetErrorScreen() {
  return get_base_screen_delegate()->GetErrorScreen();
}

void UpdateScreen::StartUpdateCheck() {
  error_message_timer_.Stop();
  GetErrorScreen()->HideCaptivePortal();

  network_portal_detector::GetInstance()->RemoveObserver(this);
  connect_request_subscription_.reset();
  if (state_ == State::STATE_ERROR)
    HideErrorMessage();

  pending_update_version_ = std::string();
  pending_update_size_ = 0;

  state_ = State::STATE_UPDATE;
  DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);
  VLOG(1) << "Initiate update check";
  DBusThreadManager::Get()->GetUpdateEngineClient()->RequestUpdateCheck(
      base::Bind(StartUpdateCallback, this));
}

void UpdateScreen::ShowErrorMessage() {
  LOG(WARNING) << "UpdateScreen::ShowErrorMessage()";

  error_message_timer_.Stop();

  state_ = State::STATE_ERROR;
  connect_request_subscription_ =
      GetErrorScreen()->RegisterConnectRequestCallback(base::Bind(
          &UpdateScreen::OnConnectRequested, base::Unretained(this)));
  GetErrorScreen()->SetUIState(NetworkError::UI_STATE_UPDATE);
  get_base_screen_delegate()->ShowErrorScreen();
  histogram_helper_->OnErrorShow(GetErrorScreen()->GetErrorState());
}

void UpdateScreen::HideErrorMessage() {
  LOG(WARNING) << "UpdateScreen::HideErrorMessage()";
  get_base_screen_delegate()->HideErrorScreen(this);
  histogram_helper_->OnErrorHide();
}

void UpdateScreen::UpdateErrorMessage(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalStatus status) {
  switch (status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      NOTREACHED();
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      GetErrorScreen()->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                      std::string());
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      DCHECK(network);
      GetErrorScreen()->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                                      network->name());
      if (is_first_portal_notification_) {
        is_first_portal_notification_ = false;
        GetErrorScreen()->FixCaptivePortal();
      }
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      GetErrorScreen()->SetErrorState(NetworkError::ERROR_STATE_PROXY,
                                      std::string());
      break;
    default:
      NOTREACHED();
      break;
  }
}

void UpdateScreen::DelayErrorMessage() {
  if (error_message_timer_.IsRunning())
    return;

  state_ = State::STATE_ERROR;
  error_message_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kDelayErrorMessageSec), this,
      &UpdateScreen::ShowErrorMessage);
}

void UpdateScreen::OnConnectRequested() {
  if (state_ == State::STATE_ERROR) {
    LOG(WARNING) << "Hiding error message since AP was reselected";
    StartUpdateCheck();
  }
}

}  // namespace chromeos
