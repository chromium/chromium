// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/feature_discovery_metric_util.h"

namespace ash::feature_discovery {

namespace {

// The histogram that records the mock feature's discovery duration.
const char kMockFeatureHistogram[] = "FeatureDiscoveryTestMockFeature";

// The mock feature's name.
const char kMockFeatureName[] = "kMockFeature";

}  // namespace

const std::array<TrackableFeatureInfo, 1> kTrackableFeatureArray{
    TrackableFeatureInfo{TrackableFeature::kMockFeature, kMockFeatureName,
                         kMockFeatureHistogram}};

}  // namespace ash::feature_discovery
