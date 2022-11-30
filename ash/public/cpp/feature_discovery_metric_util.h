// Copyright 2022 The Chromium Authors
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
// Ensure that kMax is always the last one.
enum class ASH_PUBLIC_EXPORT TrackableFeature {
  // A mock feature used for testing.
  kMockFeature = 0,

  // A mock feature whose discovery duration is reported with different
  // histograms under tablet and clamshell. Used for testing only.
  kModeSeparateMockFeature,

  // App list reorder after the reorder education nudge shows.
  // TODO(https://crbug.com/1316185): split this histogram into the one for
  // clamshell and another one for tablet.
  kAppListReorderAfterEducationNudge,

  // Similar to `kAppListReorderAfterEducationNudge`. The only difference
  // is that the collected data is separated by the tablet mode state under
  // which the reorder education nudge shows.
  kAppListReorderAfterEducationNudgePerTabletMode,

  // App list reorder after the user session activation.
  // TODO(https://crbug.com/1316185): split this histogram into the one for
  // clamshell and another one for tablet.
  kAppListReorderAfterSessionActivation,

  // Used to mark the end. It should always be the last one.
  kMax,
};

struct ASH_PUBLIC_EXPORT TrackableFeatureInfo {
  // This ctor should be used when the metric data collected from this feature
  // should be separated by the mode, i.e. clamshell or tablet, under which the
  // feature observation starts.
  // In detail, when reporting the metric data collected from a feature defined
  // by this ctor, there are the following two cases:
  // 1. If the observation on this feature starts in tablet mode,
  // `param_histogram_tablet` is used for reporting;
  // 2. If the observation on this feature starts in clamshell mode,
  // `param_histogram_clamshell` is used for reporting.
  // NOTE: if a feature is registered with this ctor, do not switch this feature
  // back to non-split. Otherwise, the data left in pref service may lead to
  // poorly defined behavior.
  constexpr TrackableFeatureInfo(TrackableFeature param_feature,
                                 const char* param_name,
                                 const char* param_histogram_clamshell,
                                 const char* param_histogram_tablet);

  // This ctor should be used when the metric data collected from this feature
  // should NOT be separated by tablet mode.
  // NOTE: if a feature is registered with this ctor, do not switch this feature
  // back to tablet-mode-split.
  constexpr TrackableFeatureInfo(TrackableFeature param_feature,
                                 const char* param_name,
                                 const char* param_histogram);

  TrackableFeatureInfo(const TrackableFeatureInfo&) = delete;
  TrackableFeatureInfo& operator=(const TrackableFeatureInfo&) = delete;
  ~TrackableFeatureInfo();

  // A trackable feature's enum type.
  const TrackableFeature feature;

  // A trackable feature's name.
  const char* const name;

  // The histogram that records the discovery duration of `feature`. Used only
  // when `split_by_tablet_mode` is false.
  const char* const histogram;

  // The histograms to record data under the specified mode (tablet or
  // clamshell). Used only when `split_by_tablet_mode` is true.
  const char* const histogram_clamshell;
  const char* const histogram_tablet;

  // Indicates whether the metric recordings should be split by modes (i.e.
  // tablet or clamshell).
  // Its value should not be set explicitly by `TrackableFeatureInfo`'s users.
  // Instead, the value is calculated by the ctor.
  const bool split_by_tablet_mode;
};

// A hardcoded array of trackable features' info.
// NOTE: update `kTrackableFeatureArray` if a new trackable feature is added.
ASH_PUBLIC_EXPORT extern const std::
    array<TrackableFeatureInfo, static_cast<int>(TrackableFeature::kMax)>
        kTrackableFeatureArray;

}  // namespace ash::feature_discovery

#endif  // ASH_PUBLIC_CPP_FEATURE_DISCOVERY_METRIC_UTIL_H_
