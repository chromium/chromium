// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/remote_activity_notification_controller.h"

#include <memory>

#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"

namespace policy {

RemoteActivityNotificationController::RemoteActivityNotificationController(
    PrefService& local_state,
    base::RepeatingCallback<bool()> is_current_session_curtained)
    : is_current_session_curtained_(is_current_session_curtained) {
  observation_.Observe(session_manager::SessionManager::Get());

  remote_admin_was_present_.Init(
      prefs::kRemoteAdminWasPresent, &local_state,
      base::BindRepeating(&RemoteActivityNotificationController::
                              OnRemoteAdminWasPresentPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

RemoteActivityNotificationController::~RemoteActivityNotificationController() =
    default;

// We only can display UI elements (like the notification) after the login
// screen is properly initialized.
void RemoteActivityNotificationController::OnLoginOrLockScreenVisible() {
  Init();
}

void RemoteActivityNotificationController::OnClientConnected() {
  if (is_current_session_curtained_.Run()) {
    remote_admin_was_present_.SetValue(true);
  }
}

void RemoteActivityNotificationController::Init() {
  if (remote_admin_was_present_.GetValue()) {
    ShowNotification();
  }
}

void RemoteActivityNotificationController::ShowNotification() {
  CHECK_DEREF(ash::LoginDisplayHost::default_host())
      .ShowRemoteActivityNotificationScreen();
}

void RemoteActivityNotificationController::
    OnRemoteAdminWasPresentPrefChanged() {
  if (is_current_session_curtained_.Run() &&
      !remote_admin_was_present_.GetValue()) {
    // When the notification is dismissed from inside a curtained session
    // we must ensure the notification is shown again the next time.
    remote_admin_was_present_.SetValue(true);
  }
}

}  // namespace policy
