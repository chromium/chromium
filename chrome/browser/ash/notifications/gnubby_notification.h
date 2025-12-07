// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_GNUBBY_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_GNUBBY_NOTIFICATION_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/gnubby/gnubby_client.h"

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

  void ShowNotification();
  void DismissNotification();

 private:
  std::unique_ptr<base::OneShotTimer> update_dismiss_notification_timer_;
  bool notification_active_ = false;

  base::WeakPtrFactory<GnubbyNotification> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_GNUBBY_NOTIFICATION_H_
