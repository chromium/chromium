// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_CLIENT_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_CLIENT_H_

#include <memory>
#include <set>

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"

namespace notifications {
struct NotificationData;
struct UserActionData;
}  // namespace notifications

class ReadingListNotificationService;

// The client interface that communicates with notification scheduling system.
class ReadingListNotificationClient
    : public notifications::NotificationSchedulerClient {
 public:
  using ServiceGetter =
      base::RepeatingCallback<ReadingListNotificationService*()>;
  explicit ReadingListNotificationClient(ServiceGetter getter);
  ~ReadingListNotificationClient() override;
  ReadingListNotificationClient(const ReadingListNotificationClient&) = delete;
  ReadingListNotificationClient operator=(
      const ReadingListNotificationClient&) = delete;

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
  ReadingListNotificationService* GetNotificationService();

  ServiceGetter service_getter_;
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_CLIENT_H_
