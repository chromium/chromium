// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/smartlock_feature_usage_metrics.h"

#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/components/feature_usage/feature_usage_metrics.h"

namespace ash {

namespace {

const char kFeatureName[] = "SmartLock";
}  // namespace

SmartLockFeatureUsageMetrics::SmartLockFeatureUsageMetrics(
    base::RepeatingCallback<bool()> is_eligible_callback,
    base::RepeatingCallback<bool()> is_enabled_callback)
    : is_eligible_callback_(is_eligible_callback),
      is_enabled_callback_(is_enabled_callback),
      feature_usage_metrics_(kFeatureName, this) {}

SmartLockFeatureUsageMetrics::~SmartLockFeatureUsageMetrics() = default;

void SmartLockFeatureUsageMetrics::RecordUsage(bool success) {
  feature_usage_metrics_.RecordUsage(success);
}

bool SmartLockFeatureUsageMetrics::IsEligible() const {
  return is_eligible_callback_.Run();
}

bool SmartLockFeatureUsageMetrics::IsEnabled() const {
  return is_enabled_callback_.Run();
}

}  // namespace ash
