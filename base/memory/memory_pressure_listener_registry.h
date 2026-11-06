// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_MEMORY_PRESSURE_LISTENER_REGISTRY_H_
#define BASE_MEMORY_MEMORY_PRESSURE_LISTENER_REGISTRY_H_

#include "base/base_export.h"
#include "base/callback_list.h"
#include "base/memory/memory_pressure_level.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"

namespace base {

// Dispatches the memory pressure signal of the MemoryPressureSource to the
// registered MemoryPressureListeners.
//
// The registry must be created before the MemoryPressureSource sends any
// notification, so that it observes every change of the memory pressure level.
// This is enforced by a CHECK.
class BASE_EXPORT MemoryPressureListenerRegistry {
 public:
  static bool Exists();
  static MemoryPressureListenerRegistry& Get();
  static MemoryPressureListenerRegistry* MaybeGet();

  MemoryPressureListenerRegistry();
  ~MemoryPressureListenerRegistry();

  // Forwards to MemoryPressureSource::SetMemoryPressureLevel(). Does nothing if
  // no MemoryPressureSource exists.
  static void NotifyMemoryPressure(MemoryPressureLevel memory_pressure_level);

  // Forwards to MemoryPressureSource::SetMemoryPressureLevelFromAnyThread().
  static void NotifyMemoryPressureFromAnyThread(
      MemoryPressureLevel memory_pressure_level);

  // Adds/removes a listener.
  void AddObserver(MemoryPressureListenerRegistration* listener);
  void RemoveObserver(MemoryPressureListenerRegistration* listener);

  // Forward to the MemoryPressureSource methods of the same name.
  static bool AreNotificationsSuppressed();
  static void SimulatePressureNotification(
      MemoryPressureLevel memory_pressure_level);

 private:
  // Invoked by the MemoryPressureSource. Calls OnMemoryPressure() on all
  // listeners.
  void OnMemoryPressure(MemoryPressureLevel memory_pressure_level);

  // The last memory pressure level sent to the listeners. Used to initialize
  // new listeners.
  MemoryPressureLevel last_memory_pressure_level_
      GUARDED_BY_CONTEXT(thread_checker_) = MEMORY_PRESSURE_LEVEL_NONE;

  ObserverList<MemoryPressureListenerRegistration>::Unchecked listeners_
      GUARDED_BY_CONTEXT(thread_checker_);

  CallbackListSubscription memory_pressure_subscription_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace base

#endif  // BASE_MEMORY_MEMORY_PRESSURE_LISTENER_REGISTRY_H_
