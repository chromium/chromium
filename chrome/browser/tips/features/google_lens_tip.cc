// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/features/google_lens_tip.h"

#include <map>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "chrome/browser/tips/core/tips_prefs.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "chrome/browser/tips/core/tips_utils.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/features.h"

namespace tips {

constexpr char GoogleLensTip::kNewTabPageSearchBoxLensUserAction[];
constexpr char GoogleLensTip::kMobileOmniboxLensUserAction[];
constexpr char GoogleLensTip::kTasksSurfaceFakeBoxLensUserAction[];
constexpr char GoogleLensTip::kNotificationsTipsLensUserAction[];

GoogleLensTip::GoogleLensTip() = default;

GoogleLensTip::~GoogleLensTip() = default;

TipFeatureRank GoogleLensTip::GetRank() const {
  return TipFeatureRank::kGoogleLens;
}

TipsNotificationsFeatureType GoogleLensTip::GetFeatureType() const {
  return TipsNotificationsFeatureType::kGoogleLens;
}

std::vector<SignalDefinition> GoogleLensTip::GetRequiredSignals() const {
  return {
      UserAction(kNewTabPageSearchBoxLensUserAction, kLookbackDays),
      UserAction(kMobileOmniboxLensUserAction, kLookbackDays),
      UserAction(kTasksSurfaceFakeBoxLensUserAction, kLookbackDays),
      UserAction(kNotificationsTipsLensUserAction, kLookbackDays),
  };
}

bool GoogleLensTip::IsEligible(
    const std::map<std::string, float>& signal_values,
    const PrefService& pref_service) const {
  if (!base::FeatureList::IsEnabled(
          segmentation_platform::features::kAndroidTipsNotifications) ||
      !segmentation_platform::features::kEnableGoogleLensTip.Get()) {
    return false;
  }

  if (pref_service.GetBoolean(prefs::kAndroidTipNotificationShownLens)) {
    return false;
  }

  // If the user has interacted with Google Lens across any surface (NTP
  // searchbox, mobile omnibox, tasks surface, or tips notifications) in the
  // past 28 days (signal count > 0), do not show the tip.
  for (const char* action_name :
       {kNewTabPageSearchBoxLensUserAction, kMobileOmniboxLensUserAction,
        kTasksSurfaceFakeBoxLensUserAction, kNotificationsTipsLensUserAction}) {
    auto it = signal_values.find(action_name);
    if (it != signal_values.end() && it->second > 0) {
      return false;
    }
  }

  return true;
}

notifications::NotificationData GoogleLensTip::GetNotificationData() const {
  return GetTipsNotificationData(GetFeatureType());
}

}  // namespace tips
