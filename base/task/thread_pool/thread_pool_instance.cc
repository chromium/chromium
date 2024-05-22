// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_pool_instance.h"

#include <algorithm>
#include <string_view>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

namespace {

// |g_thread_pool| is intentionally leaked on shutdown.
ThreadPoolInstance* g_thread_pool = nullptr;

size_t GetDefaultMaxNumUtilityThreads(size_t max_num_foreground_threads_in) {
  int num_of_efficient_processors = SysInfo::NumberOfEfficientProcessors();
  if (num_of_efficient_processors != 0) {
    DCHECK_GT(num_of_efficient_processors, 0);
    return std::max<size_t>(
        2, std::min(max_num_foreground_threads_in,
                    static_cast<size_t>(num_of_efficient_processors)));
  }
  return std::max<size_t>(2, max_num_foreground_threads_in / 2);
}

}  // namespace

ThreadPoolInstance::InitParams::InitParams(size_t max_num_foreground_threads_in)
    : max_num_foreground_threads(max_num_foreground_threads_in),
      max_num_utility_threads(
          GetDefaultMaxNumUtilityThreads(max_num_foreground_threads_in)) {}

ThreadPoolInstance::InitParams::InitParams(size_t max_num_foreground_threads_in,
                                           size_t max_num_utility_threads_in)
    : max_num_foreground_threads(max_num_foreground_threads_in),
      max_num_utility_threads(max_num_utility_threads_in) {}

ThreadPoolInstance::InitParams::~InitParams() = default;

ThreadPoolInstance::ScopedExecutionFence::ScopedExecutionFence() {
  DCHECK(g_thread_pool);
  g_thread_pool->BeginFence();
}

ThreadPoolInstance::ScopedExecutionFence::~ScopedExecutionFence() {
  DCHECK(g_thread_pool);
  g_thread_pool->EndFence();
}

ThreadPoolInstance::ScopedBestEffortExecutionFence::
    ScopedBestEffortExecutionFence() {
  DCHECK(g_thread_pool);
  g_thread_pool->BeginBestEffortFence();
}

ThreadPoolInstance::ScopedBestEffortExecutionFence::
    ~ScopedBestEffortExecutionFence() {
  DCHECK(g_thread_pool);
  g_thread_pool->EndBestEffortFence();
}

ThreadPoolInstance::ScopedRestrictedTasks::ScopedRestrictedTasks() {
  DCHECK(g_thread_pool);
  g_thread_pool->BeginRestrictedTasks();
}

ThreadPoolInstance::ScopedRestrictedTasks::~ScopedRestrictedTasks() {
  DCHECK(g_thread_pool);
  g_thread_pool->EndRestrictedTasks();
}

ThreadPoolInstance::ScopedFizzleBlockShutdownTasks::
    ScopedFizzleBlockShutdownTasks() {
  // It's possible for this to be called without a ThreadPool present in tests.
  if (g_thread_pool)
    g_thread_pool->BeginFizzlingBlockShutdownTasks();
}

ThreadPoolInstance::ScopedFizzleBlockShutdownTasks::
    ~ScopedFizzleBlockShutdownTasks() {
  // It's possible for this to be called without a ThreadPool present in tests.
  if (g_thread_pool)
    g_thread_pool->EndFizzlingBlockShutdownTasks();
}

#if !BUILDFLAG(IS_NACL)
// static
void ThreadPoolInstance::CreateAndStartWithDefaultParams(
    std::string_view name) {
  Create(name);
  g_thread_pool->StartWithDefaultParams();
}

void ThreadPoolInstance::StartWithDefaultParams() {
  // Values were chosen so that:
  // * There are few background threads.
  // * Background threads never outnumber foreground threads.
  // * The system is utilized maximally by foreground threads.
  // * The main thread is assumed to be busy, cap foreground workers at
  //   |num_cores - 1|.
  const size_t max_num_foreground_threads =
      static_cast<size_t>(std::max(3, SysInfo::NumberOfProcessors() - 1));
  Start({max_num_foreground_threads});
}
#endif  // !BUILDFLAG(IS_NACL)

void ThreadPoolInstance::Create(std::string_view name) {
  Set(std::make_unique<internal::ThreadPoolImpl>(name));
}

// static
void ThreadPoolInstance::Set(std::unique_ptr<ThreadPoolInstance> thread_pool) {
  delete g_thread_pool;
  g_thread_pool = thread_pool.release();
}

// static
ThreadPoolInstance* ThreadPoolInstance::Get() {
  return g_thread_pool;
}

}  // namespace base
