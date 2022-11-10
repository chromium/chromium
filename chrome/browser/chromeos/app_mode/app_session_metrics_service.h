// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_METRICS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_METRICS_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

// Kiosk histogram metrics-related constants.
extern const char kKioskSessionStateHistogram[];
extern const char kKioskSessionCountPerDayHistogram[];
extern const char kKioskSessionDurationNormalHistogram[];
extern const char kKioskSessionDurationInDaysNormalHistogram[];
extern const char kKioskSessionDurationCrashedHistogram[];
extern const char kKioskSessionDurationInDaysCrashedHistogram[];
extern const char kKioskRamUsagePercentageHistogram[];
extern const char kKioskSwapUsagePercentageHistogram[];
extern const char kKioskDiskUsagePercentageHistogram[];
extern const char kKioskChromeProcessCountHistogram[];
extern const char kKioskSessionLastDayList[];
extern const char kKioskSessionStartTime[];

extern const base::TimeDelta kKioskSessionDurationHistogramLimit;
extern const base::TimeDelta kPeriodicMetricsInterval;

class DiskSpaceCalculator;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with respective enum in tools/metrics/histograms/enums.xml
enum class KioskSessionState {
  kStarted = 0,
  kWebStarted = 1,
  kCrashed = 2,
  kStopped = 3,
  kPluginCrashed = 4,
  kPluginHung = 5,
  // No longer used, use kWebStarted for lacros platform.
  // kWebWithLacrosStarted = 6,
  kRestored = 7,
  kMaxValue = kRestored,
};

// This object accumulates and records kiosk UMA metrics.
class AppSessionMetricsService {
 public:
  explicit AppSessionMetricsService(PrefService* prefs);
  AppSessionMetricsService(AppSessionMetricsService&) = delete;
  AppSessionMetricsService& operator=(const AppSessionMetricsService&) = delete;
  ~AppSessionMetricsService();

  static std::unique_ptr<AppSessionMetricsService> CreateForTesting(
      PrefService* prefs,
      const std::vector<std::string>& crash_dirs);

  void RecordKioskSessionStarted();
  void RecordKioskSessionWebStarted();
  void RecordKioskSessionStopped();
  void RecordKioskSessionPluginCrashed();
  void RecordKioskSessionPluginHung();

 protected:
  AppSessionMetricsService(PrefService* prefs,
                           const std::vector<std::string>& crash_dirs);

 private:
  bool IsKioskSessionRunning() const;

  void RecordKioskSessionStarted(KioskSessionState started_state);

  void StartMetricsTimer();

  void RecordPeriodicMetrics();

  void RecordRamUsage() const;

  // Not recorded if Chrome fails to return the SystemMemoryInfo.
  // This can happen, for example, if it fails to open /proc/meminfo
  // on Linux.
  void RecordSwapUsage() const;

  void RecordDiskSpaceUsage() const;

  void RecordChromeProcessCount() const;

  void RecordKioskSessionState(KioskSessionState state) const;

  void RecordKioskSessionCountPerDay();

  void RecordKioskSessionDuration(
      const std::string& kiosk_session_duration_histogram,
      const std::string& kiosk_session_duration_in_days_histogram);

  void RecordKioskSessionDuration(
      const std::string& kiosk_session_duration_histogram,
      const std::string& kiosk_session_duration_in_days_histogram,
      const base::Time& start_time) const;

  void RecordPreviousKioskSessionCrashIfAny();

  size_t RetrieveLastDaySessionCount(base::Time session_start_time);

  void ClearStartTime();

  void RecordPreviousKioskSessionCrashed(const base::Time& start_time) const;

  void OnPreviousKioskSessionResult(const base::Time& start_time,
                                    bool crashed) const;

  raw_ptr<PrefService> prefs_;

  // Initialized once the kiosk session is started or during recording of the
  // previously crashed kiosk session metrics.
  // Cleared once the session's duration metric is recorded:
  // either the session is successfully finished or crashed or on the next
  // session startup.
  base::Time start_time_;

  // Invokes callback to record continuously monitored metrics. Starts when
  // the kiosk session is started.
  base::RepeatingTimer metrics_timer_;

  const std::unique_ptr<DiskSpaceCalculator> disk_space_calculator_;

  const std::vector<std::string> crash_dirs_;

  base::WeakPtrFactory<AppSessionMetricsService> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_METRICS_SERVICE_H_
