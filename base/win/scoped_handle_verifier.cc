// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_handle_verifier.h"

#include <windows.h>

#include <stddef.h>

#include <unordered_map>
#include <utility>

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/stack_trace.h"
#include "base/memory/raw_ref.h"
#include "base/synchronization/lock_impl.h"
#include "base/trace_event/base_tracing.h"
#include "base/win/base_win_buildflags.h"
#include "base/win/current_module.h"
#include "base/win/scoped_handle.h"

extern "C" {
__declspec(dllexport) void* GetHandleVerifier();

void* GetHandleVerifier() {
  return base::win::internal::ScopedHandleVerifier::Get();
}
}  // extern C

namespace base {
namespace win {
namespace internal {

namespace {

ScopedHandleVerifier* g_active_verifier = nullptr;
constinit thread_local bool closing = false;
using GetHandleVerifierFn = void* (*)();
using HandleMap =
    std::unordered_map<HANDLE, ScopedHandleVerifierInfo, HandleHash>;
using NativeLock = base::internal::LockImpl;

NOINLINE void ReportErrorOnScopedHandleOperation(
    const debug::StackTrace& creation_stack,
    HandleOperation operation) {
  auto creation_stack_copy = creation_stack;
  debug::Alias(&creation_stack_copy);
  debug::Alias(&operation);
  CHECK(false) << operation;
  __builtin_unreachable();
}

NOINLINE void ReportErrorOnScopedHandleOperation(
    const debug::StackTrace& creation_stack,
    const ScopedHandleVerifierInfo& other,
    HandleOperation operation) {
  auto other_stack_copy = *other.stack;
  debug::Alias(&other_stack_copy);
  auto creation_stack_copy = creation_stack;
  debug::Alias(&creation_stack_copy);
  debug::Alias(&operation);
  CHECK(false) << operation;
  __builtin_unreachable();
}

}  // namespace

// Simple automatic locking using a native critical section so it supports
// recursive locking.
class AutoNativeLock {
 public:
  explicit AutoNativeLock(NativeLock& lock) : lock_(lock) { lock_->Lock(); }

  AutoNativeLock(const AutoNativeLock&) = delete;
  AutoNativeLock& operator=(const AutoNativeLock&) = delete;

  ~AutoNativeLock() { lock_->Unlock(); }

 private:
  const raw_ref<NativeLock> lock_;
};

ScopedHandleVerifierInfo::ScopedHandleVerifierInfo(
    const void* owner,
    const void* pc1,
    const void* pc2,
    std::unique_ptr<debug::StackTrace> stack,
    DWORD thread_id)
    : owner(owner),
      pc1(pc1),
      pc2(pc2),
      stack(std::move(stack)),
      thread_id(thread_id) {}

ScopedHandleVerifierInfo::~ScopedHandleVerifierInfo() = default;

ScopedHandleVerifierInfo::ScopedHandleVerifierInfo(
    ScopedHandleVerifierInfo&&) noexcept = default;
ScopedHandleVerifierInfo& ScopedHandleVerifierInfo::operator=(
    ScopedHandleVerifierInfo&&) noexcept = default;

ScopedHandleVerifier::ScopedHandleVerifier(bool enabled)
    : enabled_(enabled), lock_(GetLock()) {}

// static
ScopedHandleVerifier* ScopedHandleVerifier::Get() {
  if (!g_active_verifier)
    ScopedHandleVerifier::InstallVerifier();

  return g_active_verifier;
}

bool CloseHandleWrapper(HANDLE handle) {
  if (!::CloseHandle(handle))
    CHECK(false) << "CloseHandle failed";
  return true;
}

// Assigns the g_active_verifier global within the ScopedHandleVerifier lock.
// If |existing_verifier| is non-null then |enabled| is ignored.
// static
void ScopedHandleVerifier::ThreadSafeAssignOrCreateScopedHandleVerifier(
    ScopedHandleVerifier* existing_verifier,
    bool enabled) {
  AutoNativeLock lock(*GetLock());
  // Another thread in this module might be trying to assign the global
  // verifier, so check that within the lock here.
  if (g_active_verifier)
    return;
  g_active_verifier =
      existing_verifier ? existing_verifier : new ScopedHandleVerifier(enabled);
}

// static
void ScopedHandleVerifier::InstallVerifier() {
#if BUILDFLAG(SINGLE_MODULE_MODE_HANDLE_VERIFIER)
  // Component build has one Active Verifier per module.
  ThreadSafeAssignOrCreateScopedHandleVerifier(nullptr, true);
#else
  // If you are reading this, wondering why your process seems deadlocked, take
  // a look at your DllMain code and remove things that should not be done
  // there, like doing whatever gave you that nice windows handle you are trying
  // to store in a ScopedHandle.
  HMODULE main_module = ::GetModuleHandle(NULL);
  GetHandleVerifierFn get_handle_verifier =
      reinterpret_cast<GetHandleVerifierFn>(
          ::GetProcAddress(main_module, "GetHandleVerifier"));

  // This should only happen if running in a DLL is linked with base but the
  // hosting EXE is not. In this case, create a ScopedHandleVerifier for the
  // current module but leave it disabled.
  if (!get_handle_verifier) {
    ThreadSafeAssignOrCreateScopedHandleVerifier(nullptr, false);
    return;
  }

  // Check if in the main module.
  if (get_handle_verifier == GetHandleVerifier) {
    ThreadSafeAssignOrCreateScopedHandleVerifier(nullptr, true);
    return;
  }

  ScopedHandleVerifier* main_module_verifier =
      reinterpret_cast<ScopedHandleVerifier*>(get_handle_verifier());

  // Main module should always on-demand create a verifier.
  DCHECK(main_module_verifier);

  ThreadSafeAssignOrCreateScopedHandleVerifier(main_module_verifier, false);
#endif
}

bool ScopedHandleVerifier::CloseHandle(HANDLE handle) {
  if (!enabled_)
    return CloseHandleWrapper(handle);

  const AutoReset<bool> resetter(&closing, true);
  CloseHandleWrapper(handle);

  return true;
}

// static
NativeLock* ScopedHandleVerifier::GetLock() {
  static auto* native_lock = new NativeLock();
  return native_lock;
}

void ScopedHandleVerifier::StartTracking(HANDLE handle,
                                         const void* owner,
                                         const void* pc1,
                                         const void* pc2) {
  if (enabled_)
    StartTrackingImpl(handle, owner, pc1, pc2);
}

void ScopedHandleVerifier::StopTracking(HANDLE handle,
                                        const void* owner,
                                        const void* pc1,
                                        const void* pc2) {
  if (enabled_)
    StopTrackingImpl(handle, owner, pc1, pc2);
}

void ScopedHandleVerifier::Disable() {
  enabled_ = false;
}

void ScopedHandleVerifier::OnHandleBeingClosed(HANDLE handle,
                                               HandleOperation operation) {
  if (enabled_)
    OnHandleBeingClosedImpl(handle, operation);
}

HMODULE ScopedHandleVerifier::GetModule() const {
  return CURRENT_MODULE();
}

NOINLINE void ScopedHandleVerifier::StartTrackingImpl(HANDLE handle,
                                                      const void* owner,
                                                      const void* pc1,
                                                      const void* pc2) {
  // Grab the thread id before the lock.
  DWORD thread_id = GetCurrentThreadId();

  // Grab the thread stacktrace before the lock.
  auto stacktrace = std::make_unique<debug::StackTrace>();

  AutoNativeLock lock(*lock_);
  std::pair<HandleMap::iterator, bool> result = map_.emplace(
      handle, ScopedHandleVerifierInfo{owner, pc1, pc2, std::move(stacktrace),
                                       thread_id});
  if (!result.second) {
    // Attempt to start tracking already tracked handle.
    ReportErrorOnScopedHandleOperation(creation_stack_, result.first->second,
                                       HandleOperation::kHandleAlreadyTracked);
  }
}

NOINLINE void ScopedHandleVerifier::StopTrackingImpl(HANDLE handle,
                                                     const void* owner,
                                                     const void* pc1,
                                                     const void* pc2) {
  AutoNativeLock lock(*lock_);
  HandleMap::iterator i = map_.find(handle);
  if (i == map_.end()) {
    // Attempting to close an untracked handle.
    ReportErrorOnScopedHandleOperation(creation_stack_,
                                       HandleOperation::kCloseHandleNotTracked);
  }

  if (i->second.owner != owner) {
    // Attempting to close a handle not owned by opener.
    ReportErrorOnScopedHandleOperation(creation_stack_, i->second,
                                       HandleOperation::kCloseHandleNotOwner);
  }

  map_.erase(i);
}

NOINLINE void ScopedHandleVerifier::OnHandleBeingClosedImpl(
    HANDLE handle,
    HandleOperation operation) {
  if (closing) {
    return;
  }

  AutoNativeLock lock(*lock_);
  HandleMap::iterator i = map_.find(handle);
  if (i != map_.end()) {
    // CloseHandle called on tracked handle.
    ReportErrorOnScopedHandleOperation(creation_stack_, i->second, operation);
  }
}

HMODULE GetHandleVerifierModuleForTesting() {
  return g_active_verifier->GetModule();
}

}  // namespace internal
}  // namespace win
}  // namespace base
