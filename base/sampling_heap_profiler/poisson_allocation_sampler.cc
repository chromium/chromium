// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"

#include <atomic>
#include <cmath>
#include <memory>
#include <utility>

#include "base/allocator/dispatcher/reentry_guard.h"
#include "base/allocator/dispatcher/tls.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"

namespace base {

namespace {

using ::base::allocator::dispatcher::ReentryGuard;

const size_t kDefaultSamplingIntervalBytes = 128 * 1024;

const intptr_t kAccumulatedBytesOffset = 1 << 29;

// Controls if sample intervals should not be randomized. Used for testing.
bool g_deterministic = false;

// Pointer to the current |LockFreeAddressHashSet|.
constinit std::atomic<LockFreeAddressHashSet*> g_sampled_addresses_set{nullptr};

// Sampling interval parameter, the mean value for intervals between samples.
constinit std::atomic_size_t g_sampling_interval{kDefaultSamplingIntervalBytes};

struct ThreadLocalData {
  // Accumulated bytes towards sample.
  intptr_t accumulated_bytes = 0;
  // Used as a workaround to avoid bias from muted samples. See
  // ScopedMuteThreadSamples for more details.
  intptr_t accumulated_bytes_snapshot = 0;
  // PoissonAllocationSampler performs allocations while handling a
  // notification. The guard protects against recursions originating from these.
  bool internal_reentry_guard = false;
  // A boolean used to distinguish first allocation on a thread:
  //   false - first allocation on the thread;
  //   true  - otherwise.
  // Since accumulated_bytes is initialized with zero the very first
  // allocation on a thread would always trigger the sample, thus skewing the
  // profile towards such allocations. To mitigate that we use the flag to
  // ensure the first allocation is properly accounted.
  bool sampling_interval_initialized = false;
};

ThreadLocalData* GetThreadLocalData() {
#if USE_LOCAL_TLS_EMULATION()
  // If available, use ThreadLocalStorage to bypass dependencies introduced by
  // Clang's implementation of thread_local.
  static base::NoDestructor<
      base::allocator::dispatcher::ThreadLocalStorage<ThreadLocalData>>
      thread_local_data("poisson_allocation_sampler");
  return thread_local_data->GetThreadLocalData();
#else
  // Notes on TLS usage:
  //
  // * There's no safe way to use TLS in malloc() as both C++ thread_local and
  //   pthread do not pose any guarantees on whether they allocate or not.
  // * We think that we can safely use thread_local w/o re-entrancy guard
  //   because the compiler will use "tls static access model" for static builds
  //   of Chrome [https://www.uclibc.org/docs/tls.pdf].
  //   But there's no guarantee that this will stay true, and in practice
  //   it seems to have problems on macOS/Android. These platforms do allocate
  //   on the very first access to a thread_local on each thread.
  // * Directly using/warming-up platform TLS seems to work on all platforms,
  //   but is also not guaranteed to stay true. We make use of it for reentrancy
  //   guards on macOS/Android.
  // * We cannot use Windows Tls[GS]etValue API as it modifies the result of
  //   GetLastError.
  //
  // Android thread_local seems to be using __emutls_get_address from libgcc:
  // https://github.com/gcc-mirror/gcc/blob/master/libgcc/emutls.c
  // macOS version is based on _tlv_get_addr from dyld:
  // https://opensource.apple.com/source/dyld/dyld-635.2/src/threadLocalHelpers.s.auto.html
  thread_local ThreadLocalData thread_local_data;
  return &thread_local_data;
#endif
}

}  // namespace

PoissonAllocationSampler::ScopedMuteThreadSamples::ScopedMuteThreadSamples() {
  ThreadLocalData* const thread_local_data = GetThreadLocalData();

  DCHECK(!thread_local_data->internal_reentry_guard);
  thread_local_data->internal_reentry_guard = true;

  // We mute thread samples immediately after taking a sample, which is when we
  // reset g_tls_accumulated_bytes. This breaks the random sampling requirement
  // of the poisson process, and causes us to systematically overcount all other
  // allocations. That's because muted allocations rarely trigger a sample
  // [which would cause them to be ignored] since they occur right after
  // g_tls_accumulated_bytes is reset.
  //
  // To counteract this, we drop g_tls_accumulated_bytes by a large, fixed
  // amount to lower the probability that a sample is taken to close to 0. Then
  // we reset it after we're done muting thread samples.
  thread_local_data->accumulated_bytes_snapshot =
      thread_local_data->accumulated_bytes;
  thread_local_data->accumulated_bytes -= kAccumulatedBytesOffset;
}

PoissonAllocationSampler::ScopedMuteThreadSamples::~ScopedMuteThreadSamples() {
  ThreadLocalData* const thread_local_data = GetThreadLocalData();
  DCHECK(thread_local_data->internal_reentry_guard);
  thread_local_data->internal_reentry_guard = false;
  thread_local_data->accumulated_bytes =
      thread_local_data->accumulated_bytes_snapshot;
}

// static
bool PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted() {
  ThreadLocalData* const thread_local_data = GetThreadLocalData();
  return thread_local_data->internal_reentry_guard;
}

PoissonAllocationSampler::ScopedSuppressRandomnessForTesting::
    ScopedSuppressRandomnessForTesting() {
  DCHECK(!g_deterministic);
  g_deterministic = true;
  // The accumulated_bytes may contain a random value from previous
  // test runs, which would make the behaviour of the next call to
  // RecordAlloc unpredictable.
  ThreadLocalData* const thread_local_data = GetThreadLocalData();
  thread_local_data->accumulated_bytes = 0;
}

PoissonAllocationSampler::ScopedSuppressRandomnessForTesting::
    ~ScopedSuppressRandomnessForTesting() {
  DCHECK(g_deterministic);
  g_deterministic = false;
}

// static
bool PoissonAllocationSampler::ScopedSuppressRandomnessForTesting::
    IsSuppressed() {
  return g_deterministic;
}

PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting::
    ScopedMuteHookedSamplesForTesting() {
  SetProfilingStateFlag(ProfilingStateFlag::kHookedSamplesMutedForTesting);

  // Reset the accumulated bytes to 0 on this thread.
  ThreadLocalData* const thread_local_data = GetThreadLocalData();
  accumulated_bytes_snapshot_ = thread_local_data->accumulated_bytes;
  thread_local_data->accumulated_bytes = 0;
}

PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting::
    ~ScopedMuteHookedSamplesForTesting() {
  // Restore the accumulated bytes.
  ThreadLocalData* const thread_local_data = GetThreadLocalData();
  thread_local_data->accumulated_bytes = accumulated_bytes_snapshot_;
  ResetProfilingStateFlag(ProfilingStateFlag::kHookedSamplesMutedForTesting);
}

PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting::
    ScopedMuteHookedSamplesForTesting(ScopedMuteHookedSamplesForTesting&&) =
        default;

PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting&
PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting::operator=(
    ScopedMuteHookedSamplesForTesting&&) = default;

// static
constinit std::atomic<PoissonAllocationSampler::ProfilingStateFlagMask>
    PoissonAllocationSampler::profiling_state_{0};

PoissonAllocationSampler::PoissonAllocationSampler() {
  Init();
  auto* sampled_addresses = new LockFreeAddressHashSet(64);
  g_sampled_addresses_set.store(sampled_addresses, std::memory_order_release);
}

// static
void PoissonAllocationSampler::Init() {
  [[maybe_unused]] static bool init_once = [] {
    // Touch thread local data on initialization to enforce proper setup of
    // underlying storage system.
    GetThreadLocalData();
    ReentryGuard::InitTLSSlot();
    return true;
  }();
}

void PoissonAllocationSampler::SetSamplingInterval(
    size_t sampling_interval_bytes) {
  // TODO(alph): Reset the sample being collected if running.
  g_sampling_interval.store(sampling_interval_bytes, std::memory_order_relaxed);
}

size_t PoissonAllocationSampler::SamplingInterval() const {
  return g_sampling_interval.load(std::memory_order_relaxed);
}

// static
size_t PoissonAllocationSampler::GetNextSampleInterval(size_t interval) {
  if (g_deterministic) [[unlikely]] {
    return interval;
  }

  // We sample with a Poisson process, with constant average sampling
  // interval. This follows the exponential probability distribution with
  // parameter λ = 1/interval where |interval| is the average number of bytes
  // between samples.
  // Let u be a uniformly distributed random number (0,1], then
  // next_sample = -ln(u) / λ
  // RandDouble returns numbers [0,1). We use 1-RandDouble to correct it to
  // avoid a possible floating point exception from taking the log of 0.
  // The allocator shim uses the PoissonAllocationSampler, hence avoid
  // allocation to avoid infinite recursion.
  double uniform = internal::RandDoubleAvoidAllocation();
  double value = -log(1 - uniform) * interval;
  size_t min_value = sizeof(intptr_t);
  // We limit the upper bound of a sample interval to make sure we don't have
  // huge gaps in the sampling stream. Probability of the upper bound gets hit
  // is exp(-20) ~ 2e-9, so it should not skew the distribution.
  size_t max_value = interval * 20;
  if (value < min_value) [[unlikely]] {
    return min_value;
  }
  if (value > max_value) [[unlikely]] {
    return max_value;
  }
  return static_cast<size_t>(value);
}

void PoissonAllocationSampler::DoRecordAllocation(
    const ProfilingStateFlagMask state,
    void* address,
    size_t size,
    base::allocator::dispatcher::AllocationSubsystem type,
    const char* context) {
  ThreadLocalData* const thread_local_data = GetThreadLocalData();

  thread_local_data->accumulated_bytes += size;
  intptr_t accumulated_bytes = thread_local_data->accumulated_bytes;
  if (accumulated_bytes < 0) [[likely]] {
    return;
  }

  if (!(state & ProfilingStateFlag::kIsRunning)) [[unlikely]] {
    // Sampling was in fact disabled when the hook was called. Reset the state
    // of the sampler. We do this check off the fast-path, because it's quite a
    // rare state when the sampler is stopped after it's started. (The most
    // common caller of PoissonAllocationSampler starts it and leaves it running
    // for the rest of the Chrome session.)
    thread_local_data->sampling_interval_initialized = false;
    thread_local_data->accumulated_bytes = 0;
    return;
  }

  // Failed allocation? Skip the sample.
  if (!address) [[unlikely]] {
    return;
  }

  size_t mean_interval = g_sampling_interval.load(std::memory_order_relaxed);
  if (!thread_local_data->sampling_interval_initialized) [[unlikely]] {
    thread_local_data->sampling_interval_initialized = true;
    // This is the very first allocation on the thread. It always makes it
    // passing the condition at |RecordAlloc|, because accumulated_bytes
    // is initialized with zero due to TLS semantics.
    // Generate proper sampling interval instance and make sure the allocation
    // has indeed crossed the threshold before counting it as a sample.
    accumulated_bytes -= GetNextSampleInterval(mean_interval);
    if (accumulated_bytes < 0) {
      thread_local_data->accumulated_bytes = accumulated_bytes;
      return;
    }
  }

  // This cast is safe because this function is only called with a positive
  // value of `accumulated_bytes`.
  size_t samples = static_cast<size_t>(accumulated_bytes) / mean_interval;
  accumulated_bytes %= mean_interval;

  do {
    accumulated_bytes -= GetNextSampleInterval(mean_interval);
    ++samples;
  } while (accumulated_bytes >= 0);

  thread_local_data->accumulated_bytes = accumulated_bytes;

  if (ScopedMuteThreadSamples::IsMuted()) [[unlikely]] {
    return;
  }

  ScopedMuteThreadSamples no_reentrancy_scope;
  std::vector<SamplesObserver*> observers_copy;
  {
    AutoLock lock(mutex_);

    // TODO(alph): Sometimes RecordAlloc is called twice in a row without
    // a RecordFree in between. Investigate it.
    if (sampled_addresses_set().Contains(address)) {
      return;
    }
    sampled_addresses_set().Insert(address);
    BalanceAddressesHashSet();
    observers_copy = observers_;
  }

  size_t total_allocated = mean_interval * samples;
  for (base::PoissonAllocationSampler::SamplesObserver* observer :
       observers_copy) {
    observer->SampleAdded(address, size, total_allocated, type, context);
  }
}

void PoissonAllocationSampler::DoRecordFree(void* address) {
  // There is a rare case on macOS and Android when the very first thread_local
  // access in ScopedMuteThreadSamples constructor may allocate and
  // thus reenter DoRecordAlloc. However the call chain won't build up further
  // as RecordAlloc accesses are guarded with pthread TLS-based ReentryGuard.
  ScopedMuteThreadSamples no_reentrancy_scope;
  std::vector<SamplesObserver*> observers_copy;
  {
    AutoLock lock(mutex_);
    observers_copy = observers_;
    sampled_addresses_set().Remove(address);
  }
  for (base::PoissonAllocationSampler::SamplesObserver* observer :
       observers_copy) {
    observer->SampleRemoved(address);
  }
}

void PoissonAllocationSampler::BalanceAddressesHashSet() {
  // Check if the load_factor of the current addresses hash set becomes higher
  // than 1, allocate a new twice larger one, copy all the data,
  // and switch to using it.
  // During the copy process no other writes are made to both sets
  // as it's behind the lock.
  // All the readers continue to use the old one until the atomic switch
  // process takes place.
  LockFreeAddressHashSet& current_set = sampled_addresses_set();
  if (current_set.load_factor() < 1) {
    return;
  }
  auto new_set =
      std::make_unique<LockFreeAddressHashSet>(current_set.buckets_count() * 2);
  new_set->Copy(current_set);
  // Atomically switch all the new readers to the new set.
  g_sampled_addresses_set.store(new_set.release(), std::memory_order_release);
  // We leak the older set because we still have to keep all the old maps alive
  // as there might be reader threads that have already obtained the map,
  // but haven't yet managed to access it.
}

// static
LockFreeAddressHashSet& PoissonAllocationSampler::sampled_addresses_set() {
  return *g_sampled_addresses_set.load(std::memory_order_acquire);
}

// static
PoissonAllocationSampler* PoissonAllocationSampler::Get() {
  static NoDestructor<PoissonAllocationSampler> instance;
  return instance.get();
}

// static
void PoissonAllocationSampler::SetProfilingStateFlag(ProfilingStateFlag flag) {
  ProfilingStateFlagMask flags = flag;
  if (flag == ProfilingStateFlag::kIsRunning) {
    flags |= ProfilingStateFlag::kWasStarted;
  }
  ProfilingStateFlagMask old_state =
      profiling_state_.fetch_or(flags, std::memory_order_relaxed);
  DCHECK(!(old_state & flag));
}

// static
void PoissonAllocationSampler::ResetProfilingStateFlag(
    ProfilingStateFlag flag) {
  DCHECK_NE(flag, kWasStarted);
  ProfilingStateFlagMask old_state =
      profiling_state_.fetch_and(~flag, std::memory_order_relaxed);
  DCHECK(old_state & flag);
}

void PoissonAllocationSampler::AddSamplesObserver(SamplesObserver* observer) {
  // The following implementation (including ScopedMuteThreadSamples) will use
  // `thread_local`, which may cause a reentrancy issue.  So, temporarily
  // disable the sampling by having a ReentryGuard.
  ReentryGuard guard;

  ScopedMuteThreadSamples no_reentrancy_scope;
  AutoLock lock(mutex_);
  DCHECK(ranges::find(observers_, observer) == observers_.end());
  bool profiler_was_stopped = observers_.empty();
  observers_.push_back(observer);

  // Adding the observer will enable profiling. This will use
  // `g_sampled_address_set` so it had better be initialized.
  DCHECK(g_sampled_addresses_set.load(std::memory_order_relaxed));

  // Start the profiler if this was the first observer. Setting/resetting
  // kIsRunning isn't racy because it's performed based on `observers_.empty()`
  // while holding `mutex_`.
  if (profiler_was_stopped) {
    SetProfilingStateFlag(ProfilingStateFlag::kIsRunning);
  }
  DCHECK(profiling_state_.load(std::memory_order_relaxed) &
         ProfilingStateFlag::kIsRunning);
}

void PoissonAllocationSampler::RemoveSamplesObserver(
    SamplesObserver* observer) {
  // The following implementation (including ScopedMuteThreadSamples) will use
  // `thread_local`, which may cause a reentrancy issue.  So, temporarily
  // disable the sampling by having a ReentryGuard.
  ReentryGuard guard;

  ScopedMuteThreadSamples no_reentrancy_scope;
  AutoLock lock(mutex_);
  auto it = ranges::find(observers_, observer);
  CHECK(it != observers_.end(), base::NotFatalUntil::M125);
  observers_.erase(it);

  // Stop the profiler if there are no more observers. Setting/resetting
  // kIsRunning isn't racy because it's performed based on `observers_.empty()`
  // while holding `mutex_`.
  DCHECK(profiling_state_.load(std::memory_order_relaxed) &
         ProfilingStateFlag::kIsRunning);
  if (observers_.empty()) {
    ResetProfilingStateFlag(ProfilingStateFlag::kIsRunning);
  }
}

}  // namespace base
