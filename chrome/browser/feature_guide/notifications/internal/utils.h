// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_UTILS_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_UTILS_H_

#include <map>
#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"

namespace feature_guide {

// Serialize a given FeatureType to notification custom data.
void FeatureToCustomData(FeatureType feature,
                         std::map<std::string, std::string>* custom_data);

// Deserialize the FeatureType from notification custom data.
FeatureType FeatureFromCustomData(
    const std::map<std::string, std::string>& custom_data);

// Get a fixed notification ID for the given feature.
std::string NotificationIdForFeature(FeatureType feature);

// Returns the feature type from the notification ID.
FeatureType NotificationIdToFeature(const std::string& notification_id);

#if defined(OS_ANDROID)
// Returns the notification IPH feature for the given feature.
base::Feature GetNotificationIphFeatureForFeature(FeatureType& feature);
#endif

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_UTILS_H_
