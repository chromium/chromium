// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/session_state_notification_blocker.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ui/message_center/message_center.h"

using session_manager::SessionState;

namespace ash {

namespace {

bool CalculateShouldShowNotification() {
  SessionControllerImpl* const session_controller =
      Shell::Get()->session_controller();

  return !session_controller->IsRunningInAppMode();
}

bool CalculateShouldShowPopup() {
  SessionControllerImpl* const session_controller =
      Shell::Get()->session_controller();

  if (session_controller->IsRunningInAppMode() ||
      session_controller->GetSessionState() != SessionState::ACTIVE) {
    return false;
  }

  const UserSession* active_user_session =
      session_controller->GetUserSession(0);
  return active_user_session && session_controller->GetUserPrefServiceForUser(
                                    active_user_session->user_info.account_id);
}

}  // namespace

SessionStateNotificationBlocker::SessionStateNotificationBlocker(
    message_center::MessageCenter* message_center)
    : NotificationBlocker(message_center),
      should_show_notification_(CalculateShouldShowNotification()),
      should_show_popup_(CalculateShouldShowPopup()) {
  Shell::Get()->session_controller()->AddObserver(this);
}

SessionStateNotificationBlocker::~SessionStateNotificationBlocker() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

bool SessionStateNotificationBlocker::ShouldShowNotification(
    const message_center::Notification& notification) const {
  return should_show_notification_;
}

bool SessionStateNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  SessionControllerImpl* const session_controller =
      Shell::Get()->session_controller();

  // Never show notifications in kiosk mode.
  if (session_controller->IsRunningInAppMode())
    return false;

  if (notification.notifier_id().profile_id.empty() &&
      notification.priority() >= message_center::SYSTEM_PRIORITY) {
    return true;
  }

  return should_show_popup_;
}

void SessionStateNotificationBlocker::OnSessionStateChanged(
    SessionState state) {
  CheckStateAndNotifyIfChanged();
}

void SessionStateNotificationBlocker::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  CheckStateAndNotifyIfChanged();
}

void SessionStateNotificationBlocker::CheckStateAndNotifyIfChanged() {
  const bool new_should_show_notification = CalculateShouldShowNotification();
  const bool new_should_show_popup = CalculateShouldShowPopup();
  if (new_should_show_notification == should_show_notification_ &&
      new_should_show_popup == should_show_popup_) {
    return;
  }

  should_show_notification_ = new_should_show_notification;
  should_show_popup_ = new_should_show_popup;
  NotifyBlockingStateChanged();
}

}  // namespace ash
