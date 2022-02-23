// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"

#include "base/feature_list.h"

namespace feature_guide {
namespace features {

const base::Feature kFeatureNotificationGuide{
    "FeatureNotificationGuide", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSegmentationModelLowEngagedUsers{
    "SegmentationModelLowEngagedUsers", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSkipCheckForLowEngagedUsers{
    "FeatureNotificationGuideSkipCheckForLowEngagedUsers",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseFeatureEngagementForUserTargeting{
    "UseFeatureEngagementForUserTargeting", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

FeatureNotificationGuideService::FeatureNotificationGuideService() = default;

FeatureNotificationGuideService::~FeatureNotificationGuideService() = default;

FeatureNotificationGuideService::Delegate::~Delegate() = default;

void FeatureNotificationGuideService::Delegate::SetService(
    FeatureNotificationGuideService* service) {
  service_ = service;
}

FeatureNotificationGuideService*
FeatureNotificationGuideService::Delegate::GetService() {
  return service_;
}

}  // namespace feature_guide
