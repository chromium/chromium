// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TASK_INFO_METRICS_PROVIDER_MAC_H_
#define CHROME_BROWSER_METRICS_TASK_INFO_METRICS_PROVIDER_MAC_H_

#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_handle.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/metrics/metrics_provider_process_observer.h"
#include "components/metrics/metrics_provider.h"
#include "content/public/common/child_process_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace features {

BASE_DECLARE_FEATURE(kTaskInfoMetricsMac);
extern const base::FeatureParam<int> kTaskInfoMetricsMac_DownsamplingFactor;
extern const base::FeatureParam<base::TimeDelta>
    kTaskInfoMetricsMac_SamplingPeriod;

}  // namespace features

// Queries macOS process task info (proc_taskinfo) for performance counters.
// Specifically, records per-second throughput and sampled metrics for various
// Chrome processes.
class TaskInfoMetricsProviderMac
    : public metrics::MetricsProvider,
      public metrics::MetricsProviderProcessObserver::Delegate {
 public:
  TaskInfoMetricsProviderMac();
  ~TaskInfoMetricsProviderMac() override;

  TaskInfoMetricsProviderMac(const TaskInfoMetricsProviderMac&) = delete;
  TaskInfoMetricsProviderMac& operator=(const TaskInfoMetricsProviderMac&) =
      delete;

  // metrics::MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;

  // metrics::MetricsProviderProcessObserver::Delegate:
  void StartListeningToProcess(content::ChildProcessId content_id,
                               base::ProcessId pid,
                               std::string_view process_type_suffix) override;
  void StopListeningToProcess(content::ChildProcessId content_id) override;

  class QueryHandler {
   public:
    QueryHandler();
    ~QueryHandler();
    QueryHandler(const QueryHandler&) = delete;
    QueryHandler& operator=(const QueryHandler&) = delete;

    void StopRecording();

    void Sample();

    void StartListeningToProcess(content::ChildProcessId content_id,
                                 base::ProcessId pid,
                                 std::string_view process_type_suffix);
    void StopListeningToProcess(content::ChildProcessId content_id);

   private:
    struct ProcessState {
      ProcessState(base::ProcessId pid, std::string_view process_type_suffix);
      ~ProcessState();
      ProcessState(const ProcessState&) = delete;
      ProcessState& operator=(const ProcessState&) = delete;
      ProcessState(ProcessState&&);
      ProcessState& operator=(ProcessState&&);

      void Record();

      // Tracks the metric recording lifecycle for a process. Because rate
      // and delta counters require two snapshots over time to compute a value,
      // metric emission progresses through three distinct stages.
      enum class SamplingState : uint8_t {
        // No baseline snapshot has been recorded yet. The next Record() call
        // will take the initial measurement without emitting a UMA histogram.
        kNoBaseline,
        // An initial baseline snapshot exists, but no UMA histogram has been
        // emitted yet. The next Record() call will emit the initial metric
        // with a ".FirstSample" suffix attached to the histogram name.
        kHasBaseline,
        // Steady-state sampling is active. Subsequent Record() calls will
        // emit standard UMA histograms without any special suffix.
        kSteadyState,
      };

      std::string_view process_type_suffix;
      base::TimeTicks last_sample_time;
      base::ProcessId pid;
      uint32_t last_faults = 0;
      uint32_t last_pageins = 0;
      uint32_t last_cow_faults = 0;
      uint32_t last_csw = 0;
      SamplingState sampling_state = SamplingState::kNoBaseline;
    };

    absl::flat_hash_map<content::ChildProcessId, ProcessState> process_states_;
    base::RepeatingTimer timer_;
  };

  base::SequenceBound<QueryHandler>& GetQueryHandlerForTesting() {
    return query_handler_;
  }

 private:
  base::SequenceBound<QueryHandler> query_handler_;
  std::unique_ptr<metrics::MetricsProviderProcessObserver> process_observer_;
};

#endif  // CHROME_BROWSER_METRICS_TASK_INFO_METRICS_PROVIDER_MAC_H_
