// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/feature_discovery_metric_util.h"

#include "ash/public/cpp/app_list/app_list_metrics.h"

namespace ash::feature_discovery {

namespace {

// The histogram that records the mock feature's discovery duration.
const char kMockFeatureHistogram[] = "FeatureDiscoveryTestMockFeature";

// The mock histograms that report metrics data under clamshell/tablet.
const char kMockFeatureClamshellHistogram[] =
    "FeatureDiscoveryTestMockFeature.clamshell";
const char kMockFeatureTabletHistogram[] =
    "FeatureDiscoveryTestMockFeature.tablet";

// The mock features' names.
const char kMockFeatureName[] = "kMockFeature";
const char kModeSeparateMockFeatureName[] = "kMockFeatureSeparate";

}  // namespace

// TrackableFeatureInfo --------------------------------------------------------

constexpr TrackableFeatureInfo::TrackableFeatureInfo(
    TrackableFeature param_feature,
    const char* param_feature_name,
    const char* param_histogram_clamshell,
    const char* param_histogram_tablet)
    : feature(param_feature),
      name(param_feature_name),
      histogram(nullptr),
      histogram_clamshell(param_histogram_clamshell),
      histogram_tablet(param_histogram_tablet),
      split_by_tablet_mode(true) {}

constexpr TrackableFeatureInfo::TrackableFeatureInfo(
    TrackableFeature param_feature,
    const char* param_feature_name,
    const char* param_histogram)
    : feature(param_feature),
      name(param_feature_name),
      histogram(param_histogram),
      histogram_clamshell(nullptr),
      histogram_tablet(nullptr),
      split_by_tablet_mode(false) {}

TrackableFeatureInfo::~TrackableFeatureInfo() = default;

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
