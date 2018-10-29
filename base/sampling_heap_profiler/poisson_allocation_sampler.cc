// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/sampling_heap_profiler.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/atomicops.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/partition_alloc_buildflags.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <pthread.h>
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace base {

using allocator::AllocatorDispatch;
using subtle::Atomic32;
using subtle::AtomicWord;

namespace {

// PoissonAllocationSampler cannot use ThreadLocalStorage, as during thread
// exiting when TLS storage is already released, there might be a call to
// |free| which would trigger the profiler hook and would make it access TLS.
// It instead uses OS primitives directly. As it only stores POD types it
// does not need thread exit callbacks.
#if defined(OS_WIN)

using TLSKey = DWORD;

void TLSInit(TLSKey* key) {
  *key = ::TlsAlloc();
  CHECK_NE(TLS_OUT_OF_INDEXES, *key);
}

uintptr_t TLSGetValue(const TLSKey& key) {
  return reinterpret_cast<uintptr_t>(::TlsGetValue(key));
}

void TLSSetValue(const TLSKey& key, uintptr_t value) {
  ::TlsSetValue(key, reinterpret_cast<LPVOID>(value));
}

#else  // defined(OS_WIN)

using TLSKey = pthread_key_t;

void TLSInit(TLSKey* key) {
  int result = pthread_key_create(key, nullptr);
  CHECK_EQ(0, result);
}

uintptr_t TLSGetValue(const TLSKey& key) {
  return reinterpret_cast<uintptr_t>(pthread_getspecific(key));
}

void TLSSetValue(const TLSKey& key, uintptr_t value) {
  pthread_setspecific(key, reinterpret_cast<void*>(value));
}

#endif

// On MacOS the implementation of libmalloc sometimes calls malloc recursively,
// delegating allocations between zones. That causes our hooks being called
// twice. The scoped guard allows us to detect that.
#if defined(OS_MACOSX)

class ReentryGuard {
 public:
  ReentryGuard() : allowed_(!TLSGetValue(entered_key_)) {
    TLSSetValue(entered_key_, true);
  }

  ~ReentryGuard() {
    if (LIKELY(allowed_))
      TLSSetValue(entered_key_, false);
  }

  operator bool() { return allowed_; }

  static void Init() { TLSInit(&entered_key_); }

 private:
  bool allowed_;
  static TLSKey entered_key_;
};

TLSKey ReentryGuard::entered_key_;

#else

class ReentryGuard {
 public:
  operator bool() { return true; }
  static void Init() {}
};

#endif

TLSKey g_internal_reentry_guard;

const size_t kDefaultSamplingIntervalBytes = 128 * 1024;

// Accumulated bytes towards sample thread local key.
TLSKey g_accumulated_bytes_tls;

// A boolean used to distinguish first allocation on a thread.
//   false - first allocation on the thread.
//   true  - otherwise
// Since g_accumulated_bytes_tls is initialized with zero the very first
// allocation on a thread would always trigger the sample, thus skewing the
// profile towards such allocations. To mitigate that we use the flag to
// ensure the first allocation is properly accounted.
TLSKey g_sampling_interval_initialized_tls;

// Controls if sample intervals should not be randomized. Used for testing.
bool g_deterministic;

// A positive value if profiling is running, otherwise it's zero.
Atomic32 g_running;

// Pointer to the current |LockFreeAddressHashSet|.
AtomicWord g_sampled_addresses_set;

// Sampling interval parameter, the mean value for intervals between samples.
AtomicWord g_sampling_interval = kDefaultSamplingIntervalBytes;

void (*g_hooks_install_callback)();
Atomic32 g_hooks_installed;

void* AllocFn(const AllocatorDispatch* self, size_t size, void* context) {
  ReentryGuard guard;
  void* address = self->next->alloc_function(self->next, size, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, size, PoissonAllocationSampler::kMalloc, nullptr);
  }
  return address;
}

void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                             size_t n,
                             size_t size,
                             void* context) {
  ReentryGuard guard;
  void* address =
      self->next->alloc_zero_initialized_function(self->next, n, size, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, n * size, PoissonAllocationSampler::kMalloc, nullptr);
  }
  return address;
}

void* AllocAlignedFn(const AllocatorDispatch* self,
                     size_t alignment,
                     size_t size,
                     void* context) {
  ReentryGuard guard;
  void* address =
      self->next->alloc_aligned_function(self->next, alignment, size, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, size, PoissonAllocationSampler::kMalloc, nullptr);
  }
  return address;
}

void* ReallocFn(const AllocatorDispatch* self,
                void* address,
                size_t size,
                void* context) {
  ReentryGuard guard;
  // Note: size == 0 actually performs free.
  PoissonAllocationSampler::RecordFree(address);
  address = self->next->realloc_function(self->next, address, size, context);
  if (LIKELY(guard)) {
    PoissonAllocationSampler::RecordAlloc(
        address, size, PoissonAllocationSampler::kMalloc, nullptr);
  }
  return address;
}

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {
  // Note: The RecordFree should be called before free_function
  // (here and in other places).
  // That is because we need to remove the recorded allocation sample before
  // free_function, as once the latter is executed the address becomes available
  // and can be allocated by another thread. That would be racy otherwise.
  PoissonAllocationSampler::RecordFree(address);
  self->next->free_function(self->next, address, context);
}

size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                         void* address,
                         void* context) {
  return self->next->get_size_estimate_function(self->next, address, context);
}

unsigned BatchMallocFn(const AllocatorDispatch* self,
                       size_t size,
                       void** results,
                       unsigned num_requested,
                       void* context) {
  ReentryGuard guard;
  unsigned num_allocated = self->next->batch_malloc_function(
      self->next, size, results, num_requested, context);
  if (LIKELY(guard)) {
    for (unsigned i = 0; i < num_allocated; ++i) {
      PoissonAllocationSampler::RecordAlloc(
          results[i], size, PoissonAllocationSampler::kMalloc, nullptr);
    }
  }
  return num_allocated;
}

void BatchFreeFn(const AllocatorDispatch* self,
                 void** to_be_freed,
                 unsigned num_to_be_freed,
                 void* context) {
  for (unsigned i = 0; i < num_to_be_freed; ++i)
    PoissonAllocationSampler::RecordFree(to_be_freed[i]);
  self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                  context);
}

void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                        void* address,
                        size_t size,
                        void* context) {
  PoissonAllocationSampler::RecordFree(address);
  self->next->free_definite_size_function(self->next, address, size, context);
}

AllocatorDispatch g_allocator_dispatch = {&AllocFn,
                                          &AllocZeroInitializedFn,
                                          &AllocAlignedFn,
                                          &ReallocFn,
                                          &FreeFn,
                                          &GetSizeEstimateFn,
                                          &BatchMallocFn,
                                          &BatchFreeFn,
                                          &FreeDefiniteSizeFn,
                                          nullptr};

#if BUILDFLAG(USE_PARTITION_ALLOC) && !defined(OS_NACL)

void PartitionAllocHook(void* address, size_t size, const char* type) {
  PoissonAllocationSampler::RecordAlloc(
      address, size, PoissonAllocationSampler::kPartitionAlloc, type);
}

void PartitionFreeHook(void* address) {
  PoissonAllocationSampler::RecordFree(address);
}

#endif  // BUILDFLAG(USE_PARTITION_ALLOC) && !defined(OS_NACL)

}  // namespace

PoissonAllocationSampler::ScopedMuteThreadSamples::ScopedMuteThreadSamples() {
  DCHECK(!TLSGetValue(g_internal_reentry_guard));
  TLSSetValue(g_internal_reentry_guard, true);
}

PoissonAllocationSampler::ScopedMuteThreadSamples::~ScopedMuteThreadSamples() {
  DCHECK(TLSGetValue(g_internal_reentry_guard));
  TLSSetValue(g_internal_reentry_guard, false);
}

PoissonAllocationSampler* PoissonAllocationSampler::instance_;

PoissonAllocationSampler::PoissonAllocationSampler() {
  CHECK_EQ(nullptr, instance_);
  instance_ = this;
  Init();
  auto sampled_addresses = std::make_unique<LockFreeAddressHashSet>(64);
  subtle::NoBarrier_Store(
      &g_sampled_addresses_set,
      reinterpret_cast<AtomicWord>(sampled_addresses.get()));
  sampled_addresses_stack_.push_back(std::move(sampled_addresses));
}

// static
void PoissonAllocationSampler::Init() {
  static bool init_once = []() {
    ReentryGuard::Init();
    TLSInit(&g_internal_reentry_guard);
    TLSInit(&g_accumulated_bytes_tls);
    TLSInit(&g_sampling_interval_initialized_tls);
    return true;
  }();
  ignore_result(init_once);
}

// static
void PoissonAllocationSampler::InstallAllocatorHooksOnce() {
  static bool hook_installed = InstallAllocatorHooks();
  ignore_result(hook_installed);
}

// static
bool PoissonAllocationSampler::InstallAllocatorHooks() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocator::InsertAllocatorDispatch(&g_allocator_dispatch);
#else
  ignore_result(g_allocator_dispatch);
  DLOG(WARNING)
      << "base::allocator shims are not available for memory sampling.";
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

#if BUILDFLAG(USE_PARTITION_ALLOC) && !defined(OS_NACL)
  PartitionAllocHooks::SetAllocationHook(&PartitionAllocHook);
  PartitionAllocHooks::SetFreeHook(&PartitionFreeHook);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC) && !defined(OS_NACL)

  int32_t hooks_install_callback_has_been_set =
      subtle::Acquire_CompareAndSwap(&g_hooks_installed, 0, 1);
  if (hooks_install_callback_has_been_set)
    g_hooks_install_callback();

  return true;
}

// static
void PoissonAllocationSampler::SetHooksInstallCallback(
    void (*hooks_install_callback)()) {
  CHECK(!g_hooks_install_callback && hooks_install_callback);
  g_hooks_install_callback = hooks_install_callback;

  int32_t profiler_has_already_been_initialized =
      subtle::Release_CompareAndSwap(&g_hooks_installed, 0, 1);
  if (profiler_has_already_been_initialized)
    g_hooks_install_callback();
}

void PoissonAllocationSampler::Start() {
  InstallAllocatorHooksOnce();
  subtle::Barrier_AtomicIncrement(&g_running, 1);
}

void PoissonAllocationSampler::Stop() {
  AtomicWord count = subtle::Barrier_AtomicIncrement(&g_running, -1);
  CHECK_GE(count, 0);
}

void PoissonAllocationSampler::SetSamplingInterval(size_t sampling_interval) {
  // TODO(alph): Reset the sample being collected if running.
  subtle::Release_Store(&g_sampling_interval,
                        static_cast<AtomicWord>(sampling_interval));
}

// static
size_t PoissonAllocationSampler::GetNextSampleInterval(size_t interval) {
  if (UNLIKELY(g_deterministic))
    return interval;

  // We sample with a Poisson process, with constant average sampling
  // interval. This follows the exponential probability distribution with
  // parameter λ = 1/interval where |interval| is the average number of bytes
  // between samples.
  // Let u be a uniformly distributed random number between 0 and 1, then
  // next_sample = -ln(u) / λ
  double uniform = RandDouble();
  double value = -log(uniform) * interval;
  size_t min_value = sizeof(intptr_t);
  // We limit the upper bound of a sample interval to make sure we don't have
  // huge gaps in the sampling stream. Probability of the upper bound gets hit
  // is exp(-20) ~ 2e-9, so it should not skew the distribution.
  size_t max_value = interval * 20;
  if (UNLIKELY(value < min_value))
    return min_value;
  if (UNLIKELY(value > max_value))
    return max_value;
  return static_cast<size_t>(value);
}

// static
void PoissonAllocationSampler::RecordAlloc(void* address,
                                           size_t size,
                                           AllocatorType type,
                                           const char* context) {
  if (UNLIKELY(!subtle::NoBarrier_Load(&g_running)))
    return;
  intptr_t accumulated_bytes = TLSGetValue(g_accumulated_bytes_tls) + size;
  if (LIKELY(accumulated_bytes < 0))
    TLSSetValue(g_accumulated_bytes_tls, accumulated_bytes);
  else
    instance_->DoRecordAlloc(accumulated_bytes, size, address, type, context);
}

void PoissonAllocationSampler::DoRecordAlloc(intptr_t accumulated_bytes,
                                             size_t size,
                                             void* address,
                                             AllocatorType type,
                                             const char* context) {
  size_t mean_interval = subtle::NoBarrier_Load(&g_sampling_interval);
  size_t samples = accumulated_bytes / mean_interval;
  accumulated_bytes %= mean_interval;

  do {
    accumulated_bytes -= GetNextSampleInterval(mean_interval);
    ++samples;
  } while (accumulated_bytes >= 0);

  TLSSetValue(g_accumulated_bytes_tls, accumulated_bytes);

  if (UNLIKELY(!TLSGetValue(g_sampling_interval_initialized_tls))) {
    TLSSetValue(g_sampling_interval_initialized_tls, true);
    // This is the very first allocation on the thread. It always produces an
    // extra sample because g_accumulated_bytes_tls is initialized with zero
    // due to TLS semantics.
    // Make sure we don't count this extra sample.
    if (!--samples)
      return;
  }

  if (UNLIKELY(TLSGetValue(g_internal_reentry_guard)))
    return;

  ScopedMuteThreadSamples no_reentrancy_scope;
  AutoLock lock(mutex_);

  // TODO(alph): Sometimes RecordAlloc is called twice in a row without
  // a RecordFree in between. Investigate it.
  if (sampled_addresses_set().Contains(address))
    return;
  sampled_addresses_set().Insert(address);
  BalanceAddressesHashSet();

  size_t total_allocated = mean_interval * samples;
  for (auto* observer : observers_)
    observer->SampleAdded(address, size, total_allocated, type, context);
}

// static
void PoissonAllocationSampler::RecordFree(void* address) {
  if (UNLIKELY(address == nullptr))
    return;
  if (UNLIKELY(sampled_addresses_set().Contains(address)))
    instance_->DoRecordFree(address);
}

void PoissonAllocationSampler::DoRecordFree(void* address) {
  if (UNLIKELY(TLSGetValue(g_internal_reentry_guard)))
    return;
  ScopedMuteThreadSamples no_reentrancy_scope;
  AutoLock lock(mutex_);
  for (auto* observer : observers_)
    observer->SampleRemoved(address);
  sampled_addresses_set().Remove(address);
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
  if (current_set.load_factor() < 1)
    return;
  auto new_set =
      std::make_unique<LockFreeAddressHashSet>(current_set.buckets_count() * 2);
  new_set->Copy(current_set);
  // Atomically switch all the new readers to the new set.
  subtle::Release_Store(&g_sampled_addresses_set,
                        reinterpret_cast<AtomicWord>(new_set.get()));
  // We still have to keep all the old maps alive to resolve the theoretical
  // race with readers in |RecordFree| that have already obtained the map,
  // but haven't yet managed to access it.
  sampled_addresses_stack_.push_back(std::move(new_set));
}

// static
LockFreeAddressHashSet& PoissonAllocationSampler::sampled_addresses_set() {
  return *reinterpret_cast<LockFreeAddressHashSet*>(
      subtle::NoBarrier_Load(&g_sampled_addresses_set));
}

// static
PoissonAllocationSampler* PoissonAllocationSampler::Get() {
  static NoDestructor<PoissonAllocationSampler> instance;
  return instance.get();
}

// static
void PoissonAllocationSampler::SuppressRandomnessForTest(bool suppress) {
  g_deterministic = suppress;
}

void PoissonAllocationSampler::AddSamplesObserver(SamplesObserver* observer) {
  ScopedMuteThreadSamples no_reentrancy_scope;
  AutoLock lock(mutex_);
  observers_.push_back(observer);
}

void PoissonAllocationSampler::RemoveSamplesObserver(
    SamplesObserver* observer) {
  ScopedMuteThreadSamples no_reentrancy_scope;
  AutoLock lock(mutex_);
  auto it = std::find(observers_.begin(), observers_.end(), observer);
  CHECK(it != observers_.end());
  observers_.erase(it);
}

}  // namespace base
