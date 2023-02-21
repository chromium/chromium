// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Activity tracking originally provided a low-overhead method of collecting
// information about the state of the application for analysis both while it was
// running and after it had terminated unexpectedly. To keep overhead low, a
// GlobalActivityTracker object was only created when the ExtendedCrashReporting
// feature was enabled. If no GlobalActivityTracker object existed, the
// ActivityUserData and ScopedActivity classes, and all subclasses, would
// discard any data passed to them.
//
// All classes related to activity tracking have been deleted except for those
// in activity_tracker.h, which have callers throughout the code base. This file
// now contains only stub versions of the classes and methods that are
// referenced in other files. These will compile and link but do nothing if
// called.
//
// TODO(crbug.com/1415328): Clean up all callers and delete activity_tracker.h.

#ifndef BASE_DEBUG_ACTIVITY_TRACKER_H_
#define BASE_DEBUG_ACTIVITY_TRACKER_H_

#include <atomic>

#include "base/base_export.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/pending_task.h"
#include "base/process/process_handle.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"

// These headers are no longer needed by activity_tracker.h, but many files that
// transitively include it have hidden dependencies on them.
// TODO(crbug.com/1415328): Fix all IWYU errors and delete activity_tracker.h.
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"

namespace base {

class PlatformThreadHandle;
class Process;
class WaitableEvent;

namespace internal {
class LockImpl;
}  // namespace internal

namespace debug {

class BASE_EXPORT ActivityUserData {
 public:
  ActivityUserData() = default;
  ~ActivityUserData() = default;

  ActivityUserData(const ActivityUserData&) = delete;
  ActivityUserData& operator=(const ActivityUserData&) = delete;

  void SetInt(StringPiece name, int64_t value) {}

  std::atomic<uint64_t>* SetUint(StringPiece name, uint64_t value) {
    // Only one caller (mojo/core/channel_win.cc) uses the return value of this
    // method, expecting a pointer into ActivityUserData memory where it can
    // write a running count. It checks for nullptr before writing to the
    // uint64_t, so this is safe.
    return nullptr;
  }

  void SetString(StringPiece name, StringPiece value) {}
};

class BASE_EXPORT GlobalActivityTracker {
 public:
  enum ProcessPhase : int {
    PROCESS_PHASE_UNKNOWN = 0,
    PROCESS_LAUNCHED = 1,
    PROCESS_LAUNCH_FAILED = 2,
    PROCESS_EXITED_CLEANLY = 10,
    PROCESS_EXITED_WITH_CODE = 11,
    PROCESS_SHUTDOWN_STARTED = 100,
    PROCESS_MAIN_LOOP_STARTED = 101,
  };

  // The deleted constructor ensures that there is no way to create a
  // GlobalActivityTracker object. This class and all its methods exist only
  // keep existing callers compiling.
  GlobalActivityTracker() = delete;
  ~GlobalActivityTracker() = delete;

  GlobalActivityTracker(const GlobalActivityTracker&) = delete;
  GlobalActivityTracker& operator=(const GlobalActivityTracker&) = delete;

  // Always return nullptr. All callers check the return value of Get(), or
  // use the static IfEnabled() functions that were always no-ops if Get()
  // returned nullptr. Originally Get() returned a value iff the
  // ExtendedCrashReporting feature was enabled. Since it was disabled by
  // default all callers can cleanly handle a nullptr return value.
  static GlobalActivityTracker* Get() { return nullptr; }

  void RecordProcessLaunch(ProcessId process_id,
                           const FilePath::StringType& cmd) {}
  static void RecordProcessLaunchIfEnabled(ProcessId process_id,
                                           const FilePath::StringType& cmd) {}
  static void RecordProcessLaunchIfEnabled(ProcessId process_id,
                                           const FilePath::StringType& exe,
                                           const FilePath::StringType& args) {}
  static void RecordProcessExitIfEnabled(ProcessId process_id, int exit_code) {}

  void SetProcessPhase(ProcessPhase phase) {}
  static void SetProcessPhaseIfEnabled(ProcessPhase phase) {}

  void RecordLogMessage(StringPiece message) {}

  ActivityUserData& process_data() { return process_data_; }

 private:
  ActivityUserData process_data_;
};

// Record entry in to and out of an arbitrary block of code. This class and its
// subclasses are instantiated by many callers, and are expected to record the
// data passed to them in the object returned by GlobalActivityTracker::Get(),
// or do nothing if Get() returns nullptr. Since Get() now always returns
// nullptr, they now always do nothing.
class BASE_EXPORT ScopedActivity {
 public:
  ScopedActivity() = default;
  ~ScopedActivity() = default;

  ScopedActivity(uint8_t action, uint32_t id, int32_t info) {}
  ScopedActivity(Location from_here,
                 uint8_t action,
                 uint32_t id,
                 int32_t info) {}

  ScopedActivity(const ScopedActivity&) = delete;
  ScopedActivity& operator=(const ScopedActivity&) = delete;

  bool IsRecorded() { return false; }

  ActivityUserData& user_data() { return user_data_; }

 private:
  ActivityUserData user_data_;
};

// These "scoped" classes provide easy tracking of various blocking actions.

class BASE_EXPORT ScopedTaskRunActivity : public ScopedActivity {
 public:
  explicit ScopedTaskRunActivity(const PendingTask& task) {}
};

class BASE_EXPORT ScopedLockAcquireActivity : public ScopedActivity {
 public:
  explicit ScopedLockAcquireActivity(const base::internal::LockImpl* lock) {}
};

class BASE_EXPORT ScopedEventWaitActivity : public ScopedActivity {
 public:
  explicit ScopedEventWaitActivity(const WaitableEvent* event) {}
};

class BASE_EXPORT ScopedThreadJoinActivity : public ScopedActivity {
 public:
  explicit ScopedThreadJoinActivity(const PlatformThreadHandle* thread) {}
};

// Some systems don't have base::Process
#if !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_IOS)
class BASE_EXPORT ScopedProcessWaitActivity : public ScopedActivity {
 public:
  explicit ScopedProcessWaitActivity(const Process* process) {}
};
#endif

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_ACTIVITY_TRACKER_H_
