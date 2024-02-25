// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_SESSION_STATE_NOTIFICATION_BLOCKER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_SESSION_STATE_NOTIFICATION_BLOCKER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/timer/timer.h"
#include "ui/message_center/notification_blocker.h"

namespace ash {

// A notification blocker which suppresses notifications popups based on the
// session state and active user PrefService readiness reported by the
// SessionController. Only active (logged in, unlocked) sessions with
// initialized PrefService will show user notifications. Kiosk mode sessions
// will never show even system notifications. System notifications with
// elevated priority will be shown regardless of the login/lock state.
class ASH_EXPORT SessionStateNotificationBlocker
    : public message_center::NotificationBlocker,
      public SessionObserver {
 public:
  explicit SessionStateNotificationBlocker(
      message_center::MessageCenter* message_center);

  SessionStateNotificationBlocker(const SessionStateNotificationBlocker&) =
      delete;
  SessionStateNotificationBlocker& operator=(
      const SessionStateNotificationBlocker&) = delete;

  ~SessionStateNotificationBlocker() override;

  static void SetUseLoginNotificationDelayForTest(bool use_delay);

 private:
  void OnLoginTimerEnded();

  // message_center::NotificationBlocker overrides:
  bool ShouldShowNotification(
      const message_center::Notification& notification) const override;
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

  // SessionObserver overrides:
  void OnFirstSessionStarted() override;
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void CheckStateAndNotifyIfChanged();

  base::OneShotTimer login_delay_timer_;
  bool should_show_notification_ = false;
  bool should_show_popup_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_SESSION_STATE_NOTIFICATION_BLOCKER_H_
