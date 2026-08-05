// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/task_info_metrics_provider_mac.h"

#include <libproc.h>
#include <sys/proc_info.h>

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/process/process_handle.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/common/child_process_id.h"

namespace features {

BASE_FEATURE(kTaskInfoMetricsMac, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kTaskInfoMetricsMac_DownsamplingFactor{
    &kTaskInfoMetricsMac, "task_info_metrics_mac_downsampling_factor", 20};

const base::FeatureParam<base::TimeDelta> kTaskInfoMetricsMac_SamplingPeriod{
    &kTaskInfoMetricsMac, "task_info_metrics_mac_sampling_period",
    base::Seconds(30)};

}  // namespace features

TaskInfoMetricsProviderMac::TaskInfoMetricsProviderMac() = default;
TaskInfoMetricsProviderMac::~TaskInfoMetricsProviderMac() = default;

void TaskInfoMetricsProviderMac::OnRecordingEnabled() {
  query_handler_.emplace(base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

  process_observer_ = std::make_unique<metrics::MetricsProviderProcessObserver>(
      this, features::kTaskInfoMetricsMac_DownsamplingFactor.Get());
}

void TaskInfoMetricsProviderMac::OnRecordingDisabled() {
  process_observer_.reset();
  // If recording is disabled and re-enabled in quick succession, a new
  // QueryHandler could be created while the previous instance is still shutting
  // down asynchronously on its own distinct sequence. This is not problematic
  // from a resource perspective as they do not share state, and not problematic
  // from a global resource perspective, because their kernel lock contention is
  // extremely minimal. In any case, this problem is more or less theoretical,
  // because recording is not enabled/disabled in quick enough succession.
  query_handler_.Reset();
}

void TaskInfoMetricsProviderMac::StartListeningToProcess(
    content::ChildProcessId content_id,
    base::ProcessId pid,
    std::string_view process_type_suffix) {
  if (query_handler_) {
    query_handler_
        .AsyncCall(
            &TaskInfoMetricsProviderMac::QueryHandler::StartListeningToProcess)
        .WithArgs(content_id, pid, process_type_suffix);
  }
}

void TaskInfoMetricsProviderMac::StopListeningToProcess(
    content::ChildProcessId content_id) {
  if (query_handler_) {
    query_handler_
        .AsyncCall(
            &TaskInfoMetricsProviderMac::QueryHandler::StopListeningToProcess)
        .WithArgs(content_id);
  }
}

TaskInfoMetricsProviderMac::QueryHandler::QueryHandler() {
  timer_.Start(FROM_HERE, features::kTaskInfoMetricsMac_SamplingPeriod.Get(),
               this, &TaskInfoMetricsProviderMac::QueryHandler::Sample);
}

TaskInfoMetricsProviderMac::QueryHandler::~QueryHandler() = default;

void TaskInfoMetricsProviderMac::QueryHandler::StopRecording() {
  timer_.Stop();
  process_states_.clear();
}

void TaskInfoMetricsProviderMac::QueryHandler::StartListeningToProcess(
    content::ChildProcessId content_id,
    base::ProcessId pid,
    std::string_view process_type_suffix) {
  process_states_.try_emplace(content_id, pid, process_type_suffix);
}

void TaskInfoMetricsProviderMac::QueryHandler::StopListeningToProcess(
    content::ChildProcessId content_id) {
  process_states_.erase(content_id);
}

void TaskInfoMetricsProviderMac::QueryHandler::Sample() {
  for (auto& [id, state] : process_states_) {
    state.Record();
  }
}

TaskInfoMetricsProviderMac::QueryHandler::ProcessState::ProcessState(
    base::ProcessId pid,
    std::string_view process_type_suffix)
    : process_type_suffix(process_type_suffix),
      last_sample_time(base::TimeTicks::Now()),
      pid(pid) {}

TaskInfoMetricsProviderMac::QueryHandler::ProcessState::~ProcessState() =
    default;

TaskInfoMetricsProviderMac::QueryHandler::ProcessState::ProcessState(
    ProcessState&&) = default;

TaskInfoMetricsProviderMac::QueryHandler::ProcessState&
TaskInfoMetricsProviderMac::QueryHandler::ProcessState::operator=(
    ProcessState&&) = default;

void TaskInfoMetricsProviderMac::QueryHandler::ProcessState::Record() {
  struct proc_taskinfo info = {};
  int bytes_returned =
      proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &info, sizeof(info));
  if (bytes_returned <= 0 ||
      static_cast<size_t>(bytes_returned) != sizeof(info)) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  double elapsed_sec = (now - last_sample_time).InSecondsF();
  if (sampling_state != SamplingState::kNoBaseline && elapsed_sec <= 0) {
    return;
  }

  uint32_t current_faults = base::saturated_cast<uint32_t>(info.pti_faults);
  uint32_t current_pageins = base::saturated_cast<uint32_t>(info.pti_pageins);
  uint32_t current_cow_faults =
      base::saturated_cast<uint32_t>(info.pti_cow_faults);
  uint32_t current_csw = base::saturated_cast<uint32_t>(info.pti_csw);

  uint32_t delta_faults = current_faults - last_faults;
  uint32_t delta_pageins = current_pageins - last_pageins;
  uint32_t delta_cow_faults = current_cow_faults - last_cow_faults;
  uint32_t delta_csw = current_csw - last_csw;

  last_faults = current_faults;
  last_pageins = current_pageins;
  last_cow_faults = current_cow_faults;
  last_csw = current_csw;
  last_sample_time = now;

  if (sampling_state == SamplingState::kNoBaseline) {
    sampling_state = SamplingState::kHasBaseline;
    return;
  }

  std::string_view first_sample_suffix =
      (sampling_state == SamplingState::kHasBaseline) ? ".FirstSample"
                                                      : std::string_view();
  sampling_state = SamplingState::kSteadyState;

  // Throughput metrics (per second)
  base::UmaHistogramCounts100000(
      base::StrCat({"Mac.Experimental.Process.PageFaultsPerSec.",
                    process_type_suffix, first_sample_suffix}),
      base::ClampRound(delta_faults / elapsed_sec));
  base::UmaHistogramCounts100000(
      base::StrCat({"Mac.Experimental.Process.MajorPageFaultsPerSec.",
                    process_type_suffix, first_sample_suffix}),
      base::ClampRound(delta_pageins / elapsed_sec));
  base::UmaHistogramCounts100000(
      base::StrCat({"Mac.Experimental.Process.CowFaultsPerSec.",
                    process_type_suffix, first_sample_suffix}),
      base::ClampRound(delta_cow_faults / elapsed_sec));
  base::UmaHistogramCounts10M(
      base::StrCat({"Mac.Experimental.Process.ContextSwitchesPerSec.",
                    process_type_suffix, first_sample_suffix}),
      base::ClampRound(delta_csw / elapsed_sec));

  base::UmaHistogramMemoryMB(
      base::StrCat({"Mac.Experimental.Process.VirtualSize.",
                    process_type_suffix, first_sample_suffix}),
      static_cast<int>(info.pti_virtual_size / (1024 * 1024)));
  base::UmaHistogramMemoryMB(
      base::StrCat({"Mac.Experimental.Process.ResidentSize.",
                    process_type_suffix, first_sample_suffix}),
      static_cast<int>(info.pti_resident_size / (1024 * 1024)));

  int ratio = 0;
  if (info.pti_virtual_size > 0) {
    ratio = base::saturated_cast<int>(
        (base::CheckMul(info.pti_resident_size, 100) / info.pti_virtual_size)
            .ValueOrDefault(0));
  }
  base::UmaHistogramPercentage(
      base::StrCat({"Mac.Experimental.Process.ResidentVirtualRatio.",
                    process_type_suffix, first_sample_suffix}),
      ratio);

  base::UmaHistogramCounts100000(
      base::StrCat({"Mac.Experimental.Process.ThreadCount.",
                    process_type_suffix, first_sample_suffix}),
      info.pti_threadnum);
  base::UmaHistogramCounts100000(
      base::StrCat({"Mac.Experimental.Process.RunningThreadCount.",
                    process_type_suffix, first_sample_suffix}),
      info.pti_numrunning);
}
