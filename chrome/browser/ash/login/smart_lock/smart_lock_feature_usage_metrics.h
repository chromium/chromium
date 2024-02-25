// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_FEATURE_USAGE_METRICS_H_
#define CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_FEATURE_USAGE_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/ash/components/proximity_auth/smart_lock_metrics_recorder.h"

namespace ash {

namespace multidevice_setup {
class MultiDeviceSetupClient;
}  // namespace multidevice_setup

// Tracks Smart Lock feature usage for the Standard Feature Usage Logging
// (SFUL) framework.
class SmartLockFeatureUsageMetrics
    : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  SmartLockFeatureUsageMetrics(
      multidevice_setup::MultiDeviceSetupClient* multi_device_setup_client);

  SmartLockFeatureUsageMetrics(SmartLockFeatureUsageMetrics&) = delete;
  SmartLockFeatureUsageMetrics& operator=(SmartLockFeatureUsageMetrics&) =
      delete;
  ~SmartLockFeatureUsageMetrics() override;

  void RecordUsage(bool success);

 private:
  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  bool IsEnabled() const override;

  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  feature_usage::FeatureUsageMetrics feature_usage_metrics_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_SMART_LOCK_FEATURE_USAGE_METRICS_H_
