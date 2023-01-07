// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_notification_client.h"

#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/internal/utils.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

using ThrottleConfigCallback =
    notifications::NotificationSchedulerClient::ThrottleConfigCallback;

namespace feature_guide {

FeatureNotificationGuideNotificationClient::
    FeatureNotificationGuideNotificationClient(ServiceGetter getter)
    : service_getter_(getter) {}

FeatureNotificationGuideNotificationClient::
    ~FeatureNotificationGuideNotificationClient() = default;

void FeatureNotificationGuideNotificationClient::BeforeShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    NotificationDataCallback callback) {
  DCHECK(notification_data.get());
  GetNotificationService()->BeforeShowNotification(std::move(notification_data),
                                                   std::move(callback));
}

void FeatureNotificationGuideNotificationClient::OnSchedulerInitialized(
    bool success,
    std::set<std::string> guids) {
  if (!success)
    return;

  GetNotificationService()->OnSchedulerInitialized(guids);
}

void FeatureNotificationGuideNotificationClient::OnUserAction(
    const notifications::UserActionData& action_data) {
  if (action_data.action_type == notifications::UserActionType::kClick) {
    FeatureType feature = FeatureFromCustomData(action_data.custom_data);
    DCHECK(feature != FeatureType::kInvalid);
    GetNotificationService()->OnClick(feature);
  }
}

void FeatureNotificationGuideNotificationClient::GetThrottleConfig(
    ThrottleConfigCallback callback) {
  // No throttle.
  std::move(callback).Run(nullptr);
}

FeatureNotificationGuideService*
FeatureNotificationGuideNotificationClient::GetNotificationService() {
  return service_getter_.Run();
}

}  // namespace feature_guide
