// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_SYSTEM_PDH_METRICS_PROVIDER_WIN_H_
#define CHROME_BROWSER_METRICS_SYSTEM_PDH_METRICS_PROVIDER_WIN_H_

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/scoped_generic.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/scoped_pdh_query.h"
#include "base/win/windows_types.h"
#include "chrome/browser/browser_features.h"
#include "components/metrics/metrics_provider.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace features {

BASE_DECLARE_FEATURE(kSystemPdhMetrics);
extern const base::FeatureParam<int> kSystemPdhMetrics_DownsamplingFactor;
extern const base::FeatureParam<base::TimeDelta>
    kSystemPdhMetrics_SamplingPeriod;

}  // namespace features

// Queries various PDH performance counters. Specifically, records various
// per-process metrics such as CPU usage and IO operations.
class SystemPdhMetricsProvider : public metrics::MetricsProvider {
 public:
  SystemPdhMetricsProvider();

  ~SystemPdhMetricsProvider() override;

  SystemPdhMetricsProvider(const SystemPdhMetricsProvider&) = delete;
  SystemPdhMetricsProvider& operator=(const SystemPdhMetricsProvider&) = delete;

  // MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;

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

    void StartListeningToProcessPdhMetrics(content::ChildProcessId content_id,
                                           base::ProcessId pid,
                                           std::string process_type_name);
    void StopListeningToProcessPdhMetrics(content::ChildProcessId pid);

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
                     std::string process_type_suffix,
                     DWORD format);
      ~ProcessCounter();
      ProcessCounter(const ProcessCounter&) = delete;
      ProcessCounter operator=(const ProcessCounter&) = delete;

      void Record();

     private:
      ScopedPdhCounter counter_handle_;

      // Since it takes two queries to record data in the counters, record
      // whether more than one instance of the given counters have been
      // recorded.
      bool first_sample_ = true;

      // The first sample after the baseline (first_sample_ = false) will be
      // recorded with a ".FirstSample" suffix.
      bool first_emission_ = true;

      const std::string uma_name_;
      const std::string process_type_suffix_;
      const DWORD format_;
    };

    using ProcessCounterArray = std::array<ProcessCounter, 18>;
    absl::flat_hash_map<content::ChildProcessId,
                        std::unique_ptr<ProcessCounterArray>>
        process_counters_;

    // Initialized during metric recording, and cleared when stopped.
    base::win::ScopedPdhQuery pdh_query_;

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
  class ProcessMetricsObserver;
  base::SequenceBound<PdhQueryHandler> query_handler_;
  std::unique_ptr<ProcessMetricsObserver> process_observer_;
};

#endif  // CHROME_BROWSER_METRICS_SYSTEM_PDH_METRICS_PROVIDER_WIN_H_
