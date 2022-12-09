// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_METRICS_PERIODIC_METRICS_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_MODE_METRICS_PERIODIC_METRICS_SERVICE_H_

#include "base/timer/timer.h"
#include "components/prefs/pref_service.h"

namespace ash {

extern const char kKioskRamUsagePercentageHistogram[];
extern const char kKioskSwapUsagePercentageHistogram[];
extern const char kKioskDiskUsagePercentageHistogram[];
extern const char kKioskChromeProcessCountHistogram[];
extern const char kKioskSessionRestartInternetAccessHistogram[];
extern const char kKioskInternetAccessInfo[];

extern const base::TimeDelta kPeriodicMetricsInterval;

// These values are used in UMA metrics. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with respective enum in
// tools/metrics/histograms/enums.xml
enum class KioskInternetAccessInfo {
  kOnlineAndAppRequiresInternet = 0,
  kOnlineAndAppSupportsOffline = 1,
  kOfflineAndAppRequiresInternet = 2,
  kOfflineAndAppSupportsOffline = 3,
  kMaxValue = kOfflineAndAppSupportsOffline,
};

class DiskSpaceCalculator;

// This class save and record kiosk UMA metrics every
// `kPeriodicMetricsInterval`.
class PeriodicMetricsService {
 public:
  explicit PeriodicMetricsService(PrefService* prefs);
  PeriodicMetricsService(const PeriodicMetricsService&) = delete;
  PeriodicMetricsService& operator=(const PeriodicMetricsService&) = delete;
  ~PeriodicMetricsService();

  // Record metrics about the previous kiosk session. Should be called in the
  // beginning of the kiosk session and before `StartRecordingPeriodicMetrics`.
  void RecordPreviousSessionMetrics() const;

  void StartRecordingPeriodicMetrics(bool is_offline_enabled);

 private:
  void RecordPeriodicMetrics();

  void RecordRamUsage() const;

  void RecordSwapUsage() const;

  void RecordDiskSpaceUsage() const;

  void RecordChromeProcessCount() const;

  void RecordPreviousInternetAccessInfo() const;

  // Save the Internet access info to record
  // `kKioskSessionRestartInternetAccessHistogram` during the next kiosk session
  // start.
  void SaveInternetAccessInfo() const;

  // Invokes callback to record continuously monitored metrics. Starts when
  // the kiosk session is started.
  base::RepeatingTimer metrics_timer_;

  bool is_offline_enabled_ = true;

  raw_ptr<PrefService> prefs_;

  const std::unique_ptr<DiskSpaceCalculator> disk_space_calculator_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_METRICS_PERIODIC_METRICS_SERVICE_H_
