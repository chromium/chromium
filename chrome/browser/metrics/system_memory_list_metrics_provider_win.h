// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_SYSTEM_MEMORY_LIST_METRICS_PROVIDER_WIN_H_
#define CHROME_BROWSER_METRICS_SYSTEM_MEMORY_LIST_METRICS_PROVIDER_WIN_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "base/win/windows_types.h"
#include "components/metrics/metrics_provider.h"

struct SYSTEM_MEMORY_LIST_INFORMATION {
  uintptr_t ZeroPageCount;
  uintptr_t FreePageCount;
  uintptr_t ModifiedPageCount;
  uintptr_t ModifiedNoWritePageCount;
  uintptr_t BadPageCount;
  uintptr_t PageCountByPriority[8];
  uintptr_t RepurposedPagesByPriority[8];
  uintptr_t ModifiedPageCountPageFile;
};

// Manages the lifetime of the ExhaustedIntervalMetricsProvider thread, which
// samples the size of the Windows kernel system memory partition lists every
// hundred milliseconds for the purposes of metrics recording.
class SystemMemoryListMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit SystemMemoryListMetricsProvider(
      base::TimeDelta sampling_interval = base::Milliseconds(100),
      base::TimeDelta histogram_emission_interval = base::Seconds(30));

  ~SystemMemoryListMetricsProvider() override;

  SystemMemoryListMetricsProvider(const SystemMemoryListMetricsProvider&) =
      delete;
  SystemMemoryListMetricsProvider& operator=(
      const SystemMemoryListMetricsProvider&) = delete;

  // Sets the fields in `memory_list_info` with the counts of pages in the
  // various lists. This call can fail. Returns the status of the
  // NtQuerySystemInformation() call. Can be called on any thread. This is
  // public for use in other Chromium subsystems.
  //
  // This API is undocumented. See ReactOS/PHNT/pinvoke/geoffchappell for
  // documentation on the call, also see MEMINFO events in ETW (via `tracerpt`),
  // because they contain this same info. Also see Windows Internals, 7th
  // edition, Chapter 5, section on `KernelObjects\MemoryPartition0`, because
  // this is the internal mechanism by which we're making the query. For the
  // definition of the various page lists, see Windows Internals, 7th edition,
  // table 5-19.
  static NTSTATUS QueryNtSystemMemoryListInformation(
      SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info);

  // MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;

 private:
  // Gets system memory list size. This is virtual as a unittesting hook.
  // Returns the status of the NtQuerySystemInformation() call. Can be called on
  // any thread.
  virtual NTSTATUS GetSystemMemoryListInformation(
      SYSTEM_MEMORY_LIST_INFORMATION& memory_list_info);

  // Run the memory list exhausted interval sampler as its own thread to
  // increase its resiliency to memory exhaustion by reducing its footprint per
  // iteration (constant stack load, mostly use local variables, ditch the
  // massive Chromium task overhead, permit it to run while the main thread is
  // stalled).
  class ExhaustedIntervalThreadDelegate
      : public base::DelegateSimpleThread::Delegate {
   public:
    explicit ExhaustedIntervalThreadDelegate(
        SystemMemoryListMetricsProvider& outer,
        base::TimeDelta sampling_interval_,
        base::TimeDelta recording_interval_);
    void Run() override;

    // This thread will run until this function is called. It must be called
    // before destruction of the thread and delegate.
    void SignalExit();

    // Reset the state of the delegate to permit the sampling thread to restart.
    void Reset();

   private:
    const raw_ref<SystemMemoryListMetricsProvider> outer_;
    const base::TimeDelta sampling_interval_;
    const base::TimeDelta recording_interval_;
    base::WaitableEvent exit_signal_;
  };

  // The thread implementation for the ExhaustedIntervalMetricsProvider.
  ExhaustedIntervalThreadDelegate exhausted_interval_thread_delegate_;
  std::optional<base::DelegateSimpleThread>
      exhausted_interval_recording_thread_ = std::nullopt;
};

#endif  // CHROME_BROWSER_METRICS_SYSTEM_MEMORY_LIST_METRICS_PROVIDER_WIN_H_
