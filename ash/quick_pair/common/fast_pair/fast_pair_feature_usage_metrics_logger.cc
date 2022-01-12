// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_feature_usage_metrics_logger.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "components/prefs/pref_service.h"

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
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  return pref_service && pref_service->GetBoolean(ash::prefs::kFastPairEnabled);
}

}  // namespace quick_pair
}  // namespace ash
