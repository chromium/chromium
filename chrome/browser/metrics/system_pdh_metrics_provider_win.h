// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_SYSTEM_PDH_METRICS_PROVIDER_WIN_H_
#define CHROME_BROWSER_METRICS_SYSTEM_PDH_METRICS_PROVIDER_WIN_H_

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/scoped_pdh_query.h"
#include "base/win/windows_types.h"
#include "components/metrics/metrics_provider.h"

// Queries various PDH performance counters. Specifically, records the number of
// pages read from disk per second to satisfy hard faults, the % user vs kernel
// CPU time, and the % utilization of `C:\pagefile.sys`.
class SystemPdhMetricsProvider : public metrics::MetricsProvider {
 public:
  // Must be more than 1s as per
  // https://learn.microsoft.com/en-us/windows/win32/PerfCtrs/about-performance-counters.
  static constexpr base::TimeDelta kSamplingPeriod = base::Seconds(5);

  SystemPdhMetricsProvider();

  ~SystemPdhMetricsProvider() override;

  SystemPdhMetricsProvider(const SystemPdhMetricsProvider&) = delete;
  SystemPdhMetricsProvider& operator=(const SystemPdhMetricsProvider&) = delete;

  // MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;

  static constexpr std::string_view kHardFaultCountHistogram =
      "Memory.Experimental.Windows.HardFaultsFulfilledPerSecond";
  static constexpr std::string_view kDemandZeroFaultCountHistogram =
      "Memory.Experimental.Windows.DemandZeroFaultsFulfilledPerSecond2";
  static constexpr std::string_view kPagefileUtilizationHistogram =
      "Memory.Experimental.Windows.PagefileUtilization";
  static constexpr std::string_view kUserTimeHistogram =
      "CPU.Experimental.Windows.UserTime";
  static constexpr std::string_view kKernelTimeHistogram =
      "CPU.Experimental.Windows.KernelTime";
  static constexpr std::string_view kUserKernelRatioHistogram =
      "CPU.Experimental.Windows.UserKernelRatio";

 private:
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

   private:
    // Checks the case where a PDH function call failed and records debug
    // histograms. Returns `true` if the result is valid, `false` if it is
    // invalid and recording should stop.
    bool VerifyPdhResult(PDH_STATUS status, PDH_FMT_COUNTERVALUE* value);

    // Initialized during metric recording, and cleared when stopped.
    base::win::ScopedPdhQuery pdh_query_;

    // These 'handles' do not need to be freed. Their lifetime is associated
    // with pdh_query_. They are reinitialized every time recording is
    // enabled/disabled.
    PDH_HCOUNTER pages_input_per_second_ = {};
    PDH_HCOUNTER demand_zero_faults_per_second_ = {};
    PDH_HCOUNTER pagefile_utilization_ = {};
    PDH_HCOUNTER user_cpu_time_ = {};
    PDH_HCOUNTER kernel_cpu_time_ = {};

    // Used to Sample() on a timer.
    base::RepeatingTimer timer_;
  };

  base::SequenceBound<PdhQueryHandler> query_handler_;
};

#endif  // CHROME_BROWSER_METRICS_SYSTEM_PDH_METRICS_PROVIDER_WIN_H_
