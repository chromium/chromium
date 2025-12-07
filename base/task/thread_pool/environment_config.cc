// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/environment_config.h"

#include "base/base_switches.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/lock_impl.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/background_thread_pool_field_trial.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace base::internal {
namespace {

bool CanUseBackgroundThreadTypeForWorkerThreadImpl() {
#if BUILDFLAG(IS_ANDROID)
  return base::android::BackgroundThreadPoolFieldTrial::
      ShouldUseBackgroundThreadPool();
#else   // BUILDFLAG(IS_ANDROID)
  // When Lock doesn't handle multiple thread priorities, run all
  // WorkerThread with a normal priority to avoid priority inversion when a
  // thread running with a normal priority tries to acquire a lock held by a
  // thread running with a background priority.
  if (!Lock::HandlesMultipleThreadPriorities()) {
    return false;
  }

  // When thread type can't be increased to kNormal, run all threads with a
  // kNormal thread type to avoid priority inversions on shutdown
  // (ThreadPoolImpl increases kBackground threads type to kNormal on shutdown
  // while resolving remaining shutdown blocking tasks).
  //
  // This is ignored on Android, because it doesn't have a clean shutdown phase.
  if (!PlatformThread::CanChangeThreadType(ThreadType::kBackground,
                                           ThreadType::kDefault)) {
    return false;
  }

  return true;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool CanUseUtilityThreadTypeForWorkerThreadImpl() {
#if !BUILDFLAG(IS_ANDROID)
  // Same as CanUseBackgroundThreadTypeForWorkerThreadImpl()
  if (!PlatformThread::CanChangeThreadType(ThreadType::kUtility,
                                           ThreadType::kDefault)) {
    return false;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  return true;
}

}  // namespace

bool CanUseBackgroundThreadTypeForWorkerThread() {
  static const bool can_use_background_thread_type_for_worker_thread =
      CanUseBackgroundThreadTypeForWorkerThreadImpl();
  return can_use_background_thread_type_for_worker_thread;
}

bool CanUseUtilityThreadTypeForWorkerThread() {
  static const bool can_use_utility_thread_type_for_worker_thread =
      CanUseUtilityThreadTypeForWorkerThreadImpl();
  return can_use_utility_thread_type_for_worker_thread;
}

}  // namespace base::internal
