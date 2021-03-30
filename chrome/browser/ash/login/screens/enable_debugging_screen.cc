// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/enable_debugging_screen.h"

#include "base/check.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/login_web_dialog.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kUserActionCancel[] = "cancel";
constexpr char kUserActionDone[] = "done";
constexpr char kUserActionLearnMore[] = "learnMore";
constexpr char kUserActionRemoveRootFSProtection[] = "removeRootFSProtection";
}  // namespace

namespace chromeos {

EnableDebuggingScreen::EnableDebuggingScreen(
    EnableDebuggingScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(EnableDebuggingScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DEVELOPER_MODIFICATION),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

EnableDebuggingScreen::~EnableDebuggingScreen() {
  if (view_)
    view_->SetDelegate(nullptr);
}

void EnableDebuggingScreen::OnViewDestroyed(EnableDebuggingScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void EnableDebuggingScreen::ShowImpl() {
  if (view_) {
    view_->Show();
    WaitForCryptohome();
  }
}

void EnableDebuggingScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void EnableDebuggingScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionCancel || action_id == kUserActionDone) {
    exit_callback_.Run();
  } else if (action_id == kUserActionLearnMore) {
    HandleLearnMore();
  } else if (action_id == kUserActionRemoveRootFSProtection) {
    HandleRemoveRootFSProtection();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void EnableDebuggingScreen::HandleLearnMore() {
  VLOG(1) << "Trying to view the help article about debugging features.";
  const std::string help_content =
      l10n_util::GetStringUTF8(IDS_ENABLE_DEBUGGING_HELP);
  const GURL data_url = GURL("data:text/html;charset=utf-8," + help_content);

  LoginWebDialog* dialog = new LoginWebDialog(
      Profile::FromWebUI(
          LoginDisplayHost::default_host()->GetOobeUI()->web_ui()),
      nullptr, LoginDisplayHost::default_host()->GetNativeWindow(),
      std::u16string(), data_url);
  dialog->Show();
}

void EnableDebuggingScreen::HandleRemoveRootFSProtection() {
  UpdateUIState(EnableDebuggingScreenView::UI_STATE_WAIT);
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->RemoveRootfsVerification(
      base::BindOnce(&EnableDebuggingScreen::OnRemoveRootfsVerification,
                     weak_ptr_factory_.GetWeakPtr()));
}

// Removes rootfs verification, add flag to start with enable debugging features
// screen and reboots the machine.
void EnableDebuggingScreen::OnRemoveRootfsVerification(bool success) {
  if (!success) {
    UpdateUIState(EnableDebuggingScreenView::UI_STATE_ERROR);
    return;
  }

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kDebuggingFeaturesRequested, true);
  prefs->CommitPendingWrite();
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER,
      "login debugging screen removing rootfs verification");
}

void EnableDebuggingScreen::WaitForCryptohome() {
  UpdateUIState(EnableDebuggingScreenView::UI_STATE_WAIT);
  chromeos::UserDataAuthClient* client = chromeos::UserDataAuthClient::Get();
  client->WaitForServiceToBeAvailable(base::BindOnce(
      &EnableDebuggingScreen::OnCryptohomeDaemonAvailabilityChecked,
      weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreen::OnCryptohomeDaemonAvailabilityChecked(
    bool service_is_available) {
  DVLOG(1) << "Enable-debugging-screen: cryptohomed availability="
           << service_is_available;
  if (!service_is_available) {
    LOG(ERROR) << "Crypthomed is not available.";
    UpdateUIState(EnableDebuggingScreenView::UI_STATE_ERROR);
    return;
  }

  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->WaitForServiceToBeAvailable(base::BindOnce(
      &EnableDebuggingScreen::OnDebugDaemonServiceAvailabilityChecked,
      weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreen::OnDebugDaemonServiceAvailabilityChecked(
    bool service_is_available) {
  DVLOG(1) << "Enable-debugging-screen: debugd availability="
           << service_is_available;
  if (!service_is_available) {
    LOG(ERROR) << "Debug daemon is not available.";
    UpdateUIState(EnableDebuggingScreenView::UI_STATE_ERROR);
    return;
  }

  // Check the status of debugging features.
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->QueryDebuggingFeatures(
      base::BindOnce(&EnableDebuggingScreen::OnQueryDebuggingFeatures,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreen::OnQueryDebuggingFeatures(bool success,
                                                     int features_flag) {
  DVLOG(1) << "Enable-debugging-screen: OnQueryDebuggingFeatures"
           << ", success=" << success << ", features=" << features_flag;
  if (!success ||
      features_flag == debugd::DevFeatureFlag::DEV_FEATURES_DISABLED) {
    UpdateUIState(EnableDebuggingScreenView::UI_STATE_ERROR);
    return;
  }

  if ((features_flag &
       debugd::DevFeatureFlag::DEV_FEATURE_ROOTFS_VERIFICATION_REMOVED) == 0) {
    UpdateUIState(EnableDebuggingScreenView::UI_STATE_REMOVE_PROTECTION);
    return;
  }

  if ((features_flag & DebugDaemonClient::DEV_FEATURE_ALL_ENABLED) !=
      DebugDaemonClient::DEV_FEATURE_ALL_ENABLED) {
    UpdateUIState(EnableDebuggingScreenView::UI_STATE_SETUP);
  } else {
    UpdateUIState(EnableDebuggingScreenView::UI_STATE_DONE);
  }
}

void EnableDebuggingScreen::HandleSetup(const std::string& password) {
  UpdateUIState(EnableDebuggingScreenView::UI_STATE_WAIT);
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->EnableDebuggingFeatures(
      password,
      base::BindOnce(&EnableDebuggingScreen::OnEnableDebuggingFeatures,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreen::OnEnableDebuggingFeatures(bool success) {
  if (!success) {
    UpdateUIState(EnableDebuggingScreenView::UI_STATE_ERROR);
    return;
  }

  UpdateUIState(EnableDebuggingScreenView::UI_STATE_DONE);
}

void EnableDebuggingScreen::UpdateUIState(
    EnableDebuggingScreenView::UIState state) {
  if (state == EnableDebuggingScreenView::UI_STATE_SETUP ||
      state == EnableDebuggingScreenView::UI_STATE_ERROR ||
      state == EnableDebuggingScreenView::UI_STATE_DONE) {
    PrefService* prefs = g_browser_process->local_state();
    prefs->ClearPref(prefs::kDebuggingFeaturesRequested);
    prefs->CommitPendingWrite();
  }
  view_->UpdateUIState(state);
}

}  // namespace chromeos
