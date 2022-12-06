// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_BLUETOOTH_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_BLUETOOTH_METRICS_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BluetoothStackName {
  kBlueZ = 0,
  kFloss = 1,
  kUnknown = 2,
  kMaxValue = kUnknown
};

// BluetoothMetricsProvider reports the Bluetooth stack identifiers.
class BluetoothMetricsProvider : public metrics::MetricsProvider {
 public:
  BluetoothMetricsProvider();

  BluetoothMetricsProvider(const BluetoothMetricsProvider&) = delete;
  BluetoothMetricsProvider& operator=(const BluetoothMetricsProvider&) = delete;

  ~BluetoothMetricsProvider() override;

  // metrics::MetricsProvider:
  bool ProvideHistograms() override;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_BLUETOOTH_METRICS_PROVIDER_H_
