// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/features/bottom_omnibox_tip.h"

#include <map>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "chrome/browser/tips/core/tips_prefs.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "chrome/browser/tips/core/tips_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"

namespace tips {

BottomOmniboxTip::BottomOmniboxTip() = default;

BottomOmniboxTip::~BottomOmniboxTip() = default;

TipFeatureRank BottomOmniboxTip::GetRank() const {
  return TipFeatureRank::kBottomOmnibox;
}

TipsNotificationsFeatureType BottomOmniboxTip::GetFeatureType() const {
  return TipsNotificationsFeatureType::kBottomOmnibox;
}

std::vector<SignalDefinition> BottomOmniboxTip::GetRequiredSignals() const {
  return {};
}

bool BottomOmniboxTip::IsEligible(
    const std::map<std::string, float>& signal_values,
    const PrefService& pref_service) const {
  if (!base::FeatureList::IsEnabled(
          segmentation_platform::features::kAndroidTipsNotifications) ||
      !segmentation_platform::features::kEnableBottomOmniboxTip.Get()) {
    return false;
  }

  if (pref_service.GetBoolean(
          prefs::kAndroidTipNotificationShownBottomOmnibox)) {
    return false;
  }

  if (pref_service.GetBoolean(omnibox::kBottomOmniboxEverUsed)) {
    return false;
  }

  // If the bottom omnibox is currently enabled/active in the UI (custom runtime
  // signal passed via JNI), do not show the tip.
  auto it = signal_values.find(segmentation_platform::kBottomOmniboxStatus);
  if (it != signal_values.end() && it->second != 0.0f) {
    return false;
  }

  return true;
}

notifications::NotificationData BottomOmniboxTip::GetNotificationData() const {
  return CreateTipsNotificationData(
      GetFeatureType(), IDS_TIPS_NOTIFICATIONS_BOTTOM_OMNIBOX_TITLE,
      IDS_TIPS_NOTIFICATIONS_BOTTOM_OMNIBOX_SUBTITLE);
}

}  // namespace tips
