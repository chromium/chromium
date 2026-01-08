// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/system_memory_list_metrics_provider_win.h"

#include <windows.h>
#include <winternl.h>

#include <optional>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"

SystemMemoryListMetricsProvider::SystemMemoryListMetricsProvider(
    base::TimeDelta sampling_interval,
    base::TimeDelta recording_interval)
    : exhausted_interval_thread_delegate_(*this,
                                          sampling_interval,
                                          recording_interval) {}

SystemMemoryListMetricsProvider::~SystemMemoryListMetricsProvider() {
  if (exhausted_interval_recording_thread_.has_value()) {
    exhausted_interval_thread_delegate_.SignalExit();
    exhausted_interval_recording_thread_->Join();
  }
}

// static
NTSTATUS SystemMemoryListMetricsProvider::QueryNtSystemMemoryListInformation(
    SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) {
  // Also sometimes called SYSTEM_MEMORY_LIST_COMMAND. See below for more
  // context on the call.
  static constexpr auto SystemMemoryListInformation =
      static_cast<SYSTEM_INFORMATION_CLASS>(80);
  return ::NtQuerySystemInformation(SystemMemoryListInformation,
                                    &memory_list_info, sizeof(memory_list_info),
                                    /*ReturnLength=*/nullptr);
}

void SystemMemoryListMetricsProvider::OnRecordingEnabled() {
  // Should not be called multiple times in a row.
  CHECK(!exhausted_interval_recording_thread_.has_value());
  exhausted_interval_recording_thread_.emplace(
      &exhausted_interval_thread_delegate_, "MemoryListSampler");
  exhausted_interval_recording_thread_->StartAsync();
}

void SystemMemoryListMetricsProvider::OnRecordingDisabled() {
  // Should be matched with a call to OnRecordingEnabled(), but can be called
  // without enabling recording in ChromeDriver tests.
  if (exhausted_interval_recording_thread_.has_value()) {
    exhausted_interval_thread_delegate_.SignalExit();
    exhausted_interval_recording_thread_->Join();

    // Clear the state for next time.
    exhausted_interval_recording_thread_ = std::nullopt;
    exhausted_interval_thread_delegate_.Reset();
  }
}

NTSTATUS SystemMemoryListMetricsProvider::GetSystemMemoryListInformation(
    SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info) {
  return QueryNtSystemMemoryListInformation(memory_list_info);
}

SystemMemoryListMetricsProvider::ExhaustedIntervalThreadDelegate::
    ExhaustedIntervalThreadDelegate(SystemMemoryListMetricsProvider& outer,
                                    base::TimeDelta sampling_interval,
                                    base::TimeDelta recording_interval)
    : outer_(outer),
      sampling_interval_(sampling_interval),
      recording_interval_(recording_interval) {}

void SystemMemoryListMetricsProvider::ExhaustedIntervalThreadDelegate::
    SignalExit() {
  exit_signal_.Signal();
}

void SystemMemoryListMetricsProvider::ExhaustedIntervalThreadDelegate::Reset() {
  exit_signal_.Reset();
}

void SystemMemoryListMetricsProvider::ExhaustedIntervalThreadDelegate::Run() {
  // Though we're querying the memory list size once every 100ms for the purpose
  // of recording "exhausted" intervals per thirty seconds, we only bump these
  // counters most of the time, and record the UMA histograms only once every
  // 30s, and then we reset these.
  int zero_list_exhausted_interval_count_ = 0;
  int free_list_exhausted_interval_count_ = 0;
  int total_intervals_recorded_ = 0;
  base::TimeTicks last_pressured_interval_emission_time_ =
      base::TimeTicks::Now();

  while (!exit_signal_.TimedWait(sampling_interval_)) {
    SYSTEM_MEMORY_LIST_INFORMATION memory_list_information;

    NTSTATUS status =
        outer_->GetSystemMemoryListInformation(memory_list_information);
    if (NT_SUCCESS(status)) {
      ++total_intervals_recorded_;
      if (memory_list_information.ZeroPageCount == 0) {
        ++zero_list_exhausted_interval_count_;
      }
      if (memory_list_information.FreePageCount == 0) {
        ++free_list_exhausted_interval_count_;
      }

      const base::TimeTicks now = base::TimeTicks::Now();
      if (last_pressured_interval_emission_time_ <=
          (now - recording_interval_)) {
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds."
            "ZeroList",
            zero_list_exhausted_interval_count_);
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds."
            "FreeList",
            free_list_exhausted_interval_count_);
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.TotalIntervalsRecorded",
            total_intervals_recorded_);
        // Record a massively subsampled record of page counts.
        base::UmaHistogramCustomCounts(
            "Memory.SystemMemoryLists.FreePageCount",
            base::saturated_cast<int>(memory_list_information.FreePageCount), 1,
            500000000, 75);
        base::UmaHistogramCustomCounts(
            "Memory.SystemMemoryLists.ZeroPageCount",
            base::saturated_cast<int>(memory_list_information.ZeroPageCount), 1,
            500000000, 75);
        base::UmaHistogramCustomCounts(
            "Memory.SystemMemoryLists.ModifiedPageCount",
            base::saturated_cast<int>(
                memory_list_information.ModifiedPageCount),
            1, 500000000, 75);

        free_list_exhausted_interval_count_ = 0;
        zero_list_exhausted_interval_count_ = 0;
        total_intervals_recorded_ = 0;
        last_pressured_interval_emission_time_ = now;
      }
    } else {
      // Record the status for the system call. NtQuerySystemInformation() can
      // return STATUS_ACCESS_DENIED in the case that
      // SeProfileSingleProcessPrivilege is not held by the user, but that's
      // fine since the subset of users which have this permission should still
      // be a representative sample.
      base::UmaHistogramSparse("Memory.SystemMemoryLists.QueryFailureStatus",
                               status);

      // Exit the thread on error to not spam the API for no reason.
      return;
    }
  }
}
