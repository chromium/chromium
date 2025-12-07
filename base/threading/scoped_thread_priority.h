// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_SCOPED_THREAD_PRIORITY_H_
#define BASE_THREADING_SCOPED_THREAD_PRIORITY_H_

#include <atomic>
#include <optional>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros/uniquify.h"
#include "base/memory/raw_ptr.h"
#include "base/task/task_observer.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base {

class Location;
enum class ThreadType : int;

// All code that may load a DLL on a background thread must be surrounded by a
// scope that starts with this macro.
//
// Example:
//   Foo();
//   {
//     SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
//     LoadMyDll();
//   }
//   Bar();
//
// The macro raises the thread priority to match ThreadType::kDefault for the
// scope if no other thread has completed the current scope already (multiple
// threads can racily begin the initialization and will all be boosted for it).
// On Windows, loading a DLL on a background thread can lead to a priority
// inversion on the loader lock and cause huge janks.
#define SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY()                  \
  static std::atomic_bool BASE_UNIQUIFY(already_loaded){false};           \
  base::internal::ScopedMayLoadLibraryAtBackgroundPriority BASE_UNIQUIFY( \
      scoped_may_load_library_at_background_priority)(                    \
      FROM_HERE, &BASE_UNIQUIFY(already_loaded));

// Like SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY, but raises the thread
// priority every time the scope is entered. Use this around code that may
// conditionally load a DLL each time it is executed, or which repeatedly loads
// and unloads DLLs.
#define SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY()       \
  base::internal::ScopedMayLoadLibraryAtBackgroundPriority BASE_UNIQUIFY( \
      scoped_may_load_library_at_background_priority)(FROM_HERE, nullptr);

// Boosts the current thread's priority to match the priority of threads of
// `target_thread_type` in this scope. `target_thread_type` must be lower
// priority than kRealtimeAudio, since realtime priority should only be used by
// dedicated media threads.
class BASE_EXPORT ScopedBoostPriority {
 public:
  explicit ScopedBoostPriority(ThreadType target_thread_type);
  ~ScopedBoostPriority();

  ScopedBoostPriority(const ScopedBoostPriority&) = delete;
  ScopedBoostPriority& operator=(const ScopedBoostPriority&) = delete;

 private:
  std::optional<ThreadType> original_thread_type_;
};

// Allows another thread to temporarily boost the current thread's priority to
// match the priority of threads of `target_thread_type`. The priority is reset
// when the object is destroyed, which must happens on the current thread.
// `target_thread_type` must be lower priority than kRealtimeAudio, since
// realtime priority should only be used by dedicated media threads.
class BASE_EXPORT ScopedBoostablePriority {
 public:
  ScopedBoostablePriority();
  ~ScopedBoostablePriority();

  ScopedBoostablePriority(const ScopedBoostablePriority&) = delete;
  ScopedBoostablePriority& operator=(const ScopedBoostablePriority& other) =
      delete;

  // Boosts the priority of the thread where this ScopedBoostablePriority was
  // created. Can be called from any thread, but requires proper external
  // synchronization with the constructor, destructor and any other call to
  // BoostPriority. If called multiple times, only the first call takes effect.
  bool BoostPriority(ThreadType target_thread_type);

 private:
  const ThreadType initial_thread_type_;
  PlatformThreadHandle thread_handle_;
#if BUILDFLAG(IS_WIN)
  win::ScopedHandle scoped_handle_;
#endif
  bool did_override_priority_{false};
  internal::PlatformPriorityOverride priority_override_handle_;
  THREAD_CHECKER(thread_checker_);
};

// This wraps ScopedBoostPriority with a callback to determine whether
// the priority should be boosted or not before every task execution.
class BASE_EXPORT TaskMonitoringScopedBoostPriority : public TaskObserver {
 public:
  explicit TaskMonitoringScopedBoostPriority(
      ThreadType target_thread_type,
      RepeatingCallback<bool()> should_boost_callback);
  ~TaskMonitoringScopedBoostPriority() override;

  TaskMonitoringScopedBoostPriority(const TaskMonitoringScopedBoostPriority&) =
      delete;
  TaskMonitoringScopedBoostPriority& operator=(
      const TaskMonitoringScopedBoostPriority&) = delete;

  // TaskObserver implementation:
  void WillProcessTask(const PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override;
  void DidProcessTask(const PendingTask& pending_task) override {}

 private:
  std::optional<ScopedBoostPriority> scoped_boost_priority_;
  ThreadType target_thread_type_;
  RepeatingCallback<bool()> should_boost_callback_;
};

namespace internal {

class BASE_EXPORT ScopedMayLoadLibraryAtBackgroundPriority {
 public:
  // Boosts thread priority to match ThreadType::kDefault within its scope if
  // `already_loaded` is nullptr or set to false.
  explicit ScopedMayLoadLibraryAtBackgroundPriority(
      const Location& from_here,
      std::atomic_bool* already_loaded);

  ScopedMayLoadLibraryAtBackgroundPriority(
      const ScopedMayLoadLibraryAtBackgroundPriority&) = delete;
  ScopedMayLoadLibraryAtBackgroundPriority& operator=(
      const ScopedMayLoadLibraryAtBackgroundPriority&) = delete;

  ~ScopedMayLoadLibraryAtBackgroundPriority();

 private:
#if BUILDFLAG(IS_WIN)
  // The original priority when invoking entering the scope().
  std::optional<ThreadType> original_thread_type_;
  const raw_ptr<std::atomic_bool> already_loaded_;
#endif
};

}  // namespace internal

}  // namespace base

#endif  // BASE_THREADING_SCOPED_THREAD_PRIORITY_H_
