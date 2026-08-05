// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/system_pdh_metrics_provider_win.h"

#include <windows.h>

#include <iterator>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/win/pdh_shim.h"
#include "base/win/scoped_pdh_query.h"
#include "chrome/browser/browser_features.h"
#include "content/public/common/child_process_id.h"

namespace {

struct CounterDefinition {
  std::wstring_view counter_name;
  std::string_view uma_name;
  DWORD format;
};

constexpr CounterDefinition kProcessCounterDefinitions[] = {
    {L"% User Time", "UserTime", PDH_FMT_DOUBLE},
    {L"% Privileged Time", "PrivilegedTime", PDH_FMT_DOUBLE},
    {L"Handle Count", "HandleCount", PDH_FMT_LONG},
    {L"IO Data Bytes/sec", "IODataBytesPerSec", PDH_FMT_LARGE},
    {L"IO Data Operations/sec", "IODataOperationsPerSec", PDH_FMT_LONG},
    {L"IO Other Bytes/sec", "IOOtherBytesPerSec", PDH_FMT_LARGE},
    {L"IO Read Bytes/sec", "IOReadBytesPerSec", PDH_FMT_LARGE},
    {L"IO Read Operations/sec", "IOReadOperationsPerSec", PDH_FMT_LONG},
    {L"IO Write Bytes/sec", "IOWriteBytesPerSec", PDH_FMT_LARGE},
    {L"IO Write Operations/sec", "IOWriteOperationsPerSec", PDH_FMT_LONG},
    {L"Page Faults/sec", "PageFaultsPerSec", PDH_FMT_LONG},
    {L"Page File Bytes", "PageFileBytes", PDH_FMT_LARGE},
    {L"Page File Bytes Peak", "PageFileBytesPeak", PDH_FMT_LARGE},
    {L"Private Bytes", "PrivateBytes", PDH_FMT_LARGE},
    {L"Thread Count", "ThreadCount", PDH_FMT_LONG},
    {L"Working Set", "WorkingSet", PDH_FMT_LARGE},
    {L"Working Set - Private", "WorkingSetPrivate", PDH_FMT_LARGE},
    {L"Working Set Peak", "WorkingSetPeak", PDH_FMT_LARGE},
};

}  // namespace

namespace features {

// When enabled, the browser process will register the Pdh metrics provider and
// will listen to system-wide, and per-process (Process V2) Pdh counters in the
// browser, network service, GPU, and a subset of renderer and utility
// processes.
BASE_FEATURE(kSystemPdhMetrics, base::FEATURE_DISABLED_BY_DEFAULT);

// The downsampling ratio at which generic renderer/utility processes will be
// recorded.
const base::FeatureParam<int> kSystemPdhMetrics_DownsamplingFactor{
    &kSystemPdhMetrics, "system_pdh_metrics_downsampling_factor", 20};

// The period at which system Pdh metrics are sampled. Must be more than 1s as
// per
// https://learn.microsoft.com/en-us/windows/win32/PerfCtrs/about-performance-counters.
//
// The cost per sample does not increase as the sampling period goes up, so a
// long sampling period can reduce the user-perceived cost.
const base::FeatureParam<base::TimeDelta> kSystemPdhMetrics_SamplingPeriod{
    &kSystemPdhMetrics, "system_pdh_metrics_sampling_period",
    base::Seconds(30)};

const base::FeatureParam<int> kSystemPdhMetrics_MetricsPerProcess{
    &kSystemPdhMetrics, "system_pdh_metrics_metrics_per_process",
    std::size(kProcessCounterDefinitions)};

}  // namespace features

SystemPdhMetricsProvider::SystemPdhMetricsProvider() = default;
SystemPdhMetricsProvider::~SystemPdhMetricsProvider() = default;

void SystemPdhMetricsProvider::OnRecordingEnabled() {
  // The task runner is BEST_EFFORT because they can be delayed without much
  // consequence, MUST_USE_FOREGROUND to avoid priority inversions with the
  // DLL loader lock, and CONTINUE_ON_SHUTDOWN to avoid blocking shutdown if
  // they hang.
  query_handler_.emplace(base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::MUST_USE_FOREGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

  process_observer_ = std::make_unique<metrics::MetricsProviderProcessObserver>(
      this, features::kSystemPdhMetrics_DownsamplingFactor.Get());
}

void SystemPdhMetricsProvider::OnRecordingDisabled() {
  process_observer_.reset();
  query_handler_.Reset();
}

void SystemPdhMetricsProvider::StartListeningToProcess(
    content::ChildProcessId content_id,
    base::ProcessId pid,
    std::string_view process_type_suffix) {
  query_handler_
      .AsyncCall(
          &SystemPdhMetricsProvider::PdhQueryHandler::StartListeningToProcess)
      .WithArgs(content_id, pid, process_type_suffix);
}

void SystemPdhMetricsProvider::StopListeningToProcess(
    content::ChildProcessId content_id) {
  query_handler_
      .AsyncCall(
          &SystemPdhMetricsProvider::PdhQueryHandler::StopListeningToProcess)
      .WithArgs(content_id);
}

SystemPdhMetricsProvider::PdhQueryHandler::PdhQueryHandler()
    : pdh_query_(base::win::ScopedPdhQuery::Create()) {
  // Any early return from this function will not start the timer, meaning that
  // if these calls fail, the metrics will not be recorded until
  // OnRecordingEnabled() is called once again.
  if (!pdh_query_.is_valid()) {
    return;
  }

  timer_.Start(FROM_HERE, features::kSystemPdhMetrics_SamplingPeriod.Get(),
               this, &SystemPdhMetricsProvider::PdhQueryHandler::Sample);
}

SystemPdhMetricsProvider::PdhQueryHandler::~PdhQueryHandler() = default;

void SystemPdhMetricsProvider::PdhQueryHandler::StopRecording() {
  timer_.Stop();
  process_counters_.clear();
  pdh_query_.reset();
}

SystemPdhMetricsProvider::PdhQueryHandler::ProcessCounter::ProcessCounter(
    base::win::ScopedPdhQuery& query,
    std::wstring_view instance_name,
    std::wstring_view process_counter_name,
    std::string_view uma_name,
    std::string_view process_type_suffix,
    DWORD format)
    : uma_name_(uma_name),
      process_type_suffix_(process_type_suffix),
      counter_handle_(nullptr),
      format_(format) {
  if (!query.is_valid()) {
    return;
  }
  auto path = base::StrCat(
      {L"\\Process V2(", instance_name, L")\\", process_counter_name});
  counter_handle_ = ScopedPdhCounter::Create(query.get(), path);
  // It's possible the process has already died, so don't kill the entire
  // metrics provider for a failure here.
}

SystemPdhMetricsProvider::PdhQueryHandler::ProcessCounter::~ProcessCounter() =
    default;

SystemPdhMetricsProvider::PdhQueryHandler::ProcessCounter::ProcessCounter(
    ProcessCounter&&) = default;

SystemPdhMetricsProvider::PdhQueryHandler::ProcessCounter&
SystemPdhMetricsProvider::PdhQueryHandler::ProcessCounter::operator=(
    ProcessCounter&&) = default;

void SystemPdhMetricsProvider::PdhQueryHandler::ProcessCounter::Record() {
  if (!counter_handle_.is_valid()) {
    return;
  }

  // Since counters can't be observed until they are recorded twice, only
  // observe on the next Record() call.
  if (sampling_state_ == SamplingState::kNoBaseline) {
    sampling_state_ = SamplingState::kHasBaseline;
    return;
  }

  PDH_FMT_COUNTERVALUE process_value;
  PDH_STATUS query_status =
      ::PdhGetFormattedCounterValue(counter_handle_.get(), format_,
                                    /*lpdwType=*/nullptr, &process_value);
  if (query_status == ERROR_SUCCESS &&
      (process_value.CStatus == PDH_CSTATUS_VALID_DATA ||
       process_value.CStatus == PDH_CSTATUS_NEW_DATA)) {
    static constexpr std::string_view kPrefix(
        "Windows.Experimental.Pdh.ProcessV2.");
    auto histogram_name =
        base::StrCat({kPrefix, uma_name_, ".", process_type_suffix_});
    if (sampling_state_ == SamplingState::kHasBaseline) {
      sampling_state_ = SamplingState::kSteadyState;
      base::StrAppend(&histogram_name, {".FirstSample"});
    }
    switch (format_) {
      case PDH_FMT_DOUBLE:
        base::UmaHistogramPercentage(
            histogram_name, base::ClampRound(process_value.doubleValue));
        break;

      case PDH_FMT_LARGE:
        base::UmaHistogramCustomCounts(histogram_name, process_value.largeValue,
                                       1, 1000000000, 50);
        break;

      case PDH_FMT_LONG:
        base::UmaHistogramCounts100000(histogram_name, process_value.longValue);
        break;

      default:
        NOTREACHED();
    }
  } else if (query_status != ERROR_SUCCESS) {
    counter_handle_.reset();
    base::UmaHistogramSparse(base::win::ScopedPdhQuery::kQueryErrorHistogram,
                             query_status);
  } else if (process_value.CStatus != PDH_CSTATUS_VALID_DATA &&
             process_value.CStatus != PDH_CSTATUS_NEW_DATA) {
    // This has never been observed to occur in months of real world data, so if
    // it does, something is likely wrong with the query, and it can be
    // discarded.
    counter_handle_.reset();
  }
}

void SystemPdhMetricsProvider::PdhQueryHandler::StartListeningToProcess(
    content::ChildProcessId content_id,
    base::ProcessId pid,
    std::string_view process_type_suffix) {
  auto [it, inserted] = process_counters_.try_emplace(content_id);
  if (!inserted) {
    // Already tracking this process.
    return;
  }

  // Format the instance name as expected by "Process V2".
  std::wstring instance_name =
      base::StrCat({process_base_name_, L":", base::NumberToWString(pid)});
  const int metrics_per_process =
      features::kSystemPdhMetrics_MetricsPerProcess.Get();
  const size_t num_definitions = std::size(kProcessCounterDefinitions);

  if (metrics_per_process <= 0 ||
      static_cast<size_t>(metrics_per_process) > num_definitions) {
    return;
  }

  std::vector<size_t> indices(num_definitions);
  std::iota(indices.begin(), indices.end(), 0);
  base::RandomShuffle(indices.begin(), indices.end());
  indices.resize(metrics_per_process);
  base::span<const CounterDefinition> definitions(kProcessCounterDefinitions);

  it->second.reserve(indices.size());
  for (size_t index : indices) {
    it->second.emplace_back(pdh_query_, instance_name,
                            definitions[index].counter_name,
                            definitions[index].uma_name, process_type_suffix,
                            definitions[index].format);
  }
}

void SystemPdhMetricsProvider::PdhQueryHandler::StopListeningToProcess(
    content::ChildProcessId content_id) {
  process_counters_.erase(content_id);
}

void SystemPdhMetricsProvider::PdhQueryHandler::Sample() {
  CHECK(pdh_query_.is_valid());

  PDH_STATUS status = ::PdhCollectQueryData(pdh_query_.get());
  if (!VerifyPdhResult(status, nullptr)) {
    return;
  }

  for (auto& [pid, counters] : process_counters_) {
    for (auto& counter : counters) {
      counter.Record();
    }
  }
}

bool SystemPdhMetricsProvider::PdhQueryHandler::VerifyPdhResult(
    PDH_STATUS status,
    PDH_FMT_COUNTERVALUE* value) {
  if (status != ERROR_SUCCESS) {
    base::UmaHistogramSparse(base::win::ScopedPdhQuery::kQueryErrorHistogram,
                             status);
    StopRecording();
    // Do not check `value` if the query itself failed.
    return false;
  }

  // Only check `value` if it is present.
  if (value && value->CStatus != PDH_CSTATUS_VALID_DATA &&
      value->CStatus != PDH_CSTATUS_NEW_DATA) {
    StopRecording();
    return false;
  }

  return true;
}
