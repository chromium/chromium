// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/suspend_perf_reporter.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/dummy_histogram.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"

namespace ash {

namespace {

constexpr base::TimeDelta kDelayBetweenSnapshot = base::Minutes(1);

constexpr char kAfterResumeMetricSuffix[] = ".1MinAfterResume";

// UMA metrics to collect for 1 minutes after resuming the device.
//
// The metrics to take snapshots should satisfy these conditions.
//
// * The metrics should be submitted frequently enough in the 1 minute window.
// * The metrics should be submitted on the browser process mainly.
//   `HistogramBase::SnapshotSamples()` may not contains samples of
//   sub-processes depending on the timing.
static const constexpr std::string_view kMetricNames[] = {
    // Browser.MainThreadsCongestion is submitted every 30 seconds on the
    // browser process. At least 1 data point is collected in 1 minute window.
    "Browser.MainThreadsCongestion",
    // Ash.EventLatency.MousePressed.TotalLatency is submitted on the main
    // thread of the browser process every frame drop which is frequent enough.
    "Ash.EventLatency.MousePressed.TotalLatency",
    // The characteristcs of Ash.EventLatency.KeyPressed.TotalLatency is the
    // same as Ash.EventLatency.MousePressed.TotalLatency.
    "Ash.EventLatency.KeyPressed.TotalLatency",
    // Graphics.Smoothness.PercentDroppedFrames3.AllAnimations is submitted
    // every 100 frames which is less than 2 seconds if it is 60fps.
    // This is mainly submitted on the main thread of the browser process, but
    // renderer process can submit.
    "Graphics.Smoothness.PercentDroppedFrames3.AllAnimations",
    // The characteristcs of
    // Graphics.Smoothness.PercentDroppedFrames3.AllInteractions is the same as
    // Graphics.Smoothness.PercentDroppedFrames3.AllAnimations.
    "Graphics.Smoothness.PercentDroppedFrames3.AllInteractions"};

struct MetricsSnapshot {
  std::string_view histogram_name;
  std::unique_ptr<base::HistogramSamples> samples;
};

void OneMinuteAfterResume(std::vector<MetricsSnapshot> snapshots) {
  DCHECK(snapshots.size() == std::size(kMetricNames));

  for (auto& snapshot : snapshots) {
    base::HistogramBase* histogram_base =
        base::StatisticsRecorder::FindHistogram(snapshot.histogram_name);
    if (!histogram_base) {
      continue;
    }

    switch (histogram_base->GetHistogramType()) {
      case base::HistogramType::HISTOGRAM:
      case base::HistogramType::LINEAR_HISTOGRAM:
      case base::HistogramType::BOOLEAN_HISTOGRAM:
      case base::HistogramType::CUSTOM_HISTOGRAM:
        // These 4 histogram types inherit base::Histogram.
        break;
      default:
        DLOG(FATAL) << "SuspendPerfReporter does not support histogram type of "
                    << snapshot.histogram_name;
        continue;
    }
    base::Histogram* histogram = static_cast<base::Histogram*>(histogram_base);

    std::unique_ptr<base::HistogramSamples> samples =
        histogram->SnapshotSamples();
    samples->Subtract(*snapshot.samples.get());

    base::Histogram::FactoryGet(
        base::StrCat({snapshot.histogram_name, kAfterResumeMetricSuffix}),
        histogram->declared_min(), histogram->declared_max(),
        histogram->bucket_count(),
        base::HistogramBase::Flags::kUmaTargetedHistogramFlag)
        ->AddSamples(*samples.get());
  }
}

}  // namespace

SuspendPerfReporter::SuspendPerfReporter(
    chromeos::PowerManagerClient* power_manager_client) {
  observation_.Observe(power_manager_client);
}

SuspendPerfReporter::~SuspendPerfReporter() = default;

void SuspendPerfReporter::SuspendDone(base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<MetricsSnapshot> snapshot_samples;
  for (auto& histogram_name : kMetricNames) {
    base::HistogramBase* histogram =
        base::StatisticsRecorder::FindHistogram(histogram_name);
    if (!histogram) {
      histogram = base::DummyHistogram::GetInstance();
    }
    snapshot_samples.push_back({histogram_name, histogram->SnapshotSamples()});
  }

  // Note: If user suspends and resumes the device again in a minute, the
  // previous metrics is replaced with the new data.
  timer_.Start(
      FROM_HERE, kDelayBetweenSnapshot,
      base::BindOnce(&OneMinuteAfterResume, std::move(snapshot_samples)));
}

}  // namespace ash
