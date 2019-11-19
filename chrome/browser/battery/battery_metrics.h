// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BATTERY_BATTERY_METRICS_H_
#define CHROME_BROWSER_BATTERY_BATTERY_METRICS_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/battery_status.mojom.h"

// Records metrics around battery usage on all platforms. Connects to
// Battery monitor via mojo.
class BatteryMetrics {
 public:
  BatteryMetrics();
  ~BatteryMetrics();

 private:
  // Start recording UMA for the session/profile.
  void StartRecording();

  // Calls |QueryNextStatus()| on |battery_monitor_|.
  void QueryNextStatus();

  // Called when a new BatteryStatus update occurs.
  void DidChange(device::mojom::BatteryStatusPtr battery_status);

  // Records the drop in battery by percents of total battery.
  void RecordBatteryDropUMA(const device::mojom::BatteryStatus& battery_status);

  // The battery level at the last time the battery level was recorded. This
  // value is updated by the amount of battery drop reported, so may be
  // different from the last update by .01.
  base::Optional<float> last_recorded_battery_level_;

  // The battery monitor backend for the device Chrome is running on.
  mojo::Remote<device::mojom::BatteryMonitor> battery_monitor_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BatteryMetrics> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BatteryMetrics);
};

#endif  // CHROME_BROWSER_BATTERY_BATTERY_METRICS_H_
