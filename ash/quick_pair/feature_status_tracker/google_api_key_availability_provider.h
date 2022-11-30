// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_GOOGLE_API_KEY_AVAILABILITY_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_GOOGLE_API_KEY_AVAILABILITY_PROVIDER_H_

#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"

namespace ash {
namespace quick_pair {

// Exposes whether Google API keys are available on the current build.  Since
// this cannot be changed at runtime, there is no need to register an observer.
class GoogleApiKeyAvailabilityProvider : public BaseEnabledProvider {
 public:
  GoogleApiKeyAvailabilityProvider();
  ~GoogleApiKeyAvailabilityProvider() override;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_GOOGLE_API_KEY_AVAILABILITY_PROVIDER_H_
