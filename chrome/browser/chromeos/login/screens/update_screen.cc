// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_screen.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chromeos/network/network_state.h"

namespace chromeos {

namespace {

constexpr const char kUserActionAcceptUpdateOverCellular[] =
    "update-accept-cellular";
constexpr const char kUserActionRejectUpdateOverCellular[] =
    "update-reject-cellular";

constexpr const char kUserActionCancelUpdateShortcut[] = "cancel-update";

const char kUpdateDeadlineFile[] = "/tmp/update-check-response-deadline";

// Delay before showing error message if captive portal is detected.
// We wait for this delay to let captive portal to perform redirect and show
// its login page before error message appears.
constexpr const base::TimeDelta kDelayErrorMessage =
    base::TimeDelta::FromSeconds(10);

constexpr const base::TimeDelta kShowDelay =
    base::TimeDelta::FromMicroseconds(400);

}  // anonymous namespace

// static
UpdateScreen* UpdateScreen::Get(ScreenManager* manager) {
  return static_cast<UpdateScreen*>(manager->GetScreen(UpdateView::kScreenId));
}

UpdateScreen::UpdateScreen(UpdateView* view,
                           ErrorScreen* error_screen,
                           const ScreenExitCallback& exit_callback)
    : BaseScreen(UpdateView::kScreenId),
      view_(view),
      error_screen_(error_screen),
      exit_callback_(exit_callback),
      histogram_helper_(
          std::make_unique<ErrorScreensHistogramHelper>("Update")),
      version_updater_(std::make_unique<VersionUpdater>(this)) {
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

void UpdateScreen::Show() {
#if !defined(OFFICIAL_BUILD)
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

void UpdateScreen::Hide() {
  show_timer_.Stop();
  if (view_)
    view_->Hide();
  is_shown_ = false;
}

void UpdateScreen::OnUserAction(const std::string& action_id) {
  bool is_official_build = false;
#if defined(OFFICIAL_BUILD)
  is_official_build = true;
#endif

  if (!is_official_build && action_id == kUserActionCancelUpdateShortcut) {
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
  if (view_)
    view_->SetUpdateCompleted(true);
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
  error_screen_->SetHideCallback(base::BindRepeating(
      &UpdateScreen::OnErrorScreenHidden, weak_factory_.GetWeakPtr()));
  error_screen_->Show();
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
      // Do nothing in these cases, we don't want to notify the user of the
      // check unless there is an update.
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::IDLE:
      need_refresh_view = false;
      break;
    case update_engine::Operation::UPDATE_AVAILABLE:
    case update_engine::Operation::DOWNLOADING:
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
          VLOG(1) << "Critical update available: " << status.new_version();
        }
      }
      break;
    case update_engine::Operation::VERIFYING:
    case update_engine::Operation::FINALIZING:
    case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
      MakeSureScreenIsShown();
      break;
    case update_engine::Operation::UPDATED_NEED_REBOOT:
      MakeSureScreenIsShown();
      if (HasCriticalUpdate()) {
        version_updater_->RebootAfterUpdate();
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
  if (need_refresh_view)
    RefreshView(update_info);
}

void UpdateScreen::FinishExitUpdate(Result result) {
  show_timer_.Stop();
  exit_callback_.Run(result);
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
  Show();
}

}  // namespace chromeos
