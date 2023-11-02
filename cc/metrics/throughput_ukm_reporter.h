// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_THROUGHPUT_UKM_REPORTER_H_
#define CC_METRICS_THROUGHPUT_UKM_REPORTER_H_

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cc {
class UkmManager;

enum class AggregationType {
  kAllAnimations,
  kAllInteractions,
  kAllSequences,
};

// A helper class that takes throughput data from a FrameSequenceTracker and
// talk to UkmManager to report it.
class CC_EXPORT ThroughputUkmReporter {
 public:
  explicit ThroughputUkmReporter(UkmManager* ukm_manager);
  ~ThroughputUkmReporter();

  ThroughputUkmReporter(const ThroughputUkmReporter&) = delete;
  ThroughputUkmReporter& operator=(const ThroughputUkmReporter&) = delete;

  void ReportThroughputUkm(const absl::optional<int>& slower_throughput_percent,
                           const absl::optional<int>& impl_throughput_percent,
                           const absl::optional<int>& main_throughput_percent,
                           FrameSequenceTrackerType type);

  void ReportAggregateThroughput(AggregationType aggregation_type,
                                 int throughput);

  uint32_t GetSamplesToNextEventForTesting(int index);

 private:
  // Sampling control. We sample the event here to not throttle the UKM system.
  // Currently, the same sampling rate is applied to all existing trackers. We
  // might want to iterate on this based on the collected data.
  uint32_t samples_to_next_event_[static_cast<int>(
      FrameSequenceTrackerType::kMaxType)] = {0};
  uint32_t samples_for_aggregated_report_ = 0;

  // This is pointing to the LayerTreeHostImpl::ukm_manager_, which is
  // initialized right after the LayerTreeHostImpl is created. So when this
  // pointer is initialized, there should be no trackers yet.
  const raw_ptr<UkmManager> ukm_manager_;
};

}  // namespace cc

#endif  // CC_METRICS_THROUGHPUT_UKM_REPORTER_H_
