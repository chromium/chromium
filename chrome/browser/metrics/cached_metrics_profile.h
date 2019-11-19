// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CACHED_METRICS_PROFILE_H_
#define CHROME_BROWSER_METRICS_CACHED_METRICS_PROFILE_H_

#include "base/macros.h"

class Profile;

namespace metrics {

// Caches a user profile to use in metrics providers if needed. Uses the first
// signed-in profile, and sticks with it until that profile becomes invalid.
class CachedMetricsProfile {
 public:
  CachedMetricsProfile();
  ~CachedMetricsProfile();

  // Returns the profile for which metrics will be gathered. Once a suitable
  // profile has been found, future calls will continue to return the same
  // value so that reported metrics are consistent.
  Profile* GetMetricsProfile();

 private:
  // The profile for which metrics can be gathered. Once a profile is found,
  // its value is cached here so that GetMetricsProfile() can return a
  // consistent value.
  Profile* cached_profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CachedMetricsProfile);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CACHED_METRICS_PROFILE_H_
