// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/metrics/quick_insert_feature_usage_metrics.h"

#include <string_view>

#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

namespace ash {

QuickInsertFeatureUsageMetrics::QuickInsertFeatureUsageMetrics()
    : feature_usage_metrics_("Picker", this) {}

QuickInsertFeatureUsageMetrics::~QuickInsertFeatureUsageMetrics() = default;

bool QuickInsertFeatureUsageMetrics::IsEligible() const {
  // All devices support Quick Insert.
  return true;
}

std::optional<bool> QuickInsertFeatureUsageMetrics::IsAccessible() const {
  // TODO(b/321865738): Check enterprise policy.
  return true;
}

bool QuickInsertFeatureUsageMetrics::IsEnabled() const {
  // TODO(b/321865738): Check settings.
  return true;
}

void QuickInsertFeatureUsageMetrics::StartUsage() {
  // There are no "failed" usages. All attempts should succeed.
  feature_usage_metrics_.RecordUsage(/*success=*/true);
  feature_usage_metrics_.StartSuccessfulUsage();
}

void QuickInsertFeatureUsageMetrics::StopUsage() {
  feature_usage_metrics_.StopSuccessfulUsage();
}

}  // namespace ash
