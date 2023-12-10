// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_FEATURE_USAGE_METRICS_LOGGER_H_
#define ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_FEATURE_USAGE_METRICS_LOGGER_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

// Tracks Fast Pair feature usage for the Standard Feature Usage Logging
// (SFUL) framework.
class COMPONENT_EXPORT(QUICK_PAIR_COMMON)
    FastPairFeatureUsageMetricsLogger final
    : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  FastPairFeatureUsageMetricsLogger();
  FastPairFeatureUsageMetricsLogger(const FastPairFeatureUsageMetricsLogger&) =
      delete;
  FastPairFeatureUsageMetricsLogger& operator=(
      const FastPairFeatureUsageMetricsLogger&) = delete;
  ~FastPairFeatureUsageMetricsLogger() override;

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  bool IsEnabled() const override;
  std::optional<bool> IsAccessible() const override;
  void RecordUsage(bool success);

 private:
  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  feature_usage::FeatureUsageMetrics feature_usage_metrics_;
  base::WeakPtrFactory<FastPairFeatureUsageMetricsLogger> weak_ptr_factory_{
      this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_FEATURE_USAGE_METRICS_LOGGER_H_
