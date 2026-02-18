// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/available_physical_memory_monitor.h"

#include "base/process/process_metrics.h"

namespace base {

namespace {
constexpr base::TimeDelta kPollInterval = base::Seconds(2);
}

// static
AvailablePhysicalMemoryMonitor* AvailablePhysicalMemoryMonitor::Get() {
  static base::NoDestructor<AvailablePhysicalMemoryMonitor> instance;
  return instance.get();
}

AvailablePhysicalMemoryMonitor::AvailablePhysicalMemoryMonitor() = default;

AvailablePhysicalMemoryMonitor::~AvailablePhysicalMemoryMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AvailablePhysicalMemoryMonitor::AddObserver(Observer* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(obs);
  StartPolling();
}

void AvailablePhysicalMemoryMonitor::RemoveObserver(Observer* obs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(obs);
  if (observers_.empty()) {
    StopPolling();
  }
}

std::optional<AvailablePhysicalMemoryMonitor::MemorySample>
AvailablePhysicalMemoryMonitor::GetLastSample() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_sample_;
}

void AvailablePhysicalMemoryMonitor::StartPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timer_.IsRunning()) {
    return;
  }

  timer_.Start(FROM_HERE, kPollInterval, this,
               &AvailablePhysicalMemoryMonitor::OnMemoryCheckTimer);
}

void AvailablePhysicalMemoryMonitor::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();
  // Clear the cache to avoid serving stale data if monitoring restarts later.
  last_sample_.reset();
}

void AvailablePhysicalMemoryMonitor::OnMemoryCheckTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<base::ByteSize> available_memory = ComputeAvailableMemory();
  if (!available_memory.has_value()) {
    // Reset the last sample, since it is now outdated.
    last_sample_.reset();
    return;
  }

  MemorySample sample;
  sample.timestamp = base::TimeTicks::Now();
  sample.available_bytes = *available_memory;
  last_sample_ = sample;

  for (Observer& obs : observers_) {
    obs.OnAvailableMemoryUpdated(sample);
  }
}

std::optional<base::ByteSize>
AvailablePhysicalMemoryMonitor::ComputeAvailableMemory() {
  // This is a stateless wrapper around a system call and does not
  // require a sequence check.
  base::SystemMemoryInfo info;
  if (!base::GetSystemMemoryInfo(&info)) {
    return std::nullopt;
  }
  return info.GetAvailablePhysicalMemory();
}

}  // namespace base
