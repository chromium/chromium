// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_notification_client.h"

#include "base/notreached.h"

using ThrottleConfigCallback =
    notifications::NotificationSchedulerClient::ThrottleConfigCallback;

ReadingListNotificationClient::ReadingListNotificationClient() = default;
ReadingListNotificationClient::~ReadingListNotificationClient() = default;

void ReadingListNotificationClient::BeforeShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    NotificationDataCallback callback) {
  NOTIMPLEMENTED();
}

void ReadingListNotificationClient::OnSchedulerInitialized(
    bool success,
    std::set<std::string> guids) {
  NOTIMPLEMENTED();
}

void ReadingListNotificationClient::OnUserAction(
    const notifications::UserActionData& action_data) {
  NOTIMPLEMENTED();
}

void ReadingListNotificationClient::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  NOTIMPLEMENTED();
}
