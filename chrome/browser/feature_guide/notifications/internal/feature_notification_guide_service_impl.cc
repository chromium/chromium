// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"

#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_notification_client.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"

namespace feature_guide {

std::unique_ptr<notifications::NotificationSchedulerClient>
CreateFeatureNotificationGuideNotificationClient(ServiceGetter service_getter) {
  return std::make_unique<FeatureNotificationGuideNotificationClient>(
      service_getter);
}

FeatureNotificationGuideServiceImpl::FeatureNotificationGuideServiceImpl() =
    default;

FeatureNotificationGuideServiceImpl::~FeatureNotificationGuideServiceImpl() =
    default;

void FeatureNotificationGuideServiceImpl::OnSchedulerInitialized(
    const std::set<std::string>& guids) {}

void FeatureNotificationGuideServiceImpl::BeforeShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    NotificationDataCallback callback) {}

void FeatureNotificationGuideServiceImpl::OnClick(FeatureType feature) {}

}  // namespace feature_guide
