// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/feature_discovery_metric_util.h"

#include "ash/public/cpp/app_list/app_list_metrics.h"

namespace ash::feature_discovery {

namespace {

// The histogram that records the mock feature's discovery duration.
const char kMockFeatureHistogram[] = "FeatureDiscoveryTestMockFeature";

// The mock feature's name.
const char kMockFeatureName[] = "kMockFeature";

}  // namespace

const std::array<TrackableFeatureInfo, static_cast<int>(TrackableFeature::kMax)>
    kTrackableFeatureArray{
        TrackableFeatureInfo{TrackableFeature::kMockFeature, kMockFeatureName,
                             kMockFeatureHistogram},
        TrackableFeatureInfo{
            TrackableFeature::kAppListReorderAfterEducationNudge,
            "AppListReorderAfterEducationNudge",
            kAppListSortDiscoveryDurationAfterNudge},
        TrackableFeatureInfo{
            TrackableFeature::kAppListReorderAfterSessionActivation,
            "AppListReorderAfterSessionActivation",
            kAppListSortDiscoveryDurationAfterActivation}};

}  // namespace ash::feature_discovery
