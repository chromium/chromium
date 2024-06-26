// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <tuple>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_NACL)
#include "base/posix/can_lower_nice_to.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <sys/syscall.h>
#include <atomic>
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/thread.h>

#include "base/fuchsia/koid.h"
#else
#include <sys/resource.h>
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/stack/stack.h"
#endif

namespace base {

void InitThreading();
void TerminateOnThread();
size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes);

namespace {

struct ThreadParams {
  ThreadParams() = default;

  raw_ptr<PlatformThread::Delegate> delegate = nullptr;
  bool joinable = false;
  ThreadType thread_type = ThreadType::kDefault;
  MessagePumpType message_pump_type = MessagePumpType::DEFAULT;
};

void* ThreadFunc(void* params) {
  PlatformThread::Delegate* delegate = nullptr;

  {
    std::unique_ptr<ThreadParams> thread_params(
        static_cast<ThreadParams*>(params));

    delegate = thread_params->delegate;
    if (!thread_params->joinable)
      base::DisallowSingleton();

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    partition_alloc::internal::StackTopRegistry::Get().NotifyThreadCreated();
#endif

#if !BUILDFLAG(IS_NACL)
#if BUILDFLAG(IS_APPLE)
    PlatformThread::SetCurrentThreadRealtimePeriodValue(
        delegate->GetRealtimePeriod());
#endif

    // Threads on linux/android may inherit their priority from the thread
    // where they were created. This explicitly sets the priority of all new
    // threads.
    PlatformThread::SetCurrentThreadType(thread_params->thread_type);
#endif  //  !BUILDFLAG(IS_NACL)
  }

  ThreadIdNameManager::GetInstance()->RegisterThread(
      PlatformThread::CurrentHandle().platform_handle(),
      PlatformThread::CurrentId());

  delegate->ThreadMain();

  ThreadIdNameManager::GetInstance()->RemoveName(
      PlatformThread::CurrentHandle().platform_handle(),
      PlatformThread::CurrentId());

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  partition_alloc::internal::StackTopRegistry::Get().NotifyThreadDestroyed();
#endif

  base::TerminateOnThread();
  return nullptr;
}

bool CreateThread(size_t stack_size,
                  bool joinable,
                  PlatformThread::Delegate* delegate,
                  PlatformThreadHandle* thread_handle,
                  ThreadType thread_type,
                  MessagePumpType message_pump_type) {
  DCHECK(thread_handle);
  base::InitThreading();

  pthread_attr_t attributes;
  pthread_attr_init(&attributes);

  // Pthreads are joinable by default, so only specify the detached
  // attribute if the thread should be non-joinable.
  if (!joinable)
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);

  // Get a better default if available.
  if (stack_size == 0)
    stack_size = base::GetDefaultThreadStackSize(attributes);

  if (stack_size > 0)
    pthread_attr_setstacksize(&attributes, stack_size);

  std::unique_ptr<ThreadParams> params(new ThreadParams);
  params->delegate = delegate;
  params->joinable = joinable;
  params->thread_type = thread_type;
  params->message_pump_type = message_pump_type;

  pthread_t handle;
  int err = pthread_create(&handle, &attributes, ThreadFunc, params.get());
  bool success = !err;
  if (success) {
    // ThreadParams should be deleted on the created thread after used.
    std::ignore = params.release();
  } else {
    // Value of |handle| is undefined if pthread_create fails.
    handle = 0;
    errno = err;
    PLOG(ERROR) << "pthread_create";
  }
  *thread_handle = PlatformThreadHandle(handle);

  pthread_attr_destroy(&attributes);

  return success;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Store the thread ids in local storage since calling the SWI can be
// expensive and PlatformThread::CurrentId is used liberally.
thread_local pid_t g_thread_id = -1;

// A boolean value that indicates that the value stored in |g_thread_id| on the
// main thread is invalid, because it hasn't been updated since the process
// forked.
//
// This used to work by setting |g_thread_id| to -1 in a pthread_atfork handler.
// However, when a multithreaded process forks, it is only allowed to call
// async-signal-safe functions until it calls an exec() syscall. However,
// accessing TLS may allocate (see crbug.com/1275748), which is not
// async-signal-safe and therefore causes deadlocks, corruption, and crashes.
//
// It's Atomic to placate TSAN.
std::atomic<bool> g_main_thread_tid_cache_valid = false;

// Tracks whether the current thread is the main thread, and therefore whether
// |g_main_thread_tid_cache_valid| is relevant for the current thread. This is
// also updated by PlatformThread::CurrentId().
thread_local bool g_is_main_thread = true;

class InitAtFork {
 public:
  InitAtFork() {
    pthread_atfork(nullptr, nullptr, internal::InvalidateTidCache);
  }
};

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace internal {

void InvalidateTidCache() {
  g_main_thread_tid_cache_valid.store(false, std::memory_order_relaxed);
}

}  // namespace internal

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// static
PlatformThreadId PlatformThreadBase::CurrentId() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if BUILDFLAG(IS_APPLE)
  return pthread_mach_thread_np(pthread_self());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Workaround false-positive MSAN use-of-uninitialized-value on
  // thread_local storage for loaded libraries:
  // https://github.com/google/sanitizers/issues/1265
  MSAN_UNPOISON(&g_thread_id, sizeof(pid_t));
  MSAN_UNPOISON(&g_is_main_thread, sizeof(bool));
  static InitAtFork init_at_fork;
  if (g_thread_id == -1 ||
      (g_is_main_thread &&
       !g_main_thread_tid_cache_valid.load(std::memory_order_relaxed))) {
    // Update the cached tid.
    g_thread_id = static_cast<pid_t>(syscall(__NR_gettid));
    // If this is the main thread, we can mark the tid_cache as valid.
    // Otherwise, stop the current thread from always entering this slow path.
    if (g_thread_id == getpid()) {
      g_main_thread_tid_cache_valid.store(true, std::memory_order_relaxed);
    } else {
      g_is_main_thread = false;
    }
  } else {
#if DCHECK_IS_ON()
    if (g_thread_id != syscall(__NR_gettid)) {
      RAW_LOG(
          FATAL,
          "Thread id stored in TLS is different from thread id returned by "
          "the system. It is likely that the process was forked without going "
          "through fork().");
    }
#endif
  }
  return g_thread_id;
#elif BUILDFLAG(IS_ANDROID)
  // Note: do not cache the return value inside a thread_local variable on
  // Android (as above). The reasons are:
  // - thread_local is slow on Android (goes through emutls)
  // - gettid() is fast, since its return value is cached in pthread (in the
  //   thread control block of pthread). See gettid.c in bionic.
  return gettid();
#elif BUILDFLAG(IS_FUCHSIA)
  thread_local static zx_koid_t id =
      GetKoid(*zx::thread::self()).value_or(ZX_KOID_INVALID);
  return id;
#elif BUILDFLAG(IS_SOLARIS) || BUILDFLAG(IS_QNX)
  return pthread_self();
#elif BUILDFLAG(IS_NACL) && defined(__GLIBC__)
  return pthread_self();
#elif BUILDFLAG(IS_NACL) && !defined(__GLIBC__)
  // Pointers are 32-bits in NaCl.
  return reinterpret_cast<int32_t>(pthread_self());
#elif BUILDFLAG(IS_POSIX) && BUILDFLAG(IS_AIX)
  return pthread_self();
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_AIX)
  return reinterpret_cast<int64_t>(pthread_self());
#endif
}

// static
PlatformThreadRef PlatformThreadBase::CurrentRef() {
  return PlatformThreadRef(pthread_self());
}

// static
PlatformThreadHandle PlatformThreadBase::CurrentHandle() {
  return PlatformThreadHandle(pthread_self());
}

#if !BUILDFLAG(IS_APPLE)
// static
void PlatformThreadBase::YieldCurrentThread() {
  sched_yield();
}
#endif  // !BUILDFLAG(IS_APPLE)

// static
void PlatformThreadBase::Sleep(TimeDelta duration) {
  struct timespec sleep_time, remaining;

  // Break the duration into seconds and nanoseconds.
  // NOTE: TimeDelta's microseconds are int64s while timespec's
  // nanoseconds are longs, so this unpacking must prevent overflow.
  sleep_time.tv_sec = static_cast<time_t>(duration.InSeconds());
  duration -= Seconds(sleep_time.tv_sec);
  sleep_time.tv_nsec = static_cast<long>(duration.InMicroseconds() * 1000);

  while (nanosleep(&sleep_time, &remaining) == -1 && errno == EINTR)
    sleep_time = remaining;
}

// static
const char* PlatformThreadBase::GetName() {
  return ThreadIdNameManager::GetInstance()->GetName(CurrentId());
}

// static
bool PlatformThreadBase::CreateWithType(size_t stack_size,
                                    Delegate* delegate,
                                    PlatformThreadHandle* thread_handle,
                                    ThreadType thread_type,
                                    MessagePumpType pump_type_hint) {
  return CreateThread(stack_size, true /* joinable thread */, delegate,
                      thread_handle, thread_type, pump_type_hint);
}

// static
bool PlatformThreadBase::CreateNonJoinable(size_t stack_size, Delegate* delegate) {
  return CreateNonJoinableWithType(stack_size, delegate, ThreadType::kDefault);
}

// static
bool PlatformThreadBase::CreateNonJoinableWithType(size_t stack_size,
                                               Delegate* delegate,
                                               ThreadType thread_type,
                                               MessagePumpType pump_type_hint) {
  PlatformThreadHandle unused;

  bool result = CreateThread(stack_size, false /* non-joinable thread */,
                             delegate, &unused, thread_type, pump_type_hint);
  return result;
}

// static
void PlatformThreadBase::Join(PlatformThreadHandle thread_handle) {
  // Joining another thread may block the current thread for a long time, since
  // the thread referred to by |thread_handle| may still be running long-lived /
  // blocking tasks.
  base::internal::ScopedBlockingCallWithBaseSyncPrimitives scoped_blocking_call(
      FROM_HERE, base::BlockingType::MAY_BLOCK);
  CHECK_EQ(0, pthread_join(thread_handle.platform_handle(), nullptr));
}

// static
void PlatformThreadBase::Detach(PlatformThreadHandle thread_handle) {
  CHECK_EQ(0, pthread_detach(thread_handle.platform_handle()));
}

// Mac and Fuchsia have their own SetCurrentThreadType() and
// GetCurrentThreadPriorityForTest() implementations.
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_FUCHSIA)

// static
bool PlatformThreadBase::CanChangeThreadType(ThreadType from, ThreadType to) {
#if BUILDFLAG(IS_NACL)
  return false;
#else
  if (from >= to) {
    // Decreasing thread priority on POSIX is always allowed.
    return true;
  }
  if (to == ThreadType::kRealtimeAudio) {
    return internal::CanSetThreadTypeToRealtimeAudio();
  }

  return internal::CanLowerNiceTo(internal::ThreadTypeToNiceValue(to));
#endif  // BUILDFLAG(IS_NACL)
}

namespace internal {

void SetCurrentThreadTypeImpl(ThreadType thread_type,
                              MessagePumpType pump_type_hint) {
#if BUILDFLAG(IS_NACL)
  NOTIMPLEMENTED();
#else
  if (internal::SetCurrentThreadTypeForPlatform(thread_type, pump_type_hint))
    return;

  // setpriority(2) should change the whole thread group's (i.e. process)
  // priority. However, as stated in the bugs section of
  // http://man7.org/linux/man-pages/man2/getpriority.2.html: "under the current
  // Linux/NPTL implementation of POSIX threads, the nice value is a per-thread
  // attribute". Also, 0 is prefered to the current thread id since it is
  // equivalent but makes sandboxing easier (https://crbug.com/399473).
  const int nice_setting = internal::ThreadTypeToNiceValue(thread_type);
  if (setpriority(PRIO_PROCESS, 0, nice_setting)) {
    DVPLOG(1) << "Failed to set nice value of thread ("
              << PlatformThread::CurrentId() << ") to " << nice_setting;
  }
#endif  // BUILDFLAG(IS_NACL)
}

}  // namespace internal

// static
ThreadPriorityForTest PlatformThreadBase::GetCurrentThreadPriorityForTest() {
#if BUILDFLAG(IS_NACL)
  NOTIMPLEMENTED();
  return ThreadPriorityForTest::kNormal;
#else
  // Mirrors SetCurrentThreadPriority()'s implementation.
  auto platform_specific_priority =
      internal::GetCurrentThreadPriorityForPlatformForTest();  // IN-TEST
  if (platform_specific_priority)
    return platform_specific_priority.value();

  int nice_value = internal::GetCurrentThreadNiceValue();

  return internal::NiceValueToThreadPriorityForTest(nice_value);  // IN-TEST
#endif  // !BUILDFLAG(IS_NACL)
}

#endif  // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_FUCHSIA)

// static
size_t PlatformThreadBase::GetDefaultThreadStackSize() {
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  return base::GetDefaultThreadStackSize(attributes);
}

}  // namespace base
