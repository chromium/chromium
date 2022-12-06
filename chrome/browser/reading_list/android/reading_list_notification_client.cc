// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_notification_client.h"

#include <ostream>

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/reading_list/android/reading_list_notification_service.h"

using ThrottleConfigCallback =
    notifications::NotificationSchedulerClient::ThrottleConfigCallback;

ReadingListNotificationClient::ReadingListNotificationClient(
    ServiceGetter getter)
    : service_getter_(getter) {}

ReadingListNotificationClient::~ReadingListNotificationClient() = default;

void ReadingListNotificationClient::BeforeShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    NotificationDataCallback callback) {
  DCHECK(notification_data.get());
  GetNotificationService()->BeforeShowNotification(std::move(notification_data),
                                                   std::move(callback));
}

void ReadingListNotificationClient::OnSchedulerInitialized(
    bool success,
    std::set<std::string> guids) {
  DCHECK_LE(guids.size(), 1u)
      << "Only should have at most one reading list notification.";
}

void ReadingListNotificationClient::OnUserAction(
    const notifications::UserActionData& action_data) {
  if (action_data.action_type == notifications::UserActionType::kClick) {
    GetNotificationService()->OnClick();
  }
}

void ReadingListNotificationClient::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  // No throttle.
  std::move(callback).Run(nullptr);
}

ReadingListNotificationService*
ReadingListNotificationClient::GetNotificationService() {
  return service_getter_.Run();
}
