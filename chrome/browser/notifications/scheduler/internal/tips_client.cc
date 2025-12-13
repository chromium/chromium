// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/tips_client.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/notifications/scheduler/internal/stats.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/public/tips_agent.h"
#include "chrome/browser/notifications/scheduler/public/tips_utils.h"

namespace notifications {

TipsClient::TipsClient(std::unique_ptr<TipsAgent> tips_agent,
                       PrefService* pref_service)
    : tips_agent_(std::move(tips_agent)), pref_service_(pref_service) {}

TipsClient::~TipsClient() = default;

void TipsClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  // Check that there is a valid feature type for the tip.
  auto it = notification_data->custom_data.find(kTipsNotificationsFeatureType);
  if (it != notification_data->custom_data.end()) {
    std::string feature_type = it->second;
    int type_int;
    base::StringToInt(feature_type, &type_int);
    TipsNotificationsFeatureType type =
        static_cast<TipsNotificationsFeatureType>(type_int);

    // Set a pref to mark that a notification for this feature type has shown.
    std::string pref = GetFeatureTypePref(type);
    pref_service_->SetBoolean(pref, true);

    stats::LogTipsNotificationFeatureTypeShown(type);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  std::move(callback).Run(std::move(notification_data));
}

void TipsClient::OnSchedulerInitialized(bool success,
                                        std::set<std::string> guids) {}

void TipsClient::OnUserAction(const UserActionData& action_data) {
#if BUILDFLAG(IS_ANDROID)
  // Check that a valid feature type for tips notifications is requested.
  auto it = action_data.custom_data.find(kTipsNotificationsFeatureType);
  if (it != action_data.custom_data.end()) {
    std::string feature_type = it->second;
    int type_int;
    base::StringToInt(feature_type, &type_int);
    TipsNotificationsFeatureType type =
        static_cast<TipsNotificationsFeatureType>(type_int);

    stats::LogTipsNotificationFeatureTypeAction(action_data.action_type, type);

    // Early exit if the action is a dismissal.
    if (action_data.action_type == UserActionType::kDismiss) {
      return;
    }

    tips_agent_->ShowTipsPromo(type);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void TipsClient::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace notifications
