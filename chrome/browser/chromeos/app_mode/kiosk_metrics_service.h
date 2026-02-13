// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_METRICS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_METRICS_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

// Kiosk histogram metrics-related constants.
extern const char kKioskSessionStateHistogram[];
extern const char kKioskSessionCountPerDayHistogram[];
extern const char kKioskSessionDurationNormalHistogram[];
extern const char kKioskSessionDurationInDaysNormalHistogram[];
extern const char kKioskSessionDurationCrashedHistogram[];
extern const char kKioskSessionDurationInDaysCrashedHistogram[];
extern const char kKioskSessionLastDayList[];
extern const char kKioskSessionStartTime[];

extern const base::TimeDelta kKioskSessionDurationHistogramLimit;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with respective enum in tools/metrics/histograms/enums.xml
enum class KioskSessionState {
  kStarted = 0,
  kWebStarted = 1,
  kCrashed = 2,
  kStopped = 3,
  // No longer used, There are no plugins now.
  // kPluginCrashed = 4,
  kPluginHung = 5,
  // No longer used, use kWebStarted for lacros platform.
  // kWebWithLacrosStarted = 6,
  kRestored = 7,
  kIwaStarted = 8,
  kMaxValue = kIwaStarted,
};

// This class accumulates and records kiosk UMA metrics.
class KioskMetricsService {
 public:
  explicit KioskMetricsService(PrefService* prefs);
  KioskMetricsService(KioskMetricsService&) = delete;
  KioskMetricsService& operator=(const KioskMetricsService&) = delete;
  ~KioskMetricsService();

  static std::unique_ptr<KioskMetricsService> CreateForTesting(
      PrefService* prefs,
      const std::vector<std::string>& crash_dirs);

  void RecordKioskSessionStarted();
  void RecordKioskSessionWebStarted();
  void RecordKioskSessionIwaStarted();
  void RecordKioskSessionStopped();
  void RecordKioskSessionPluginHung();

 protected:
  KioskMetricsService(PrefService* prefs,
                      const std::vector<std::string>& crash_dirs);

 private:
  bool IsKioskSessionRunning() const;

  void RecordKioskSessionStarted(KioskSessionState started_state);

  void RecordKioskSessionState(KioskSessionState state) const;

  void RecordKioskSessionCountPerDay();

  void RecordKioskSessionDuration(
      const std::string& kiosk_session_duration_histogram,
      const std::string& kiosk_session_duration_in_days_histogram);

  void RecordKioskSessionDuration(
      const std::string& kiosk_session_duration_histogram,
      const std::string& kiosk_session_duration_in_days_histogram,
      const base::Time& start_time) const;

  void CheckIfPreviousSessionCrashed();

  void RecordPreviousKioskSessionCrashed(const base::Time& start_time,
                                         bool crashed) const;

  size_t RetrieveLastDaySessionCount(base::Time session_start_time);

  void ClearStartTime();

  void OnPreviousKioskSessionResult(const base::Time& start_time,
                                    bool crashed) const;

  raw_ptr<PrefService> prefs_;

  // Initialized once the kiosk session is started or during recording of the
  // previously crashed kiosk session metrics.
  // Cleared once the session's duration metric is recorded:
  // either the session is successfully finished or crashed or on the next
  // session startup.
  base::Time start_time_;

  const std::vector<std::string> crash_dirs_;

  base::WeakPtrFactory<KioskMetricsService> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_METRICS_SERVICE_H_
