// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CACHED_METRICS_PROFILE_H_
#define CHROME_BROWSER_METRICS_CACHED_METRICS_PROFILE_H_

#include "base/memory/raw_ptr.h"

class Profile;

namespace metrics {

// Caches a user profile to use in metrics providers if needed. Uses the first
// signed-in profile, and sticks with it until that profile becomes invalid.
class CachedMetricsProfile {
 public:
  CachedMetricsProfile();
  CachedMetricsProfile(const CachedMetricsProfile&) = delete;
  CachedMetricsProfile& operator=(const CachedMetricsProfile&) = delete;
  ~CachedMetricsProfile();

  // Returns the profile for which metrics will be gathered. Once a suitable
  // profile has been found, future calls will continue to return the same
  // value so that reported metrics are consistent.
  Profile* GetMetricsProfile();

 private:
  // The profile for which metrics can be gathered. Once a profile is found,
  // its value is cached here so that GetMetricsProfile() can return a
  // consistent value.
  // Note: This may be dangling if the profile being pointed to is deleted
  // before |this|. However, this is safe because GetMetricsProfile() above
  // verifies that this is still valid before returning it. If new accesses are
  // made to this field, care must be taken to ensure that it is still valid.
  raw_ptr<Profile, DisableDanglingPtrDetection> cached_profile_ = nullptr;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CACHED_METRICS_PROFILE_H_
