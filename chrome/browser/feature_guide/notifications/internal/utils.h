// Copyright 2021 The Chromium Authors
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

#if BUILDFLAG(IS_ANDROID)
// Returns the notification IPH feature for the given feature.
const base::Feature& GetNotificationIphFeatureForFeature(FeatureType& feature);

// Returns an IPH feature for the given |feature|, which can be used to
// determine whether the |feature| has been already used by the user.
// Returns null if the used check is done in another way other than using IPH.
const base::Feature* GetUsedIphFeatureForFeature(FeatureType& feature);

#endif

// Whether the feature should only target low engaged users.
bool ShouldTargetLowEngagedUsers(FeatureType feature);

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_UTILS_H_
