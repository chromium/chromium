// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_NOTIFICATION_EXPIRY_MONITOR_H_
#define ASH_ASSISTANT_ASSISTANT_NOTIFICATION_EXPIRY_MONITOR_H_

#include <memory>
#include <vector>

#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "base/optional.h"
#include "base/timer/timer.h"

namespace ash {

class AssistantNotificationController;

// Will track all Assistant notifications by subscribing to the given
// |controller| and will call
// |AssistantNotificationController::RemoveNotificationById| when the
// notification expires (i.e. when the current time passes the value in the
// expiry_time| field).
class AssistantNotificationExpiryMonitor {
 public:
  using AssistantNotification =
      chromeos::assistant::mojom::AssistantNotification;

  explicit AssistantNotificationExpiryMonitor(
      AssistantNotificationController* controller);
  ~AssistantNotificationExpiryMonitor();

 private:
  using NotificationId = std::string;
  class Observer;

  // Start/stop the timer waiting for the next expiry time.
  // If the timer is already running this will start a new timer with the
  // (new) expiry time that will expire first.
  void UpdateTimer();

  base::Optional<base::TimeDelta> GetTimerTimeout() const;
  base::Optional<base::Time> GetTimerEndTime() const;
  void RemoveExpiredNotifications();
  std::vector<NotificationId> GetExpiredNotifications() const;
  std::vector<const AssistantNotification*> GetNotifications() const;

  base::OneShotTimer timer_;
  AssistantNotificationController* const controller_;
  std::unique_ptr<Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationExpiryMonitor);
};

}  // namespace ash
#endif  // ASH_ASSISTANT_ASSISTANT_NOTIFICATION_EXPIRY_MONITOR_H_
