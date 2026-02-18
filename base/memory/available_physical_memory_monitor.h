// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_AVAILABLE_PHYSICAL_MEMORY_MONITOR_H_
#define BASE_MEMORY_AVAILABLE_PHYSICAL_MEMORY_MONITOR_H_

#include <cstdint>
#include <optional>

#include "base/base_export.h"
#include "base/byte_size.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {

// A singleton monitor that periodically polls the system for available physical
// memory.
//
// This class optimizes resource usage by only running the polling timer when
// there are active observers.
//
// Threading: This class is not thread-safe. It must be accessed and used
// exclusively on the main thread.
class BASE_EXPORT AvailablePhysicalMemoryMonitor {
 public:
  // Represents a snapshot of system memory state at a specific point in time.
  struct MemorySample {
    base::TimeTicks timestamp;
    base::ByteSize available_bytes;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when a new memory sample is available.
    virtual void OnAvailableMemoryUpdated(const MemorySample& sample) = 0;
  };

  // Returns the singleton instance.
  static AvailablePhysicalMemoryMonitor* Get();

  AvailablePhysicalMemoryMonitor(const AvailablePhysicalMemoryMonitor&) =
      delete;
  AvailablePhysicalMemoryMonitor& operator=(
      const AvailablePhysicalMemoryMonitor&) = delete;

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
  AvailablePhysicalMemoryMonitor();
  virtual ~AvailablePhysicalMemoryMonitor();

  // Called periodically by |timer_| to update the cached memory state.
  virtual void OnMemoryCheckTimer();

  // Performs the actual system call to retrieve available physical memory.
  // Virtual for testing.
  virtual std::optional<base::ByteSize> ComputeAvailableMemory();

 private:
  friend class base::NoDestructor<AvailablePhysicalMemoryMonitor>;

  void StartPolling();
  void StopPolling();

  base::ObserverList<Observer> observers_;
  base::RepeatingTimer timer_;

  // Caches the last sample to support GetLastSample().
  std::optional<MemorySample> last_sample_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace base

#endif  // BASE_MEMORY_AVAILABLE_PHYSICAL_MEMORY_MONITOR_H_
