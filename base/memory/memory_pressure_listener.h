// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryPressure provides static APIs for handling memory pressure on
// platforms that have such signals, such as Android and ChromeOS.
// The app will try to discard buffers that aren't deemed essential (individual
// modules will implement their own policy).

#ifndef BASE_MEMORY_MEMORY_PRESSURE_LISTENER_H_
#define BASE_MEMORY_MEMORY_PRESSURE_LISTENER_H_

#include <memory>

#include "base/base_export.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/memory_pressure_level.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_checker.h"

namespace base {

class SingleThreadTaskRunner;

// To start listening, create a new instance, passing a callback to a
// function that takes a MemoryPressureLevel parameter. To stop listening,
// simply delete the listener object. The implementation guarantees
// that the callback will always be called on the thread that created
// the listener.
//
// Note that even on the same thread, the MemoryPressureCallback will not be
// called within the system memory pressure broadcast. If synchronous
// invocation is desired, then SyncMemoryPressureListener must be used. This
// version is notified synchronously, but it must live on the main thread.
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

// Synchronous version of MemoryPressureListener. Must live on the main thread.
class BASE_EXPORT SyncMemoryPressureListener {
 public:
  using MemoryPressureCallback = RepeatingCallback<void(MemoryPressureLevel)>;

  explicit SyncMemoryPressureListener(
      MemoryPressureCallback memory_pressure_callback);

  SyncMemoryPressureListener(const SyncMemoryPressureListener&) = delete;
  SyncMemoryPressureListener& operator=(const SyncMemoryPressureListener&) =
      delete;

  ~SyncMemoryPressureListener();

  void Notify(MemoryPressureLevel memory_pressure_level);

 private:
  MemoryPressureCallback memory_pressure_callback_
      GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

class BASE_EXPORT MemoryPressureListener {
 public:
  // MemoryPressureLevel used to be defined here instead of in
  // base/memory/memory_pressure_level.h. The using statements here avoids the
  // needs to refactor the whole codebase.
  using MemoryPressureLevel = MemoryPressureLevel;
  using enum MemoryPressureLevel;

  using MemoryPressureCallback = RepeatingCallback<void(MemoryPressureLevel)>;

  MemoryPressureListener(const Location& creation_location,
                         MemoryPressureCallback memory_pressure_callback);

  MemoryPressureListener(const MemoryPressureListener&) = delete;
  MemoryPressureListener& operator=(const MemoryPressureListener&) = delete;

  ~MemoryPressureListener();

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
  // Invokes `SimulatePressureNotification` asynchronously on the main thread,
  // ensuring that any pending registration tasks have completed by the time it
  // runs.
  static void SimulatePressureNotificationAsync(
      MemoryPressureLevel memory_pressure_level);

 private:
  class MainThread;

  void Notify(MemoryPressureLevel memory_pressure_level);

  const MemoryPressureCallback memory_pressure_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Handle to the main thread's task runner. This is cached because it might no
  // longer be registered at the time this instance is destroyed.
  scoped_refptr<SingleThreadTaskRunner> main_thread_task_runner_;

  // Parts of this class that lives on the main thread.
  std::unique_ptr<MainThread> main_thread_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const Location creation_location_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  WeakPtrFactory<MemoryPressureListener> weak_ptr_factory_{this};
};

}  // namespace base

#endif  // BASE_MEMORY_MEMORY_PRESSURE_LISTENER_H_
