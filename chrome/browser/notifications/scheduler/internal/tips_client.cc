// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/tips_client.h"

#include <utility>

#include "base/notimplemented.h"

namespace notifications {

TipsClient::TipsClient() = default;

TipsClient::~TipsClient() = default;

void TipsClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
  std::move(callback).Run(std::move(notification_data));
}

void TipsClient::OnSchedulerInitialized(
    bool success,
    std::set<std::string> guids) {
  NOTIMPLEMENTED();
}

void TipsClient::OnUserAction(const UserActionData& action_data) {
  NOTIMPLEMENTED();
}

void TipsClient::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace notifications
