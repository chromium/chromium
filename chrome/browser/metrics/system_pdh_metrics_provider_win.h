// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_SYSTEM_PDH_METRICS_PROVIDER_WIN_H_
#define CHROME_BROWSER_METRICS_SYSTEM_PDH_METRICS_PROVIDER_WIN_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/scoped_generic.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/scoped_pdh_query.h"
#include "base/win/windows_types.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/metrics/metrics_provider_process_observer.h"
#include "components/metrics/metrics_provider.h"
#include "content/public/common/child_process_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace features {

BASE_DECLARE_FEATURE(kSystemPdhMetrics);
extern const base::FeatureParam<int> kSystemPdhMetrics_DownsamplingFactor;
extern const base::FeatureParam<base::TimeDelta>
    kSystemPdhMetrics_SamplingPeriod;
extern const base::FeatureParam<int> kSystemPdhMetrics_MetricsPerProcess;

}  // namespace features

// Queries various PDH performance counters. Specifically, records various
// per-process metrics such as CPU usage and IO operations.
class SystemPdhMetricsProvider
    : public metrics::MetricsProvider,
      public metrics::MetricsProviderProcessObserver::Delegate {
 public:
  SystemPdhMetricsProvider();

  ~SystemPdhMetricsProvider() override;

  SystemPdhMetricsProvider(const SystemPdhMetricsProvider&) = delete;
  SystemPdhMetricsProvider& operator=(const SystemPdhMetricsProvider&) = delete;

  // MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;

  // metrics::MetricsProviderProcessObserver::Delegate:
  void StartListeningToProcess(content::ChildProcessId content_id,
                               base::ProcessId pid,
                               std::string_view process_type_suffix) override;
  void StopListeningToProcess(content::ChildProcessId content_id) override;

  class PdhQueryHandler {
   public:
    // Initializes the Pdh query with the counters of interest, and begins
    // sampling periodically.
    PdhQueryHandler();
    ~PdhQueryHandler();
    PdhQueryHandler(const PdhQueryHandler&) = delete;
    PdhQueryHandler operator=(const PdhQueryHandler&) = delete;

    // Cancels all pending recurring callbacks and resets the Pdh query. Used
    // when Pdh calls return errors.
    void StopRecording();

    // Samples each of the counters from the Pdh query, and records these to
    // UMA. Called on intervals of kSamplingPeriod.
    void Sample();

    void StartListeningToProcess(content::ChildProcessId content_id,
                                 base::ProcessId pid,
                                 std::string_view process_type_suffix);
    void StopListeningToProcess(content::ChildProcessId content_id);

   private:
    // Checks the case where a PDH function call failed and records debug
    // histograms. Returns `true` if the result is valid, `false` if it is
    // invalid and recording should stop.
    bool VerifyPdhResult(PDH_STATUS status, PDH_FMT_COUNTERVALUE* value);

    struct ScopedPdhCounterTraits {
      static PDH_HQUERY InvalidValue() { return nullptr; }
      static void Free(PDH_HCOUNTER counter) {
        if (counter) {
          ::PdhRemoveCounter(counter);
        }
      }
    };
    class ScopedPdhCounter
        : public base::ScopedGeneric<PDH_HCOUNTER, ScopedPdhCounterTraits> {
     public:
      explicit ScopedPdhCounter(PDH_HCOUNTER counter_handle)
          : ScopedGeneric(counter_handle) {}

      static ScopedPdhCounter Create(PDH_HQUERY query,
                                     const std::wstring& name) {
        PDH_HCOUNTER counter;
        NTSTATUS status = ::PdhAddEnglishCounter(query, name.c_str(),
                                                 /*dwUserData=*/0, &counter);
        if (status == ERROR_SUCCESS) {
          return ScopedPdhCounter(counter);
        }
        base::UmaHistogramSparse(
            base::win::ScopedPdhQuery::kQueryErrorHistogram, status);
        return ScopedPdhCounter(nullptr);
      }
    };

    class ProcessCounter {
     public:
      ProcessCounter(base::win::ScopedPdhQuery& query,
                     std::wstring_view instance_name,
                     std::wstring_view process_counter_name,
                     std::string_view base_name,
                     std::string_view process_type_suffix,
                     DWORD format);
      ~ProcessCounter();
      ProcessCounter(const ProcessCounter&) = delete;
      ProcessCounter& operator=(const ProcessCounter&) = delete;
      ProcessCounter(ProcessCounter&&);
      ProcessCounter& operator=(ProcessCounter&&);
      void Record();

     private:
      std::string uma_name_;
      std::string process_type_suffix_;
      ScopedPdhCounter counter_handle_;
      DWORD format_;
      // Tracks the metric recording lifecycle for a process counter. Because
      // rate and delta counters require two snapshots over time to compute a
      // value, metric emission progresses through three distinct stages.
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
      SamplingState sampling_state_ = SamplingState::kNoBaseline;
    };

    // Initialized during metric recording, and cleared when stopped.
    base::win::ScopedPdhQuery pdh_query_;

    // Must be destroyed before `pdh_query_`.
    absl::flat_hash_map<content::ChildProcessId, std::vector<ProcessCounter>>
        process_counters_;

    // This is the name without extension of the current exe binary name. For
    // example, assuming that the current process is chrome.exe, this returns
    // 'chrome'. This is needed because of the format of the Pdh counter
    // instances which are composed of this string.
    const std::wstring process_base_name_{
        base::PathService::CheckedGet(base::FILE_EXE)
            .BaseName()
            .RemoveExtension()
            .value()};

    // Used to Sample() on a timer.
    base::RepeatingTimer timer_;
  };

  base::SequenceBound<PdhQueryHandler>& GetQueryHandlerForTesting() {
    return query_handler_;
  }

 private:
  base::SequenceBound<PdhQueryHandler> query_handler_;
  std::unique_ptr<metrics::MetricsProviderProcessObserver> process_observer_;
};

#endif  // CHROME_BROWSER_METRICS_SYSTEM_PDH_METRICS_PROVIDER_WIN_H_
