// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_AVAILABLE_MEMORY_MONITOR_H_
#define BASE_MEMORY_AVAILABLE_MEMORY_MONITOR_H_

#include <optional>

#include "base/base_export.h"
#include "base/byte_size.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"

namespace base {

// A singleton monitor that periodically polls the system for available memory.
//
// This class optimizes resource usage by only running the polling timer when
// there are active observers.
//
// Threading: This class is not thread-safe. It must be accessed and used
// exclusively on the main thread.
class BASE_EXPORT AvailableMemoryMonitor {
 public:
  // Represents a snapshot of system memory state at a specific point in time.
  struct MemorySample {
    base::TimeTicks timestamp;
    base::ByteSize available_physical_bytes;

#if BUILDFLAG(IS_WIN)
    // The following commit metrics are retrieved via the Windows MEMORYSTATUSEX
    // API. For details on the underlying fields, see:
    // https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-memorystatusex

    // The maximum amount of memory the current process can commit. This is
    // usually the system-wide available commit space, but can be smaller if
    // the process is subject to a Job object quota (ullAvailPageFile).
    base::ByteSize available_commit_bytes;

    // The current committed memory limit for the system or the current process,
    // whichever is smaller (ullTotalPageFile).
    base::ByteSize total_commit_bytes;
#endif
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when a new memory sample is available.
    virtual void OnAvailableMemoryUpdated(const MemorySample& sample) = 0;
  };

  // Returns the singleton instance.
  static AvailableMemoryMonitor* Get();

  AvailableMemoryMonitor(const AvailableMemoryMonitor&) = delete;
  AvailableMemoryMonitor& operator=(const AvailableMemoryMonitor&) = delete;

  // Adds an observer to the monitor. Starts the polling timer if it is not
  // currently running.
  //
  // Note: This does NOT trigger an immediate callback to the observer. If the
  // observer needs the current state immediately upon registration, it should
  // call `GetLastSample()`.
  void AddObserver(Observer* obs);

  // Removes an observer from the monitor. Stops the polling timer if there are
  // no remaining observers.
  void RemoveObserver(Observer* obs);

  // Returns the most recently computed memory sample, or std::nullopt if
  // monitoring has not yet started or completed its first check.
  std::optional<MemorySample> GetLastSample() const;

 protected:
  // Protected for testing.
  AvailableMemoryMonitor();
  virtual ~AvailableMemoryMonitor();

  // Called periodically by |timer_| to update the cached memory state.
  virtual void OnMemoryCheckTimer();

  // Performs the actual system call to retrieve available memory.
  // Virtual for testing.
  virtual std::optional<MemorySample> ComputeAvailableMemory();

 private:
  friend class base::NoDestructor<AvailableMemoryMonitor>;

  void StartPolling();
  void StopPolling();

  base::ObserverList<Observer> observers_;
  base::RepeatingTimer timer_;

  // Caches the last sample to support GetLastSample().
  std::optional<MemorySample> last_sample_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace base

#endif  // BASE_MEMORY_AVAILABLE_MEMORY_MONITOR_H_
