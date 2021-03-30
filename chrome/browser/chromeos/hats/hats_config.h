// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_HATS_HATS_CONFIG_H_
#define CHROME_BROWSER_CHROMEOS_HATS_HATS_CONFIG_H_

#include "base/time/time.h"

namespace base {
struct Feature;
}  // namespace base

namespace chromeos {

struct HatsConfig {
  HatsConfig(const base::Feature& feature,
             const base::TimeDelta& hatsNewDeviceThreshold,
             const char* const hatsIsSelectedPrefName,
             const char* const hatsCycleEndTimestampPrefName);
  HatsConfig(const HatsConfig&) = delete;
  HatsConfig& operator=(const HatsConfig&) = delete;

  // Chrome OS-level feature (switch) we use to retrieve whether this HaTS
  // survey is enabled or not, and its parameters.
  const base::Feature& feature;

  // Minimum amount of time after initial login or oobe after which we can show
  // the HaTS notification.
  const base::TimeDelta hatsNewDeviceThreshold;

  // Preference name for a boolean that stores whether we were selected for the
  // current survey cycle.
  const char* const hatsIsSelectedPrefName;

  // Preference name for an int64 that stores the current survey cycle end.
  const char* const hatsCycleEndTimestampPrefName;
};

// CrOS HaTS configs are declared here and defined in hats_config.cc
extern const HatsConfig kHatsGeneralSurvey;
extern const HatsConfig kHatsOnboardingSurvey;

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_HATS_HATS_CONFIG_H_
