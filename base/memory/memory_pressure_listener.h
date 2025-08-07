// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryPressure provides static APIs for handling memory pressure on
// platforms that have such signals, such as Android and ChromeOS.
// The app will try to discard buffers that aren't deemed essential (individual
// modules will implement their own policy).

#ifndef BASE_MEMORY_MEMORY_PRESSURE_LISTENER_H_
#define BASE_MEMORY_MEMORY_PRESSURE_LISTENER_H_

#include "base/base_export.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/memory_pressure_level.h"
#include "base/threading/thread_checker.h"

namespace base {

// To start listening, create a new instance, passing a callback to a
// function that takes a MemoryPressureLevel parameter. To stop listening,
// simply delete the listener object. The implementation guarantees
// that the callback will always be called on the thread that created
// the listener.
//
// Note that even on the same thread, the MemoryPressureCallback will not be
// called within the system memory pressure broadcast. If synchronous
// invocation is desired, then SyncMemoryPressureListener must be used.
// However, deleting a listener with a synchronous callback from within a
// synchronous callback is not supported and will deadlock.
//
// Please see notes in MemoryPressureLevel enum below: some levels are
// absolutely critical, and if not enough memory is returned to the system,
// it'll potentially kill the app, and then later the app will have to be
// cold-started.
//
// Example:
//
//    void OnMemoryPressure(MemoryPressureLevel memory_pressure_level) {
//       ...
//    }
//
//    // Start listening.
//    auto listener = std::make_unique<MemoryPressureListener>(
//        base::BindRepeating(&OnMemoryPressure));
//
//    ...
//
//    // Stop listening.
//    listener.reset();
//
class BASE_EXPORT MemoryPressureListener {
 public:
  // MemoryPressureLevel used to be defined here instead of in
  // base/memory/memory_pressure_level.h. The using statements here avoids the
  // needs to refactor the whole codebase.
  using MemoryPressureLevel = MemoryPressureLevel;
  using enum MemoryPressureLevel;

  using MemoryPressureCallback = RepeatingCallback<void(MemoryPressureLevel)>;
  using SyncMemoryPressureCallback =
      RepeatingCallback<void(MemoryPressureLevel)>;

  MemoryPressureListener(
      const base::Location& creation_location,
      const MemoryPressureCallback& memory_pressure_callback);

  MemoryPressureListener(const MemoryPressureListener&) = delete;
  MemoryPressureListener& operator=(const MemoryPressureListener&) = delete;

  ~MemoryPressureListener();

  void Notify(MemoryPressureLevel memory_pressure_level);
  void SyncNotify(MemoryPressureLevel memory_pressure_level);

  // Intended for use by the platform specific implementation.
  // Note: This simply forwards the call to MemoryPressureListenerRegistry to
  // avoid the need to refactor the whole codebase.
  static void NotifyMemoryPressure(MemoryPressureLevel memory_pressure_level);

  // These methods should not be used anywhere else but in memory measurement
  // code, where they are intended to maintain stable conditions across
  // measurements.
  // Note: This simply forwards the call to MemoryPressureListenerRegistry to
  // avoid the need to refactor the whole codebase.
  static bool AreNotificationsSuppressed();
  static void SetNotificationsSuppressed(bool suppressed);
  static void SimulatePressureNotification(
      MemoryPressureLevel memory_pressure_level);

  bool has_sync_callback() const {
    return !sync_memory_pressure_callback_.is_null();
  }

 private:
  friend class SyncMemoryPressureListener;

  MemoryPressureListener(
      const base::Location& creation_location,
      const MemoryPressureCallback& memory_pressure_callback,
      const SyncMemoryPressureCallback& sync_memory_pressure_callback);

  const MemoryPressureCallback callback_;
  const SyncMemoryPressureCallback sync_memory_pressure_callback_;

  const base::Location creation_location_;
};

class BASE_EXPORT SyncMemoryPressureListener {
 public:
  using SyncMemoryPressureCallback =
      RepeatingCallback<void(MemoryPressureLevel)>;

  explicit SyncMemoryPressureListener(SyncMemoryPressureCallback callback);

  SyncMemoryPressureListener(const SyncMemoryPressureListener&) = delete;
  SyncMemoryPressureListener& operator=(const SyncMemoryPressureListener&) =
      delete;

  ~SyncMemoryPressureListener();

 private:
  void OnMemoryPressure(MemoryPressureLevel memory_pressure_level);

  SyncMemoryPressureCallback callback_;

  MemoryPressureListener memory_pressure_listener_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace base

#endif  // BASE_MEMORY_MEMORY_PRESSURE_LISTENER_H_
