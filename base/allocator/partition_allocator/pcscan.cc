// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/pcscan.h"

#include <map>
#include <set>
// TODO(bikineev): Change to base's thread.
#include <thread>
#include <vector>

#include "base/allocator/partition_allocator/object_bitmap.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace internal {

namespace {

ThreadSafePartitionRoot& PCScanMetadataAllocator() {
  static base::NoDestructor<ThreadSafePartitionRoot> allocator{
      PartitionOptions{PartitionOptions::Alignment::kRegular,
                       PartitionOptions::ThreadCache::kDisabled,
                       PartitionOptions::PCScan::kDisabled}};
  return *allocator;
}

// STL allocator which is needed to keep internal data structures required by
// PCScan.
template <typename T>
class MetadataAllocator {
 public:
  using value_type = T;

  MetadataAllocator() = default;

  template <typename U>
  MetadataAllocator(const MetadataAllocator<U>&) {}  // NOLINT

  template <typename U>
  MetadataAllocator& operator=(const MetadataAllocator<U>&) {}

  template <typename U>
  bool operator==(const MetadataAllocator<U>&) {
    return true;
  }

  value_type* allocate(size_t size) {
    return static_cast<value_type*>(PCScanMetadataAllocator().AllocFlagsNoHooks(
        0, size * sizeof(value_type)));
  }

  void deallocate(value_type* ptr, size_t size) {
    PCScanMetadataAllocator().FreeNoHooks(ptr);
  }
};

void ReportStats(size_t swept_bytes, size_t last_size, size_t new_size) {
  VLOG(2) << "swept bytes: " << swept_bytes;
  VLOG(2) << "quarantine size: " << last_size << " -> " << new_size;
  VLOG(2) << "quarantine survival rate: "
          << static_cast<double>(new_size) / last_size;
}

}  // namespace

// This class is responsible for performing the entire PCScan task.
template <bool thread_safe>
class PCScan<thread_safe>::PCScanTask final {
 public:
  // Creates and initializes a PCScan state from PartitionRoot.
  PCScanTask(PCScan& pcscan, Root& root);

  // Only allow moving to make sure that the state is not redundantly copied.
  PCScanTask(PCScanTask&&) noexcept = default;
  PCScanTask& operator=(PCScanTask&&) noexcept = default;

  // Execute PCScan. Must be executed only once.
  void RunOnce() &&;

 private:
  using SlotSpan = SlotSpanMetadata<thread_safe>;

  struct ScanArea {
    uintptr_t* begin = nullptr;
    uintptr_t* end = nullptr;
  };
  using ScanAreas = std::vector<ScanArea, MetadataAllocator<ScanArea>>;

  // Super pages only correspond to normal buckets.
  // TODO(bikineev): Consider flat containers since the number of elements is
  // relatively small. This requires making base containers allocator-aware.
  using SuperPages =
      std::set<uintptr_t, std::less<>, MetadataAllocator<uintptr_t>>;

  QuarantineBitmap* FindScannerBitmapForPointer(uintptr_t maybe_ptr) const;

  // Lookup and marking functions. Return size of the object if marked or zero
  // otherwise.
  size_t TryMarkObjectInNormalBucketPool(uintptr_t maybe_ptr);

  // Clear quarantined objects inside the PCScan task.
  void ClearQuarantinedObjects() const;

  // Scans the partition and marks reachable quarantined objects. Returns the
  // size of marked objects. The function race-fully reads the heap and
  // therefore tsan is disabled for it.
  size_t ScanPartition() NO_SANITIZE("thread");

  // Sweeps (frees) unreachable quarantined entries. Returns the size of swept
  // objects.
  size_t SweepQuarantine();

  PCScan<thread_safe>& pcscan_;
  PartitionRoot<thread_safe>& root_;

  ScanAreas scan_areas_;
  SuperPages super_pages_;
};

template <bool thread_safe>
QuarantineBitmap* PCScan<thread_safe>::PCScanTask::FindScannerBitmapForPointer(
    uintptr_t maybe_ptr) const {
  // TODO(bikineev): Consider using the bitset in AddressPoolManager::Pool to
  // quickly find a super page.
  const auto super_page_base = maybe_ptr & kSuperPageBaseMask;

  auto it = super_pages_.lower_bound(super_page_base);
  if (it == super_pages_.end() || *it != super_page_base)
    return nullptr;

  if (!IsWithinSuperPagePayload(reinterpret_cast<char*>(maybe_ptr),
                                true /*with pcscan*/))
    return nullptr;

  // We are certain here that |maybe_ptr| points to the super page payload.
  return QuarantineBitmapFromPointer(QuarantineBitmapType::kScanner,
                                     pcscan_.quarantine_data_.epoch(),
                                     reinterpret_cast<char*>(maybe_ptr));
}

// Looks up and marks a potential dangling pointer. Returns the size of the slot
// (which is then accounted as quarantined) or zero if no object is found.
// For normal bucket super pages, PCScan uses two quarantine bitmaps, the
// mutator and the scanner one. The former is used by mutators when objects are
// freed, while the latter is used concurrently by the PCScan thread. The
// bitmaps are swapped as soon as PCScan is triggered. Once a dangling pointer
// (which points to an object in the scanner bitmap) is found,
// TryMarkObjectInNormalBucketPool() marks it again in the bitmap and clears
// from the scanner bitmap. This way, when scanning is done, all uncleared
// entries in the scanner bitmap correspond to unreachable objects.
template <bool thread_safe>
size_t PCScan<thread_safe>::PCScanTask::TryMarkObjectInNormalBucketPool(
    uintptr_t maybe_ptr) {
  // Check if maybe_ptr points somewhere to the heap.
  auto* bitmap = FindScannerBitmapForPointer(maybe_ptr);
  if (!bitmap)
    return 0;

  // Check if pointer was in the quarantine bitmap.
  const uintptr_t base = bitmap->FindPotentialObjectBeginning(maybe_ptr);
  if (!base)
    return 0;

  PA_DCHECK((maybe_ptr & kSuperPageBaseMask) == (base & kSuperPageBaseMask));

  auto target_slot_span =
      SlotSpan::FromPointerNoAlignmentCheck(reinterpret_cast<void*>(base));
  PA_DCHECK(&root_ ==
            PartitionRoot<thread_safe>::FromSlotSpan(target_slot_span));

  const size_t usable_size = PartitionSizeAdjustSubtract(
      root_.allow_extras, target_slot_span->GetUtilizedSlotSize());
  // Range check for inner pointers.
  if (maybe_ptr >= base + usable_size)
    return 0;

  // Now we are certain that |maybe_ptr| is a dangling pointer. Mark it again in
  // the mutator bitmap and clear from the scanner bitmap.
  bitmap->ClearBit(base);
  QuarantineBitmapFromPointer(QuarantineBitmapType::kMutator,
                              pcscan_.quarantine_data_.epoch(),
                              reinterpret_cast<char*>(base))
      ->SetBit(base);
  return target_slot_span->bucket->slot_size;
}

template <bool thread_safe>
void PCScan<thread_safe>::PCScanTask::ClearQuarantinedObjects() const {
  const bool allow_extras = root_.allow_extras;
  for (auto super_page : super_pages_) {
    auto* bitmap = QuarantineBitmapFromPointer(
        QuarantineBitmapType::kScanner, pcscan_.quarantine_data_.epoch(),
        reinterpret_cast<char*>(super_page));
    bitmap->Iterate([allow_extras](uintptr_t ptr) {
      auto* object = reinterpret_cast<void*>(ptr);
      auto* slot_span = SlotSpan::FromPointerNoAlignmentCheck(object);
      // Use zero as a zapping value to speed up the fast bailout check in
      // ScanPartition.
      memset(object, 0,
             PartitionSizeAdjustSubtract(allow_extras,
                                         slot_span->GetUtilizedSlotSize()));
    });
  }
}

template <bool thread_safe>
size_t PCScan<thread_safe>::PCScanTask::ScanPartition() NO_SANITIZE("thread") {
  static_assert(alignof(uintptr_t) % alignof(void*) == 0,
                "Alignment of uintptr_t must be at least as strict as "
                "alignment of a pointer type.");
  size_t new_quarantine_size = 0;

  for (auto scan_area : scan_areas_) {
    for (uintptr_t* payload = scan_area.begin; payload < scan_area.end;
         ++payload) {
      PA_DCHECK(reinterpret_cast<uintptr_t>(payload) % alignof(void*) == 0);
      auto maybe_ptr = *payload;
      if (!maybe_ptr)
        continue;
      size_t slot_size = 0;
// TODO(bikineev): Remove the preprocessor condition after 32bit GigaCage is
// implemented.
#if defined(PA_HAS_64_BITS_POINTERS)
      // On partitions without extras (partitions with aligned allocations),
      // memory is not allocated from the GigaCage.
      if (features::IsPartitionAllocGigaCageEnabled() && root_.allow_extras) {
        // With GigaCage, we first do a fast bitmask check to see if the pointer
        // points to the normal bucket pool.
        if (!PartitionAddressSpace::IsInNormalBucketPool(
                reinterpret_cast<void*>(maybe_ptr)))
          continue;
        // Otherwise, search in the list of super pages.
        slot_size = TryMarkObjectInNormalBucketPool(maybe_ptr);
        // TODO(bikineev): Check IsInDirectBucketPool.
      } else
#endif
      {
        slot_size = TryMarkObjectInNormalBucketPool(maybe_ptr);
      }

      new_quarantine_size += slot_size;
    }
  }

  return new_quarantine_size;
}

template <bool thread_safe>
size_t PCScan<thread_safe>::PCScanTask::SweepQuarantine() {
  size_t swept_bytes = 0;

  for (auto super_page : super_pages_) {
    auto* bitmap = QuarantineBitmapFromPointer(
        QuarantineBitmapType::kScanner, pcscan_.quarantine_data_.epoch(),
        reinterpret_cast<char*>(super_page));
    bitmap->Iterate([this, &swept_bytes](uintptr_t ptr) {
      auto* object = reinterpret_cast<void*>(ptr);
      auto* slot_span = SlotSpan::FromPointerNoAlignmentCheck(object);
      swept_bytes += slot_span->bucket->slot_size;
      root_.FreeNoHooksImmediate(object, slot_span);
    });
    bitmap->Clear();
  }

  return swept_bytes;
}

template <bool thread_safe>
PCScan<thread_safe>::PCScanTask::PCScanTask(PCScan& pcscan, Root& root)
    : pcscan_(pcscan), root_(root) {
  // Take a snapshot of all allocated non-empty slot spans.
  static constexpr size_t kScanAreasReservationSlack = 10;
  const size_t kScanAreasReservationSize = root_.total_size_of_committed_pages /
                                           PartitionPageSize() /
                                           kScanAreasReservationSlack;
  scan_areas_.reserve(kScanAreasReservationSize);

  typename Root::ScopedGuard guard(root.lock_);
  // Take a snapshot of all super pages and scannable slot spans.
  // TODO(bikineev): Consider making current_extent lock-free and moving it to
  // the concurrent thread.
  for (auto* super_page_extent = root_.first_extent; super_page_extent;
       super_page_extent = super_page_extent->next) {
    for (char* super_page = super_page_extent->super_page_base;
         super_page != super_page_extent->super_pages_end;
         super_page += kSuperPageSize) {
      // TODO(bikineev): Consider following freelists instead of slot spans.
      IterateActiveAndFullSlotSpans<thread_safe>(
          super_page, true /*with pcscan*/, [this](SlotSpan* slot_span) {
            auto* payload_begin =
                static_cast<uintptr_t*>(SlotSpan::ToPointer(slot_span));
            auto* payload_end =
                payload_begin +
                (slot_span->bucket->get_bytes_per_span() / sizeof(uintptr_t));
            scan_areas_.push_back({payload_begin, payload_end});
          });
      super_pages_.insert(reinterpret_cast<uintptr_t>(super_page));
    }
  }
}

// TODO(bikineev): Synchronize task execution with destruction of
// PartitionRoot/PCScan.
template <bool thread_safe>
PCScan<thread_safe>::~PCScan() = default;

template <bool thread_safe>
void PCScan<thread_safe>::PCScanTask::RunOnce() && {
  TRACE_EVENT0("partition_alloc", "PCScan");

  // Clear all quarantined objects.
  ClearQuarantinedObjects();

  // Mark and sweep the quarantine list.
  const auto new_quarantine_size = ScanPartition();
  const auto swept_bytes = SweepQuarantine();

  ReportStats(swept_bytes, pcscan_.quarantine_data_.last_size(),
              new_quarantine_size);

  pcscan_.quarantine_data_.Account(new_quarantine_size);
  pcscan_.quarantine_data_.GrowLimitIfNeeded();

  // Check that concurrent task can't be scheduled twice.
  PA_CHECK(pcscan_.in_progress_.exchange(false));
}

template <bool thread_safe>
void PCScan<thread_safe>::ScheduleTask(TaskType task_type) {
  PA_DCHECK(root_);
  PA_DCHECK(root_->pcscan);

  if (in_progress_.exchange(true)) {
    // Bail out if PCScan is already in progress.
    return;
  }

  quarantine_data_.ResetAndAdvanceEpoch();

  // Initialize PCScan task.
  PCScanTask task(*this, *root_);

  // Post PCScan task.
  const auto callback = [](PCScanTask task) { std::move(task).RunOnce(); };
  if (UNLIKELY(task_type == TaskType::kBlockingForTesting)) {
    // Blocking is only used for testing.
    callback(std::move(task));
  } else if (LIKELY(base::ThreadPoolInstance::Get())) {
    // If available, try to use base::ThreadPool.
    base::ThreadPool::PostTask(FROM_HERE,
                               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
                               base::BindOnce(callback, std::move(task)));
  } else {
    // Otherwise, retreat to kernel threads. TODO(bikineev): Use base's threads.
    std::thread{callback, std::move(task)}.detach();
  }
}

template class PCScan<ThreadSafe>;
template class PCScan<NotThreadSafe>;

}  // namespace internal
}  // namespace base
