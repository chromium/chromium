// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_notification_expiry_monitor.h"

#include <algorithm>

#include "ash/assistant/assistant_notification_controller_impl.h"
#include "ash/assistant/model/assistant_notification_model.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"

namespace ash {

namespace {

bool HasExpired(const AssistantNotificationExpiryMonitor::AssistantNotification*
                    notification) {
  return notification->expiry_time.has_value() &&
         (notification->expiry_time.value() <= base::Time::Now());
}

// Returns the minimum of the base::Time instances that actually have a value.
std::optional<base::Time> Min(std::optional<base::Time> left,
                              std::optional<base::Time> right) {
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
  explicit Observer(AssistantNotificationExpiryMonitor* monitor)
      : monitor_(monitor) {}

  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;

  ~Observer() override = default;

  void OnNotificationAdded(const AssistantNotification& notification) override {
    monitor_->UpdateTimer();
  }

  void OnNotificationUpdated(
      const AssistantNotification& notification) override {
    monitor_->UpdateTimer();
  }

  void OnNotificationRemoved(const AssistantNotification& notification,
                             bool from_server) override {
    monitor_->UpdateTimer();
  }

  void OnAllNotificationsRemoved(bool from_server) override {
    monitor_->UpdateTimer();
  }

 private:
  const raw_ptr<AssistantNotificationExpiryMonitor> monitor_;
};

AssistantNotificationExpiryMonitor::AssistantNotificationExpiryMonitor(
    AssistantNotificationControllerImpl* controller)
    : controller_(controller), observer_(std::make_unique<Observer>(this)) {
  DCHECK(controller_);
  controller_->model()->AddObserver(observer_.get());
}

AssistantNotificationExpiryMonitor::~AssistantNotificationExpiryMonitor() =
    default;

void AssistantNotificationExpiryMonitor::UpdateTimer() {
  std::optional<base::TimeDelta> timeout = GetTimerTimeout();
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

std::optional<base::TimeDelta>
AssistantNotificationExpiryMonitor::GetTimerTimeout() const {
  std::optional<base::Time> endtime = GetTimerEndTime();
  if (endtime)
    return endtime.value() - base::Time::Now();
  return std::nullopt;
}

std::optional<base::Time> AssistantNotificationExpiryMonitor::GetTimerEndTime()
    const {
  std::optional<base::Time> result = std::nullopt;
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
