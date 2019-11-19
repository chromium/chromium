// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_uma_session.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing.h"

namespace arc {

namespace {

// Defines the delay to start tracing after ARC++ window gets activated.
// This is done to avoid likely redundant statistics collection during the app
// initialization/loading time.
constexpr base::TimeDelta kInitTracingDelay = base::TimeDelta::FromMinutes(1);

// Defines the delay to start next session of capturing statistics for the same
// active app or in case the app was already reported.
constexpr base::TimeDelta kNextTracingDelay = base::TimeDelta::FromMinutes(20);

// Defines the period to capture tracing results. Can be overwritten for
// testing.
base::TimeDelta tracing_period = base::TimeDelta::FromSeconds(15);

std::string GetHistogramName(const std::string& category,
                             const std::string& name) {
  return base::StringPrintf("Arc.Runtime.Performance.%s.%s", name.c_str(),
                            category.c_str());
}

void ReportFPS(const std::string& category_name, double fps) {
  DCHECK(!category_name.empty());
  DCHECK_GT(fps, 0);
  base::UmaHistogramCounts100(GetHistogramName(category_name, "FPS"),
                              static_cast<int>(std::round(fps)));
}

void ReportCommitDeviation(const std::string& category_name, double error_mcs) {
  DCHECK(!category_name.empty());
  DCHECK_GE(error_mcs, 0);
  base::UmaHistogramCustomCounts(
      GetHistogramName(category_name, "CommitDeviation"),
      static_cast<int>(std::round(error_mcs)), 100 /* min */, 5000 /* max */,
      50 /* buckets */);
}

void ReportQuality(const std::string& category_name, double quality) {
  DCHECK(!category_name.empty());
  DCHECK_GT(quality, 0);
  // Report quality from 0 to 100%.
  const int sample = (int)(quality * 100.0);
  base::UmaHistogramPercentage(GetHistogramName(category_name, "RenderQuality"),
                               sample);
}

}  // namespace

ArcAppPerformanceTracingUmaSession::ArcAppPerformanceTracingUmaSession(
    ArcAppPerformanceTracing* owner,
    const std::string& category)
    : ArcAppPerformanceTracingSession(owner), category_(category) {}

ArcAppPerformanceTracingUmaSession::~ArcAppPerformanceTracingUmaSession() =
    default;

// static
void ArcAppPerformanceTracingUmaSession::SetTracingPeriodForTesting(
    const base::TimeDelta& period) {
  tracing_period = period;
}

void ArcAppPerformanceTracingUmaSession::Schedule() {
  ScheduleInternal(true /* detect_idles */, GetStartDelay(), tracing_period);
}

void ArcAppPerformanceTracingUmaSession::OnTracingDone(double fps,
                                                       double commit_deviation,
                                                       double render_quality) {
  VLOG(1) << "Analyzing is done for " << category_ << " "
          << " FPS: " << fps << ", quality: " << render_quality
          << ", commit_deviation: " << commit_deviation;

  ReportFPS(category_, fps);
  ReportCommitDeviation(category_, commit_deviation);
  ReportQuality(category_, render_quality);

  // Report category is processed.
  owner()->SetReported(category_);
  Schedule();
}

void ArcAppPerformanceTracingUmaSession::OnTracingFailed() {
  // It valid case, just reschedule.
  Schedule();
}

base::TimeDelta ArcAppPerformanceTracingUmaSession::GetStartDelay() const {
  return owner()->WasReported(category_) ? kNextTracingDelay
                                         : kInitTracingDelay;
}

}  // namespace arc
