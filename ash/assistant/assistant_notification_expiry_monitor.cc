// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_notification_expiry_monitor.h"

#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/model/assistant_notification_model.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "base/bind.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace ash {

namespace {

bool HasExpired(const AssistantNotificationExpiryMonitor::AssistantNotification*
                    notification) {
  return notification->expiry_time.has_value() &&
         (notification->expiry_time.value() <= base::Time::Now());
}

// Returns the minimum of the base::Time instances that actually have a value.
base::Optional<base::Time> Min(base::Optional<base::Time> left,
                               base::Optional<base::Time> right) {
  if (!left.has_value())
    return right;

  if (!right.has_value())
    return left;

  return std::min(left.value(), right.value());
}

}  // namespace

class AssistantNotificationExpiryMonitor::Observer
    : public AssistantNotificationModelObserver {
 public:
  Observer(AssistantNotificationExpiryMonitor* monitor) : monitor_(monitor) {}
  ~Observer() override = default;

  void OnNotificationAdded(const AssistantNotification* notification) override {
    monitor_->UpdateTimer();
  }

  void OnNotificationUpdated(
      const AssistantNotification* notification) override {
    monitor_->UpdateTimer();
  }

  void OnNotificationRemoved(const AssistantNotification* notification,
                             bool from_server) override {
    monitor_->UpdateTimer();
  }

  void OnAllNotificationsRemoved(bool from_server) override {
    monitor_->UpdateTimer();
  }

 private:
  AssistantNotificationExpiryMonitor* const monitor_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

AssistantNotificationExpiryMonitor::AssistantNotificationExpiryMonitor(
    AssistantNotificationController* controller)
    : controller_(controller), observer_(std::make_unique<Observer>(this)) {
  DCHECK(controller_);
  controller_->AddModelObserver(observer_.get());
}

AssistantNotificationExpiryMonitor::~AssistantNotificationExpiryMonitor() =
    default;

void AssistantNotificationExpiryMonitor::UpdateTimer() {
  base::Optional<base::TimeDelta> timeout = GetTimerTimeout();
  if (timeout) {
    timer_.Start(
        FROM_HERE, timeout.value(),
        base::BindOnce(
            &AssistantNotificationExpiryMonitor::RemoveExpiredNotifications,
            base::Unretained(this)));
  } else {
    timer_.Stop();
  }
}

base::Optional<base::TimeDelta>
AssistantNotificationExpiryMonitor::GetTimerTimeout() const {
  base::Optional<base::Time> endtime = GetTimerEndTime();
  if (endtime)
    return endtime.value() - base::Time::Now();
  return base::nullopt;
}

base::Optional<base::Time> AssistantNotificationExpiryMonitor::GetTimerEndTime()
    const {
  base::Optional<base::Time> result = base::nullopt;
  for (const AssistantNotification* notification : GetNotifications())
    result = Min(result, notification->expiry_time);
  return result;
}

void AssistantNotificationExpiryMonitor::RemoveExpiredNotifications() {
  for (const NotificationId& id : GetExpiredNotifications()) {
    VLOG(1) << "Removing expired notification '" << id << "'";
    controller_->RemoveNotificationById(id, /*from_server=*/false);
  }

  UpdateTimer();
}

std::vector<AssistantNotificationExpiryMonitor::NotificationId>
AssistantNotificationExpiryMonitor::GetExpiredNotifications() const {
  std::vector<NotificationId> result;
  for (const AssistantNotification* notification : GetNotifications()) {
    if (HasExpired(notification))
      result.push_back(notification->client_id);
  }
  return result;
}

std::vector<const AssistantNotificationExpiryMonitor::AssistantNotification*>
AssistantNotificationExpiryMonitor::GetNotifications() const {
  return controller_->model()->GetNotifications();
}

}  // namespace ash
