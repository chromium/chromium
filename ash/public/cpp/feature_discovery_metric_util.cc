// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/feature_discovery_metric_util.h"

#include "ash/public/cpp/app_list/app_list_metrics.h"

namespace ash::feature_discovery {

namespace {

// The histogram that records the mock feature's discovery duration.
constexpr char kMockFeatureHistogram[] = "FeatureDiscoveryTestMockFeature";

// The mock histograms that report metrics data under clamshell/tablet.
constexpr char kMockFeatureClamshellHistogram[] =
    "FeatureDiscoveryTestMockFeature.clamshell";
constexpr char kMockFeatureTabletHistogram[] =
    "FeatureDiscoveryTestMockFeature.tablet";

// The mock features' names.
constexpr char kMockFeatureName[] = "kMockFeature";
constexpr char kModeSeparateMockFeatureName[] = "kMockFeatureSeparate";

}  // namespace


// kTrackableFeatureArray ------------------------------------------------------

const std::array<TrackableFeatureInfo, static_cast<int>(TrackableFeature::kMax)>
    kTrackableFeatureArray{
        TrackableFeatureInfo{TrackableFeature::kMockFeature, kMockFeatureName,
                             kMockFeatureHistogram},
        TrackableFeatureInfo{TrackableFeature::kModeSeparateMockFeature,
                             kModeSeparateMockFeatureName,
                             kMockFeatureClamshellHistogram,
                             kMockFeatureTabletHistogram},
        TrackableFeatureInfo{
            TrackableFeature::kAppListReorderAfterEducationNudge,
            "AppListReorderAfterEducationNudge",
            kAppListSortDiscoveryDurationAfterNudge},
        TrackableFeatureInfo{
            TrackableFeature::kAppListReorderAfterSessionActivation,
            "AppListReorderAfterSessionActivation",
            kAppListSortDiscoveryDurationAfterActivation},
        TrackableFeatureInfo{
            TrackableFeature::kAppListReorderAfterEducationNudgePerTabletMode,
            "AppListReorderAfterEducationNudgeSeparated",
            kAppListSortDiscoveryDurationAfterNudgeClamshell,
            kAppListSortDiscoveryDurationAfterNudgeTablet}};

}  // namespace ash::feature_discovery
