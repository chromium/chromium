// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef BASE_ALLOCATOR_PARTITION_ALLOC_SUPPORT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOC_SUPPORT_H_

#include <map>
#include <string>

#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/scheduler_loop_quarantine_config.h"
#include "base/base_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/thread_cache.h"

namespace base::allocator {

// Starts a periodic timer on the current thread to purge all thread caches.
BASE_EXPORT void StartThreadCachePeriodicPurge();

BASE_EXPORT void StartMemoryReclaimer(
    scoped_refptr<SequencedTaskRunner> task_runner);

BASE_EXPORT std::map<std::string, std::string> ProposeSyntheticFinchTrials();

// Install handlers for when dangling raw_ptr(s) have been detected. This prints
// two StackTraces. One where the memory is freed, one where the last dangling
// raw_ptr stopped referencing it.
//
// This is currently effective, only when compiled with
// `enable_dangling_raw_ptr_checks` build flag.
BASE_EXPORT void InstallDanglingRawPtrChecks();
BASE_EXPORT void InstallUnretainedDanglingRawPtrChecks();

// Once called, makes `free()` do nothing. This is done to reduce
// shutdown hangs on CrOS.
// Does nothing if Dangling Pointer Detector (`docs/dangling_ptr.md`)
// is not active.
// Does nothing if allocator shim support is not built.
BASE_EXPORT void MakeFreeNoOp();

// Apply specialized configuration to the quarantine branch for the current
// thread.
BASE_EXPORT void ReconfigureSchedulerLoopQuarantineBranch(
    SchedulerLoopQuarantineBranchType branch_type);

// Allows to re-configure PartitionAlloc at run-time.
class BASE_EXPORT PartitionAllocSupport {
 public:
  struct BrpConfiguration {
    bool enable_brp = false;

    // TODO(https://crbug.com/371135823): Remove after the investigation.
    size_t extra_extras_size = 0;
    bool suppress_double_free_detected_crash = false;
    bool suppress_corruption_detected_crash = false;
  };

  // Reconfigure* functions re-configure PartitionAlloc. It is impossible to
  // configure PartitionAlloc before/at its initialization using information not
  // known at compile-time (e.g. process type, Finch), because by the time this
  // information is available memory allocations would have surely happened,
  // that requiring a functioning allocator.
  //
  // *Earlyish() is called as early as it is reasonably possible.
  // *AfterZygoteFork() is its complement to finish configuring process-specific
  // stuff that had to be postponed due to *Earlyish() being called with
  // |process_type==kZygoteProcess|.
  // *AfterFeatureListInit() is called in addition to the above, once
  // FeatureList has been initialized and ready to use. It is guaranteed to be
  // called on non-zygote processes or after the zygote has been forked.
  // *AfterTaskRunnerInit() is called once it is possible to post tasks, and
  // after the previous steps.
  //
  // *Earlyish() must be called exactly once. *AfterZygoteFork() must be called
  // once iff *Earlyish() was called before with |process_type==kZygoteProcess|.
  //
  // *AfterFeatureListInit() may be called more than once, but will perform its
  // re-configuration steps exactly once.
  //
  // *AfterTaskRunnerInit() may be called more than once.
  void ReconfigureForTests();
  void ReconfigureEarlyish(const std::string& process_type);
  void ReconfigureAfterZygoteFork(const std::string& process_type);
  void ReconfigureAfterFeatureListInit(
      const std::string& process_type,
      bool configure_dangling_pointer_detector = true,
      bool is_in_death_test_child = false);
  void ReconfigureAfterTaskRunnerInit(const std::string& process_type);

  // |has_main_frame| tells us if the renderer contains a main frame.
  // The default value is intended for other process types, where the parameter
  // does not make sense.
  void OnForegrounded(bool has_main_frame = false);
  void OnBackgrounded();

#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  static std::string ExtractDanglingPtrSignatureForTests(
      std::string stacktrace);
#endif

  static PartitionAllocSupport* Get();

  static BrpConfiguration GetBrpConfiguration(const std::string& process_type);

  // Returns true if memory tagging should be enabled if available for the given
  // process type. May be called multiple times per process.
  static bool ShouldEnableMemoryTagging(const std::string& process_type);

  // For calling from within third_party/blink/.
  static bool ShouldEnableMemoryTaggingInRendererProcess();

  // Returns true if PA advanced checks should be enabled if available for the
  // given process type. May be called multiple times per process.
  static bool ShouldEnablePartitionAllocWithAdvancedChecks(
      const std::string& process_type);

 private:
  PartitionAllocSupport();

  base::Lock lock_;
  bool called_for_tests_ GUARDED_BY(lock_) = false;
  bool called_earlyish_ GUARDED_BY(lock_) = false;
  bool called_after_zygote_fork_ GUARDED_BY(lock_) = false;
  bool called_after_feature_list_init_ GUARDED_BY(lock_) = false;
  bool called_after_thread_pool_init_ GUARDED_BY(lock_) = false;
  std::string established_process_type_ GUARDED_BY(lock_) = "INVALID";

#if PA_CONFIG(THREAD_CACHE_SUPPORTED) && \
    PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  size_t largest_cached_size_ =
      ::partition_alloc::kThreadCacheDefaultSizeThreshold;
#endif
};

// Visible in header for testing.
class BASE_EXPORT MemoryReclaimerSupport {
 public:
  static MemoryReclaimerSupport& Instance();
  MemoryReclaimerSupport();
  ~MemoryReclaimerSupport();
  void Start(scoped_refptr<TaskRunner> task_runner);
  void SetForegrounded(bool in_foreground);

  void ResetForTesting();
  bool has_pending_task_for_testing() const { return has_pending_task_; }
  static TimeDelta GetInterval();

  // Visible for testing
  static constexpr base::TimeDelta kFirstPAPurgeOrReclaimDelay =
      base::Minutes(1);

 private:
  void Run();
  void MaybeScheduleTask(TimeDelta delay = TimeDelta());

  scoped_refptr<TaskRunner> task_runner_;
  bool in_foreground_ = true;
  bool has_pending_task_ = false;
};

// Utility function to detect Double-Free or Out-of-Bounds writes.
// This function can be called to memory assumed to be valid.
// If not, this may crash (not guaranteed).
// This is useful if you want to investigate crashes at `free()`,
// to know which point at execution it goes wrong.
BASE_EXPORT void CheckHeapIntegrity(const void* ptr);

// The function here is called right before crashing with
// `DoubleFreeOrCorruptionDetected()`. We provide an address for the slot start
// to the function, and it may use that for debugging purpose.
BASE_EXPORT void SetDoubleFreeOrCorruptionDetectedFn(void (*fn)(uintptr_t));

}  // namespace base::allocator

#endif  // BASE_ALLOCATOR_PARTITION_ALLOC_SUPPORT_H_
