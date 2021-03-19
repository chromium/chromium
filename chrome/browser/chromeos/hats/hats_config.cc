// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_config.h"

#include "chrome/common/chrome_features.h"

namespace chromeos {

namespace {
constexpr int MIN_DAYS_THRESHOLD = 1;
}

HatsConfig::HatsConfig(const base::Feature& feature,
                       const base::TimeDelta& hatsThreshold,
                       const base::TimeDelta& hatsGooglerThreshold,
                       const base::TimeDelta& hatsNewDeviceThreshold)
    : feature(feature),
      hatsThreshold(hatsThreshold),
      hatsGooglerThreshold(hatsGooglerThreshold),
      hatsNewDeviceThreshold(hatsNewDeviceThreshold) {
  DCHECK(hatsThreshold.InDaysFloored() >= MIN_DAYS_THRESHOLD);
  DCHECK(hatsGooglerThreshold.InDaysFloored() >= MIN_DAYS_THRESHOLD);
  DCHECK(hatsNewDeviceThreshold.InDaysFloored() >= MIN_DAYS_THRESHOLD);
}

const HatsConfig kHatsGeneralSurvey = {
    ::features::kHappinessTrackingSystem,  // feature
    base::TimeDelta::FromDays(90),         // hatsThreshold
    base::TimeDelta::FromDays(30),         // hatsGooglerThreshold
    base::TimeDelta::FromDays(7),          // hatsNewDeviceThreshold
};

}  // namespace chromeos
