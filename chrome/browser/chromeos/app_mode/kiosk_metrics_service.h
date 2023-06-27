// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_METRICS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_METRICS_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

// Kiosk histogram metrics-related constants.
extern const char kKioskSessionStateHistogram[];
extern const char kKioskSessionCountPerDayHistogram[];
extern const char kKioskSessionDurationNormalHistogram[];
extern const char kKioskSessionDurationInDaysNormalHistogram[];
extern const char kKioskSessionDurationCrashedHistogram[];
extern const char kKioskSessionDurationInDaysCrashedHistogram[];
extern const char kKioskSessionRestartReasonHistogram[];
extern const char kKioskSessionLastDayList[];
extern const char kKioskSessionStartTime[];
extern const char kKioskSessionEndReason[];

extern const base::TimeDelta kKioskSessionDurationHistogramLimit;

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

// These values are used in UMA metrics. When a kiosk session is restarted,
// ChromeOS logs a restart reason based on the previous kiosk session
// ending. Actual device reboot and a session restart without reboot are
// distinguished. Entries should not be renumbered and numeric values should
// never be reused. Keep in sync with respective enum in
// tools/metrics/histograms/enums.xml
enum class KioskSessionRestartReason {
  kStopped = 0,
  kStoppedWithReboot = 1,
  kCrashed = 2,
  kCrashedWithReboot = 3,
  kRebootPolicy = 4,
  kRemoteActionReboot = 5,
  kRestartApi = 6,
  kLocalStateWasNotSaved = 7,
  kLocalStateWasNotSavedWithReboot = 8,
  kPluginCrashed = 9,
  kPluginCrashedWithReboot = 10,
  kPluginHung = 11,
  kPluginHungWithReboot = 12,
  kMaxValue = kPluginHungWithReboot,
};

// These values are saved to the local state during the ending of kiosk session.
enum class KioskSessionEndReason {
  kStopped = 0,
  kRebootPolicy = 1,
  kRemoteActionReboot = 2,
  kRestartApi = 3,
  kPluginCrashed = 4,
  kPluginHung = 5,
  kMaxValue = kPluginHung,
};

// This class accumulates and records kiosk UMA metrics.
class KioskMetricsService : public chromeos::PowerManagerClient::Observer {
 public:
  explicit KioskMetricsService(PrefService* prefs);
  KioskMetricsService(KioskMetricsService&) = delete;
  KioskMetricsService& operator=(const KioskMetricsService&) = delete;
  ~KioskMetricsService() override;

  static std::unique_ptr<KioskMetricsService> CreateForTesting(
      PrefService* prefs,
      const std::vector<std::string>& crash_dirs);

  void RecordKioskSessionStarted();
  void RecordKioskSessionWebStarted();
  void RecordKioskSessionStopped();
  void RecordKioskSessionPluginCrashed();
  void RecordKioskSessionPluginHung();

  // chromeos::PowerManagerClient::Observer:
  void RestartRequested(power_manager::RequestRestartReason reason) override;

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

  void RecordPreviousKioskSessionEndState();

  void RecordPreviousKioskSessionCrashed(const base::Time& start_time) const;

  void RecordKioskSessionRestartReason(
      const KioskSessionRestartReason& reason) const;

  size_t RetrieveLastDaySessionCount(base::Time session_start_time);

  void ClearStartTime();

  void OnPreviousKioskSessionResult(const base::Time& start_time,
                                    bool has_recorded_session_restart_reason,
                                    bool crashed) const;

  void SaveSessionEndReason(const KioskSessionEndReason& reason);

  raw_ptr<PrefService> prefs_;

  // Initialized once the kiosk session is started or during recording of the
  // previously crashed kiosk session metrics.
  // Cleared once the session's duration metric is recorded:
  // either the session is successfully finished or crashed or on the next
  // session startup.
  base::Time start_time_;

  const std::vector<std::string> crash_dirs_;

  // Observation of chromeos::PowerManagerClient.
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};

  base::WeakPtrFactory<KioskMetricsService> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_METRICS_SERVICE_H_
