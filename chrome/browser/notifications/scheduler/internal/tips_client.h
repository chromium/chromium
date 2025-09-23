// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_TIPS_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_TIPS_CLIENT_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"

namespace notifications {

class TipsAgent;

// The client used in Clank Tips and chrome://notifications-internals for testing.
class TipsClient : public NotificationSchedulerClient {
 public:
  explicit TipsClient(std::unique_ptr<TipsAgent> tips_agent);
  TipsClient(const TipsClient&) = delete;
  TipsClient& operator=(const TipsClient&) = delete;
  ~TipsClient() override;

 private:
  // NotificationSchedulerClient implementation.
  void BeforeShowNotification(
      std::unique_ptr<NotificationData> notification_data,
      NotificationDataCallback callback) override;
  void OnSchedulerInitialized(bool success,
                              std::set<std::string> guids) override;
  void OnUserAction(const UserActionData& action_data) override;
  void GetThrottleConfig(ThrottleConfigCallback callback) override;

  std::unique_ptr<TipsAgent> tips_agent_;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_TIPS_CLIENT_H_
