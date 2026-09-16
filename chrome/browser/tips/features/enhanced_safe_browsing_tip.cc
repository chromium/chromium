// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/features/enhanced_safe_browsing_tip.h"

#include <map>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "chrome/browser/tips/core/tips_prefs.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "chrome/browser/tips/core/tips_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/segmentation_platform/public/features.h"

namespace tips {

constexpr char EnhancedSafeBrowsingTip::kEnhancedProtectionClickedUserAction[];

EnhancedSafeBrowsingTip::EnhancedSafeBrowsingTip() = default;

EnhancedSafeBrowsingTip::~EnhancedSafeBrowsingTip() = default;

TipFeatureRank EnhancedSafeBrowsingTip::GetRank() const {
  return TipFeatureRank::kEnhancedSafeBrowsing;
}

TipsNotificationsFeatureType EnhancedSafeBrowsingTip::GetFeatureType() const {
  return TipsNotificationsFeatureType::kEnhancedSafeBrowsing;
}

std::vector<SignalDefinition> EnhancedSafeBrowsingTip::GetRequiredSignals()
    const {
  return {UserAction(kEnhancedProtectionClickedUserAction, kLookbackDays)};
}

bool EnhancedSafeBrowsingTip::IsEligible(
    const std::map<std::string, float>& signal_values,
    const PrefService& pref_service) const {
  if (!base::FeatureList::IsEnabled(
          segmentation_platform::features::kAndroidTipsNotifications) ||
      !segmentation_platform::features::kEnableEnhancedSafeBrowsingTip.Get()) {
    return false;
  }

  if (safe_browsing::IsSafeBrowsingPolicyManaged(pref_service)) {
    return false;
  }

  if (pref_service.GetBoolean(prefs::kAndroidTipNotificationShownESB)) {
    return false;
  }

  if (safe_browsing::GetSafeBrowsingState(pref_service) ==
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION) {
    return false;
  }

  // If the user has already clicked or interacted with Enhanced Protection
  // settings in the past 28 days (signal count > 0), do not show the tip.
  auto it = signal_values.find(kEnhancedProtectionClickedUserAction);
  if (it != signal_values.end() && it->second > 0) {
    return false;
  }

  return true;
}

notifications::NotificationData EnhancedSafeBrowsingTip::GetNotificationData()
    const {
  return GetTipsNotificationData(GetFeatureType());
}

}  // namespace tips
