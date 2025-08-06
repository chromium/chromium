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

  void AddObserver(MemoryPressureListener* listener, bool sync);

  void RemoveObserver(MemoryPressureListener* listener);

  // These methods should not be used anywhere else but in memory measurement
  // code, where they are intended to maintain stable conditions across
  // measurements.
  static bool AreNotificationsSuppressed();
  static void SetNotificationsSuppressed(bool suppressed);
  static void SimulatePressureNotification(
      MemoryPressureLevel memory_pressure_level);

 private:
  void DoNotifyMemoryPressure(MemoryPressureLevel memory_pressure_level);

  const scoped_refptr<ObserverListThreadSafe<MemoryPressureListener>>
      async_observers_ =
          base::MakeRefCounted<ObserverListThreadSafe<MemoryPressureListener>>(
              ObserverListPolicy::EXISTING_ONLY);
  ObserverList<MemoryPressureListener>::Unchecked sync_observers_
      GUARDED_BY(sync_observers_lock_);
  Lock sync_observers_lock_;
};

}  // namespace base

#endif  // BASE_MEMORY_MEMORY_PRESSURE_LISTENER_REGISTRY_H_
