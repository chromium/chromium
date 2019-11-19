// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc_app_performance_tracing_custom_session.h"

#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing.h"

namespace arc {

ArcAppPerformanceTracingCustomSession::ArcAppPerformanceTracingCustomSession(
    ArcAppPerformanceTracing* owner)
    : ArcAppPerformanceTracingSession(owner) {}

ArcAppPerformanceTracingCustomSession::
    ~ArcAppPerformanceTracingCustomSession() = default;

void ArcAppPerformanceTracingCustomSession::Schedule() {
  ScheduleInternal(false /* detect_idles */,
                   base::TimeDelta() /* start_delay */,
                   base::TimeDelta() /* tracing_period */);
}

ArcAppPerformanceTracingCustomSession*
ArcAppPerformanceTracingCustomSession::AsCustomSession() {
  return this;
}

void ArcAppPerformanceTracingCustomSession::StopAndAnalyze(
    ResultCallback callback) {
  DCHECK(!callback_);
  DCHECK(callback);

  callback_ = std::move(callback);
  if (tracing_active())
    StopAndAnalyzeInternal();
  else
    OnTracingFailed();

  DCHECK(!callback_);
}

void ArcAppPerformanceTracingCustomSession::OnTracingDone(
    double fps,
    double commit_deviation,
    double render_quality) {
  DCHECK(callback_);
  std::move(callback_).Run(true /* success */, fps, commit_deviation,
                           render_quality);
}

void ArcAppPerformanceTracingCustomSession::OnTracingFailed() {
  DCHECK(callback_);
  std::move(callback_).Run(false /* success */, 0, 0, 0);
}

}  // namespace arc
