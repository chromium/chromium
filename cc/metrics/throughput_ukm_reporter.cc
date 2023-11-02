// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/throughput_ukm_reporter.h"

#include "cc/trees/ukm_manager.h"

namespace cc {

namespace {
// Collect UKM once per kNumberOfSamplesToReport UMA reports.
constexpr unsigned kNumberOfSamplesToReport = 100u;
}  // namespace

ThroughputUkmReporter::ThroughputUkmReporter(UkmManager* ukm_manager)
    : ukm_manager_(ukm_manager) {
  DCHECK(ukm_manager_);
}

ThroughputUkmReporter::~ThroughputUkmReporter() = default;

void ThroughputUkmReporter::ReportThroughputUkm(
    const absl::optional<int>& slower_throughput_percent,
    const absl::optional<int>& impl_throughput_percent,
    const absl::optional<int>& main_throughput_percent,
    FrameSequenceTrackerType type) {
  // It is possible that when a tab shuts down, the ukm_manager_ owned by the
  // LayerTreeHostImpl is cleared, and yet we try to report to UKM here. In this
  // case, the |ukm_manager_| here is null.
  if (!ukm_manager_)
    return;
  if (samples_to_next_event_[static_cast<int>(type)] == 0) {
    // Sample every 100 events. If a tracker reports UMA every 5s, then the
    // system collects UKM once per 100*5 = 500 seconds. This number may need to
    // be tuned to not throttle the UKM system.
    samples_to_next_event_[static_cast<int>(type)] = kNumberOfSamplesToReport;
    if (impl_throughput_percent) {
      ukm_manager_->RecordThroughputUKM(
          type, FrameInfo::SmoothEffectDrivingThread::kCompositor,
          impl_throughput_percent.value());
    }
    if (main_throughput_percent) {
      ukm_manager_->RecordThroughputUKM(
          type, FrameInfo::SmoothEffectDrivingThread::kMain,
          main_throughput_percent.value());
    }
  }
  DCHECK_GT(samples_to_next_event_[static_cast<int>(type)], 0u);
  samples_to_next_event_[static_cast<int>(type)]--;
}

void ThroughputUkmReporter::ReportAggregateThroughput(
    AggregationType aggregation_type,
    int throughput) {
  if (samples_for_aggregated_report_ == 0) {
    samples_for_aggregated_report_ = kNumberOfSamplesToReport;
    ukm_manager_->RecordAggregateThroughput(aggregation_type, throughput);
  }
  DCHECK_GT(samples_for_aggregated_report_, 0u);
  --samples_for_aggregated_report_;
}

uint32_t ThroughputUkmReporter::GetSamplesToNextEventForTesting(int index) {
  DCHECK_LT(index, static_cast<int>(FrameSequenceTrackerType::kMaxType));
  return samples_to_next_event_[index];
}

}  // namespace cc
