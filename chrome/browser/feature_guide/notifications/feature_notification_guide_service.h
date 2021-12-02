// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_H_

#include "base/feature_list.h"
#include "components/keyed_service/core/keyed_service.h"

namespace feature_guide {
namespace features {

// Main feature flag for the feature notification guide feature.
extern const base::Feature kFeatureNotificationGuide;

}  // namespace features

// The central class responsible for managing feature notification guide in
// chrome.
class FeatureNotificationGuideService : public KeyedService {
 public:
  FeatureNotificationGuideService();
  ~FeatureNotificationGuideService() override;

  FeatureNotificationGuideService(const FeatureNotificationGuideService&) =
      delete;
  FeatureNotificationGuideService& operator=(
      const FeatureNotificationGuideService&) = delete;
};

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_H_
