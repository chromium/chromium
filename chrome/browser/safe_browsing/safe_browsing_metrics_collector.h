// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace safe_browsing {

// This class is for logging Safe Browsing metrics regularly. Metrics are logged
// everyday or at startup, if the last logging time was more than a day ago.
// It is also responsible for adding Safe Browsing events in prefs and logging
// metrics when enhanced protection is disabled.
class SafeBrowsingMetricsCollector : public KeyedService {
 public:
  // Enum representing different types of Safe Browsing events for measuring
  // user friction. They are used as keys of the SafeBrowsingEventTimestamps
  // pref. They are used for logging histograms, entries must not be removed or
  // reordered. Please update the enums.xml file if new values are added.
  // They are also used to construct suffixes of histograms. Please update the
  // MetricsCollectorBypassEventType variants in the histograms.xml file if new
  // values are added.
  enum EventType {
    // The user state is disabled.
    USER_STATE_DISABLED = 0,
    // The user state is enabled.
    USER_STATE_ENABLED = 1,
    // The user bypasses the interstitial that is triggered by the Safe Browsing
    // database.
    DATABASE_INTERSTITIAL_BYPASS = 2,
    // The user bypasses the interstitial that is triggered by client-side
    // detection.
    CSD_INTERSITITAL_BYPASS = 3,
    // The user bypasses the interstitial that is triggered by real time URL
    // check.
    REAL_TIME_INTERSTITIAL_BYPASS = 4,
    // The user bypasses the dangerous download warning based on server
    // verdicts.
    DANGEROUS_DOWNLOAD_BYPASS = 5,
    // The user bypasses the password reuse modal warning.
    PASSWORD_REUSE_MODAL_BYPASS = 6,

    kMaxValue = PASSWORD_REUSE_MODAL_BYPASS
  };

  // Enum representing the current user state. They are used as keys of the
  // SafeBrowsingEventTimestamps pref, entries must not be removed or reordered.
  // They are also used to construct suffixes of histograms. Please update the
  // MetricsCollectorUserState variants in the histograms.xml file if new values
  // are added.
  enum UserState {
    // Standard protection is enabled.
    STANDARD_PROTECTION = 0,
    // Enhanced protection is enabled.
    ENHANCED_PROTECTION = 1,
    // Safe Browsing is managed.
    MANAGED = 2
  };

  explicit SafeBrowsingMetricsCollector(PrefService* pref_service_);
  ~SafeBrowsingMetricsCollector() override = default;

  // Checks the last logging time. If the time is longer than a day ago, log
  // immediately. Otherwise, schedule the next logging with delay.
  void StartLogging();

  // Add |event_type| and the current timestamp to pref.
  void AddSafeBrowsingEventToPref(EventType event_type);

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingMetricsCollectorTest, GetUserState);
  struct Event {
    Event(EventType type, base::Time timestamp);
    EventType type;
    base::Time timestamp;
  };
  static bool IsBypassEventType(const EventType& type);
  static std::string GetUserStateMetricSuffix(const UserState& user_state);
  static std::string GetEventTypeMetricSuffix(const EventType& event_type);

  // For daily metrics.
  void LogMetricsAndScheduleNextLogging();
  void ScheduleNextLoggingAfterInterval(base::TimeDelta interval);
  void LogDailyOptInMetrics();
  void LogDailyEventMetrics();
  void RemoveOldEventsFromPref();

  // For pref listeners.
  void OnEnhancedProtectionPrefChanged();
  void LogEnhancedProtectionDisabledMetrics();

  // Helper functions for Safe Browsing events in pref.
  void AddSafeBrowsingEventAndUserStateToPref(UserState user_state,
                                              EventType event_type);
  int GetEventCountSince(UserState user_state,
                         EventType event_type,
                         base::Time since_time);
  UserState GetUserState();

  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  base::OneShotTimer metrics_collector_timer_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingMetricsCollector);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_H_
