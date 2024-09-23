// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/enable_debugging_screen.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_web_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {
namespace {

constexpr char kUserActionCancel[] = "cancel";
constexpr char kUserActionDone[] = "done";
constexpr char kUserActionSetup[] = "setup";
constexpr char kUserActionLearnMore[] = "learnMore";
constexpr char kUserActionRemoveRootFSProtection[] = "removeRootFSProtection";

}  // namespace

EnableDebuggingScreen::EnableDebuggingScreen(
    base::WeakPtr<EnableDebuggingScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(EnableDebuggingScreenView::kScreenId,
                 OobeScreenPriority::SCREEN_DEVICE_DEVELOPER_MODIFICATION),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

EnableDebuggingScreen::~EnableDebuggingScreen() = default;

void EnableDebuggingScreen::ShowImpl() {
  if (view_) {
    view_->Show();
    WaitForCryptohome();
  }
}

void EnableDebuggingScreen::HideImpl() {}

void EnableDebuggingScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCancel || action_id == kUserActionDone) {
    exit_callback_.Run();
    return;
  }
  if (action_id == kUserActionLearnMore) {
    HandleLearnMore();
    return;
  }
  if (action_id == kUserActionRemoveRootFSProtection) {
    HandleRemoveRootFSProtection();
    return;
  }
  if (action_id == kUserActionSetup) {
    CHECK_EQ(args.size(), 2u);
    const std::string& password = args[1].GetString();
    HandleSetup(password);
    return;
  }
  BaseScreen::OnUserAction(args);
}

void EnableDebuggingScreen::HandleLearnMore() {
  VLOG(1) << "Trying to view the help article about debugging features.";
  const std::string help_content =
      l10n_util::GetStringUTF8(IDS_ENABLE_DEBUGGING_HELP);
  const GURL data_url = GURL("data:text/html;charset=utf-8," + help_content);

  LoginWebDialog* dialog = new LoginWebDialog(
      Profile::FromWebUI(
          LoginDisplayHost::default_host()->GetOobeUI()->web_ui()),
      LoginDisplayHost::default_host()->GetNativeWindow(), std::u16string(),
      data_url);
  dialog->Show();
}

void EnableDebuggingScreen::HandleRemoveRootFSProtection() {
  UpdateUIState(EnableDebuggingScreenView::kUIStateWait);
  DebugDaemonClient::Get()->RemoveRootfsVerification(
      base::BindOnce(&EnableDebuggingScreen::OnRemoveRootfsVerification,
                     weak_ptr_factory_.GetWeakPtr()));
}

// Removes rootfs verification, add flag to start with enable debugging features
// screen and reboots the machine.
void EnableDebuggingScreen::OnRemoveRootfsVerification(bool success) {
  if (!success) {
    UpdateUIState(EnableDebuggingScreenView::kUIStateError);
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
  UpdateUIState(EnableDebuggingScreenView::kUIStateWait);
  UserDataAuthClient* client = UserDataAuthClient::Get();
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
    UpdateUIState(EnableDebuggingScreenView::kUIStateError);
    return;
  }

  DebugDaemonClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &EnableDebuggingScreen::OnDebugDaemonServiceAvailabilityChecked,
      weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreen::OnDebugDaemonServiceAvailabilityChecked(
    bool service_is_available) {
  DVLOG(1) << "Enable-debugging-screen: debugd availability="
           << service_is_available;
  if (!service_is_available) {
    LOG(ERROR) << "Debug daemon is not available.";
    UpdateUIState(EnableDebuggingScreenView::kUIStateError);
    return;
  }

  // Check the status of debugging features.
  DebugDaemonClient::Get()->QueryDebuggingFeatures(
      base::BindOnce(&EnableDebuggingScreen::OnQueryDebuggingFeatures,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreen::OnQueryDebuggingFeatures(bool success,
                                                     int features_flag) {
  DVLOG(1) << "Enable-debugging-screen: OnQueryDebuggingFeatures"
           << ", success=" << success << ", features=" << features_flag;
  if (!success ||
      features_flag == debugd::DevFeatureFlag::DEV_FEATURES_DISABLED) {
    UpdateUIState(EnableDebuggingScreenView::kUIStateError);
    return;
  }

  if ((features_flag &
       debugd::DevFeatureFlag::DEV_FEATURE_ROOTFS_VERIFICATION_REMOVED) == 0) {
    UpdateUIState(EnableDebuggingScreenView::kUIStateRemoveProtection);
    return;
  }

  if ((features_flag & DebugDaemonClient::DEV_FEATURE_ALL_ENABLED) !=
      DebugDaemonClient::DEV_FEATURE_ALL_ENABLED) {
    UpdateUIState(EnableDebuggingScreenView::kUIStateSetup);
  } else {
    UpdateUIState(EnableDebuggingScreenView::kUIStateDone);
  }
}

void EnableDebuggingScreen::HandleSetup(const std::string& password) {
  UpdateUIState(EnableDebuggingScreenView::kUIStateWait);
  DebugDaemonClient::Get()->EnableDebuggingFeatures(
      password,
      base::BindOnce(&EnableDebuggingScreen::OnEnableDebuggingFeatures,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreen::OnEnableDebuggingFeatures(bool success) {
  if (!success) {
    UpdateUIState(EnableDebuggingScreenView::kUIStateError);
    return;
  }

  UpdateUIState(EnableDebuggingScreenView::kUIStateDone);
}

void EnableDebuggingScreen::UpdateUIState(
    EnableDebuggingScreenView::UIState state) {
  if (state == EnableDebuggingScreenView::kUIStateSetup ||
      state == EnableDebuggingScreenView::kUIStateError ||
      state == EnableDebuggingScreenView::kUIStateDone) {
    PrefService* prefs = g_browser_process->local_state();
    prefs->ClearPref(prefs::kDebuggingFeaturesRequested);
    prefs->CommitPendingWrite();
  }
  if (view_)
    view_->UpdateUIState(state);
}

}  // namespace ash
