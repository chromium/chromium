// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/system_pdh_metrics_provider_win.h"

#include <windows.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/win/pdh_shim.h"
#include "base/win/scoped_pdh_query.h"

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
}

void SystemPdhMetricsProvider::OnRecordingDisabled() {
  query_handler_.Reset();
}

SystemPdhMetricsProvider::PdhQueryHandler::PdhQueryHandler() {
  // Any early return from this function will not start the timer, meaning that
  // if these calls fail, the metrics will not be recorded until
  // OnRecordingEnabled() is called once again.

  // Do not reinitialize if the query is already initialized.
  if (pdh_query_.is_valid()) {
    return;
  }

  pdh_query_ = base::win::ScopedPdhQuery::Create();

  if (!pdh_query_.is_valid()) {
    return;
  }

  // Pages Input/sec is the rate at which pages are read from disk to resolve
  // hard page faults, system wide. Hard page faults occur when a process refers
  // to a page in virtual memory that is not in its working set or elsewhere in
  // physical memory, and must be retrieved from disk.
  static constexpr wchar_t kPagesInputPerSecond[] =
      L"\\Memory\\Pages Input/sec";
  PDH_STATUS status =
      ::PdhAddEnglishCounter(pdh_query_.get(), kPagesInputPerSecond,
                             /*dwUserData=*/0, &pages_input_per_second_);
  if (!VerifyPdhResult(status, nullptr)) {
    return;
  }

  // Demand Zero Faults/sec is the rate at which page faults which must be
  // fulfilled with a zero page are demanded from the operating system, system
  // wide. Demand zero faults occur whenever a zero page must be provided, which
  // includes every single private memory allocation.
  static constexpr wchar_t kDemandZeroFaultsPerSecond[] =
      L"\\Memory\\Demand Zero Faults/sec";
  status =
      ::PdhAddEnglishCounter(pdh_query_.get(), kDemandZeroFaultsPerSecond,
                             /*dwUserData=*/0, &demand_zero_faults_per_second_);
  if (!VerifyPdhResult(status, nullptr)) {
    return;
  }

  // The amount of the Page File in use in percent.
  static constexpr wchar_t kPagingFileUsage[] =
      L"\\Paging File(_Total)\\% Usage";
  status = ::PdhAddEnglishCounter(pdh_query_.get(), kPagingFileUsage,
                                  /*dwUserData=*/0, &pagefile_utilization_);
  if (!VerifyPdhResult(status, nullptr)) {
    return;
  }

  // % Privileged Time is the percentage of elapsed time since the previous
  // sample that all CPU cores spent executing code in privileged mode (i.e. in
  // the kernel or in drivers), system wide. Does not include the idle process.
  static constexpr wchar_t kKernelTime[] =
      L"\\Processor(_Total)\\% Privileged Time";
  status = ::PdhAddEnglishCounter(pdh_query_.get(), kKernelTime,
                                  /*dwUserData=*/0, &kernel_cpu_time_);
  if (!VerifyPdhResult(status, nullptr)) {
    return;
  }

  // % User Time is the percentage of elapsed time since the previous sample all
  // CPU cores spent in user mode.
  static constexpr wchar_t kUserTime[] = L"\\Processor(_Total)\\% User Time";
  status = ::PdhAddEnglishCounter(pdh_query_.get(), kUserTime, /*dwUserData=*/0,
                                  &user_cpu_time_);
  if (!VerifyPdhResult(status, nullptr)) {
    return;
  }

  // The first time data is collected, it cannot be observed, since the counters
  // are an average throughput over time, and thus only acquire meaning after 2+
  // samples (>1s apart).
  status = ::PdhCollectQueryData(pdh_query_.get());
  if (!VerifyPdhResult(status, nullptr)) {
    return;
  }

  timer_.Start(FROM_HERE, kSamplingPeriod, this,
               &SystemPdhMetricsProvider::PdhQueryHandler::Sample);
}

SystemPdhMetricsProvider::PdhQueryHandler::~PdhQueryHandler() = default;

void SystemPdhMetricsProvider::PdhQueryHandler::StopRecording() {
  timer_.Stop();
  pdh_query_.reset();
}

void SystemPdhMetricsProvider::PdhQueryHandler::Sample() {
  CHECK(pdh_query_.is_valid());

  PDH_STATUS status = ::PdhCollectQueryData(pdh_query_.get());

  if (!VerifyPdhResult(status, nullptr)) {
    return;
  }

  // Hard fault counts are absolute and can be seen in LONG.
  PDH_FMT_COUNTERVALUE counter_value;
  status = ::PdhGetFormattedCounterValue(pages_input_per_second_, PDH_FMT_LONG,
                                         nullptr, &counter_value);
  if (!VerifyPdhResult(status, &counter_value)) {
    return;
  }
  base::UmaHistogramCounts100000(kHardFaultCountHistogram,
                                 counter_value.longValue);

  // Demand zero fault counts are absolute and can be seen in LONG.
  status = ::PdhGetFormattedCounterValue(demand_zero_faults_per_second_,
                                         PDH_FMT_LONG, nullptr, &counter_value);
  if (!VerifyPdhResult(status, &counter_value)) {
    return;
  }
  base::UmaHistogramCounts10M(kDemandZeroFaultCountHistogram,
                              counter_value.longValue);

  // Since pagefile utilization is a percentage in the range [0,100], read it as
  // double, and ClampRound it to an integer.
  status = ::PdhGetFormattedCounterValue(pagefile_utilization_, PDH_FMT_DOUBLE,
                                         nullptr, &counter_value);
  if (!VerifyPdhResult(status, &counter_value)) {
    return;
  }
  base::UmaHistogramPercentage(kPagefileUtilizationHistogram,
                               base::ClampRound(counter_value.doubleValue));

  // Since kernel and user CPU time is a percentage in the range [0,100], we can
  // use it as is.
  PDH_FMT_COUNTERVALUE kernel_value;
  status = ::PdhGetFormattedCounterValue(kernel_cpu_time_, PDH_FMT_DOUBLE,
                                         nullptr, &kernel_value);
  if (!VerifyPdhResult(status, &kernel_value)) {
    return;
  }
  base::UmaHistogramPercentage(kKernelTimeHistogram,
                               base::ClampRound(kernel_value.doubleValue));

  PDH_FMT_COUNTERVALUE user_value;
  status = ::PdhGetFormattedCounterValue(user_cpu_time_, PDH_FMT_DOUBLE,
                                         nullptr, &user_value);
  if (!VerifyPdhResult(status, &user_value)) {
    return;
  }
  base::UmaHistogramPercentage(kUserTimeHistogram,
                               base::ClampRound(user_value.doubleValue));

  base::UmaHistogramPercentage(
      kUserKernelRatioHistogram,
      base::ClampRound<int, double>(base::ClampMul(
          100.0, base::ClampDiv(user_value.doubleValue,
                                base::ClampAdd(user_value.doubleValue,
                                               kernel_value.doubleValue)))));
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
    base::UmaHistogramSparse(base::win::ScopedPdhQuery::kResultErrorHistogram,
                             value->CStatus);
    StopRecording();
    return false;
  }

  return true;
}
