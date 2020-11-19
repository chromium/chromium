// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_CLIENT_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_CLIENT_H_

#include <memory>

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"

// The client interface that communicates with notification scheduling system.
class ReadingListNotificationClient
    : public notifications::NotificationSchedulerClient {
 public:
  ReadingListNotificationClient();
  ~ReadingListNotificationClient() override;
  ReadingListNotificationClient(const ReadingListNotificationClient&) = delete;
  ReadingListNotificationClient operator=(
      const ReadingListNotificationClient&) = delete;

 private:
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
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_CLIENT_H_
