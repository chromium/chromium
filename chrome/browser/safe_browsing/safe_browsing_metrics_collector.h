// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"

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
  // reordered.
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

    kMaxValue = REAL_TIME_INTERSTITIAL_BYPASS
  };

  // Enum representing the current user state. They are used as keys of the
  // SafeBrowsingEventTimestamps pref, entries must not be removed or reordered.
  enum UserState {
    // Standard protection is enabled.
    STANDARD_PROTECTION = 0,
    // Enhanced protection is enabled.
    ENHANCED_PROTECTION = 1
  };

  explicit SafeBrowsingMetricsCollector(PrefService* pref_service_);
  ~SafeBrowsingMetricsCollector() override = default;

  // Checks the last logging time. If the time is longer than a day ago, log
  // immediately. Otherwise, schedule the next logging with delay.
  void StartLogging();

  // Add |event_type| and the current timestamp to pref.
  void AddSafeBrowsingEventToPref(EventType event_type);

 private:
  void LogMetricsAndScheduleNextLogging();
  void ScheduleNextLoggingAfterInterval(base::TimeDelta interval);

  PrefService* pref_service_;
  base::OneShotTimer metrics_collector_timer_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingMetricsCollector);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_H_
