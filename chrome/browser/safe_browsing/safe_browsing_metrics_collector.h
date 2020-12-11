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
class SafeBrowsingMetricsCollector : public KeyedService {
 public:
  explicit SafeBrowsingMetricsCollector(PrefService* pref_service_);
  ~SafeBrowsingMetricsCollector() override = default;

  // Checks the last logging time. If the time is longer than a day ago, log
  // immediately. Otherwise, schedule the next logging with delay.
  void StartLogging();

 private:
  void LogMetricsAndScheduleNextLogging();
  void ScheduleNextLoggingAfterInterval(base::TimeDelta interval);

  PrefService* pref_service_;
  base::OneShotTimer metrics_collector_timer_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingMetricsCollector);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_H_
