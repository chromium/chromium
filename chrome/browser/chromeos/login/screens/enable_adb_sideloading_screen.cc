// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/enable_adb_sideloading_screen.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

constexpr const char kUserActionCancelPressed[] = "cancel-pressed";
constexpr const char kUserActionEnablePressed[] = "enable-pressed";
constexpr const char kUserActionLearnMorePressed[] = "learn-more-link";

// These values are used for metrics and should not change.
enum class AdbSideloadingPromptEvent {
  kPromptShown = 0,
  kSkipped = 1,
  kCanceled = 2,
  kEnabled = 3,
  kFailedToDisplay = 4,
  kFailedToEnable = 5,
  kFailedToDisplay_NeedPowerwash = 6,
  kMaxValue = kFailedToDisplay_NeedPowerwash,
};

void LogEvent(AdbSideloadingPromptEvent action) {
  base::UmaHistogramEnumeration("Arc.AdbSideloadingEnablingScreen", action);
}

}  // namespace

EnableAdbSideloadingScreen::EnableAdbSideloadingScreen(
    EnableAdbSideloadingScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(EnableAdbSideloadingScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

EnableAdbSideloadingScreen::~EnableAdbSideloadingScreen() {
  DCHECK(view_);
  view_->Unbind();
}

// static
void EnableAdbSideloadingScreen::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kEnableAdbSideloadingRequested, false);
}

void EnableAdbSideloadingScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionCancelPressed) {
    OnCancel();
  } else if (action_id == kUserActionEnablePressed) {
    OnEnable();
  } else if (action_id == kUserActionLearnMorePressed) {
    OnLearnMore();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void EnableAdbSideloadingScreen::Show() {
  chromeos::SessionManagerClient* client =
      chromeos::SessionManagerClient::Get();
  client->QueryAdbSideload(
      base::Bind(&EnableAdbSideloadingScreen::OnQueryAdbSideload,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnableAdbSideloadingScreen::OnQueryAdbSideload(
    SessionManagerClient::AdbSideloadResponseCode response_code,
    bool enabled) {
  DVLOG(1) << "EnableAdbSideloadingScreen::OnQueryAdbSideload"
           << ", response_code=" << static_cast<int>(response_code)
           << ", enabled=" << enabled;

  // Clear prefs so that the screen won't be triggered again.
  PrefService* prefs = g_browser_process->local_state();
  prefs->ClearPref(prefs::kEnableAdbSideloadingRequested);
  prefs->CommitPendingWrite();

  if (enabled) {
    DCHECK_EQ(response_code,
              SessionManagerClient::AdbSideloadResponseCode::SUCCESS);
    LogEvent(AdbSideloadingPromptEvent::kSkipped);
    exit_callback_.Run();
    return;
  }

  DCHECK(view_);
  EnableAdbSideloadingScreenView::UIState ui_state;
  switch (response_code) {
    case SessionManagerClient::AdbSideloadResponseCode::SUCCESS:
      LogEvent(AdbSideloadingPromptEvent::kPromptShown);
      ui_state = EnableAdbSideloadingScreenView::UIState::UI_STATE_SETUP;
      break;
    case SessionManagerClient::AdbSideloadResponseCode::NEED_POWERWASH:
      LogEvent(AdbSideloadingPromptEvent::kFailedToDisplay_NeedPowerwash);
      ui_state = EnableAdbSideloadingScreenView::UIState::UI_STATE_ERROR;
      break;
    case SessionManagerClient::AdbSideloadResponseCode::FAILED:
      LogEvent(AdbSideloadingPromptEvent::kFailedToDisplay);
      ui_state = EnableAdbSideloadingScreenView::UIState::UI_STATE_ERROR;
      break;
  }
  view_->SetScreenState(ui_state);
  view_->Show();
}

void EnableAdbSideloadingScreen::Hide() {
  DCHECK(view_);
  view_->Hide();
}

void EnableAdbSideloadingScreen::OnCancel() {
  LogEvent(AdbSideloadingPromptEvent::kCanceled);
  exit_callback_.Run();
}

void EnableAdbSideloadingScreen::OnEnable() {
  chromeos::SessionManagerClient* client =
      chromeos::SessionManagerClient::Get();
  client->EnableAdbSideload(
      base::Bind(&EnableAdbSideloadingScreen::OnEnableAdbSideload,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnableAdbSideloadingScreen::OnEnableAdbSideload(
    SessionManagerClient::AdbSideloadResponseCode response_code) {
  switch (response_code) {
    case SessionManagerClient::AdbSideloadResponseCode::SUCCESS:
      LogEvent(AdbSideloadingPromptEvent::kEnabled);
      LoginDisplayHost::default_host()->RequestSystemInfoUpdate();
      exit_callback_.Run();
      break;
    case SessionManagerClient::AdbSideloadResponseCode::NEED_POWERWASH:
    case SessionManagerClient::AdbSideloadResponseCode::FAILED:
      LogEvent(AdbSideloadingPromptEvent::kFailedToEnable);
      DCHECK(view_);
      view_->SetScreenState(
          EnableAdbSideloadingScreenView::UIState::UI_STATE_ERROR);
      break;
  }
}

void EnableAdbSideloadingScreen::OnLearnMore() {
  // TODO(victorhsieh): replace the help center link
  HelpAppLauncher::HelpTopic topic = HelpAppLauncher::HELP_POWERWASH;
  VLOG(1) << "Trying to view help article " << topic;
  if (!help_app_.get()) {
    help_app_ = new HelpAppLauncher(
        LoginDisplayHost::default_host()->GetNativeWindow());
  }
  help_app_->ShowHelpTopic(topic);
}

void EnableAdbSideloadingScreen::OnViewDestroyed(
    EnableAdbSideloadingScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

}  // namespace chromeos
