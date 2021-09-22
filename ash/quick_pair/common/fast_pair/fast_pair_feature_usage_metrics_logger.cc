// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_feature_usage_metrics_logger.h"

namespace {

const char kFastPairUmaFeatureName[] = "FastPair";

}  // namespace

namespace ash {
namespace quick_pair {

FastPairFeatureUsageMetricsLogger::FastPairFeatureUsageMetricsLogger()
    : feature_usage_metrics_(kFastPairUmaFeatureName, this) {}

FastPairFeatureUsageMetricsLogger::~FastPairFeatureUsageMetricsLogger() =
    default;

void FastPairFeatureUsageMetricsLogger::RecordUsage(bool success) {
  feature_usage_metrics_.RecordUsage(success);
}

bool FastPairFeatureUsageMetricsLogger::IsEligible() const {
  // Fast Pair is supported on all Chromebooks.
  return true;
}

bool FastPairFeatureUsageMetricsLogger::IsEnabled() const {
  // crbug/1250479: Update SFUL IsEnabled to support settings toggle
  return true;
}

}  // namespace quick_pair
}  // namespace ash
