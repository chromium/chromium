// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_OOM_KILLS_MONITOR_H_
#define CHROME_BROWSER_MEMORY_OOM_KILLS_MONITOR_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/atomic_flag.h"
#include "base/timer/timer.h"
#include "components/metrics/daily_event.h"

class PrefService;

namespace memory {

// Traces Linux Out-of-memory killer invocation count. It checks the oom_kill
// field in /proc/vmstat periodically.
class OOMKillsMonitor {
 public:
  OOMKillsMonitor(const OOMKillsMonitor&) = delete;
  OOMKillsMonitor& operator=(const OOMKillsMonitor&) = delete;

  ~OOMKillsMonitor();

  static OOMKillsMonitor& GetInstance();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void Initialize(PrefService* pref_service);

  void LogArcOOMKill(unsigned long current_oom_kills);

  // The following items are public for testing.

  // Difference for the following 2 histograms: OOMKillsCount logs the
  // cumulative OOM kills count since browser start on every OOM kills,
  // OOMKillsDaily is the total OOM kills count in a day. E.g., if there are 100
  // OOM kills in a day, OOMKillsCount logs 1 to the 0, 1, 2, 3, ...., 100
  // buckets, and OOMKillsDaily logs 1 to the 100 bucket.

  // OOM kills count histogram name.
  static const char kOOMKillsCountHistogramName[];

  // Daily OOM kills histogram name.
  static const char kOOMKillsDailyHistogramName[];

  // Stops timers to avoid unexpected timer fire in test.
  void StopTimersForTesting();

  // Trigger daily event manually for testing.
  void TriggerDailyEventForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(OOMKillsMonitorTest, TestHistograms);

  friend class base::NoDestructor<OOMKillsMonitor>;

  class DailyEventObserver;

  OOMKillsMonitor();

  // Checks system OOM count.
  void CheckOOMKill();

  // Splits CheckOOMKill and CheckOOMKillImpl for testing.
  void CheckOOMKillImpl(unsigned long current_oom_kills);

  // Reports OOM Kills to histogram.
  void ReportOOMKills(unsigned long oom_kills_delta);

  void ReportDailyMetrics(metrics::DailyEvent::IntervalType type);

  // Member variables.

  // The number of OOM kills since monitoring is started.
  unsigned long oom_kills_count_ = 0;

  // The number of OOM kills per day.
  unsigned long oom_kills_daily_count_ = 0;

  // A flag set when Initialize() is called to indicate that monitoring has
  // been started.
  bool monitoring_started_ = false;

  // The last oom kills count.
  unsigned long last_oom_kills_count_ = 0;

  // The last ARCVM OOM kills count.
  unsigned long last_arc_oom_kills_count_ = 0;

  base::RepeatingTimer checking_timer_;

  std::unique_ptr<metrics::DailyEvent> daily_event_;

  // Instructs |daily_event_| to check if a day has passed.
  base::RepeatingTimer daily_event_timer_;

  // The name of the histogram used to report that the daily event happened.
  static const char kDailyEventHistogramName[];

  // A raw pointer to the PrefService used to read and write the statistics.
  raw_ptr<PrefService, LeakedDanglingUntriaged> pref_service_;
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_OOM_KILLS_MONITOR_H_
