// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/sampling_heap_profiler/sampling_heap_profiler.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/allocator/dispatcher/tls.h"
#include "base/compiler_specific.h"
#include "base/containers/to_vector.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/threading/thread_local_storage.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"  // no-presubmit-check
#include "build/build_config.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/shim/allocator_shim.h"

#if BUILDFLAG(IS_APPLE)
#include <pthread.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <sys/prctl.h>
#endif

namespace base {

constexpr uint32_t kMaxStackEntries = 256;

namespace {

struct ThreadLocalData {
  const char* thread_name = nullptr;
};

ThreadLocalData* GetThreadLocalData() {
#if USE_LOCAL_TLS_EMULATION()
  static base::NoDestructor<
      base::allocator::dispatcher::ThreadLocalStorage<ThreadLocalData>>
      thread_local_data("sampling_heap_profiler");
  return thread_local_data->GetThreadLocalData();
#else
  static thread_local ThreadLocalData thread_local_data;
  return &thread_local_data;
#endif
}

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
BASE_FEATURE(kAvoidFramePointers,
             "AndroidHeapSamplerAvoidFramePointers",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

using StackUnwinder = SamplingHeapProfiler::StackUnwinder;
using base::allocator::dispatcher::AllocationSubsystem;

// If a thread name has been set from ThreadIdNameManager, use that. Otherwise,
// gets the thread name from kernel if available or returns a string with id.
// This function intentionally leaks the allocated strings since they are used
// to tag allocations even after the thread dies.
const char* GetAndLeakThreadName() {
  const char* thread_name =
      base::ThreadIdNameManager::GetInstance()->GetNameForCurrentThread();
  if (thread_name && *thread_name != '\0')
    return thread_name;

  // prctl requires 16 bytes, snprintf requires 19, pthread_getname_np requires
  // 64 on macOS, see PlatformThread::SetName in platform_thread_apple.mm.
  constexpr size_t kBufferLen = 64;
  char name[kBufferLen];
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // If the thread name is not set, try to get it from prctl. Thread name might
  // not be set in cases where the thread started before heap profiling was
  // enabled.
  int err = prctl(PR_GET_NAME, name);
  if (!err)
    return strdup(name);
#elif BUILDFLAG(IS_APPLE)
  int err = pthread_getname_np(pthread_self(), name, kBufferLen);
  if (err == 0 && *name != '\0')
    return strdup(name);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

  // Use tid if we don't have a thread name.
  snprintf(name, sizeof(name), "Thread %lu",
           static_cast<unsigned long>(base::PlatformThread::CurrentId()));
  return strdup(name);
}

const char* UpdateAndGetThreadName(const char* name) {
  ThreadLocalData* const thread_local_data = GetThreadLocalData();
  if (name)
    thread_local_data->thread_name = name;
  if (!thread_local_data->thread_name) {
    thread_local_data->thread_name = GetAndLeakThreadName();
  }
  return thread_local_data->thread_name;
}

// Checks whether unwinding from this function works.
[[maybe_unused]] StackUnwinder CheckForDefaultUnwindTables() {
  const void* stack[kMaxStackEntries];
  size_t frame_count = base::debug::CollectStackTrace(stack);
  // First frame is the current function and can be found without unwind tables.
  return frame_count > 1 ? StackUnwinder::kDefault
                         : StackUnwinder::kUnavailable;
}

StackUnwinder ChooseStackUnwinder() {
#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
  // Use frame pointers if available, since they can be faster than the default.
  if (!base::FeatureList::IsEnabled(kAvoidFramePointers)) {
    return StackUnwinder::kFramePointers;
  }
#endif
#if BUILDFLAG(IS_ANDROID)
  // Default unwind tables aren't always present on Android.
  return CheckForDefaultUnwindTables();
#else
  return StackUnwinder::kDefault;
#endif
}

}  // namespace

SamplingHeapProfiler::Sample::Sample(size_t size,
                                     size_t total,
                                     uint32_t ordinal)
    : size(size), total(total), ordinal(ordinal) {}

SamplingHeapProfiler::Sample::Sample(const Sample&) = default;
SamplingHeapProfiler::Sample::~Sample() = default;

SamplingHeapProfiler::SamplingHeapProfiler() = default;
SamplingHeapProfiler::~SamplingHeapProfiler() {
  if (record_thread_names_)
    base::ThreadIdNameManager::GetInstance()->RemoveObserver(this);
}

uint32_t SamplingHeapProfiler::Start() {
  const auto unwinder = ChooseStackUnwinder();
#if BUILDFLAG(IS_ANDROID)
  // Record which unwinder is in use on Android, since it's hard to keep track
  // of which methods are available at runtime.
  base::UmaHistogramEnumeration("HeapProfiling.AndroidStackUnwinder", unwinder);
#endif
  if (unwinder == StackUnwinder::kUnavailable) {
    LOG(WARNING) << "Sampling heap profiler: Stack unwinding is not available.";
    return 0;
  }
  unwinder_.store(unwinder);

  AutoLock lock(start_stop_mutex_);
  if (!running_sessions_++)
    PoissonAllocationSampler::Get()->AddSamplesObserver(this);
  return last_sample_ordinal_;
}

void SamplingHeapProfiler::Stop() {
  AutoLock lock(start_stop_mutex_);
  DCHECK_GT(running_sessions_, 0);
  if (!--running_sessions_)
    PoissonAllocationSampler::Get()->RemoveSamplesObserver(this);
}

void SamplingHeapProfiler::SetSamplingInterval(size_t sampling_interval_bytes) {
  PoissonAllocationSampler::Get()->SetSamplingInterval(sampling_interval_bytes);
}

void SamplingHeapProfiler::SetRecordThreadNames(bool value) {
  if (record_thread_names_ == value)
    return;
  record_thread_names_ = value;
  if (value) {
    base::ThreadIdNameManager::GetInstance()->AddObserver(this);
  } else {
    base::ThreadIdNameManager::GetInstance()->RemoveObserver(this);
  }
}

// static
const char* SamplingHeapProfiler::CachedThreadName() {
  return UpdateAndGetThreadName(nullptr);
}

span<const void*> SamplingHeapProfiler::CaptureStackTrace(
    span<const void*> frames) {
  size_t skip_frames = 0;
  size_t frame_count = 0;
  switch (unwinder_) {
#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
    case StackUnwinder::kFramePointers:
      frame_count = base::debug::TraceStackFramePointers(frames, skip_frames);
      return frames.first(frame_count);
#endif
    case StackUnwinder::kDefault:
      // Fall-back to capturing the stack with base::debug::CollectStackTrace,
      // which is likely slower, but more reliable.
      frame_count = base::debug::CollectStackTrace(frames);
      // Skip top frames as they correspond to the profiler itself.
      skip_frames = std::min(frame_count, size_t{3});
      return frames.first(frame_count).subspan(skip_frames);
    default:
      // Profiler should not be started if ChooseStackUnwinder() returns
      // anything else.
      NOTREACHED();
  }
}

void SamplingHeapProfiler::SampleAdded(void* address,
                                       size_t size,
                                       size_t total,
                                       AllocationSubsystem type,
                                       const char* context) {
  // CaptureStack and allocation context tracking may use TLS.
  // Bail out if it has been destroyed.
  if (base::ThreadLocalStorage::HasBeenDestroyed()) [[unlikely]] {
    return;
  }
  DCHECK(PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());
  Sample sample(size, total, ++last_sample_ordinal_);
  sample.allocator = type;
  CaptureNativeStack(context, &sample);
  AutoLock lock(mutex_);
  if (PoissonAllocationSampler::AreHookedSamplesMuted() &&
      type != AllocationSubsystem::kManualForTesting) [[unlikely]] {
    // Throw away any non-test samples that were being collected before
    // ScopedMuteHookedSamplesForTesting was enabled. This is done inside the
    // lock to catch any samples that were being collected while
    // MuteHookedSamplesForTesting is running.
    return;
  }
  RecordString(sample.context);

  // If a sample is already present with the same address, then that means that
  // the sampling heap profiler failed to observe the destruction -- possibly
  // because the sampling heap profiler was temporarily disabled. We should
  // override the old entry.
  samples_.insert_or_assign(address, std::move(sample));
}

void SamplingHeapProfiler::CaptureNativeStack(const char* context,
                                              Sample* sample) {
  const void* stack[kMaxStackEntries];
  span<const void*> frames = CaptureStackTrace(
      // One frame is reserved for the thread name.
      base::span(stack).first(kMaxStackEntries - 1));
  sample->stack = ToVector(frames);

  if (record_thread_names_)
    sample->thread_name = CachedThreadName();

  if (!context) {
    const auto* tracker =
        trace_event::AllocationContextTracker::GetInstanceForCurrentThread();
    if (tracker)
      context = tracker->TaskContext();
  }
  sample->context = context;
}

const char* SamplingHeapProfiler::RecordString(const char* string) {
  return string ? *strings_.insert(string).first : nullptr;
}

void SamplingHeapProfiler::SampleRemoved(void* address) {
  DCHECK(base::PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());
  base::AutoLock lock(mutex_);
  samples_.erase(address);
}

std::vector<SamplingHeapProfiler::Sample> SamplingHeapProfiler::GetSamples(
    uint32_t profile_id) {
  // Make sure the sampler does not invoke |SampleAdded| or |SampleRemoved|
  // on this thread. Otherwise it could have end up with a deadlock.
  // See crbug.com/882495
  PoissonAllocationSampler::ScopedMuteThreadSamples no_samples_scope;
  AutoLock lock(mutex_);
  std::vector<Sample> samples;
  samples.reserve(samples_.size());
  for (auto& it : samples_) {
    Sample& sample = it.second;
    if (sample.ordinal > profile_id)
      samples.push_back(sample);
  }
  return samples;
}

std::vector<const char*> SamplingHeapProfiler::GetStrings() {
  PoissonAllocationSampler::ScopedMuteThreadSamples no_samples_scope;
  AutoLock lock(mutex_);
  return std::vector<const char*>(strings_.begin(), strings_.end());
}

// static
void SamplingHeapProfiler::Init() {
  GetThreadLocalData();
  PoissonAllocationSampler::Init();
}

// static
SamplingHeapProfiler* SamplingHeapProfiler::Get() {
  static NoDestructor<SamplingHeapProfiler> instance;
  return instance.get();
}

void SamplingHeapProfiler::OnThreadNameChanged(const char* name) {
  UpdateAndGetThreadName(name);
}

PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting
SamplingHeapProfiler::MuteHookedSamplesForTesting() {
  // Only one ScopedMuteHookedSamplesForTesting can exist at a time.
  CHECK(!PoissonAllocationSampler::AreHookedSamplesMuted());
  PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting
      mute_hooked_samples;

  base::AutoLock lock(mutex_);
  samples_.clear();
  // Since hooked samples are muted, any samples that are waiting to take the
  // lock in SampleAdded will be discarded. Tests can now call
  // PoissonAllocationSampler::RecordAlloc with allocator type kManualForTesting
  // to add samples cleanly.
  return mute_hooked_samples;
}

}  // namespace base
