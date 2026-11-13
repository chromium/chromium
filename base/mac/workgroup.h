// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_WORKGROUP_H_
#define BASE_MAC_WORKGROUP_H_

#include <mach/mach.h>
#include <stdint.h>

#include <atomic>
#include <memory>

#include "base/apple/scoped_mach_port.h"
#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/task/thread_type.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace base::mac {

BASE_EXPORT BASE_DECLARE_FEATURE(kMacWorkgroupInterval);

// Thread-safe value handle around macOS os_workgroup_t (retained/released via
// libdispatch).
//
// Cooperating threads across processes can join the same Workgroup via Mach
// port transfer. A thread may belong to at most one os_workgroup_t at a time
// and must leave before exiting; join tokens are tracked in TLS and cleaned up
// automatically on thread exit.
class BASE_EXPORT Workgroup {
 public:
  // Imports a workgroup from a Mach send right (see CopyPort()). Does not
  // consume `port`. Returns an empty Workgroup if unsupported or disabled.
  static Workgroup CreateFromPort(const char* name, mach_port_t port);

  Workgroup() = default;
  Workgroup(const Workgroup& other);
  Workgroup(Workgroup&& other) noexcept;
  Workgroup& operator=(Workgroup other) noexcept;
  ~Workgroup();

  explicit operator bool() const { return workgroup_ != nullptr; }
  // Compares the underlying os_workgroup_t instance pointer (required by
  // libdispatch when matching leave to join or checking interval ownership).
  bool operator==(const Workgroup& other) const {
    return workgroup_ == other.workgroup_;
  }

  apple::ScopedMachSendRight CopyPort() const;

  // Joins the current thread to `workgroup` (leaving any previous non-interval
  // Workgroup first; if joining the replacement fails, the thread remains
  // unjoined). Passing an empty Workgroup leaves. Explicit joins made here are
  // not overwritten by MaybeSyncCurrentThreadToForegroundWorkgroup. Returns
  // false if the thread owns a live WorkgroupInterval or if os_workgroup_join
  // fails (e.g. EALREADY from an external framework, whose membership is left
  // untouched).
  static bool JoinCurrentThread(Workgroup workgroup);
  static void LeaveCurrentThread();

  // Returns the Workgroup the current thread is joined to, or empty if none.
  static Workgroup GetCurrentThreadWorkgroup();

 private:
  friend class WorkgroupInterval;
  friend struct ProcessSync;
  struct TlsImpl;

  Workgroup(void* workgroup, mach_port_t port);

  // Unlike operator==, also matches two handles imported via separate
  // CreateFromPort() calls in this task that wrap the same coalesced Mach
  // send-right name (`port_`). Each CreateFromPort() allocates a distinct
  // os_workgroup_t (`*this != other`), so use SameBackingAs() to skip redundant
  // leave/rejoin when already targeting the same kernel workgroup, and use
  // operator== when checking the exact joined os_workgroup_t instance.
  bool SameBackingAs(const Workgroup& other) const {
    return workgroup_ == other.workgroup_ ||
           (port_ != MACH_PORT_NULL && port_ == other.port_);
  }

  // Opaque os_workgroup_t managed by os_retain/os_release; kept as void* so
  // .cc and .mm translation units see the same layout.
  RAW_PTR_EXCLUSION void* workgroup_ = nullptr;
  // Send-right name in mach_task_self() when created via CreateFromPort(), kept
  // alive by `workgroup_` and used by SameBackingAs() for port deduplication.
  mach_port_t port_ = MACH_PORT_NULL;
};

// Excludes background/utility threads as well as audio threads whose cadences
// are independent of display VSyncs.
constexpr bool ShouldJoinProcessForegroundWorkgroup(ThreadType thread_type) {
  return thread_type == ThreadType::kDefault ||
         thread_type == ThreadType::kPresentation;
}

// Sets (or clears) the Workgroup that eligible SequenceManager threads and
// ThreadPool workers in this process join while enabled. Thread-safe.
BASE_EXPORT void SetProcessForegroundWorkgroup(Workgroup workgroup);

// Enables or suspends joining the configured process foreground workgroup
// (enabled by default). When false, threads joined via foreground sync leave on
// their next wakeup; explicit JoinCurrentThread() memberships are unaffected.
BASE_EXPORT void SetProcessForegroundWorkgroupEnabled(bool enabled);

BASE_EXPORT void ResetProcessForegroundWorkgroupForTesting();

namespace internal {

// Bumped when the effective foreground workgroup changes; stays 0 when unused.
BASE_EXPORT extern std::atomic<uint32_t> g_foreground_workgroup_generation;

BASE_EXPORT void SyncCurrentThreadToForegroundWorkgroupSlow(
    uint32_t& local_generation);

}  // namespace internal

// Syncs the calling thread's membership when the process foreground workgroup
// generation changes; `local_generation` is per-thread state initialized to 0.
inline void MaybeSyncCurrentThreadToForegroundWorkgroup(
    uint32_t& local_generation) {
  const uint32_t target = internal::g_foreground_workgroup_generation.load(
      std::memory_order_acquire);
  if (local_generation != target) [[unlikely]] {
    internal::SyncCurrentThreadToForegroundWorkgroupSlow(local_generation);
  }
}

// Thread-affine RAII wrapper around os_workgroup_interval_t. Create() joins the
// calling thread to the interval's Workgroup (returning nullptr if unsupported,
// disabled, or unable to join), and ~WorkgroupInterval() finishes any active
// interval and leaves the Workgroup on that same thread.
class BASE_EXPORT WorkgroupInterval {
 public:
  static bool IsSupported();
  static std::unique_ptr<WorkgroupInterval> Create(const char* name);

  WorkgroupInterval(const WorkgroupInterval&) = delete;
  WorkgroupInterval& operator=(const WorkgroupInterval&) = delete;
  ~WorkgroupInterval();

  const Workgroup& workgroup() const { return workgroup_; }
  apple::ScopedMachSendRight CopyPort() const { return workgroup_.CopyPort(); }

  bool is_started() const { return is_started_; }

  // Starts an interval (finishing any active one first). Both `start` and
  // `deadline` must be non-null with `start <= deadline`; `start` is clamped to
  // <= now in Mach ticks so slightly-ahead VSync timestamps do not fail.
  bool Start(TimeTicks start, TimeTicks deadline);
  // Updates the active interval's deadline (`deadline` must be non-null).
  bool Update(TimeTicks deadline);
  // Finishes the active interval; no-op returning true if not started.
  bool Finish();

 private:
  explicit WorkgroupInterval(Workgroup workgroup);

  Workgroup workgroup_;
  uint64_t current_start_mach_ = 0;
  bool is_started_ = false;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace base::mac

#endif  // BASE_MAC_WORKGROUP_H_
