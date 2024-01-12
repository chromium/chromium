// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_FAST_INITIATION_SCANNER_FEATURE_USAGE_METRICS_H_
#define CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_FAST_INITIATION_SCANNER_FEATURE_USAGE_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "device/bluetooth/bluetooth_adapter.h"

class PrefService;

// Tracks feature usage for FastInitiationScanner for the Standard Feature Usage
// Logging (SFUL) framework.
class FastInitiationScannerFeatureUsageMetrics final
    : public ash::feature_usage::FeatureUsageMetrics::Delegate {
 public:
  explicit FastInitiationScannerFeatureUsageMetrics(PrefService* pref_service);
  FastInitiationScannerFeatureUsageMetrics(
      FastInitiationScannerFeatureUsageMetrics&) = delete;
  FastInitiationScannerFeatureUsageMetrics& operator=(
      FastInitiationScannerFeatureUsageMetrics&) = delete;
  ~FastInitiationScannerFeatureUsageMetrics() final;

  // ash::feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  bool IsEnabled() const override;

  void SetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void RecordUsage(bool success);

 private:
  raw_ptr<PrefService> pref_service_;
  ash::feature_usage::FeatureUsageMetrics feature_usage_metrics_;
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FAST_INITIATION_FAST_INITIATION_SCANNER_FEATURE_USAGE_METRICS_H_
