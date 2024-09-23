// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/suspendable_thread_delegate_win.h"

#include <windows.h>

#include <winternl.h>

#include <vector>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/profiler/native_unwinder_win.h"
#include "build/build_config.h"

// IMPORTANT NOTE: Some functions within this implementation are invoked while
// the target thread is suspended so it must not do any allocation from the
// heap, including indirectly via use of DCHECK/CHECK or other logging
// statements. Otherwise this code can deadlock on heap locks acquired by the
// target thread before it was suspended. These functions are commented with "NO
// HEAP ALLOCATIONS".

namespace base {

namespace {

// The thread environment block internal type.
struct TEB {
  NT_TIB Tib;
  // Rest of struct is ignored.
};

win::ScopedHandle GetCurrentThreadHandle() {
  HANDLE thread;
  CHECK(::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(),
                          ::GetCurrentProcess(), &thread, 0, FALSE,
                          DUPLICATE_SAME_ACCESS));
  return win::ScopedHandle(thread);
}

win::ScopedHandle GetThreadHandle(PlatformThreadId thread_id) {
  // TODO(crbug.com/40620762): Move this logic to
  // GetSamplingProfilerCurrentThreadToken() and pass the handle in
  // SamplingProfilerThreadToken.
  if (thread_id == ::GetCurrentThreadId())
    return GetCurrentThreadHandle();

  // TODO(http://crbug.com/947459): Remove the test_handle* CHECKs once we
  // understand which flag is triggering the failure.
  DWORD flags = 0;
  base::debug::Alias(&flags);

  flags |= THREAD_GET_CONTEXT;
  win::ScopedHandle test_handle1(::OpenThread(flags, FALSE, thread_id));
  CHECK(test_handle1.is_valid());

  flags |= THREAD_QUERY_INFORMATION;
  win::ScopedHandle test_handle2(::OpenThread(flags, FALSE, thread_id));
  CHECK(test_handle2.is_valid());

  flags |= THREAD_SUSPEND_RESUME;
  win::ScopedHandle handle(::OpenThread(flags, FALSE, thread_id));
  CHECK(handle.is_valid());
  return handle;
}

// Returns the thread environment block pointer for |thread_handle|.
const TEB* GetThreadEnvironmentBlock(PlatformThreadId thread_id,
                                     HANDLE thread_handle) {
  // TODO(crbug.com/40620762): Move this logic to
  // GetSamplingProfilerCurrentThreadToken() and pass the TEB* in
  // SamplingProfilerThreadToken.
  if (thread_id == ::GetCurrentThreadId())
    return reinterpret_cast<TEB*>(NtCurrentTeb());

  // Define types not in winternl.h needed to invoke NtQueryInformationThread().
  constexpr auto ThreadBasicInformation = static_cast<THREADINFOCLASS>(0);
  struct THREAD_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    // RAW_PTR_EXCLUSION: Filled in by the OS so cannot use raw_ptr<>.
    RAW_PTR_EXCLUSION TEB* Teb;
    CLIENT_ID ClientId;
    KAFFINITY AffinityMask;
    LONG Priority;
    LONG BasePriority;
  };

  THREAD_BASIC_INFORMATION basic_info = {0};
  NTSTATUS status = ::NtQueryInformationThread(
      thread_handle, ThreadBasicInformation, &basic_info,
      sizeof(THREAD_BASIC_INFORMATION), nullptr);
  if (status != 0)
    return nullptr;

  return basic_info.Teb;
}

// Tests whether |stack_pointer| points to a location in the guard page. NO HEAP
// ALLOCATIONS.
bool PointsToGuardPage(uintptr_t stack_pointer) {
  MEMORY_BASIC_INFORMATION memory_info;
  SIZE_T result = ::VirtualQuery(reinterpret_cast<LPCVOID>(stack_pointer),
                                 &memory_info, sizeof(memory_info));
  return result != 0 && (memory_info.Protect & PAGE_GUARD);
}

// ScopedDisablePriorityBoost -------------------------------------------------

// Disables priority boost on a thread for the lifetime of the object.
class ScopedDisablePriorityBoost {
 public:
  ScopedDisablePriorityBoost(HANDLE thread_handle);

  ScopedDisablePriorityBoost(const ScopedDisablePriorityBoost&) = delete;
  ScopedDisablePriorityBoost& operator=(const ScopedDisablePriorityBoost&) =
      delete;

  ~ScopedDisablePriorityBoost();

 private:
  HANDLE thread_handle_;
  BOOL got_previous_boost_state_;
  BOOL boost_state_was_disabled_;
};

// NO HEAP ALLOCATIONS.
ScopedDisablePriorityBoost::ScopedDisablePriorityBoost(HANDLE thread_handle)
    : thread_handle_(thread_handle),
      got_previous_boost_state_(false),
      boost_state_was_disabled_(false) {
  got_previous_boost_state_ =
      ::GetThreadPriorityBoost(thread_handle_, &boost_state_was_disabled_);
  if (got_previous_boost_state_) {
    // Confusingly, TRUE disables priority boost.
    ::SetThreadPriorityBoost(thread_handle_, TRUE);
  }
}

ScopedDisablePriorityBoost::~ScopedDisablePriorityBoost() {
  if (got_previous_boost_state_)
    ::SetThreadPriorityBoost(thread_handle_, boost_state_was_disabled_);
}

}  // namespace

// ScopedSuspendThread --------------------------------------------------------

// NO HEAP ALLOCATIONS after ::SuspendThread.
SuspendableThreadDelegateWin::ScopedSuspendThread::ScopedSuspendThread(
    HANDLE thread_handle)
    : thread_handle_(thread_handle),
      was_successful_(::SuspendThread(thread_handle) !=
                      static_cast<DWORD>(-1)) {}

// NO HEAP ALLOCATIONS. The CHECK is OK because it provides a more noisy failure
// mode than deadlocking.
SuspendableThreadDelegateWin::ScopedSuspendThread::~ScopedSuspendThread() {
  if (!was_successful_)
    return;

  // Disable the priority boost that the thread would otherwise receive on
  // resume. We do this to avoid artificially altering the dynamics of the
  // executing application any more than we already are by suspending and
  // resuming the thread.
  //
  // Note that this can racily disable a priority boost that otherwise would
  // have been given to the thread, if the thread is waiting on other wait
  // conditions at the time of SuspendThread and those conditions are satisfied
  // before priority boost is reenabled. The measured length of this window is
  // ~100us, so this should occur fairly rarely.
  ScopedDisablePriorityBoost disable_priority_boost(thread_handle_);
  bool resume_thread_succeeded =
      ::ResumeThread(thread_handle_) != static_cast<DWORD>(-1);
  PCHECK(resume_thread_succeeded) << "ResumeThread failed";
}

bool SuspendableThreadDelegateWin::ScopedSuspendThread::WasSuccessful() const {
  return was_successful_;
}

// SuspendableThreadDelegateWin
// ----------------------------------------------------------

SuspendableThreadDelegateWin::SuspendableThreadDelegateWin(
    SamplingProfilerThreadToken thread_token)
    : thread_id_(thread_token.id),
      thread_handle_(GetThreadHandle(thread_token.id)),
      thread_stack_base_address_(reinterpret_cast<uintptr_t>(
          GetThreadEnvironmentBlock(thread_token.id, thread_handle_.get())
              ->Tib.StackBase)) {}

SuspendableThreadDelegateWin::~SuspendableThreadDelegateWin() = default;

std::unique_ptr<SuspendableThreadDelegate::ScopedSuspendThread>
SuspendableThreadDelegateWin::CreateScopedSuspendThread() {
  return std::make_unique<ScopedSuspendThread>(thread_handle_.get());
}

PlatformThreadId SuspendableThreadDelegateWin::GetThreadId() const {
  return thread_id_;
}

// NO HEAP ALLOCATIONS.
bool SuspendableThreadDelegateWin::GetThreadContext(CONTEXT* thread_context) {
  *thread_context = {0};
  thread_context->ContextFlags = CONTEXT_FULL;
  return ::GetThreadContext(thread_handle_.get(), thread_context) != 0;
}

// NO HEAP ALLOCATIONS.
uintptr_t SuspendableThreadDelegateWin::GetStackBaseAddress() const {
  return thread_stack_base_address_;
}

// Tests whether |stack_pointer| points to a location in the guard page. NO HEAP
// ALLOCATIONS.
bool SuspendableThreadDelegateWin::CanCopyStack(uintptr_t stack_pointer) {
  // Dereferencing a pointer in the guard page in a thread that doesn't own the
  // stack results in a STATUS_GUARD_PAGE_VIOLATION exception and a crash. This
  // occurs very rarely, but reliably over the population.
  return !PointsToGuardPage(stack_pointer);
}

std::vector<uintptr_t*> SuspendableThreadDelegateWin::GetRegistersToRewrite(
    CONTEXT* thread_context) {
  // Return the set of non-volatile registers.
  return {
#if defined(ARCH_CPU_X86_64)
    &thread_context->R12, &thread_context->R13, &thread_context->R14,
        &thread_context->R15, &thread_context->Rdi, &thread_context->Rsi,
        &thread_context->Rbx, &thread_context->Rbp, &thread_context->Rsp
#elif defined(ARCH_CPU_ARM64)
    &thread_context->X19, &thread_context->X20, &thread_context->X21,
        &thread_context->X22, &thread_context->X23, &thread_context->X24,
        &thread_context->X25, &thread_context->X26, &thread_context->X27,
        &thread_context->X28, &thread_context->Fp, &thread_context->Lr,
        &thread_context->Sp
#endif
  };
}

}  // namespace base
