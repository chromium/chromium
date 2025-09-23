// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/tips_client.h"

#include <utility>

#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/public/tips_agent.h"

namespace notifications {

TipsClient::TipsClient(std::unique_ptr<TipsAgent> tips_agent)
    : tips_agent_(std::move(tips_agent)) {}

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
  // Check that a valid feature type for tips notifications is requested.
  auto it = action_data.custom_data.find(kTipsNotificationsFeatureType);
  if (it != action_data.custom_data.end()) {
    std::string feature_type = it->second;
    int type_int;
    base::StringToInt(feature_type, &type_int);
    TipsNotificationsFeatureType type =
        static_cast<TipsNotificationsFeatureType>(type_int);
    tips_agent_->ShowTipsPromo(type);
  }
}

void TipsClient::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace notifications
