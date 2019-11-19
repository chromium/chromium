// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_UMA_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_UMA_SESSION_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_session.h"

namespace arc {

class ArcAppPerformanceTracing;

// Handles tracing of an app of known category. Tracing is done periodically
// during the all time of app is active. Tracing results are published in UMA.
class ArcAppPerformanceTracingUmaSession
    : public ArcAppPerformanceTracingSession {
 public:
  ArcAppPerformanceTracingUmaSession(ArcAppPerformanceTracing* owner,
                                     const std::string& category);
  ~ArcAppPerformanceTracingUmaSession() override;

  static void SetTracingPeriodForTesting(const base::TimeDelta& period);

  // ArcAppPerformanceTracingSession:
  void Schedule() override;

 protected:
  // ArcAppPerformanceTracingSession:
  void OnTracingDone(double fps,
                     double commit_deviation,
                     double render_quality) override;
  void OnTracingFailed() override;

 private:
  // Determines the tracing start delay. If we already reported this category,
  // start delay will be increased.
  base::TimeDelta GetStartDelay() const;

  // Tracing category.
  const std::string category_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppPerformanceTracingUmaSession);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_UMA_SESSION_H_
