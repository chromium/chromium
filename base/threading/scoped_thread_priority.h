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

namespace internal {

class BASE_EXPORT ScopedBoostPriorityBase {
 public:
  ScopedBoostPriorityBase();
  ~ScopedBoostPriorityBase();

  ScopedBoostPriorityBase(const ScopedBoostPriorityBase&) = delete;
  ScopedBoostPriorityBase& operator=(const ScopedBoostPriorityBase&) = delete;

 protected:
  bool ShouldBoostTo(ThreadType target_thread_type) const;

  const ThreadType initial_thread_type_;
  std::optional<ThreadType> target_thread_type_;
  internal::PlatformPriorityOverride priority_override_handle_ = {};

 private:
  raw_ptr<ScopedBoostPriorityBase> previous_boost_scope_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace internal

// Boosts the current thread's priority to match the priority of threads of
// `target_thread_type` in this scope. `target_thread_type` must be lower
// priority than kRealtimeAudio, since realtime priority should only be used by
// dedicated media threads.
class BASE_EXPORT ScopedBoostPriority
    : public internal::ScopedBoostPriorityBase {
 public:
  explicit ScopedBoostPriority(ThreadType target_thread_type);
  ~ScopedBoostPriority();
};

// Allows another thread to temporarily boost the current thread's priority to
// match the priority of threads of `target_thread_type`. The priority is reset
// when the object is destroyed, which must happens on the current thread.
// `target_thread_type` must be lower priority than kRealtimeAudio, since
// realtime priority should only be used by dedicated media threads.
class BASE_EXPORT ScopedBoostablePriority
    : public internal::ScopedBoostPriorityBase {
 public:
  ScopedBoostablePriority();
  ~ScopedBoostablePriority();

  // Boosts the priority of the thread where this ScopedBoostablePriority was
  // created. Can be called from any thread, but requires proper external
  // synchronization with the constructor, destructor and any other call to
  // BoostPriority/Reset(). If called multiple times, only the first call takes
  // effect.
  bool BoostPriority(ThreadType target_thread_type);

  // Resets the priority of the thread where this ScopedBoostablePriority was
  // created to its original priority. Can be called from any thread, but
  // requires proper external synchronization with the constructor, destructor
  // and any other call to BoostPriority/Reset().
  void Reset();

 private:
  PlatformThreadHandle thread_handle_;
#if BUILDFLAG(IS_WIN)
  win::ScopedHandle scoped_handle_;
#endif
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
  std::optional<ScopedBoostPriority> boost_priority_;
  const raw_ptr<std::atomic_bool> already_loaded_;
#endif
};

}  // namespace internal

}  // namespace base

#endif  // BASE_THREADING_SCOPED_THREAD_PRIORITY_H_
