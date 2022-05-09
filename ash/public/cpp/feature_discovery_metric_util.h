// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FEATURE_DISCOVERY_METRIC_UTIL_H_
#define ASH_PUBLIC_CPP_FEATURE_DISCOVERY_METRIC_UTIL_H_

#include <array>

#include "ash/public/cpp/ash_public_export.h"

namespace ash::feature_discovery {

// The features supported by `FeatureDiscoveryDurationReporter`.
// NOTE: `FeatureDiscoveryDurationReporter` users should add new enum types if
// the features that users expect to track are not listed here. Also, when a new
// enum type is added, make sure to update `kTrackableFeatureArray` as well.
// Do not change the values of the existing enum types except kMax.
enum class ASH_PUBLIC_EXPORT TrackableFeature {
  // A mock feature used for testing.
  kMockFeature = 0,

  // App list reorder after the reorder education nudge shows.
  // TODO(https://crbug.com/1316185): split this histogram into the one for
  // clamshell and another one for tablet.
  kAppListReorderAfterEducationNudge = 1,

  // App list reorder after the user session activation.
  // TODO(https://crbug.com/1316185): split this histogram into the one for
  // clamshell and another one for tablet.
  kAppListReorderAfterSessionActivation = 2,

  // Used to mark the end. It should always be the last one.
  kMax = 3,
};

struct ASH_PUBLIC_EXPORT TrackableFeatureInfo {
  // A trackable feature's enum type.
  TrackableFeature feature;

  // A trackable feature's name.
  const char* name;

  // The histogram that records the discovery duration of `feature`.
  const char* histogram;
};

// A hardcoded array of trackable features' info.
// NOTE: update `kTrackableFeatureArray` if a new trackable feature is added.
ASH_PUBLIC_EXPORT extern const std::
    array<TrackableFeatureInfo, static_cast<int>(TrackableFeature::kMax)>
        kTrackableFeatureArray;

}  // namespace ash::feature_discovery

#endif  // ASH_PUBLIC_CPP_FEATURE_DISCOVERY_METRIC_UTIL_H_
