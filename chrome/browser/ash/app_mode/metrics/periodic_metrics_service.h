// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_METRICS_PERIODIC_METRICS_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_MODE_METRICS_PERIODIC_METRICS_SERVICE_H_

#include "base/timer/timer.h"

namespace ash {

extern const char kKioskRamUsagePercentageHistogram[];
extern const char kKioskSwapUsagePercentageHistogram[];
extern const char kKioskDiskUsagePercentageHistogram[];
extern const char kKioskChromeProcessCountHistogram[];

extern const base::TimeDelta kPeriodicMetricsInterval;

class DiskSpaceCalculator;

// This class save and record kiosk UMA metrics every
// `kPeriodicMetricsInterval`.
class PeriodicMetricsService {
 public:
  PeriodicMetricsService();
  PeriodicMetricsService(const PeriodicMetricsService&) = delete;
  PeriodicMetricsService& operator=(const PeriodicMetricsService&) = delete;
  ~PeriodicMetricsService();

  void StartRecordingPeriodicMetrics();

 private:
  void RecordPeriodicMetrics();

  void RecordRamUsage() const;

  void RecordSwapUsage() const;

  void RecordDiskSpaceUsage() const;

  void RecordChromeProcessCount() const;

  // Invokes callback to record continuously monitored metrics. Starts when
  // the kiosk session is started.
  base::RepeatingTimer metrics_timer_;

  const std::unique_ptr<DiskSpaceCalculator> disk_space_calculator_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_METRICS_PERIODIC_METRICS_SERVICE_H_
