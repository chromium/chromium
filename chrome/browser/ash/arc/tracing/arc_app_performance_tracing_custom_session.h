// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_CUSTOM_SESSION_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_CUSTOM_SESSION_H_

#include "base/callback.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"

namespace arc {

class ArcAppPerformanceTracing;

// Handles the tracing initiated from a test. It started and stopped explicitly.
// There is no automatic idle detection.
class ArcAppPerformanceTracingCustomSession
    : public ArcAppPerformanceTracingSession {
 public:
  using ResultCallback = base::OnceCallback<void(bool success,
                                                 double fps,
                                                 double commit_deviation,
                                                 double render_quality)>;

  explicit ArcAppPerformanceTracingCustomSession(
      ArcAppPerformanceTracing* owner);

  ArcAppPerformanceTracingCustomSession(
      const ArcAppPerformanceTracingCustomSession&) = delete;
  ArcAppPerformanceTracingCustomSession& operator=(
      const ArcAppPerformanceTracingCustomSession&) = delete;

  ~ArcAppPerformanceTracingCustomSession() override;

  // ArcAppPerformanceTracingSession:
  void Schedule() override;
  ArcAppPerformanceTracingCustomSession* AsCustomSession() override;

  void StopAndAnalyze(ResultCallback callback);

 protected:
  // ArcAppPerformanceTracingSession:
  void OnTracingDone(double fps,
                     double commit_deviation,
                     double render_quality) override;
  void OnTracingFailed() override;

 private:
  ResultCallback callback_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_CUSTOM_SESSION_H_
