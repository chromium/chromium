// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/external_metrics/external_metrics.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/dir_reader_posix.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "components/metrics/serialization/metric_sample.h"
#include "components/metrics/serialization/serialization_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

bool CheckValues(const std::string& name,
                 int minimum,
                 int maximum,
                 size_t bucket_count) {
  if (!base::Histogram::InspectConstructionArguments(name, &minimum, &maximum,
                                                     &bucket_count)) {
    return false;
  }
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram) {
    return true;
  }
  return histogram->HasConstructionArguments(minimum, maximum, bucket_count);
}

bool CheckLinearValues(const std::string& name, int maximum) {
  return CheckValues(name, 1, maximum, maximum + 1);
}

// Default interval between externally-reported metrics being collected.
constexpr base::TimeDelta kDefaultCollectionInterval = base::Seconds(30);

ExternalMetrics* g_instance = nullptr;

}  // namespace

ExternalMetrics::ExternalMetrics()
    : uma_events_file_(kEventsFilePath),
      uma_events_dir_(kEventsDirectoryPath),
      uma_early_metrics_dir_(kUmaEarlyMetricsDirectoryPath),
      collection_interval_(kDefaultCollectionInterval) {
  CHECK(!g_instance);
  g_instance = this;

  const std::string flag_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kExternalMetricsCollectionInterval);
  if (!flag_value.empty()) {
    int seconds = -1;
    if (base::StringToInt(flag_value, &seconds) && seconds > 0) {
      collection_interval_ = base::Seconds(seconds);
    } else {
      LOG(WARNING) << "Ignoring bad value \"" << flag_value << "\" in --"
                   << switches::kExternalMetricsCollectionInterval;
    }
  }
}

ExternalMetrics::~ExternalMetrics() {
  CHECK(g_instance == this);
  g_instance = nullptr;
}

// static
ExternalMetrics* ExternalMetrics::Get() {
  return g_instance;
}

void ExternalMetrics::Start() {
  ScheduleCollector();
}

// static
scoped_refptr<ExternalMetrics> ExternalMetrics::CreateForTesting(
    const std::string& filename,
    const std::string& uma_events_dir,
    const std::string& uma_early_metrics_dir) {
  scoped_refptr<ExternalMetrics> external_metrics(new ExternalMetrics());
  external_metrics->uma_events_file_ = filename;
  external_metrics->uma_events_dir_ = uma_events_dir;
  external_metrics->uma_early_metrics_dir_ = uma_early_metrics_dir;
  return external_metrics;
}

void ExternalMetrics::RecordActionUI(const std::string& action_string,
                                     int num_samples) {
  for (int i = 0; i < num_samples; ++i) {
    base::RecordComputedAction(action_string);
  }
}

void ExternalMetrics::RecordAction(const metrics::MetricSample& sample) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ExternalMetrics::RecordActionUI, this,
                                sample.name(), sample.num_samples()));
}

void ExternalMetrics::RecordCrashUI(const std::string& crash_kind,
                                    int num_samples) {
  ChromeOSMetricsProvider::LogCrash(crash_kind, num_samples);
}

void ExternalMetrics::RecordCrash(const metrics::MetricSample& crash_sample) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ExternalMetrics::RecordCrashUI, this, crash_sample.name(),
                     crash_sample.num_samples()));
}

void ExternalMetrics::RecordHistogram(const metrics::MetricSample& sample) {
  CHECK_EQ(metrics::MetricSample::HISTOGRAM, sample.type());
  if (!CheckValues(sample.name(), sample.min(), sample.max(),
                   sample.bucket_count())) {
    DLOG(ERROR) << "Invalid histogram: " << sample.name();
    return;
  }

  // We don't use base::UmaHistogramCustomCounts here because it doesn't support
  // AddCount.
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      sample.name(), sample.min(), sample.max(), sample.bucket_count(),
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(sample.sample(), sample.num_samples());
}

void ExternalMetrics::RecordLinearHistogram(
    const metrics::MetricSample& sample) {
  CHECK_EQ(metrics::MetricSample::LINEAR_HISTOGRAM, sample.type());
  if (!CheckLinearValues(sample.name(), sample.max())) {
    DLOG(ERROR) << "Invalid linear histogram: " << sample.name();
    return;
  }
  // We don't use base::UmaHistogramExactLinear because it doesn't support
  // AddCount.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      sample.name(), 1, sample.max(), static_cast<size_t>(sample.max() + 1),
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(sample.sample(), sample.num_samples());
}

void ExternalMetrics::RecordSparseHistogram(
    const metrics::MetricSample& sample) {
  CHECK_EQ(metrics::MetricSample::SPARSE_HISTOGRAM, sample.type());
  // We suspect a chromeos process reports a metric as regular and then later as
  // a sparse enum histogram. See https://crbug.com/1173221
  {
    base::HistogramBase* histogram =
        base::StatisticsRecorder::FindHistogram(sample.name());
    if (histogram && histogram->GetHistogramType() != base::SPARSE_HISTOGRAM) {
      LOG(FATAL) << "crbug.com/1173221 name " << sample.name() << " "
                 << sample.ToString();
    }
  }

  // We don't use base::UmaHistogramSparse because it doesn't support AddCount.
  base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
      sample.name(), base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(sample.sample(), sample.num_samples());
}

int ExternalMetrics::ProcessSamples(
    std::vector<std::unique_ptr<metrics::MetricSample>>* samples) {
  int num_samples = 0;
  for (auto it = samples->begin(); it != samples->end(); ++it) {
    const metrics::MetricSample& sample = **it;

    num_samples += sample.num_samples();

    // Do not use the UMA_HISTOGRAM_... macros here.  They cache the Histogram
    // instance and thus only work if |sample.name()| is constant.
    switch (sample.type()) {
      case metrics::MetricSample::CRASH:
        RecordCrash(sample);
        break;
      case metrics::MetricSample::USER_ACTION:
        RecordAction(sample);
        break;
      case metrics::MetricSample::HISTOGRAM:
        RecordHistogram(sample);
        break;
      case metrics::MetricSample::LINEAR_HISTOGRAM:
        RecordLinearHistogram(sample);
        break;
      case metrics::MetricSample::SPARSE_HISTOGRAM:
        RecordSparseHistogram(sample);
        break;
    }
  }
  samples->clear();
  return num_samples;
}

int ExternalMetrics::CollectEvents() {
  std::vector<std::unique_ptr<metrics::MetricSample>> samples;
  metrics::SerializationUtils::ReadAndTruncateMetricsFromFile(uma_events_file_,
                                                              &samples);
  int cumulative_num_samples = ProcessSamples(&samples);

  // Collect UMA events for events that happen before the stateful partition is
  // mounted. Crash reporter will set the UMA events output directory since
  // the typical |uma_events_dir_| does not exist before stateful partition is
  // mounted.
  base::DirReaderPosix reader_early_metrics(uma_early_metrics_dir_.c_str());
  if (!reader_early_metrics.IsValid()) {
    LOG(ERROR) << "Failed to create DirReaderPosix. Cannot read early per-pid "
                  "uma files.";
  }

  while (reader_early_metrics.IsValid() && reader_early_metrics.Next()) {
    std::string filename(reader_early_metrics.name());
    if (filename == "." || filename == "..") {
      continue;
    }
    metrics::SerializationUtils::ReadAndDeleteMetricsFromFile(
        base::StrCat({uma_early_metrics_dir_, "/", filename}), &samples);
    cumulative_num_samples += ProcessSamples(&samples);
  }

  // Collect UMA events which happen after stateful partition is mounted.
  base::DirReaderPosix reader_metrics(uma_events_dir_.c_str());
  if (!reader_metrics.IsValid()) {
    LOG(ERROR)
        << "Failed to create DirReaderPosix. Cannot read per-pid uma files.";
  }

  while (reader_metrics.IsValid() && reader_metrics.Next()) {
    std::string filename(reader_metrics.name());
    if (filename == "." || filename == "..") {
      continue;
    }
    metrics::SerializationUtils::ReadAndDeleteMetricsFromFile(
        base::StrCat({uma_events_dir_, "/", filename}), &samples);
    cumulative_num_samples += ProcessSamples(&samples);
  }

  return cumulative_num_samples;
}

void ExternalMetrics::CollectEventsAndReschedule() {
  CollectEvents();
  ScheduleCollector();
}

void ExternalMetrics::ScheduleCollector() {
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ExternalMetrics::CollectEventsAndReschedule, this),
      collection_interval_);
}

}  // namespace ash
