// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_ENGAGEMENT_ANDROID_BLUE_BUBBLE_CONFIGURATION_PROVIDER_H_
#define CHROME_BROWSER_FEATURE_ENGAGEMENT_ANDROID_BLUE_BUBBLE_CONFIGURATION_PROVIDER_H_

#include "components/feature_engagement/public/configuration_provider.h"

namespace feature_engagement {

// Enforces standard frequency limits on Android Blue Bubble IPH features when
// `chrome::android::kBlueBubbleIphCooldownGroup` is enabled:
//   * At most 3 impressions per feature over a 360-day window.
//   * At least a 7-day cooldown between impressions of the same feature.
// Existing configurations that are already stricter (e.g. 1 per lifetime or a
// 90-day cooldown) are preserved.
class AndroidBlueBubbleConfigurationProvider : public ConfigurationProvider {
 public:
  AndroidBlueBubbleConfigurationProvider();
  ~AndroidBlueBubbleConfigurationProvider() override;

  // ConfigurationProvider:
  bool MaybeProvideFeatureConfiguration(
      const base::Feature& feature,
      FeatureConfig& config,
      const FeatureVector& known_features,
      const GroupVector& known_groups) const override;
  const char* GetConfigurationSourceDescription() const override;
};

}  // namespace feature_engagement

#endif  // CHROME_BROWSER_FEATURE_ENGAGEMENT_ANDROID_BLUE_BUBBLE_CONFIGURATION_PROVIDER_H_
