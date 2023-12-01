// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/uma_perf_reporting.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {
namespace {

// Defines the period to capture tracing results. Can be overwritten for
// testing.
base::TimeDelta tracing_period = base::Seconds(15);

std::string GetHistogramName(const std::string& category,
                             const std::string& name) {
  return base::StringPrintf("Arc.Runtime.Performance.%s.%s", name.c_str(),
                            category.c_str());
}

void ReportFPS(const std::string& category_name, double fps) {
  DCHECK(!category_name.empty());
  DCHECK_GT(fps, 0);
  base::UmaHistogramCounts100(GetHistogramName(category_name, "FPS2"),
                              static_cast<int>(std::round(fps)));
}

void ReportCommitDeviation(const std::string& category_name, double error_mcs) {
  DCHECK(!category_name.empty());
  DCHECK_GE(error_mcs, 0);
  base::UmaHistogramCustomCounts(
      GetHistogramName(category_name, "CommitDeviation2"),
      static_cast<int>(std::round(error_mcs)), 100 /* min */, 5000 /* max */,
      50 /* buckets */);
}

void ReportQuality(const std::string& category_name, double quality) {
  DCHECK(!category_name.empty());
  DCHECK_GT(quality, 0);
  // Report quality from 0 to 100%.
  const int sample = (int)(quality * 100.0);
  base::UmaHistogramPercentageObsoleteDoNotUse(
      GetHistogramName(category_name, "RenderQuality2"), sample);
}

}  // namespace

UmaPerfReporting::UmaPerfReporting() : weak_ptr_factory_(this) {}

UmaPerfReporting::~UmaPerfReporting() = default;

void UmaPerfReporting::Schedule(ArcAppPerformanceTracingSession* session,
                                const std::string& category) {
  base::TimeDelta start_delay = reported_categories_.count(category)
                                    ? kNextTracingDelay
                                    : kInitTracingDelay;

  session->Schedule(
      true /* detect_idles */, start_delay, tracing_period,
      base::BindOnce(&UmaPerfReporting::OnDone, weak_ptr_factory_.GetWeakPtr(),
                     session, category));
}

void UmaPerfReporting::SetTracingPeriodForTesting(
    const base::TimeDelta& period) {
  tracing_period = period;
}

void UmaPerfReporting::OnDone(ArcAppPerformanceTracingSession* session,
                              const std::string& category,
                              const std::optional<PerfTraceResult>& result) {
  if (result.has_value()) {
    VLOG(1) << "Analyzing is done for " << category << " "
            << " FPS: " << result->fps
            << ", quality: " << result->render_quality
            << ", commit_deviation: " << result->commit_deviation;

    ReportFPS(category, result->fps);
    ReportCommitDeviation(category, result->commit_deviation);
    ReportQuality(category, result->render_quality);

    reported_categories_.insert(category);
  }

  Schedule(session, category);
}

}  // namespace arc
