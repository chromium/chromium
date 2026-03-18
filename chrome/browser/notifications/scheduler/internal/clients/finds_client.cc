// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/clients/finds_client.h"

#include <utility>

#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/notifications/scheduler/internal/stats.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"

namespace notifications {

FindsClient::FindsClient() = default;

FindsClient::~FindsClient() = default;

void FindsClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
  std::move(callback).Run(std::move(notification_data));
}

void FindsClient::OnShowNotification(
    std::unique_ptr<NotificationData> notification_data) {
  NOTIMPLEMENTED();
}

void FindsClient::OnSchedulerInitialized(bool success,
                                         std::set<std::string> guids) {
  NOTIMPLEMENTED();
}

void FindsClient::OnUserAction(const UserActionData& action_data) {
  NOTIMPLEMENTED();
}

void FindsClient::GetThrottleConfig(ThrottleConfigCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace notifications
