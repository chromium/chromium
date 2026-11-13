// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/workgroup.h"

#include <errno.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <os/object.h>
#include <os/workgroup.h>
#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local_storage.h"
#include "build/build_config.h"

namespace base::mac {

BASE_FEATURE(kMacWorkgroupInterval, FEATURE_DISABLED_BY_DEFAULT);

namespace {

constexpr bool IsArchitectureSupported() {
#if defined(ARCH_CPU_ARM64)
  return true;
#else
  return false;
#endif
}

// os_workgroup_interval_create is libdispatch SPI exported since macOS 11.0
// without requiring entitlements. Bind it via weak_import so Chrome degrades
// cleanly if a future OS removes the symbol.
extern "C" os_workgroup_interval_t _Nullable os_workgroup_interval_create(
    const char* _Nullable name,
    os_clockid_t clock,
    os_workgroup_attr_t _Nullable attr) __attribute__((weak_import));

enum class JoinOwner {
  kNone,
  kSynced,
  kExplicit,
  kIntervalOwner,
};

// Invariant: static_cast<bool>(workgroup) == (owner != JoinOwner::kNone).
struct ThreadWorkgroupTlsState {
  Workgroup workgroup;
  os_workgroup_join_token_s token{};
  JoinOwner owner = JoinOwner::kNone;
};

struct ForegroundWorkgroupGlobal {
  Lock lock;
  Workgroup configured_workgroup GUARDED_BY(lock);
  bool is_enabled GUARDED_BY(lock) = true;
};

ForegroundWorkgroupGlobal& GetForegroundWorkgroupGlobal() {
  static NoDestructor<ForegroundWorkgroupGlobal> instance;
  return *instance;
}

}  // namespace

struct Workgroup::TlsImpl {
  static ThreadLocalStorage::Slot& GetSlot() {
    // Intentionally never destroyed so threads exiting during process shutdown
    // still run OnThreadExit without racing slot teardown.
    static NoDestructor<ThreadLocalStorage::Slot> slot(&TlsImpl::OnThreadExit);
    return *slot;
  }

  static ThreadWorkgroupTlsState* GetState() {
    return static_cast<ThreadWorkgroupTlsState*>(GetSlot().Get());
  }

  static ThreadWorkgroupTlsState* GetOrCreateState() {
    auto* state = GetState();
    if (!state) {
      state = new ThreadWorkgroupTlsState();
      GetSlot().Set(state);
    }
    return state;
  }

  static void LeaveState(ThreadWorkgroupTlsState* state) {
    DCHECK(state->workgroup);
    DCHECK_NE(state->owner, JoinOwner::kNone);
    os_workgroup_leave(static_cast<os_workgroup_t>(state->workgroup.workgroup_),
                       &state->token);
    state->token = {};
    state->owner = JoinOwner::kNone;
    state->workgroup = {};
  }

  static void OnThreadExit(void* raw_tls_state) {
    auto* state = static_cast<ThreadWorkgroupTlsState*>(raw_tls_state);
    if (!state) {
      return;
    }
    if (state->workgroup) {
      LeaveState(state);
    }
    delete state;
  }

  static bool JoinNew(ThreadWorkgroupTlsState* state,
                      Workgroup workgroup,
                      JoinOwner owner) {
    DCHECK(workgroup);
    DCHECK_NE(owner, JoinOwner::kNone);
    DCHECK(!state || (!state->workgroup && state->owner == JoinOwner::kNone));
    os_workgroup_join_token_s token{};
    const int rv = os_workgroup_join(
        static_cast<os_workgroup_t>(workgroup.workgroup_), &token);
    if (rv != 0) {
      // EALREADY means a system framework (e.g. CoreAudio) already joined this
      // thread to another workgroup; leave that membership untouched.
      DLOG_IF(WARNING, rv != EALREADY) << "os_workgroup_join failed: " << rv;
      return false;
    }
    if (!state) {
      state = GetOrCreateState();
    }
    state->token = token;
    state->workgroup = std::move(workgroup);
    state->owner = owner;
    return true;
  }

  static bool Join(Workgroup workgroup, JoinOwner new_owner) {
    DCHECK(new_owner == JoinOwner::kSynced ||
           new_owner == JoinOwner::kExplicit);
    auto* state = GetState();
    if (state && state->owner == JoinOwner::kIntervalOwner) {
      return workgroup && state->workgroup.SameBackingAs(workgroup);
    }
    if (!workgroup) {
      if (state && state->workgroup) {
        LeaveState(state);
      }
      return true;
    }
    if (state && state->workgroup) {
      if (state->workgroup.SameBackingAs(workgroup)) {
        if (new_owner == JoinOwner::kExplicit) {
          state->owner = JoinOwner::kExplicit;
        }
        return true;
      }
      // Leave the prior Workgroup before joining `workgroup`. Do not try to
      // re-join the old handle if the new join fails: EALREADY from external
      // frameworks cannot occur once our own membership has left, so failure
      // means the replacement workgroup itself is invalid/cancelled and
      // falling back to the task's default thread group is preferable.
      LeaveState(state);
    }
    return JoinNew(state, std::move(workgroup), new_owner);
  }

  static bool JoinForInterval(Workgroup workgroup) {
    DCHECK(workgroup);
    auto* state = GetState();
    if (state && state->owner == JoinOwner::kIntervalOwner) {
      return false;
    }
    if (state && state->workgroup) {
      LeaveState(state);
    }
    return JoinNew(state, std::move(workgroup), JoinOwner::kIntervalOwner);
  }

  static void Leave() {
    auto* state = GetState();
    if (state && (state->owner == JoinOwner::kSynced ||
                  state->owner == JoinOwner::kExplicit)) {
      LeaveState(state);
    }
  }

  static void LeaveForInterval(const Workgroup& workgroup) {
    auto* state = GetState();
    CHECK(state && state->workgroup == workgroup &&
          state->owner == JoinOwner::kIntervalOwner);
    LeaveState(state);
  }

  static void Sync(Workgroup target_workgroup) {
    auto* state = GetState();
    if (state && (state->owner == JoinOwner::kExplicit ||
                  state->owner == JoinOwner::kIntervalOwner)) {
      return;
    }
    const ThreadType thread_type = PlatformThread::GetCurrentThreadType();
    if (target_workgroup && ShouldJoinProcessForegroundWorkgroup(thread_type)) {
      Join(std::move(target_workgroup), JoinOwner::kSynced);
    } else if (state && state->owner == JoinOwner::kSynced) {
      LeaveState(state);
    }
  }
};

struct ProcessSync {
  static bool SameBacking(const Workgroup& a, const Workgroup& b) {
    return a.SameBackingAs(b);
  }
  static void SyncCurrentThread(Workgroup target_workgroup) {
    Workgroup::TlsImpl::Sync(std::move(target_workgroup));
  }
};

namespace internal {

std::atomic<uint32_t> g_foreground_workgroup_generation{0};

void SyncCurrentThreadToForegroundWorkgroupSlow(uint32_t& local_generation) {
  Workgroup target_workgroup;
  uint32_t observed_generation = 0;
  {
    auto& global = GetForegroundWorkgroupGlobal();
    AutoLock lock(global.lock);
    if (global.is_enabled) {
      target_workgroup = global.configured_workgroup;
    }
    observed_generation =
        g_foreground_workgroup_generation.load(std::memory_order_relaxed);
  }
  ProcessSync::SyncCurrentThread(std::move(target_workgroup));
  local_generation = observed_generation;
}

}  // namespace internal

Workgroup::Workgroup(void* workgroup, mach_port_t port)
    : workgroup_(workgroup), port_(port) {
  DCHECK(workgroup_);
}

Workgroup::Workgroup(const Workgroup& other)
    : workgroup_(other.workgroup_ ? os_retain(other.workgroup_) : nullptr),
      port_(other.port_) {}

Workgroup::Workgroup(Workgroup&& other) noexcept
    : workgroup_(std::exchange(other.workgroup_, nullptr)),
      port_(std::exchange(other.port_, MACH_PORT_NULL)) {}

Workgroup& Workgroup::operator=(Workgroup other) noexcept {
  std::swap(workgroup_, other.workgroup_);
  std::swap(port_, other.port_);
  return *this;
}

Workgroup::~Workgroup() {
  if (workgroup_) {
    os_release(workgroup_);
  }
}

// static
Workgroup Workgroup::CreateFromPort(const char* name, mach_port_t port) {
  CHECK(name);
  if (!IsArchitectureSupported() || port == MACH_PORT_NULL ||
      !MACH_PORT_VALID(port) ||
      !FeatureList::IsEnabled(kMacWorkgroupInterval)) {
    return Workgroup();
  }
  os_workgroup_t wg = os_workgroup_create_with_port(name, port);
  return wg ? Workgroup(wg, port) : Workgroup();
}

apple::ScopedMachSendRight Workgroup::CopyPort() const {
  if (!workgroup_) {
    return apple::ScopedMachSendRight();
  }
  mach_port_t port = MACH_PORT_NULL;
  const int rv =
      os_workgroup_copy_port(static_cast<os_workgroup_t>(workgroup_), &port);
  if (rv != 0 || port == MACH_PORT_NULL) {
    return apple::ScopedMachSendRight();
  }
  return apple::ScopedMachSendRight(port);
}

// static
bool Workgroup::JoinCurrentThread(Workgroup workgroup) {
  return TlsImpl::Join(std::move(workgroup), JoinOwner::kExplicit);
}

// static
void Workgroup::LeaveCurrentThread() {
  TlsImpl::Leave();
}

// static
Workgroup Workgroup::GetCurrentThreadWorkgroup() {
  auto* state = TlsImpl::GetState();
  return state ? state->workgroup : Workgroup();
}

void SetProcessForegroundWorkgroup(Workgroup workgroup) {
  if (!IsArchitectureSupported()) {
    return;
  }
  Workgroup target_workgroup;
  {
    auto& global = GetForegroundWorkgroupGlobal();
    AutoLock lock(global.lock);
    if (ProcessSync::SameBacking(global.configured_workgroup, workgroup)) {
      return;
    }
    global.configured_workgroup = std::move(workgroup);
    if (!global.is_enabled) {
      return;
    }
    target_workgroup = global.configured_workgroup;
    internal::g_foreground_workgroup_generation.fetch_add(
        1, std::memory_order_release);
  }
  ProcessSync::SyncCurrentThread(std::move(target_workgroup));
}

void SetProcessForegroundWorkgroupEnabled(bool enabled) {
  if (!IsArchitectureSupported() ||
      !FeatureList::IsEnabled(kMacWorkgroupInterval)) {
    return;
  }
  Workgroup target_workgroup;
  {
    auto& global = GetForegroundWorkgroupGlobal();
    AutoLock lock(global.lock);
    if (global.is_enabled == enabled) {
      return;
    }
    global.is_enabled = enabled;
    if (!global.configured_workgroup) {
      return;
    }
    if (enabled) {
      target_workgroup = global.configured_workgroup;
    }
    internal::g_foreground_workgroup_generation.fetch_add(
        1, std::memory_order_release);
  }
  ProcessSync::SyncCurrentThread(std::move(target_workgroup));
}

void ResetProcessForegroundWorkgroupForTesting() {
  {
    auto& global = GetForegroundWorkgroupGlobal();
    AutoLock lock(global.lock);
    global.configured_workgroup = {};
    global.is_enabled = true;
    internal::g_foreground_workgroup_generation.fetch_add(
        1, std::memory_order_release);
  }
  Workgroup::LeaveCurrentThread();
}

// static
bool WorkgroupInterval::IsSupported() {
  return IsArchitectureSupported() && os_workgroup_interval_create != nullptr;
}

// static
std::unique_ptr<WorkgroupInterval> WorkgroupInterval::Create(const char* name) {
  CHECK(name);
  if (!IsSupported() || !FeatureList::IsEnabled(kMacWorkgroupInterval)) {
    return nullptr;
  }
  os_workgroup_interval_t wgi =
      os_workgroup_interval_create(name, OS_CLOCK_MACH_ABSOLUTE_TIME, nullptr);
  if (!wgi) {
    return nullptr;
  }
  Workgroup workgroup(wgi, MACH_PORT_NULL);
  if (!Workgroup::TlsImpl::JoinForInterval(workgroup)) {
    return nullptr;
  }
  return WrapUnique(new WorkgroupInterval(std::move(workgroup)));
}

WorkgroupInterval::WorkgroupInterval(Workgroup workgroup)
    : workgroup_(std::move(workgroup)) {
  DCHECK(workgroup_);
}

WorkgroupInterval::~WorkgroupInterval() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Finish();
  if (is_started_) {
    // Leak one ref on shutdown rather than crashing the process via os_crash.
    os_retain(workgroup_.workgroup_);
  }
  Workgroup::TlsImpl::LeaveForInterval(workgroup_);
}

bool WorkgroupInterval::Start(TimeTicks start, TimeTicks deadline) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(Workgroup::GetCurrentThreadWorkgroup() == workgroup_);
  DCHECK(!start.is_null());
  DCHECK(!deadline.is_null());
  DCHECK_LE(start, deadline);
  if (is_started_ && !Finish()) {
    return false;
  }

  const uint64_t now_mach = mach_absolute_time();
  const uint64_t start_mach = std::min(start.ToMachAbsoluteTime(), now_mach);
  const uint64_t deadline_mach =
      std::max(start_mach, deadline.ToMachAbsoluteTime());

  const int rv = os_workgroup_interval_start(
      static_cast<os_workgroup_interval_t>(workgroup_.workgroup_), start_mach,
      deadline_mach, nullptr);
  if (rv != 0) {
    DLOG(WARNING) << "os_workgroup_interval_start failed: " << rv;
    return false;
  }
  current_start_mach_ = start_mach;
  is_started_ = true;
  return true;
}

bool WorkgroupInterval::Update(TimeTicks deadline) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(Workgroup::GetCurrentThreadWorkgroup() == workgroup_);
  DCHECK(!deadline.is_null());
  if (!is_started_) {
    return false;
  }
  const uint64_t deadline_mach =
      std::max(current_start_mach_, deadline.ToMachAbsoluteTime());
  const int rv = os_workgroup_interval_update(
      static_cast<os_workgroup_interval_t>(workgroup_.workgroup_),
      deadline_mach, nullptr);
  if (rv != 0) {
    DLOG(WARNING) << "os_workgroup_interval_update failed: " << rv;
    return false;
  }
  return true;
}

bool WorkgroupInterval::Finish() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(Workgroup::GetCurrentThreadWorkgroup() == workgroup_);
  if (!is_started_) {
    return true;
  }
  const int rv = os_workgroup_interval_finish(
      static_cast<os_workgroup_interval_t>(workgroup_.workgroup_), nullptr);
  if (rv != 0) {
    // libdispatch only clears its started bit when the kernel call succeeds.
    DLOG(WARNING) << "os_workgroup_interval_finish failed: " << rv;
    return false;
  }
  is_started_ = false;
  current_start_mach_ = 0;
  return true;
}

}  // namespace base::mac
