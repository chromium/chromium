// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_config.h"

#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"

namespace chromeos {

namespace {
constexpr int kMinDaysThreshold = 0;  // HaTS Onboarding Experience is immediate
}

HatsConfig::HatsConfig(const base::Feature& feature,
                       const base::TimeDelta& hatsNewDeviceThreshold,
                       const char* const hatsIsSelectedPrefName,
                       const char* const hatsCycleEndTimestampPrefName)
    : feature(feature),
      hatsNewDeviceThreshold(hatsNewDeviceThreshold),
      hatsIsSelectedPrefName(hatsIsSelectedPrefName),
      hatsCycleEndTimestampPrefName(hatsCycleEndTimestampPrefName) {
  DCHECK(hatsNewDeviceThreshold.InDaysFloored() >= kMinDaysThreshold);
}

const HatsConfig kHatsGeneralSurvey = {
    ::features::kHappinessTrackingSystem,  // feature
    base::TimeDelta::FromDays(7),          // hatsNewDeviceThreshold
    prefs::kHatsDeviceIsSelected,          // hatsIsSelectedPrefName
    prefs::kHatsSurveyCycleEndTimestamp,   // hatsCycleEndTimestampPrefName
};

// Onboarding Experience Survey -- shown after completing the Onboarding Dialog
const HatsConfig kHatsOnboardingSurvey = {
    ::features::kHappinessTrackingSystemOnboarding,  // feature
    base::TimeDelta::FromMinutes(30),                // hatsNewDeviceThreshold
    prefs::kHatsOnboardingDeviceIsSelected,          // hatsIsSelectedPrefName
    prefs::kHatsOnboardingSurveyCycleEndTs,  // hatsCycleEndTimestampPrefName
};

}  // namespace chromeos
