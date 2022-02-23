// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_FEATURE_USAGE_METRICS_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_FEATURE_USAGE_METRICS_H_

#include "ash/components/proximity_auth/smart_lock_metrics_recorder.h"
#include "chromeos/components/feature_usage/feature_usage_metrics.h"

namespace ash {

// Tracks Smart Lock feature usage for the Standard Feature Usage Logging
// (SFUL) framework.
class SmartLockFeatureUsageMetrics
    : public feature_usage::FeatureUsageMetrics::Delegate,
      public SmartLockMetricsRecorder::UsageRecorder {
 public:
  SmartLockFeatureUsageMetrics(
      base::RepeatingCallback<bool()> is_eligible_callback,
      base::RepeatingCallback<bool()> is_enabled_callback);

  SmartLockFeatureUsageMetrics(SmartLockFeatureUsageMetrics&) = delete;
  SmartLockFeatureUsageMetrics& operator=(SmartLockFeatureUsageMetrics&) =
      delete;
  ~SmartLockFeatureUsageMetrics() override;

  // SmartLockMetricsRecorder::UsageRecorder:
  void RecordUsage(bool success) override;

 private:
  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  bool IsEnabled() const override;

  base::RepeatingCallback<bool()> is_eligible_callback_;
  base::RepeatingCallback<bool()> is_enabled_callback_;
  feature_usage::FeatureUsageMetrics feature_usage_metrics_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_SMARTLOCK_FEATURE_USAGE_METRICS_H_
