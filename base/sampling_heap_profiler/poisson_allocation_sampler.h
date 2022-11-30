// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SAMPLING_HEAP_PROFILER_POISSON_ALLOCATION_SAMPLER_H_
#define BASE_SAMPLING_HEAP_PROFILER_POISSON_ALLOCATION_SAMPLER_H_

#include <vector>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace heap_profiling {
class HeapProfilerControllerTest;
}

namespace base {

class SamplingHeapProfilerTest;

// This singleton class implements Poisson sampling of the incoming allocations
// stream. It hooks onto base::allocator and base::PartitionAlloc.
// An extra custom allocator can be hooked via SetHooksInstallCallback method.
// The only control parameter is sampling interval that controls average value
// of the sampling intervals. The actual intervals between samples are
// randomized using Poisson distribution to mitigate patterns in the allocation
// stream.
// Once accumulated allocation sizes fill up the current sample interval,
// a sample is generated and sent to the observers via |SampleAdded| call.
// When the corresponding memory that triggered the sample is freed observers
// get notified with |SampleRemoved| call.
//
class BASE_EXPORT PoissonAllocationSampler {
 public:
  // The type of hooked allocator that is the source of a sample.
  // kManualForTesting is for unit tests calling RecordAlloc directly without
  // going through a hooked allocator.
  enum AllocatorType : uint32_t { kMalloc, kPartitionAlloc, kManualForTesting };

  class SamplesObserver {
   public:
    virtual ~SamplesObserver() = default;
    virtual void SampleAdded(void* address,
                             size_t size,
                             size_t total,
                             AllocatorType type,
                             const char* context) = 0;
    virtual void SampleRemoved(void* address) = 0;
  };

  // An instance of this class makes the sampler not report samples generated
  // within the object scope for the current thread.
  // It allows observers to allocate/deallocate memory while holding a lock
  // without a chance to get into reentrancy problems.
  // The current implementation doesn't support ScopedMuteThreadSamples nesting.
  class BASE_EXPORT ScopedMuteThreadSamples {
   public:
    ScopedMuteThreadSamples();
    ~ScopedMuteThreadSamples();

    ScopedMuteThreadSamples(const ScopedMuteThreadSamples&) = delete;
    ScopedMuteThreadSamples& operator=(const ScopedMuteThreadSamples&) = delete;

    static bool IsMuted();
  };

  // An instance of this class makes the sampler behave deterministically to
  // ensure test results are repeatable. Does not support nesting.
  class BASE_EXPORT ScopedSuppressRandomnessForTesting {
   public:
    ScopedSuppressRandomnessForTesting();
    ~ScopedSuppressRandomnessForTesting();

    ScopedSuppressRandomnessForTesting(
        const ScopedSuppressRandomnessForTesting&) = delete;
    ScopedSuppressRandomnessForTesting& operator=(
        const ScopedSuppressRandomnessForTesting&) = delete;

    static bool IsSuppressed();
  };

  // Must be called early during the process initialization. It creates and
  // reserves a TLS slot.
  static void Init();

  // This is an entry point for plugging in an external allocator.
  // Profiler will invoke the provided callback upon initialization.
  // The callback should install hooks onto the corresponding memory allocator
  // and make them invoke PoissonAllocationSampler::RecordAlloc and
  // PoissonAllocationSampler::RecordFree upon corresponding allocation events.
  //
  // If the method is called after profiler is initialized, the callback
  // is invoked right away.
  static void SetHooksInstallCallback(void (*hooks_install_callback)());

  void AddSamplesObserver(SamplesObserver*);

  // Note: After an observer is removed it is still possible to receive
  // a notification to that observer. This is not a problem currently as
  // the only client of this interface is the base::SamplingHeapProfiler,
  // which is a singleton.
  // If there's a need for this functionality in the future, one might
  // want to put observers notification loop under a reader-writer lock.
  void RemoveSamplesObserver(SamplesObserver*);

  // Sets the mean number of bytes that will be allocated before taking a
  // sample.
  void SetSamplingInterval(size_t sampling_interval_bytes);

  // Returns the current mean sampling interval, in bytes.
  size_t SamplingInterval() const;

  static void RecordAlloc(void* address,
                          size_t,
                          AllocatorType,
                          const char* context);
  ALWAYS_INLINE static void RecordFree(void* address);

  static PoissonAllocationSampler* Get();

  PoissonAllocationSampler(const PoissonAllocationSampler&) = delete;
  PoissonAllocationSampler& operator=(const PoissonAllocationSampler&) = delete;

  // Returns true if a ScopedMuteHookedSamplesForTesting exists. Only friends
  // can create a ScopedMuteHookedSamplesForTesting but anyone can check the
  // status of this. This can be read from any thread.
  static bool AreHookedSamplesMuted();

 private:
  // An instance of this class makes the sampler only report samples with
  // AllocatorType kManualForTesting, not those from hooked allocators. This
  // allows unit tests to set test expectations based on only explicit calls to
  // RecordAlloc and RecordFree.
  //
  // The accumulated bytes on the thread that creates a
  // ScopedMuteHookedSamplesForTesting will also be reset to 0, and restored
  // when the object leaves scope. This gives tests a known state to start
  // recording samples on one thread: a full interval must pass to record a
  // sample. Other threads will still have a random number of accumulated bytes.
  //
  // Only one instance may exist at a time.
  class BASE_EXPORT ScopedMuteHookedSamplesForTesting {
   public:
    ScopedMuteHookedSamplesForTesting();
    ~ScopedMuteHookedSamplesForTesting();

    ScopedMuteHookedSamplesForTesting(
        const ScopedMuteHookedSamplesForTesting&) = delete;
    ScopedMuteHookedSamplesForTesting& operator=(
        const ScopedMuteHookedSamplesForTesting&) = delete;

   private:
    intptr_t accumulated_bytes_snapshot_;
  };

  PoissonAllocationSampler();
  ~PoissonAllocationSampler() = delete;

  // Installs allocator hooks if they weren't already installed. This is not
  // static to ensure that allocator hooks can't be installed unless the
  // PoissonAllocationSampler singleton exists.
  void InstallAllocatorHooksOnce();

  static size_t GetNextSampleInterval(size_t base_interval);

  // Return the set of sampled addresses. This is only valid to call after
  // Init().
  static LockFreeAddressHashSet& sampled_addresses_set();

  void DoRecordAlloc(intptr_t accumulated_bytes,
                     size_t size,
                     void* address,
                     AllocatorType type,
                     const char* context);
  void DoRecordFree(void* address);

  void BalanceAddressesHashSet();

  Lock mutex_;
  // The |observers_| list is guarded by |mutex_|, however a copy of it
  // is made before invoking the observers (to avoid performing expensive
  // operations under the lock) as such the SamplesObservers themselves need
  // to be thread-safe and support being invoked racily after
  // RemoveSamplesObserver().
  std::vector<SamplesObserver*> observers_ GUARDED_BY(mutex_);

  static PoissonAllocationSampler* instance_;

  friend class heap_profiling::HeapProfilerControllerTest;
  friend class NoDestructor<PoissonAllocationSampler>;
  friend class SamplingHeapProfilerTest;
  FRIEND_TEST_ALL_PREFIXES(PoissonAllocationSamplerTest, MuteHooksWithoutInit);
  FRIEND_TEST_ALL_PREFIXES(SamplingHeapProfilerTest, HookedAllocatorMuted);
};

// static
ALWAYS_INLINE void PoissonAllocationSampler::RecordFree(void* address) {
  if (UNLIKELY(address == nullptr))
    return;
  if (UNLIKELY(sampled_addresses_set().Contains(address)))
    instance_->DoRecordFree(address);
}

}  // namespace base

#endif  // BASE_SAMPLING_HEAP_PROFILER_POISSON_ALLOCATION_SAMPLER_H_
