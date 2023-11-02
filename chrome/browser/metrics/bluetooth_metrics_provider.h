// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_BLUETOOTH_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_BLUETOOTH_METRICS_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/mac/bluetooth_utility.h"
#include "components/metrics/metrics_provider.h"
#include "device/bluetooth/bluetooth_adapter.h"

using bluetooth_utility::BluetoothAvailability;

namespace metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BluetoothStackName {
  kBlueZ = 0,
  kFloss = 1,
  kUnknown = 2,
  kMaxValue = kUnknown
};

// BluetoothMetricsProvider reports the Bluetooth usage and stack identifiers.
class BluetoothMetricsProvider : public metrics::MetricsProvider {
 public:
  BluetoothMetricsProvider();

  BluetoothMetricsProvider(const BluetoothMetricsProvider&) = delete;
  BluetoothMetricsProvider& operator=(const BluetoothMetricsProvider&) = delete;

  ~BluetoothMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void GetBluetoothAvailability();

  // bluetooth_availability_ is initialized to BLUETOOTH_AVAILABILITY_ERROR here
  // as a precaution to the asynchronized fetch of Bluetooth adapter
  // availability. This variable gets updated only once during the class
  // construction time through GetBluetoothAvailability() and its callback
  // OnGetAdapter().
  BluetoothAvailability bluetooth_availability_ =
      BluetoothAvailability::BLUETOOTH_AVAILABILITY_ERROR;
  base::WeakPtrFactory<BluetoothMetricsProvider> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_BLUETOOTH_METRICS_PROVIDER_H_
