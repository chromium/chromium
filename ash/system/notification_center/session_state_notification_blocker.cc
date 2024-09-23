// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/session_state_notification_blocker.h"

#include "ash/public/cpp/message_center/oobe_notification_constants.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/do_not_disturb_notification_controller.h"
#include "ash/system/lock_screen_notification_controller.h"
#include "ash/system/power/battery_notification.h"
#include "base/containers/contains.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller_delegate_impl.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using session_manager::SessionState;

namespace ash {

namespace {

constexpr base::TimeDelta kLoginNotificationDelay = base::Seconds(6);

// Set to false for tests so notifications can be generated without a delay.
bool g_use_login_delay_for_test = true;

bool CalculateShouldShowNotification() {
  SessionControllerImpl* const session_controller =
      Shell::Get()->session_controller();

  SessionState state = session_controller->GetSessionState();
  static const SessionState kNotificationBlockedStates[] = {
      SessionState::OOBE, SessionState::LOGIN_PRIMARY,
      SessionState::LOGIN_SECONDARY, SessionState::LOGGED_IN_NOT_ACTIVE};

  // Do not show notifications in kiosk mode or before session starts.
  if (session_controller->IsRunningInAppMode() ||
      base::Contains(kNotificationBlockedStates, state)) {
    return false;
  }

  // Do not show notifications on the lockscreen.
  if (state == SessionState::LOCKED) {
    return false;
  }

  return true;
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

bool IsAllowedDuringOOBE(std::string_view notification_id) {
  static const std::string_view kAllowedSystemNotificationIDs[] = {
      BatteryNotification::kNotificationId,
      policy::DeviceRestrictionScheduleControllerDelegateImpl::
          kPostLogoutNotificationId};
  static const std::string_view kAllowedProfileBoundNotificationIDs[] = {
      kOOBELocaleSwitchNotificationId, kOOBEGnubbyNotificationId};

  if (base::Contains(kAllowedSystemNotificationIDs, notification_id)) {
    return true;
  }

  // Check here not for a full name equivalence, but for a substring existence
  // because profile-bound notifications have a profile-specific prefix added
  // to them.
  for (const auto& id : kAllowedProfileBoundNotificationIDs) {
    if (base::Contains(notification_id, id)) {
      return true;
    }
  }

  return false;
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

// static
void SessionStateNotificationBlocker::SetUseLoginNotificationDelayForTest(
    bool use_delay) {
  g_use_login_delay_for_test = use_delay;
}

void SessionStateNotificationBlocker::OnLoginTimerEnded() {
  NotifyBlockingStateChanged();
}

bool SessionStateNotificationBlocker::ShouldShowNotification(
    const message_center::Notification& notification) const {
  // Do not show non system notifications for `kLoginNotificationsDelay`
  // duration.
  if (notification.notifier_id().type !=
          message_center::NotifierType::SYSTEM_COMPONENT &&
      login_delay_timer_.IsRunning()) {
    return false;
  }

  // Never show notifications in kiosk mode.
  if (Shell::Get()->session_controller()->IsRunningInAppMode()) {
    return false;
  }

  const SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  // Do not show the "Do not disturb" notification if there is no active
  // session.
  if (notification.id() ==
          DoNotDisturbNotificationController::kDoNotDisturbNotificationId &&
      session_state != SessionState::ACTIVE) {
    return false;
  }

  // Allow the lock screen notification to show on and only on the lock screen.
  if (notification.id() ==
      LockScreenNotificationController::kLockScreenNotificationId) {
    return session_state == SessionState::LOCKED;
  }

  // Only allow System priority notifications to be shown on the lock screen. We
  // need to provide an exception here since by default we're blocking all
  // notifications when the screen is locked.
  if (notification.priority() ==
          message_center::NotificationPriority::SYSTEM_PRIORITY &&
      session_state == SessionState::LOCKED) {
    return true;
  }

  if (IsAllowedDuringOOBE(notification.id())) {
    return true;
  }

  return should_show_notification_;
}

bool SessionStateNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  SessionControllerImpl* const session_controller =
      Shell::Get()->session_controller();

  // Never show notifications in kiosk mode.
  if (session_controller->IsRunningInAppMode()) {
    return false;
  }

  // Do not show non system notifications for `kLoginNotificationsDelay`
  // duration.
  if (notification.notifier_id().type !=
          message_center::NotifierType::SYSTEM_COMPONENT &&
      login_delay_timer_.IsRunning()) {
    return false;
  }

  if (IsAllowedDuringOOBE(notification.id())) {
    return true;
  }

  return should_show_popup_;
}

void SessionStateNotificationBlocker::OnFirstSessionStarted() {
  if (!g_use_login_delay_for_test) {
    return;
  }
  login_delay_timer_.Start(FROM_HERE, kLoginNotificationDelay, this,
                           &SessionStateNotificationBlocker::OnLoginTimerEnded);
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
