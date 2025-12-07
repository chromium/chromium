// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/enable_adb_sideloading_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {
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
    base::WeakPtr<EnableAdbSideloadingScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(EnableAdbSideloadingScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DEVELOPER_MODIFICATION),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

EnableAdbSideloadingScreen::~EnableAdbSideloadingScreen() = default;

// static
void EnableAdbSideloadingScreen::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kEnableAdbSideloadingRequested, false);
}

void EnableAdbSideloadingScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCancelPressed) {
    OnCancel();
  } else if (action_id == kUserActionEnablePressed) {
    OnEnable();
  } else if (action_id == kUserActionLearnMorePressed) {
    OnLearnMore();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void EnableAdbSideloadingScreen::ShowImpl() {
  SessionManagerClient* client = SessionManagerClient::Get();
  client->QueryAdbSideload(
      base::BindOnce(&EnableAdbSideloadingScreen::OnQueryAdbSideload,
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

  if (!view_)
    return;
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

void EnableAdbSideloadingScreen::HideImpl() {}

void EnableAdbSideloadingScreen::OnCancel() {
  LogEvent(AdbSideloadingPromptEvent::kCanceled);
  exit_callback_.Run();
}

void EnableAdbSideloadingScreen::OnEnable() {
  SessionManagerClient* client = SessionManagerClient::Get();
  client->EnableAdbSideload(
      base::BindOnce(&EnableAdbSideloadingScreen::OnEnableAdbSideload,
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
      if (view_) {
        view_->SetScreenState(
            EnableAdbSideloadingScreenView::UIState::UI_STATE_ERROR);
      }
      break;
  }
}

void EnableAdbSideloadingScreen::OnLearnMore() {
  HelpAppLauncher::HelpTopic topic = HelpAppLauncher::HELP_ADB_SIDELOADING;
  VLOG(1) << "Trying to view help article " << topic;
  if (!help_app_.get()) {
    help_app_ = new HelpAppLauncher(
        LoginDisplayHost::default_host()->GetNativeWindow());
  }
  help_app_->ShowHelpTopic(topic);
}

}  // namespace ash
