// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/remote_activity_notification_controller.h"

#include <memory>

#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
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

// Buttons (like the one on the notification) do not correctly respond to
// on-click events if they are created before the login screen is properly
// initialized, so only create the notification once that's the case.
void RemoteActivityNotificationController::OnLoginOrLockScreenVisible() {
  if (remote_admin_was_present_.GetValue()) {
    notification_.Show();
  }
}

void RemoteActivityNotificationController::OnClientConnecting() {
  // The notification is meant for a local user not for the remote admin, so
  // hide it during a CRD connection so we don't annoy them. This also prevents
  // the notification from showing when the remote admin auto-reconnects after
  // a Chrome crash/restart.
  notification_.Hide();
}

void RemoteActivityNotificationController::OnClientConnected() {
  if (is_current_session_curtained_.Run()) {
    remote_admin_was_present_.SetValue(true);
  }
}

void RemoteActivityNotificationController::OnClientDisconnected() {
  // Show the notification once the remote admin disconnects.
  if (remote_admin_was_present_.GetValue()) {
    notification_.Show();
  }
}

void RemoteActivityNotificationController::
    OnRemoteAdminWasPresentPrefChanged() {
  if (!remote_admin_was_present_.GetValue()) {
    // Remember the notification was dismissed by the user.
    notification_.SetAsHidden();
  }

  if (is_current_session_curtained_.Run()) {
    // When the notification is dismissed from inside a curtained session
    // we must ensure the notification is shown again the next time.
    remote_admin_was_present_.SetValue(true);
  }
}

void RemoteActivityNotificationController::Notification::Show() {
  if (!is_showing_) {
    auto* default_host = ash::LoginDisplayHost::default_host();
    if (default_host) {
      default_host->ShowRemoteActivityNotificationScreen();
      is_showing_ = true;
    }
  }
}

void RemoteActivityNotificationController::Notification::Hide() {
  if (is_showing_) {
    auto* default_host = ash::LoginDisplayHost::default_host();
    if (default_host) {
      default_host->HideOobeDialog();
      is_showing_ = false;
    }
  }
}

void RemoteActivityNotificationController::Notification::SetAsHidden() {
  is_showing_ = false;
}

}  // namespace policy
