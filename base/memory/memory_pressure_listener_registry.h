// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_MEMORY_PRESSURE_LISTENER_REGISTRY_H_
#define BASE_MEMORY_MEMORY_PRESSURE_LISTENER_REGISTRY_H_

#include "base/base_export.h"
#include "base/memory/memory_pressure_level.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/observer_list.h"
#include "base/observer_list_threadsafe.h"

namespace base {

// This class is thread safe and internally synchronized.
class BASE_EXPORT MemoryPressureListenerRegistry {
 public:
  MemoryPressureListenerRegistry();
  // There is at most one MemoryPressureListenerRegistry and it is never
  // deleted.
  ~MemoryPressureListenerRegistry() = delete;

  // Gets the shared MemoryPressureListenerRegistry singleton instance.
  static MemoryPressureListenerRegistry& Get();

  // Intended for use by the platform specific implementation.
  static void NotifyMemoryPressure(MemoryPressureLevel memory_pressure_level);

  // Same as NotifyMemoryPressure, but can be invoked from any thread. If not
  // called from the process's main thread, this will post a task to it.
  static void NotifyMemoryPressureFromAnyThread(
      MemoryPressureLevel memory_pressure_level);

  void AddObserver(SyncMemoryPressureListenerRegistration* listener);

  void RemoveObserver(SyncMemoryPressureListenerRegistration* listener);

  // These methods should not be used anywhere else but in memory measurement
  // code, where they are intended to maintain stable conditions across
  // measurements.
  static bool AreNotificationsSuppressed();
  static void SetNotificationsSuppressed(bool suppressed);
  static void SimulatePressureNotification(
      MemoryPressureLevel memory_pressure_level);
  static void SimulatePressureNotificationAsync(
      MemoryPressureLevel memory_pressure_level);

 private:
  void DoNotifyMemoryPressure(MemoryPressureLevel memory_pressure_level);

  base::MemoryPressureLevel last_memory_pressure_level_ =
      base::MEMORY_PRESSURE_LEVEL_NONE;

  base::ObserverList<SyncMemoryPressureListenerRegistration>::Unchecked
      listeners_;
};

}  // namespace base

#endif  // BASE_MEMORY_MEMORY_PRESSURE_LISTENER_REGISTRY_H_
