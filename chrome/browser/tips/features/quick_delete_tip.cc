// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/features/quick_delete_tip.h"

#include <map>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "chrome/browser/tips/core/tips_prefs.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "chrome/browser/tips/core/tips_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/features.h"

namespace tips {

constexpr char QuickDeleteTip::kMagicStackShownHistogram[];

QuickDeleteTip::QuickDeleteTip() = default;

QuickDeleteTip::~QuickDeleteTip() = default;

TipFeatureRank QuickDeleteTip::GetRank() const {
  return TipFeatureRank::kQuickDelete;
}

TipsNotificationsFeatureType QuickDeleteTip::GetFeatureType() const {
  return TipsNotificationsFeatureType::kQuickDelete;
}

std::vector<SignalDefinition> QuickDeleteTip::GetRequiredSignals() const {
  return {HistogramEnum(kMagicStackShownHistogram, kLookbackDays,
                        {kQuickDeleteMagicStackImpressionEnumValue})};
}

bool QuickDeleteTip::IsEligible(
    const std::map<std::string, float>& signal_values,
    const PrefService& pref_service) const {
  if (!base::FeatureList::IsEnabled(
          segmentation_platform::features::kAndroidTipsNotifications) ||
      !segmentation_platform::features::kEnableQuickDeleteTip.Get()) {
    return false;
  }

  if (pref_service.GetBoolean(prefs::kAndroidTipNotificationShownQuickDelete)) {
    return false;
  }

  if (pref_service.GetBoolean(browsing_data::prefs::kQuickDeleteEverUsed)) {
    return false;
  }

  // If the user has already seen the Quick Delete module in the Magic Stack in
  // the past 28 days (signal count > 0), do not show the tip.
  auto it = signal_values.find(kMagicStackShownHistogram);
  if (it != signal_values.end() && it->second > 0) {
    return false;
  }

  return true;
}

notifications::NotificationData QuickDeleteTip::GetNotificationData() const {
  return GetTipsNotificationData(GetFeatureType());
}

}  // namespace tips
