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
             const base::TimeDelta& hatsThreshold,
             const base::TimeDelta& hatsGooglerThreshold,
             const base::TimeDelta& hatsNewDeviceThreshold);
  HatsConfig(const HatsConfig&) = delete;
  HatsConfig& operator=(const HatsConfig&) = delete;

  // Chrome OS-level feature (switch) we use to retrieve whether this HaTS
  // survey is enabled or not, and its parameters.
  const base::Feature& feature;

  // Minimum amount of time before the notification is displayed again after a
  // user has interacted with it.
  const base::TimeDelta hatsThreshold;

  // The threshold for a Googler is less.
  const base::TimeDelta hatsGooglerThreshold;

  // Minimum amount of time after initial login or oobe after which we can show
  // the HaTS notification.
  const base::TimeDelta hatsNewDeviceThreshold;
};

// CrOS HaTS configs are declared here and defined in hats_config.cc
extern const HatsConfig kHatsGeneralSurvey;

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_HATS_HATS_CONFIG_H_
