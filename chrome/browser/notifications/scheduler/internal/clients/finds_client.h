// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_CLIENTS_FINDS_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_CLIENTS_FINDS_CLIENT_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"

namespace notifications {

// The client used for Chrome Finds notifications.
class FindsClient : public NotificationSchedulerClient {
 public:
  FindsClient();
  FindsClient(const FindsClient&) = delete;
  FindsClient& operator=(const FindsClient&) = delete;
  ~FindsClient() override;

 private:
  // NotificationSchedulerClient implementation.
  void BeforeShowNotification(
      std::unique_ptr<NotificationData> notification_data,
      NotificationDataCallback callback) override;
  void OnShowNotification(
      std::unique_ptr<NotificationData> notification_data) override;
  void OnSchedulerInitialized(bool success,
                              std::set<std::string> guids) override;
  void OnUserAction(const UserActionData& action_data) override;
  void GetThrottleConfig(ThrottleConfigCallback callback) override;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_CLIENTS_FINDS_CLIENT_H_
