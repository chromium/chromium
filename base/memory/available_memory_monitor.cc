// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/available_memory_monitor.h"

#include "base/process/process_metrics.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base {

namespace {
constexpr base::TimeDelta kPollInterval = base::Seconds(2);
}

// static
AvailableMemoryMonitor* AvailableMemoryMonitor::Get() {
  static base::NoDestructor<AvailableMemoryMonitor> instance;
  return instance.get();
}

AvailableMemoryMonitor::AvailableMemoryMonitor() = default;

AvailableMemoryMonitor::~AvailableMemoryMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AvailableMemoryMonitor::AddObserver(Observer* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(obs);
  StartPolling();
}

void AvailableMemoryMonitor::RemoveObserver(Observer* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(obs);
  if (observers_.empty()) {
    StopPolling();
  }
}

std::optional<AvailableMemoryMonitor::MemorySample>
AvailableMemoryMonitor::GetLastSample() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_sample_;
}

void AvailableMemoryMonitor::StartPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timer_.IsRunning()) {
    return;
  }

  timer_.Start(FROM_HERE, kPollInterval, this,
               &AvailableMemoryMonitor::OnMemoryCheckTimer);
}

void AvailableMemoryMonitor::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();
  // Clear the cache to avoid serving stale data if monitoring restarts later.
  last_sample_.reset();
}

void AvailableMemoryMonitor::OnMemoryCheckTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<MemorySample> sample = ComputeAvailableMemory();
  if (!sample.has_value()) {
    // Reset the last sample, since it is now outdated or the OS call failed.
    last_sample_.reset();
    return;
  }

  last_sample_ = sample;

  for (Observer& obs : observers_) {
    obs.OnAvailableMemoryUpdated(*sample);
  }
}

std::optional<AvailableMemoryMonitor::MemorySample>
AvailableMemoryMonitor::ComputeAvailableMemory() {
  base::SystemMemoryInfo info;
  if (!base::GetSystemMemoryInfo(&info)) {
    return std::nullopt;
  }

  MemorySample sample;
  sample.available_physical_bytes = info.GetAvailablePhysicalMemory();

#if BUILDFLAG(IS_WIN)
  // On Windows, GetSystemMemoryInfo maps ullAvailPageFile to swap_free
  // and ullTotalPageFile to swap_total.
  sample.available_commit_bytes = info.swap_free;
  sample.total_commit_bytes = info.swap_total;
#endif

  sample.timestamp = base::TimeTicks::Now();
  return sample;
}

}  // namespace base
