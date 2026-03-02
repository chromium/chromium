// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/system_memory_list_metrics_provider_win.h"

#include <windows.h>
#include <winternl.h>

#include <stdint.h>

#include <optional>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/profiler/sample_metadata.h"
#include "base/strings/string_number_conversions.h"
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
  bool last_zero_interval_was_exhausted = false;
  bool last_free_interval_was_exhausted = false;
  bool last_standby_interval_was_exhausted = false;
  int zero_list_exhausted_interval_count = 0;
  int free_list_exhausted_interval_count = 0;
  int standby_list_exhausted_interval_count = 0;
  int all_lists_exhausted_interval_count = 0;

  // An interval qualifies as "low" if the size of the list is less than 2MiB,
  // which is the size of one NT 'large page', the unit at which the memory
  // manager works.
  static constexpr int kLowMemoryPageThreshold = 512;
  bool last_zero_interval_was_low = false;
  bool last_free_interval_was_low = false;
  bool last_standby_interval_was_low = false;
  int zero_list_low_interval_count = 0;
  int free_list_low_interval_count = 0;
  int standby_list_low_interval_count = 0;
  int all_lists_low_interval_count = 0;

  int total_intervals_recorded = 0;
  base::TimeTicks last_pressured_interval_emission_time =
      base::TimeTicks::Now();

  base::SampleMetadata zero_page_sample_metadata{
      "WindowsZeroPageCount", base::SampleMetadataScope::kProcess};

  base::SampleMetadata available_page_sample_metadata{
      "WindowsAvailablePageCount", base::SampleMetadataScope::kProcess};

  while (!exit_signal_.TimedWait(sampling_interval_)) {
    SYSTEM_MEMORY_LIST_INFORMATION memory_list_information;

    NTSTATUS status =
        outer_->GetSystemMemoryListInformation(memory_list_information);
    if (NT_SUCCESS(status)) {
      ++total_intervals_recorded;

      // Since the total number of standby pages isn't accessible in the data
      // structure, sum the size of each list here.
      uintptr_t total_standby_pages = 0;
      for (uintptr_t standby_page_count :
           memory_list_information.PageCountByPriority) {
        total_standby_pages += standby_page_count;
      }

      // Record the number of zero pages in SSM metadata.
      zero_page_sample_metadata.Set(memory_list_information.ZeroPageCount);
      // Record the number of pages accessible in all "available" pages
      // (according to the Windows Research Kernel) in SSM metadata.
      available_page_sample_metadata.Set(memory_list_information.ZeroPageCount +
                                         memory_list_information.FreePageCount +
                                         total_standby_pages);

      // Exhausted Intervals:
      // All lists:
      if (last_free_interval_was_exhausted &&
          last_zero_interval_was_exhausted &&
          last_standby_interval_was_exhausted &&
          memory_list_information.ZeroPageCount == 0 &&
          memory_list_information.FreePageCount == 0 &&
          total_standby_pages == 0) {
        ++all_lists_exhausted_interval_count;
      }
      // Zero:
      if (memory_list_information.ZeroPageCount != 0) {
        last_zero_interval_was_exhausted = false;
      } else if (std::exchange(last_zero_interval_was_exhausted, true)) {
        ++zero_list_exhausted_interval_count;
      }
      // Free:
      if (memory_list_information.FreePageCount != 0) {
        last_free_interval_was_exhausted = false;
      } else if (std::exchange(last_free_interval_was_exhausted, true)) {
        ++free_list_exhausted_interval_count;
      }
      // Standby:
      if (total_standby_pages != 0) {
        last_standby_interval_was_exhausted = false;
      } else if (std::exchange(last_standby_interval_was_exhausted, true)) {
        ++standby_list_exhausted_interval_count;
      }

      // Low Intervals:
      // All lists:
      if (last_free_interval_was_low && last_zero_interval_was_low &&
          last_standby_interval_was_low &&
          memory_list_information.ZeroPageCount +
                  memory_list_information.FreePageCount + total_standby_pages <=
              kLowMemoryPageThreshold) {
        ++all_lists_low_interval_count;
      }
      // Zero:
      if (memory_list_information.ZeroPageCount != 0) {
        last_zero_interval_was_low = false;
      } else if (std::exchange(last_zero_interval_was_low, true)) {
        ++zero_list_low_interval_count;
      }
      // Free:
      if (memory_list_information.FreePageCount != 0) {
        last_free_interval_was_low = false;
      } else if (std::exchange(last_free_interval_was_low, true)) {
        ++free_list_low_interval_count;
      }
      // Standby:
      if (total_standby_pages != 0) {
        last_standby_interval_was_low = false;
      } else if (std::exchange(last_standby_interval_was_low, true)) {
        ++standby_list_low_interval_count;
      }

      const base::TimeTicks now = base::TimeTicks::Now();
      if (last_pressured_interval_emission_time <=
          (now - recording_interval_)) {
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds."
            "ZeroList",
            zero_list_exhausted_interval_count);
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds."
            "FreeList",
            free_list_exhausted_interval_count);
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds."
            "StandbyList",
            standby_list_exhausted_interval_count);
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.ExhaustedIntervalsPerThirtySeconds."
            "AllLists",
            all_lists_exhausted_interval_count);
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.LowIntervalsPerThirtySeconds."
            "ZeroList",
            zero_list_low_interval_count);
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.LowIntervalsPerThirtySeconds."
            "FreeList",
            free_list_low_interval_count);
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.LowIntervalsPerThirtySeconds."
            "StandbyList",
            standby_list_low_interval_count);
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.LowIntervalsPerThirtySeconds."
            "AllLists",
            all_lists_low_interval_count);
        base::UmaHistogramCounts1000(
            "Memory.SystemMemoryLists.TotalIntervalsRecorded",
            total_intervals_recorded);
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

        int priority_number = 1;
        for (uintptr_t standby_page_count :
             memory_list_information.PageCountByPriority) {
          base::UmaHistogramCustomCounts(
              base::StrCat(
                  {"Memory.SystemMemoryLists.StandbyPageCountByPriority.",
                   base::NumberToString(priority_number++)}),
              standby_page_count, 1, 500000000, 75);
        }
        base::UmaHistogramCustomCounts(
            "Memory.SystemMemoryLists.StandbyPageCount", total_standby_pages, 1,
            500000000, 75);

        free_list_exhausted_interval_count = 0;
        zero_list_exhausted_interval_count = 0;
        standby_list_exhausted_interval_count = 0;
        all_lists_exhausted_interval_count = 0;
        free_list_low_interval_count = 0;
        zero_list_low_interval_count = 0;
        standby_list_low_interval_count = 0;
        all_lists_low_interval_count = 0;
        total_intervals_recorded = 0;
        last_pressured_interval_emission_time = now;
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
      break;
    }
  }
  zero_page_sample_metadata.Remove();
  available_page_sample_metadata.Remove();
}
