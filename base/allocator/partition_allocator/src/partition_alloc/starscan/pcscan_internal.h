// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_STARSCAN_PCSCAN_INTERNAL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_STARSCAN_PCSCAN_INTERNAL_H_

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "partition_alloc/internal_allocator_forward.h"
#include "partition_alloc/partition_alloc_base/memory/scoped_refptr.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/starscan/pcscan.h"
#include "partition_alloc/starscan/starscan_fwd.h"
#include "partition_alloc/starscan/write_protector.h"

namespace partition_alloc::internal {

class PCScanTask;

// Internal PCScan singleton. The separation between frontend and backend is
// needed to keep access to the hot data (quarantine) in the frontend fast,
// whereas the backend can hold cold data.
class PCScanInternal final {
 public:
  using Root = PCScan::Root;
  using TaskHandle = scoped_refptr<PCScanTask>;

  using SuperPages =
      std::vector<uintptr_t, internal::InternalAllocator<uintptr_t>>;
  using RootsMap = std::unordered_map<
      Root*,
      SuperPages,
      std::hash<Root*>,
      std::equal_to<>,
      internal::InternalAllocator<std::pair<Root* const, SuperPages>>>;

  static PCScanInternal& Instance() {
    // Since the data that PCScanInternal holds is cold, it's fine to have the
    // runtime check for thread-safe local static initialization.
    static internal::base::NoDestructor<PCScanInternal> instance;
    return *instance;
  }

  PCScanInternal(const PCScanInternal&) = delete;
  PCScanInternal& operator=(const PCScanInternal&) = delete;

  ~PCScanInternal();

  void Initialize(PCScan::InitConfig);
  bool is_initialized() const { return is_initialized_; }

  void PerformScan(PCScan::InvocationMode);
  void PerformScanIfNeeded(PCScan::InvocationMode);
  void PerformDelayedScan(base::TimeDelta delay);
  void JoinScan();

  TaskHandle CurrentPCScanTask() const;
  void SetCurrentPCScanTask(TaskHandle task);
  void ResetCurrentPCScanTask();

  void RegisterScannableRoot(Root*);
  void RegisterNonScannableRoot(Root*);

  RootsMap& scannable_roots() { return scannable_roots_; }
  const RootsMap& scannable_roots() const { return scannable_roots_; }

  RootsMap& nonscannable_roots() { return nonscannable_roots_; }
  const RootsMap& nonscannable_roots() const { return nonscannable_roots_; }

  void RegisterNewSuperPage(Root* root, uintptr_t super_page_base);

  void SetProcessName(const char* name);
  const char* process_name() const { return process_name_; }

  // Get size of all committed pages from scannable and nonscannable roots.
  size_t CalculateTotalHeapSize() const;

  SimdSupport simd_support() const { return simd_support_; }

  void EnableStackScanning();
  void DisableStackScanning();
  bool IsStackScanningEnabled() const;

  void EnableImmediateFreeing() { immediate_freeing_enabled_ = true; }
  bool IsImmediateFreeingEnabled() const { return immediate_freeing_enabled_; }

  void NotifyThreadCreated(void* stack_top);
  void NotifyThreadDestroyed();

  void* GetCurrentThreadStackTop() const;

  bool WriteProtectionEnabled() const;
  void ProtectPages(uintptr_t begin, size_t size);
  void UnprotectPages(uintptr_t begin, size_t size);

  void ClearRootsForTesting();                // IN-TEST
  void ReinitForTesting(PCScan::InitConfig);  // IN-TEST
  void FinishScanForTesting();                // IN-TEST

  void RegisterStatsReporter(partition_alloc::StatsReporter* reporter);
  partition_alloc::StatsReporter& GetReporter();

 private:
  friend internal::base::NoDestructor<PCScanInternal>;
  friend class StarScanSnapshot;

  using StackTops = std::unordered_map<
      internal::base::PlatformThreadId,
      void*,
      std::hash<internal::base::PlatformThreadId>,
      std::equal_to<>,
      internal::InternalAllocator<
          std::pair<const internal::base::PlatformThreadId, void*>>>;

  PCScanInternal();

  TaskHandle current_task_;
  mutable std::mutex current_task_mutex_;

  RootsMap scannable_roots_;
  RootsMap nonscannable_roots_;
  mutable std::mutex roots_mutex_;

  bool stack_scanning_enabled_{false};
  // TLS emulation of stack tops. Since this is guaranteed to go through
  // non-quarantinable partition, using it from safepoints is safe.
  StackTops stack_tops_;
  mutable std::mutex stack_tops_mutex_;

  bool immediate_freeing_enabled_{false};

  const char* process_name_ = nullptr;
  const SimdSupport simd_support_;

  std::unique_ptr<WriteProtector> write_protector_;
  partition_alloc::StatsReporter* stats_reporter_ = nullptr;

  bool is_initialized_ = false;
};

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_STARSCAN_PCSCAN_INTERNAL_H_
