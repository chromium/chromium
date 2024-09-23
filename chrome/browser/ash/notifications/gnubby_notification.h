// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_GNUBBY_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_GNUBBY_NOTIFICATION_H_

#include "ash/public/cpp/message_center/oobe_notification_constants.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chromeos/ash/components/dbus/gnubby/gnubby_client.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {
// GnubbyNotification manages the notification informing the user of gnubby
// U2FD Authentication.  It is responsible for both creating, showing, and
// closing the notification.

class GnubbyNotification : public GnubbyClient::Observer {
 public:
  GnubbyNotification();

  GnubbyNotification(const GnubbyNotification&) = delete;
  GnubbyNotification& operator=(const GnubbyNotification&) = delete;

  // Resets GnubbyClient NotificationHandler.
  ~GnubbyNotification() override;

  // Called when dbus client receives a U2FD Auth request
  // Displays graphic prompting user to press power button
  // Dismisses graphic after timeout
  void PromptUserAuth() override;

  void CreateNotification();
  void ShowNotification();
  void DismissNotification();

 private:
  std::unique_ptr<message_center::Notification> notification_prompt_;
  std::unique_ptr<base::OneShotTimer> update_dismiss_notification_timer_;
  bool notificationActive = false;

  base::WeakPtrFactory<GnubbyNotification> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_GNUBBY_NOTIFICATION_H_
