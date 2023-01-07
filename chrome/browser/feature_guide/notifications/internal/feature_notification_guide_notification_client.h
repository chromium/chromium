// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_FEATURE_NOTIFICATION_GUIDE_NOTIFICATION_CLIENT_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_FEATURE_NOTIFICATION_GUIDE_NOTIFICATION_CLIENT_H_

#include <memory>

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"

namespace notifications {
struct NotificationData;
struct UserActionData;
}  // namespace notifications

namespace feature_guide {
class FeatureNotificationGuideService;

// The client interface that communicates with notification scheduling system.
class FeatureNotificationGuideNotificationClient
    : public notifications::NotificationSchedulerClient {
 public:
  using ServiceGetter =
      base::RepeatingCallback<FeatureNotificationGuideService*()>;
  explicit FeatureNotificationGuideNotificationClient(ServiceGetter getter);
  ~FeatureNotificationGuideNotificationClient() override;
  FeatureNotificationGuideNotificationClient(
      const FeatureNotificationGuideNotificationClient&) = delete;
  FeatureNotificationGuideNotificationClient operator=(
      const FeatureNotificationGuideNotificationClient&) = delete;

  // notifications::NotificationSchedulerClient implementation.
  void BeforeShowNotification(
      std::unique_ptr<notifications::NotificationData> notification_data,
      NotificationDataCallback callback) override;
  void OnSchedulerInitialized(bool success,
                              std::set<std::string> guids) override;
  void OnUserAction(const notifications::UserActionData& action_data) override;
  void GetThrottleConfig(
      notifications::NotificationSchedulerClient::ThrottleConfigCallback
          callback) override;

 private:
  FeatureNotificationGuideService* GetNotificationService();

  ServiceGetter service_getter_;
};

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_FEATURE_NOTIFICATION_GUIDE_NOTIFICATION_CLIENT_H_
