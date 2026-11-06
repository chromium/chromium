// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MEMORY_PRESSURE_SOURCE_H_
#define BASE_MEMORY_COORDINATOR_MEMORY_PRESSURE_SOURCE_H_

#include <optional>

#include "base/base_export.h"
#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/memory_pressure_level.h"
#include "base/threading/thread_checker.h"
#include "base/types/pass_key.h"

namespace base {

class MemoryPressureListenerRegistry;

// MemoryPressureSource holds the memory pressure level of the process, as
// reported by the platform-specific monitors and other producers of the memory
// pressure signal, and notifies its subscribers of it.
//
// There is at most one instance per process. It is owned by the embedder and
// lives on the main thread. Unless noted otherwise, all methods must be called
// on the main thread.
//
// Subscribing to the memory pressure signal is restricted to the plumbing that
// propagates it (e.g. the memory coordinator). Code that wants to reduce its
// memory usage in response to memory pressure must implement
// base::MemoryConsumer instead.
class BASE_EXPORT MemoryPressureSource {
 public:
  using Callback = RepeatingCallback<void(MemoryPressureLevel)>;

  static bool Exists();
  static MemoryPressureSource& Get();
  static MemoryPressureSource* MaybeGet();

  MemoryPressureSource();

  MemoryPressureSource(const MemoryPressureSource&) = delete;
  MemoryPressureSource& operator=(const MemoryPressureSource&) = delete;

  ~MemoryPressureSource();

  // Sets the memory pressure level of the process, and notifies subscribers
  // unless notifications are suppressed. Intended for use by the producers of
  // the memory pressure signal.
  void SetMemoryPressureLevel(MemoryPressureLevel memory_pressure_level);

  // Same as SetMemoryPressureLevel(), but can be invoked from any thread. If
  // not called from the process's main thread, this will post a task to it.
  // Does nothing if no MemoryPressureSource exists.
  static void SetMemoryPressureLevelFromAnyThread(
      MemoryPressureLevel memory_pressure_level);

  // These methods should not be used anywhere else but in memory measurement
  // code, where they are intended to maintain stable conditions across
  // measurements. Tests should use ScopedMemoryPressureLevelOverride instead.
  static bool AreNotificationsSuppressed();
  static void SimulatePressureNotification(
      MemoryPressureLevel memory_pressure_level);

  // Registers `callback` to be invoked synchronously with the memory pressure
  // level whenever subscribers are notified. Repeated notifications of the same
  // level are delivered, except for MEMORY_PRESSURE_LEVEL_NONE. The current
  // level is not reported upon registration. Destroying the returned
  // subscription unregisters `callback`.
  //
  // Restricted to the plumbing of the memory pressure signal. See the class
  // comment.
  [[nodiscard]] CallbackListSubscription AddCallback(Callback callback);

  // Returns the level of the last notification sent to subscribers: the
  // simulated level while notifications are suppressed, the real level
  // otherwise.
  // TODO(406578344): Remove with MemoryPressureListenerRegistry.
  MemoryPressureLevel GetLastNotifiedMemoryPressureLevel(
      PassKey<MemoryPressureListenerRegistry>) const;

 private:
  friend class MemoryPressureSuppressionToken;

  static void IncreaseNotificationSuppressionCount();
  static void DecreaseNotificationSuppressionCount();

  // Invokes all callbacks with `memory_pressure_level`.
  void SendMemoryPressureNotification(
      MemoryPressureLevel memory_pressure_level);

  bool AreNotificationsSuppressedImpl();
  void IncreaseNotificationSuppressionCountImpl();
  void DecreaseNotificationSuppressionCountImpl();
  void SimulatePressureNotificationImpl(
      MemoryPressureLevel memory_pressure_level);

  MemoryPressureLevel last_memory_pressure_level_
      GUARDED_BY_CONTEXT(thread_checker_) = MEMORY_PRESSURE_LEVEL_NONE;

  // While regular memory pressure notifications are suppressed, it's still
  // possible to simulate memory pressure using
  // `SimulatePressureNotification()`.
  std::optional<MemoryPressureLevel> simulated_memory_pressure_level_
      GUARDED_BY_CONTEXT(thread_checker_);

  size_t notification_suppression_count_ GUARDED_BY_CONTEXT(thread_checker_) =
      0u;

  RepeatingCallbackList<void(MemoryPressureLevel)> callbacks_
      GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

// Used to suppress memory pressure notifications, as long as an instance of
// this class exists.
class BASE_EXPORT MemoryPressureSuppressionToken {
 public:
  MemoryPressureSuppressionToken();

  MemoryPressureSuppressionToken(const MemoryPressureSuppressionToken&) =
      delete;
  MemoryPressureSuppressionToken& operator=(
      const MemoryPressureSuppressionToken&) = delete;

  ~MemoryPressureSuppressionToken();
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_MEMORY_PRESSURE_SOURCE_H_
