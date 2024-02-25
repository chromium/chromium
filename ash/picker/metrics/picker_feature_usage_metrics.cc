// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/metrics/picker_feature_usage_metrics.h"

#include <string_view>

#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

namespace ash {

PickerFeatureUsageMetrics::PickerFeatureUsageMetrics()
    : feature_usage_metrics_("Picker", this) {}

PickerFeatureUsageMetrics::~PickerFeatureUsageMetrics() = default;

bool PickerFeatureUsageMetrics::IsEligible() const {
  // All devices support Picker.
  return true;
}

std::optional<bool> PickerFeatureUsageMetrics::IsAccessible() const {
  // TODO(b/321865738): Check enterprise policy.
  return true;
}

bool PickerFeatureUsageMetrics::IsEnabled() const {
  // TODO(b/321865738): Check settings.
  return true;
}

void PickerFeatureUsageMetrics::StartUsage() {
  // There are no "failed" usages. All attempts should succeed.
  feature_usage_metrics_.RecordUsage(/*success=*/true);
  feature_usage_metrics_.StartSuccessfulUsage();
}

void PickerFeatureUsageMetrics::StopUsage() {
  feature_usage_metrics_.StopSuccessfulUsage();
}

}  // namespace ash
