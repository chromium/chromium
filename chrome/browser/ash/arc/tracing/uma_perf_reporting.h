// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_UMA_PERF_REPORTING_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_UMA_PERF_REPORTING_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"

namespace arc {

// Defines the delay to start tracing after ARC++ window gets activated.
// This is done to avoid likely redundant statistics collection during the app
// initialization/loading time.
static constexpr base::TimeDelta kInitTracingDelay = base::Minutes(1);

// Defines the delay to start next session of capturing statistics for the same
// active app or in case the app was already reported.
static constexpr base::TimeDelta kNextTracingDelay = base::Minutes(20);

// Schedules and reports UMA performance measurements.
class UmaPerfReporting {
 public:
  UmaPerfReporting();
  ~UmaPerfReporting();

  void Schedule(ArcAppPerformanceTracingSession* session,
                const std::string& category);

  static void SetTracingPeriodForTesting(const base::TimeDelta& period);

 private:
  void OnDone(ArcAppPerformanceTracingSession* session,
              const std::string& category,
              const std::optional<PerfTraceResult>& result);

  // Set of already reported ARC++ apps for the current session. Used to prevent
  // capturing too frequently.
  std::set<std::string> reported_categories_;

  base::WeakPtrFactory<UmaPerfReporting> weak_ptr_factory_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_UMA_PERF_REPORTING_H_
