// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_HPS_NOTIFY_NOTIFICATION_BLOCKER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_HPS_NOTIFY_NOTIFICATION_BLOCKER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/unified/hps_notify_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/message_center/notification_blocker.h"

class PrefService;
class PrefChangeRegistrar;

namespace message_center {
class MessageCenter;
class Notification;
}  // namespace message_center

namespace ash {

// A notification blocker that suppresses popup notifications when the HPS
// service detects a person looking over the user's shoulder.
//
// TODO(crbug.com/1241706): make this naming less opaque. Currently using "HPS
// notify" because it was the feature name early in development, but paths /
// identifiers will be renamed in one fell swoop.
class ASH_EXPORT HpsNotifyNotificationBlocker
    : public SessionObserver,
      public message_center::NotificationBlocker,
      public HpsNotifyController::Observer {
 public:
  HpsNotifyNotificationBlocker(message_center::MessageCenter* message_center,
                               HpsNotifyController* controller);

  HpsNotifyNotificationBlocker(const HpsNotifyNotificationBlocker&) = delete;
  HpsNotifyNotificationBlocker& operator=(const HpsNotifyNotificationBlocker&) =
      delete;

  ~HpsNotifyNotificationBlocker() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // message_center::NotificationBlocker:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

  // HpsNotifyController::Observer:
  void OnSnoopingStatusChanged(bool snooper) override;

 private:
  HpsNotifyController* const controller_;

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};
  base::ScopedObservation<HpsNotifyController, HpsNotifyController::Observer>
      controller_observer_{this};

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Must be last.
  base::WeakPtrFactory<HpsNotifyNotificationBlocker> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_HPS_NOTIFY_NOTIFICATION_BLOCKER_H_
