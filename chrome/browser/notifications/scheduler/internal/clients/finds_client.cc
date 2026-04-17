// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/clients/finds_client.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/finds/core/finds_metrics.h"
#include "chrome/browser/finds/core/finds_utils.h"
#include "chrome/browser/notifications/scheduler/internal/stats.h"
#include "chrome/browser/notifications/scheduler/public/finds_agent.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "url/gurl.h"

namespace notifications {

FindsClient::FindsClient(std::unique_ptr<FindsAgent> finds_agent,
                         PrefService* pref_service)
    : finds_agent_(std::move(finds_agent)), pref_service_(pref_service) {}

FindsClient::~FindsClient() = default;

void FindsClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
  if (!finds::IsAllowedByEnterprisePolicy(pref_service_)) {
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(std::move(notification_data));
}

void FindsClient::OnShowNotification(
    std::unique_ptr<NotificationData> notification_data) {
  finds::RecordNotificationShown();
  // This is a failsafe in case cooldown timestamp is not set during scheduling.
  finds::MarkModelExecutionLastTimestamp(pref_service_);
}

void FindsClient::OnSchedulerInitialized(bool success,
                                         std::set<std::string> guids) {
  NOTIMPLEMENTED();
}

void FindsClient::OpenNotificationAction(const UserActionData& action_data) {
  // Finds the notification URL in the notification data and opens the link
  // with Intent created by FindsAgent.
  auto url_it = action_data.custom_data.find(kChromeFindsNotificationsUrl);
  if (url_it != action_data.custom_data.end()) {
    std::string finds_url_string = url_it->second;
    GURL finds_gurl = GURL(finds_url_string);
    DCHECK(finds_gurl.is_valid());
    finds_agent_->OpenNotificationUrl(finds_gurl);
  }
}

void FindsClient::NotInterestedAction(const UserActionData& action_data) {
  finds::RecordNotificationInteraction(
      finds::FindsNotificationUserInteraction::kUnhelpfulButtonClick);
  using SuggestionTheme =
      optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme;
  SuggestionTheme::ThemeType theme_type = SuggestionTheme::UNKNOWN;
  auto theme_it =
      action_data.custom_data.find(kChromeFindsNotificationsThemeType);
  if (theme_it != action_data.custom_data.end()) {
    int theme_int;
    if (base::StringToInt(theme_it->second, &theme_int)) {
      theme_type = static_cast<SuggestionTheme::ThemeType>(theme_int);
    }
  }
  finds::MarkThemeAsNotInterested(pref_service_, theme_type);
}

void FindsClient::HandleNotificationButtonClick(
    const UserActionData& action_data) {
  DCHECK(action_data.button_click_info.has_value());
  // Open in Chrome button.
  if (action_data.button_click_info->type == ActionButtonType::kHelpful) {
    finds::RecordNotificationInteraction(
        finds::FindsNotificationUserInteraction::kHelpfulButtonClick);
    OpenNotificationAction(action_data);
    // Not Interested button.
  } else if (action_data.button_click_info->type ==
             ActionButtonType::kUnhelpful) {
    NotInterestedAction(action_data);
  } else {
    // There should not be any other buttons in the finds notification.
    NOTREACHED();
  }
}

void FindsClient::OnUserAction(const UserActionData& action_data) {
  switch (action_data.action_type) {
    case UserActionType::kClick: {
      finds::RecordNotificationInteraction(
          finds::FindsNotificationUserInteraction::kClick);
      OpenNotificationAction(action_data);
      break;
    }
    case UserActionType::kDismiss:
      finds::RecordNotificationInteraction(
          finds::FindsNotificationUserInteraction::kDismiss);
      break;
    case UserActionType::kButtonClick:
      HandleNotificationButtonClick(action_data);
      break;
    default:
      NOTREACHED();
  }
}

void FindsClient::GetThrottleConfig(ThrottleConfigCallback callback) {
  std::move(callback).Run(nullptr);
}

}  // namespace notifications
