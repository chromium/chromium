// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_win.h"

#include <windows.h>

#include <stddef.h>

#include <string>

#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/profiler.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/threading_features.h"
#include "base/time/time_override.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/stack/stack.h"
#endif

namespace base {

BASE_FEATURE(kUseThreadPriorityLowest,
             "UseThreadPriorityLowest",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAboveNormalCompositingBrowserWin,
             "AboveNormalCompositingBrowserWin",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// Flag used to set thread priority to |THREAD_PRIORITY_LOWEST| for
// |kUseThreadPriorityLowest| Feature.
std::atomic<bool> g_use_thread_priority_lowest{false};
// Flag used to map Compositing ThreadType |THREAD_PRIORITY_ABOVE_NORMAL| on the
// UI thread for |kAboveNormalCompositingBrowserWin| Feature.
std::atomic<bool> g_above_normal_compositing_browser{true};

// These values are sometimes returned by ::GetThreadPriority().
constexpr int kWinDisplayPriority1 = 5;
constexpr int kWinDisplayPriority2 = 6;

// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD kVCThreadNameException = 0x406D1388;

typedef struct tagTHREADNAME_INFO {
  DWORD dwType;  // Must be 0x1000.
  LPCSTR szName;  // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;  // Reserved for future use, must be zero.
} THREADNAME_INFO;

// The SetThreadDescription API was brought in version 1607 of Windows 10.
typedef HRESULT(WINAPI* SetThreadDescription)(HANDLE hThread,
                                              PCWSTR lpThreadDescription);

// This function has try handling, so it is separated out of its caller.
void SetNameInternal(PlatformThreadId thread_id, const char* name) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = thread_id;
  info.dwFlags = 0;

  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info) / sizeof(ULONG_PTR),
                   reinterpret_cast<ULONG_PTR*>(&info));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

struct ThreadParams {
  raw_ptr<PlatformThread::Delegate> delegate;
  bool joinable;
  ThreadType thread_type;
  MessagePumpType message_pump_type;
};

DWORD __stdcall ThreadFunc(void* params) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(params);
  PlatformThread::Delegate* delegate = thread_params->delegate;
  if (!thread_params->joinable)
    base::DisallowSingleton();

  if (thread_params->thread_type != ThreadType::kDefault)
    internal::SetCurrentThreadType(thread_params->thread_type,
                                   thread_params->message_pump_type);

  // Retrieve a copy of the thread handle to use as the key in the
  // thread name mapping.
  PlatformThreadHandle::Handle platform_handle;
  BOOL did_dup = DuplicateHandle(GetCurrentProcess(),
                                GetCurrentThread(),
                                GetCurrentProcess(),
                                &platform_handle,
                                0,
                                FALSE,
                                DUPLICATE_SAME_ACCESS);

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  partition_alloc::internal::StackTopRegistry::Get().NotifyThreadCreated();
#endif

  win::ScopedHandle scoped_platform_handle;

  if (did_dup) {
    scoped_platform_handle.Set(platform_handle);
    ThreadIdNameManager::GetInstance()->RegisterThread(
        scoped_platform_handle.get(), PlatformThread::CurrentId());
  }

  delete thread_params;
  delegate->ThreadMain();

  if (did_dup) {
    ThreadIdNameManager::GetInstance()->RemoveName(scoped_platform_handle.get(),
                                                   PlatformThread::CurrentId());
  }

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  partition_alloc::internal::StackTopRegistry::Get().NotifyThreadDestroyed();
#endif

  // Ensure thread priority is at least NORMAL before initiating thread
  // destruction. Thread destruction on Windows holds the LdrLock while
  // performing TLS destruction which causes hangs if performed at background
  // priority (priority inversion) (see: http://crbug.com/1096203).
  if (::GetThreadPriority(::GetCurrentThread()) < THREAD_PRIORITY_NORMAL)
    PlatformThread::SetCurrentThreadType(ThreadType::kDefault);

  return 0;
}

// CreateThreadInternal() matches PlatformThread::CreateWithType(), except
// that |out_thread_handle| may be nullptr, in which case a non-joinable thread
// is created.
bool CreateThreadInternal(size_t stack_size,
                          PlatformThread::Delegate* delegate,
                          PlatformThreadHandle* out_thread_handle,
                          ThreadType thread_type,
                          MessagePumpType message_pump_type) {
  unsigned int flags = 0;
  if (stack_size > 0) {
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
#if defined(ARCH_CPU_32_BITS)
  } else {
    // The process stack size is increased to give spaces to |RendererMain| in
    // |chrome/BUILD.gn|, but keep the default stack size of other threads to
    // 1MB for the address space pressure.
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
    static BOOL is_wow64 = -1;
    if (is_wow64 == -1 && !IsWow64Process(GetCurrentProcess(), &is_wow64))
      is_wow64 = FALSE;
    // When is_wow64 is set that means we are running on 64-bit Windows and we
    // get 4 GiB of address space. In that situation we can afford to use 1 MiB
    // of address space for stacks. When running on 32-bit Windows we only get
    // 2 GiB of address space so we need to conserve. Typically stack usage on
    // these threads is only about 100 KiB.
    if (is_wow64)
      stack_size = 1024 * 1024;
    else
      stack_size = 512 * 1024;
#endif
  }

  ThreadParams* params = new ThreadParams;
  params->delegate = delegate;
  params->joinable = out_thread_handle != nullptr;
  params->thread_type = thread_type;
  params->message_pump_type = message_pump_type;

  // Using CreateThread here vs _beginthreadex makes thread creation a bit
  // faster and doesn't require the loader lock to be available.  Our code will
  // have to work running on CreateThread() threads anyway, since we run code on
  // the Windows thread pool, etc.  For some background on the difference:
  // http://www.microsoft.com/msj/1099/win32/win321099.aspx
  void* thread_handle =
      ::CreateThread(nullptr, stack_size, ThreadFunc, params, flags, nullptr);

  if (!thread_handle) {
    DWORD last_error = ::GetLastError();

    switch (last_error) {
      case ERROR_NOT_ENOUGH_MEMORY:
      case ERROR_OUTOFMEMORY:
      case ERROR_COMMITMENT_LIMIT:
      case ERROR_COMMITMENT_MINIMUM:
        TerminateBecauseOutOfMemory(stack_size);

      default:
        static auto* last_error_crash_key = debug::AllocateCrashKeyString(
            "create_thread_last_error", debug::CrashKeySize::Size32);
        debug::SetCrashKeyString(last_error_crash_key,
                                 base::NumberToString(last_error));
        break;
    }

    delete params;
    return false;
  }

  if (out_thread_handle)
    *out_thread_handle = PlatformThreadHandle(thread_handle);
  else
    CloseHandle(thread_handle);
  return true;
}

}  // namespace

namespace internal {

void AssertMemoryPriority(HANDLE thread, int memory_priority) {
#if DCHECK_IS_ON()
  static const auto get_thread_information_fn =
      reinterpret_cast<decltype(&::GetThreadInformation)>(::GetProcAddress(
          ::GetModuleHandle(L"Kernel32.dll"), "GetThreadInformation"));

  DCHECK(get_thread_information_fn);

  MEMORY_PRIORITY_INFORMATION memory_priority_information = {};
  DCHECK(get_thread_information_fn(thread, ::ThreadMemoryPriority,
                                   &memory_priority_information,
                                   sizeof(memory_priority_information)));

  DCHECK_EQ(memory_priority,
            static_cast<int>(memory_priority_information.MemoryPriority));
#endif
}

}  // namespace internal

// static
PlatformThreadId PlatformThread::CurrentId() {
  return ::GetCurrentThreadId();
}

// static
PlatformThreadRef PlatformThread::CurrentRef() {
  return PlatformThreadRef(::GetCurrentThreadId());
}

// static
PlatformThreadHandle PlatformThread::CurrentHandle() {
  return PlatformThreadHandle(::GetCurrentThread());
}

// static
void PlatformThread::YieldCurrentThread() {
  ::Sleep(0);
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  // When measured with a high resolution clock, Sleep() sometimes returns much
  // too early. We may need to call it repeatedly to get the desired duration.
  // PlatformThread::Sleep doesn't support mock-time, so this always uses
  // real-time.
  const TimeTicks end = subtle::TimeTicksNowIgnoringOverride() + duration;
  for (TimeTicks now = subtle::TimeTicksNowIgnoringOverride(); now < end;
       now = subtle::TimeTicksNowIgnoringOverride()) {
    ::Sleep(static_cast<DWORD>((end - now).InMillisecondsRoundedUp()));
  }
}

// static
void PlatformThread::SetName(const std::string& name) {
  SetNameCommon(name);

  // The SetThreadDescription API works even if no debugger is attached.
  static auto set_thread_description_func =
      reinterpret_cast<SetThreadDescription>(::GetProcAddress(
          ::GetModuleHandle(L"Kernel32.dll"), "SetThreadDescription"));
  if (set_thread_description_func) {
    set_thread_description_func(::GetCurrentThread(),
                                base::UTF8ToWide(name).c_str());
  }

  // The debugger needs to be around to catch the name in the exception.  If
  // there isn't a debugger, we are just needlessly throwing an exception.
  if (!::IsDebuggerPresent())
    return;

  SetNameInternal(CurrentId(), name.c_str());
}

// static
const char* PlatformThread::GetName() {
  return ThreadIdNameManager::GetInstance()->GetName(CurrentId());
}

// static
bool PlatformThread::CreateWithType(size_t stack_size,
                                    Delegate* delegate,
                                    PlatformThreadHandle* thread_handle,
                                    ThreadType thread_type,
                                    MessagePumpType pump_type_hint) {
  DCHECK(thread_handle);
  return CreateThreadInternal(stack_size, delegate, thread_handle, thread_type,
                              pump_type_hint);
}

// static
bool PlatformThread::CreateNonJoinable(size_t stack_size, Delegate* delegate) {
  return CreateNonJoinableWithType(stack_size, delegate, ThreadType::kDefault);
}

// static
bool PlatformThread::CreateNonJoinableWithType(size_t stack_size,
                                               Delegate* delegate,
                                               ThreadType thread_type,
                                               MessagePumpType pump_type_hint) {
  return CreateThreadInternal(stack_size, delegate, nullptr /* non-joinable */,
                              thread_type, pump_type_hint);
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  DCHECK(thread_handle.platform_handle());

  DWORD thread_id = 0;
  thread_id = ::GetThreadId(thread_handle.platform_handle());
  DWORD last_error = 0;
  if (!thread_id)
    last_error = ::GetLastError();

  // Record information about the exiting thread in case joining hangs.
  base::debug::Alias(&thread_id);
  base::debug::Alias(&last_error);

  base::internal::ScopedBlockingCallWithBaseSyncPrimitives scoped_blocking_call(
      FROM_HERE, base::BlockingType::MAY_BLOCK);

  // Wait for the thread to exit.  It should already have terminated but make
  // sure this assumption is valid.
  CHECK_EQ(WAIT_OBJECT_0,
           WaitForSingleObject(thread_handle.platform_handle(), INFINITE));
  CloseHandle(thread_handle.platform_handle());
}

// static
void PlatformThread::Detach(PlatformThreadHandle thread_handle) {
  CloseHandle(thread_handle.platform_handle());
}

// static
bool PlatformThread::CanChangeThreadType(ThreadType from, ThreadType to) {
  return true;
}

namespace {

void SetCurrentThreadPriority(ThreadType thread_type,
                              MessagePumpType pump_type_hint) {
  if (thread_type == ThreadType::kDisplayCritical &&
      pump_type_hint == MessagePumpType::UI &&
      !g_above_normal_compositing_browser) {
    // Ignore kDisplayCritical thread type for UI thread as Windows has a
    // priority boost mechanism. See
    // https://docs.microsoft.com/en-us/windows/win32/procthread/priority-boosts
    return;
  }

  PlatformThreadHandle::Handle thread_handle =
      PlatformThread::CurrentHandle().platform_handle();

  if (!g_use_thread_priority_lowest && thread_type != ThreadType::kBackground) {
    // Exit background mode if the new priority is not BACKGROUND. This is a
    // no-op if not in background mode.
    ::SetThreadPriority(thread_handle, THREAD_MODE_BACKGROUND_END);
    // We used to DCHECK that memory priority is MEMORY_PRIORITY_NORMAL here,
    // but found that it is not always the case (e.g. in the installer).
    // crbug.com/1340578#c2
  }

  int desired_priority = THREAD_PRIORITY_ERROR_RETURN;
  switch (thread_type) {
    case ThreadType::kBackground:
      // Using THREAD_MODE_BACKGROUND_BEGIN instead of THREAD_PRIORITY_LOWEST
      // improves input latency and navigation time. See
      // https://docs.google.com/document/d/16XrOwuwTwKWdgPbcKKajTmNqtB4Am8TgS9GjbzBYLc0
      //
      // MSDN recommends THREAD_MODE_BACKGROUND_BEGIN for threads that perform
      // background work, as it reduces disk and memory priority in addition to
      // CPU priority.
      desired_priority =
          g_use_thread_priority_lowest.load(std::memory_order_relaxed)
              ? THREAD_PRIORITY_LOWEST
              : THREAD_MODE_BACKGROUND_BEGIN;
      break;
    case ThreadType::kUtility:
      desired_priority = THREAD_PRIORITY_BELOW_NORMAL;
      break;
    case ThreadType::kResourceEfficient:
    case ThreadType::kDefault:
      desired_priority = THREAD_PRIORITY_NORMAL;
      break;
    case ThreadType::kDisplayCritical:
      desired_priority = THREAD_PRIORITY_ABOVE_NORMAL;
      break;
    case ThreadType::kRealtimeAudio:
      desired_priority = THREAD_PRIORITY_TIME_CRITICAL;
      break;
  }
  DCHECK_NE(desired_priority, THREAD_PRIORITY_ERROR_RETURN);

  [[maybe_unused]] const BOOL cpu_priority_success =
      ::SetThreadPriority(thread_handle, desired_priority);
  DPLOG_IF(ERROR, !cpu_priority_success)
      << "Failed to set thread priority to " << desired_priority;

  if (desired_priority == THREAD_MODE_BACKGROUND_BEGIN) {
    // Override the memory priority.
    MEMORY_PRIORITY_INFORMATION memory_priority{.MemoryPriority =
                                                    MEMORY_PRIORITY_NORMAL};
    [[maybe_unused]] const BOOL memory_priority_success =
        SetThreadInformation(thread_handle, ::ThreadMemoryPriority,
                             &memory_priority, sizeof(memory_priority));
    DPLOG_IF(ERROR, !memory_priority_success)
        << "Set thread memory priority failed.";
  }

  if (!g_use_thread_priority_lowest && thread_type == ThreadType::kBackground) {
    // In a background process, THREAD_MODE_BACKGROUND_BEGIN lowers the memory
    // and I/O priorities but not the CPU priority (kernel bug?). Use
    // THREAD_PRIORITY_LOWEST to also lower the CPU priority.
    // https://crbug.com/901483
    if (PlatformThread::GetCurrentThreadPriorityForTest() !=
        ThreadPriorityForTest::kBackground) {
      ::SetThreadPriority(thread_handle, THREAD_PRIORITY_LOWEST);
      // We used to DCHECK that memory priority is MEMORY_PRIORITY_VERY_LOW
      // here, but found that it is not always the case (e.g. in the installer).
      // crbug.com/1340578#c2
    }
  }
}

void SetCurrentThreadQualityOfService(ThreadType thread_type) {
  // QoS and power throttling were introduced in Win10 1709.
  bool desire_ecoqos = false;
  switch (thread_type) {
    case ThreadType::kBackground:
    case ThreadType::kUtility:
    case ThreadType::kResourceEfficient:
      desire_ecoqos = true;
      break;
    case ThreadType::kDefault:
    case ThreadType::kDisplayCritical:
    case ThreadType::kRealtimeAudio:
      desire_ecoqos = false;
      break;
  }

  THREAD_POWER_THROTTLING_STATE thread_power_throttling_state{
      .Version = THREAD_POWER_THROTTLING_CURRENT_VERSION,
      .ControlMask =
          desire_ecoqos ? THREAD_POWER_THROTTLING_EXECUTION_SPEED : 0ul,
      .StateMask =
          desire_ecoqos ? THREAD_POWER_THROTTLING_EXECUTION_SPEED : 0ul,
  };
  [[maybe_unused]] const BOOL success = ::SetThreadInformation(
      ::GetCurrentThread(), ::ThreadPowerThrottling,
      &thread_power_throttling_state, sizeof(thread_power_throttling_state));
  // Failure is expected on versions of Windows prior to RS3.
  DPLOG_IF(ERROR, !success && win::GetVersion() >= win::Version::WIN10_RS3)
      << "Failed to set EcoQoS to " << std::boolalpha << desire_ecoqos;
}

}  // namespace

namespace internal {

void SetCurrentThreadTypeImpl(ThreadType thread_type,
                              MessagePumpType pump_type_hint) {
  SetCurrentThreadPriority(thread_type, pump_type_hint);
  SetCurrentThreadQualityOfService(thread_type);
}

}  // namespace internal

// static
ThreadPriorityForTest PlatformThread::GetCurrentThreadPriorityForTest() {
  static_assert(
      THREAD_PRIORITY_IDLE < 0,
      "THREAD_PRIORITY_IDLE is >= 0 and will incorrectly cause errors.");
  static_assert(
      THREAD_PRIORITY_LOWEST < 0,
      "THREAD_PRIORITY_LOWEST is >= 0 and will incorrectly cause errors.");
  static_assert(THREAD_PRIORITY_BELOW_NORMAL < 0,
                "THREAD_PRIORITY_BELOW_NORMAL is >= 0 and will incorrectly "
                "cause errors.");
  static_assert(
      THREAD_PRIORITY_NORMAL == 0,
      "The logic below assumes that THREAD_PRIORITY_NORMAL is zero. If it is "
      "not, ThreadPriorityForTest::kBackground may be incorrectly detected.");
  static_assert(THREAD_PRIORITY_ABOVE_NORMAL >= 0,
                "THREAD_PRIORITY_ABOVE_NORMAL is < 0 and will incorrectly be "
                "translated to ThreadPriorityForTest::kBackground.");
  static_assert(THREAD_PRIORITY_HIGHEST >= 0,
                "THREAD_PRIORITY_HIGHEST is < 0 and will incorrectly be "
                "translated to ThreadPriorityForTest::kBackground.");
  static_assert(THREAD_PRIORITY_TIME_CRITICAL >= 0,
                "THREAD_PRIORITY_TIME_CRITICAL is < 0 and will incorrectly be "
                "translated to ThreadPriorityForTest::kBackground.");
  static_assert(THREAD_PRIORITY_ERROR_RETURN >= 0,
                "THREAD_PRIORITY_ERROR_RETURN is < 0 and will incorrectly be "
                "translated to ThreadPriorityForTest::kBackground.");

  const int priority =
      ::GetThreadPriority(PlatformThread::CurrentHandle().platform_handle());

  // Negative values represent a background priority. We have observed -3, -4,
  // -6 when THREAD_MODE_BACKGROUND_* is used. THREAD_PRIORITY_IDLE,
  // THREAD_PRIORITY_LOWEST and THREAD_PRIORITY_BELOW_NORMAL are other possible
  // negative values.
  if (priority < THREAD_PRIORITY_BELOW_NORMAL)
    return ThreadPriorityForTest::kBackground;

  switch (priority) {
    case THREAD_PRIORITY_BELOW_NORMAL:
      return ThreadPriorityForTest::kUtility;
    case THREAD_PRIORITY_NORMAL:
      return ThreadPriorityForTest::kNormal;
    case kWinDisplayPriority1:
      [[fallthrough]];
    case kWinDisplayPriority2:
      return ThreadPriorityForTest::kDisplay;
    case THREAD_PRIORITY_ABOVE_NORMAL:
    case THREAD_PRIORITY_HIGHEST:
      return ThreadPriorityForTest::kDisplay;
    case THREAD_PRIORITY_TIME_CRITICAL:
      return ThreadPriorityForTest::kRealtimeAudio;
    case THREAD_PRIORITY_ERROR_RETURN:
      DPCHECK(false) << "::GetThreadPriority error";
  }

  NOTREACHED() << "::GetThreadPriority returned " << priority << ".";
}

void InitializePlatformThreadFeatures() {
  g_use_thread_priority_lowest.store(
      FeatureList::IsEnabled(kUseThreadPriorityLowest),
      std::memory_order_relaxed);
  g_above_normal_compositing_browser.store(
      FeatureList::IsEnabled(kAboveNormalCompositingBrowserWin),
      std::memory_order_relaxed);
}

// static
size_t PlatformThread::GetDefaultThreadStackSize() {
  return 0;
}

}  // namespace base
