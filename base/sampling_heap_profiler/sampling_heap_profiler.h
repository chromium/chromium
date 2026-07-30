// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SAMPLING_HEAP_PROFILER_SAMPLING_HEAP_PROFILER_H_
#define BASE_SAMPLING_HEAP_PROFILER_SAMPLING_HEAP_PROFILER_H_

#include <atomic>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/base_export.h"
#include "base/byte_size.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/no_destructor.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/types/id_type.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace base {

// The class implements sampling profiling of native memory heap.
// It uses PoissonAllocationSampler to aggregate the heap allocations and
// record samples.
// The recorded samples can then be retrieved using GetSamples method.
class BASE_EXPORT SamplingHeapProfiler
    : private PoissonAllocationSampler::SamplesObserver,
      public base::ThreadIdNameManager::Observer {
 public:
  class BASE_EXPORT Sample {
   public:
    Sample(const Sample&);
    ~Sample();

    // Allocation size.
    size_t size;
    // Total size attributed to the sample.
    size_t total;
    // Type of the allocator.
    base::allocator::dispatcher::AllocationSubsystem allocator;
    // Context as provided by the allocation hook.
    const char* context = nullptr;
    // Name of the thread that made the sampled allocation.
    const char* thread_name = nullptr;
    // Call stack of PC addresses responsible for the allocation.
    // RAW_PTR_EXCLUSION: executable addresses are never in PA partitions
    RAW_PTR_EXCLUSION std::vector<const void*> stack;

    // Public for testing.
    Sample(size_t size, size_t total, uint32_t ordinal);

   private:
    friend class SamplingHeapProfiler;

    uint32_t ordinal;
  };

  enum class StackUnwinder {
    // Use default unwind tables.
    kDefault,
    // No stack unwinder available - profiler will be disabled.
    kUnavailable,
    // Use frame pointers, which are faster if available.
    kFramePointers,
  };

  enum class Priority {
    kBackground,
    kInteractive,
  };
  using SessionId = base::IdTypeU32<class SessionIdMarker>;

  struct Session {
    Session(SessionId id, uint32_t start_ordinal)
        : id(id), start_ordinal(start_ordinal) {}
    SessionId id;
    uint32_t start_ordinal;
  };

  // Starts collecting allocation samples. Returns a Session struct containing
  // the unique session ID and the start ordinal.
  std::optional<Session> Start(base::ByteSize sampling_interval,
                               Priority priority);

  // Stops recording allocation samples for the given session.
  void Stop(const Session& session);

  // Enables recording thread name that made the sampled allocation.
  void EnableRecordThreadNames();

  // Returns the current thread name.
  static const char* CachedThreadName();

  // Returns current samples recorded for the profile session.
  // Returns only the samples recorded after the corresponding |Start|
  // invocation. If |session| is nullopt, returns all collected samples.
  std::vector<Sample> GetSamples(std::optional<Session> session);

  // List of strings used in the profile call stacks.
  std::vector<const char*> GetStrings();

  // Captures stack `frames`, up to as many as the size of the `frames` span.
  // Returns a subspan of `frames` holding the captured frames. The top-most
  // frame is at the front of the returned span.
  span<const void*> CaptureStackTrace(span<const void*> frames);

  static void Init();
  static SamplingHeapProfiler* Get();

  SamplingHeapProfiler(const SamplingHeapProfiler&) = delete;
  SamplingHeapProfiler& operator=(const SamplingHeapProfiler&) = delete;

  // ThreadIdNameManager::Observer implementation:
  void OnThreadNameChanged(const char* name) override;

  // Deletes all samples recorded, to ensure the profiler is in a consistent
  // state at the beginning of a test, and creates a
  // ScopedMuteHookedSamplesForTesting so that new hooked samples don't arrive
  // while it's running.
  PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting
  MuteHookedSamplesForTesting();

 private:
  SamplingHeapProfiler();
  ~SamplingHeapProfiler() override;

  // PoissonAllocationSampler::SamplesObserver
  void SampleAdded(void* address,
                   size_t size,
                   size_t total,
                   base::allocator::dispatcher::AllocationSubsystem type,
                   const char* context) override;
  void SampleRemoved(void* address) override;

  void CaptureNativeStack(const char* context, Sample* sample);
  const char* RecordString(const char* string) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Mutex to access |samples_| and |strings_|.
  Lock mutex_;

  // Samples of the currently live allocations.
  std::unordered_map<void*, Sample> samples_ GUARDED_BY(mutex_);

  // Contains pointers to static sample context strings that are never deleted.
  std::unordered_set<const char*> strings_ GUARDED_BY(mutex_);

  // Mutex to guard |running_sessions_| and Add/Remove samples.
  Lock start_stop_mutex_;

  struct SessionInfo {
    base::ByteSize sampling_interval = base::ByteSize::Max();
    Priority priority = Priority::kBackground;
  };

  void UpdateSamplingInterval() EXCLUSIVE_LOCKS_REQUIRED(start_stop_mutex_);

  absl::flat_hash_map<SessionId, SessionInfo> sessions_
      GUARDED_BY(start_stop_mutex_);
  SessionId::Generator session_id_generator_ GUARDED_BY(start_stop_mutex_);

  // Last sample ordinal used to mark samples recorded during single session.
  std::atomic<uint32_t> last_sample_ordinal_{1};

  // Whether it should record thread names.
  std::atomic<bool> record_thread_names_{false};

  // Which unwinder to use.
  std::atomic<StackUnwinder> unwinder_{StackUnwinder::kDefault};

  friend class NoDestructor<SamplingHeapProfiler>;
  friend class SamplingHeapProfilerTest;
};

}  // namespace base

#endif  // BASE_SAMPLING_HEAP_PROFILER_SAMPLING_HEAP_PROFILER_H_
