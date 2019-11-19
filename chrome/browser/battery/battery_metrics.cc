// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/battery/battery_metrics.h"

#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "content/public/browser/system_connector.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_filter.h"

BatteryMetrics::BatteryMetrics() {
  StartRecording();
}

BatteryMetrics::~BatteryMetrics() = default;

void BatteryMetrics::QueryNextStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(battery_monitor_.is_bound());

  battery_monitor_->QueryNextStatus(
      base::BindOnce(&BatteryMetrics::DidChange, weak_factory_.GetWeakPtr()));
}

void BatteryMetrics::StartRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!battery_monitor_.is_bound());

  // Don't create a long lived BatteryMonitor on windows. crbug.com/794105.
#if !defined(OS_WIN)
  content::GetSystemConnector()->Connect(
      service_manager::ServiceFilter::ByName(device::mojom::kServiceName),
      battery_monitor_.BindNewPipeAndPassReceiver());
  QueryNextStatus();
#endif  // !defined(OS_WIN)
}

void BatteryMetrics::DidChange(device::mojom::BatteryStatusPtr battery_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QueryNextStatus();
  RecordBatteryDropUMA(*battery_status);
}

void BatteryMetrics::RecordBatteryDropUMA(
    const device::mojom::BatteryStatus& battery_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (battery_status.charging) {
    // If the battery charges, drop the stored battery level.
    last_recorded_battery_level_ = base::nullopt;
    return;
  }

  if (!last_recorded_battery_level_) {
    // If the battery is not charging, and we don't have a stored battery level,
    // record the current battery level.
    last_recorded_battery_level_ = battery_status.level;
    return;
  }

  // Record the percentage drop event every time the battery drops by 1 percent
  // or more.
  if (last_recorded_battery_level_) {
    float battery_drop_percent_floored =
        std::floor(last_recorded_battery_level_.value() * 100.f -
                   battery_status.level * 100.f);
    if (battery_drop_percent_floored > 0) {
      UMA_HISTOGRAM_PERCENTAGE("Power.BatteryPercentDrop",
                               static_cast<int>(battery_drop_percent_floored));
      // Record the old level minus the recorded drop.
      last_recorded_battery_level_ = last_recorded_battery_level_.value() -
                                     (battery_drop_percent_floored / 100.f);
    }
  }
}
