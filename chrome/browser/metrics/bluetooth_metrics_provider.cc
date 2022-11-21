// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/bluetooth_metrics_provider.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "device/bluetooth/floss/floss_features.h"

namespace metrics {

BluetoothMetricsProvider::BluetoothMetricsProvider() = default;

BluetoothMetricsProvider::~BluetoothMetricsProvider() = default;

void BluetoothMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  base::UmaHistogramEnumeration("Bluetooth.StackName",
                                floss::features::IsFlossEnabled()
                                    ? BluetoothStackName::kFloss
                                    : BluetoothStackName::kBlueZ);
}

}  // namespace metrics
