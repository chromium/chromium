// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/starscan/pcscan_internal.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <set>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/allocation_guard.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/cpu.h"
#include "partition_alloc/partition_alloc_base/debug/alias.h"
#include "partition_alloc/partition_alloc_base/immediate_crash.h"
#include "partition_alloc/partition_alloc_base/memory/ref_counted.h"
#include "partition_alloc/partition_alloc_base/memory/scoped_refptr.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "partition_alloc/partition_alloc_buildflags.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_freelist_entry.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/stack/stack.h"
#include "partition_alloc/starscan/pcscan_scheduling.h"
#include "partition_alloc/starscan/raceful_worklist.h"
#include "partition_alloc/starscan/scan_loop.h"
#include "partition_alloc/starscan/snapshot.h"
#include "partition_alloc/starscan/stats_collector.h"
#include "partition_alloc/starscan/stats_reporter.h"
#include "partition_alloc/tagging.h"
#include "partition_alloc/thread_cache.h"

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#include "partition_alloc/address_pool_manager_bitmap.h"
#endif

#if PA_CONFIG(STARSCAN_NOINLINE_SCAN_FUNCTIONS)
#define PA_SCAN_INLINE PA_NOINLINE
#else
#define PA_SCAN_INLINE PA_ALWAYS_INLINE
#endif

namespace partition_alloc::internal {

[[noreturn]] PA_NOINLINE PA_NOT_TAIL_CALLED void DoubleFreeAttempt() {
  PA_NO_CODE_FOLDING();
  PA_IMMEDIATE_CRASH();
}

namespace {

#if PA_CONFIG(HAS_ALLOCATION_GUARD)
// Currently, check reentracy only on Linux. On Android TLS is emulated by the
// runtime lib, which can allocate and therefore cause reentrancy.
struct ReentrantScannerGuard final {
 public:
  ReentrantScannerGuard() {
    PA_CHECK(!guard_);
    guard_ = true;
  }
  ~ReentrantScannerGuard() { guard_ = false; }

 private:
  // Since this variable has hidden visibility (not referenced by other DSOs),
  // assume that thread_local works on all supported architectures.
  static thread_local size_t guard_;
};
thread_local size_t ReentrantScannerGuard::guard_ = 0;
#else
struct [[maybe_unused]] ReentrantScannerGuard final {};
#endif  // PA_CONFIG(HAS_ALLOCATION_GUARD)

// Scope that disables MTE checks. Only used inside scanning to avoid the race:
// a slot tag is changed by the mutator, while the scanner sees an old value.
struct DisableMTEScope final {
  DisableMTEScope() {
    ::partition_alloc::ChangeMemoryTaggingModeForCurrentThread(
        ::partition_alloc::TagViolationReportingMode::kDisabled);
  }
  ~DisableMTEScope() {
    ::partition_alloc::ChangeMemoryTaggingModeForCurrentThread(
        parent_tagging_mode);
  }

 private:
  ::partition_alloc::TagViolationReportingMode parent_tagging_mode =
      ::partition_alloc::internal::GetMemoryTaggingModeForCurrentThread();
};

#if PA_CONFIG(STARSCAN_USE_CARD_TABLE)
// Bytemap that represent regions (cards) that contain quarantined slots.
// A single PCScan cycle consists of the following steps:
// 1) clearing (memset quarantine + marking cards that contain quarantine);
// 2) scanning;
// 3) sweeping (freeing + unmarking cards that contain freed slots).
// Marking cards on step 1) ensures that the card table stays in the consistent
// state while scanning. Unmarking on the step 3) ensures that unmarking
// actually happens (and we don't hit too many false positives).
//
// The code here relies on the fact that |address| is in the regular pool and
// that the card table (this object) is allocated at the very beginning of that
// pool.
class QuarantineCardTable final {
 public:
  // Avoid the load of the base of the regular pool.
  PA_ALWAYS_INLINE static QuarantineCardTable& GetFrom(uintptr_t address) {
    PA_SCAN_DCHECK(IsManagedByPartitionAllocRegularPool(address));
    return *reinterpret_cast<QuarantineCardTable*>(
        address & PartitionAddressSpace::RegularPoolBaseMask());
  }

  PA_ALWAYS_INLINE void Quarantine(uintptr_t begin, size_t size) {
    return SetImpl(begin, size, true);
  }

  PA_ALWAYS_INLINE void Unquarantine(uintptr_t begin, size_t size) {
    return SetImpl(begin, size, false);
  }

  // Returns whether the card to which |address| points to contains quarantined
  // slots. May return false positives for but should never return false
  // negatives, as otherwise this breaks security.
  PA_ALWAYS_INLINE bool IsQuarantined(uintptr_t address) const {
    const size_t byte = Byte(address);
    PA_SCAN_DCHECK(byte < bytes_.size());
    return bytes_[byte];
  }

 private:
  static constexpr size_t kCardSize = kPoolMaxSize / kSuperPageSize;
  static constexpr size_t kBytes = kPoolMaxSize / kCardSize;

  QuarantineCardTable() = default;

  PA_ALWAYS_INLINE static size_t Byte(uintptr_t address) {
    return (address & ~PartitionAddressSpace::RegularPoolBaseMask()) /
           kCardSize;
  }

  PA_ALWAYS_INLINE void SetImpl(uintptr_t begin, size_t size, bool value) {
    const size_t byte = Byte(begin);
    const size_t need_bytes = (size + (kCardSize - 1)) / kCardSize;
    PA_SCAN_DCHECK(bytes_.size() >= byte + need_bytes);
    PA_SCAN_DCHECK(IsManagedByPartitionAllocRegularPool(begin));
    for (size_t i = byte; i < byte + need_bytes; ++i) {
      bytes_[i] = value;
    }
  }

  std::array<bool, kBytes> bytes_;
};
static_assert(kSuperPageSize >= sizeof(QuarantineCardTable),
              "Card table size must be less than kSuperPageSize, since this is "
              "what is committed");
#endif  // PA_CONFIG(STARSCAN_USE_CARD_TABLE)

template <typename T>
using MetadataVector = std::vector<T, InternalAllocator<T>>;
template <typename T>
using MetadataSet = std::set<T, std::less<>, InternalAllocator<T>>;
template <typename K, typename V>
using MetadataHashMap =
    std::unordered_map<K,
                       V,
                       std::hash<K>,
                       std::equal_to<>,
                       InternalAllocator<std::pair<const K, V>>>;

struct GetSlotStartResult final {
  PA_ALWAYS_INLINE bool is_found() const {
    PA_SCAN_DCHECK(!slot_start || slot_size);
    return slot_start;
  }

  uintptr_t slot_start = 0;
  size_t slot_size = 0;
};

// Returns the start of a slot, or 0 if |maybe_inner_address| is not inside of
// an existing slot span. The function may return a non-0 address even inside a
// decommitted or free slot span, it's the caller responsibility to check if
// memory is actually allocated.
//
// |maybe_inner_address| must be within a normal-bucket super page and can also
// point to guard pages or slot-span metadata.
PA_SCAN_INLINE GetSlotStartResult
GetSlotStartInSuperPage(uintptr_t maybe_inner_address) {
  PA_SCAN_DCHECK(IsManagedByNormalBuckets(maybe_inner_address));
  // Don't use SlotSpanMetadata/PartitionPage::FromAddr() and family, because
  // they expect an address within a super page payload area, which we don't
  // know yet if |maybe_inner_address| is.
  const uintptr_t super_page = maybe_inner_address & kSuperPageBaseMask;

  const uintptr_t partition_page_index =
      (maybe_inner_address & kSuperPageOffsetMask) >> PartitionPageShift();
  auto* page =
      PartitionSuperPageToMetadataArea(super_page) + partition_page_index;
  // Check if page is valid. The check also works for the guard pages and the
  // metadata page.
  if (!page->is_valid) {
    return {};
  }

  page -= page->slot_span_metadata_offset;
  PA_SCAN_DCHECK(page->is_valid);
  PA_SCAN_DCHECK(!page->slot_span_metadata_offset);
  auto* slot_span = &page->slot_span_metadata;
  // Check if the slot span is actually used and valid.
  if (!slot_span->bucket) {
    return {};
  }
#if PA_SCAN_DCHECK_IS_ON()
  DCheckIsValidSlotSpan(slot_span);
#endif
  const uintptr_t slot_span_start =
      SlotSpanMetadata::ToSlotSpanStart(slot_span);
  const ptrdiff_t ptr_offset = maybe_inner_address - slot_span_start;
  PA_SCAN_DCHECK(0 <= ptr_offset &&
                 ptr_offset < static_cast<ptrdiff_t>(
                                  slot_span->bucket->get_pages_per_slot_span() *
                                  PartitionPageSize()));
  // Slot span size in bytes is not necessarily multiple of partition page.
  // Don't check if the pointer points outside of usable area, since checking
  // the quarantine bit will anyway return false in this case.
  const size_t slot_size = slot_span->bucket->slot_size;
  const size_t slot_number = slot_span->bucket->GetSlotNumber(ptr_offset);
  const uintptr_t slot_start = slot_span_start + (slot_number * slot_size);
  PA_SCAN_DCHECK(slot_start <= maybe_inner_address &&
                 maybe_inner_address < slot_start + slot_size);
  return {.slot_start = slot_start, .slot_size = slot_size};
}

#if PA_SCAN_DCHECK_IS_ON()
bool IsQuarantineEmptyOnSuperPage(uintptr_t super_page) {
  auto* bitmap = SuperPageStateBitmap(super_page);
  size_t visited = 0;
  bitmap->IterateQuarantined([&visited](auto) { ++visited; });
  return !visited;
}
#endif

SimdSupport DetectSimdSupport() {
#if PA_CONFIG(STARSCAN_NEON_SUPPORTED)
  return SimdSupport::kNEON;
#else
  const base::CPU& cpu = base::CPU::GetInstanceNoAllocation();
  if (cpu.has_avx2()) {
    return SimdSupport::kAVX2;
  }
  if (cpu.has_sse41()) {
    return SimdSupport::kSSE41;
  }
  return SimdSupport::kUnvectorized;
#endif  // PA_CONFIG(STARSCAN_NEON_SUPPORTED)
}

void CommitCardTable() {
#if PA_CONFIG(STARSCAN_USE_CARD_TABLE)
  RecommitSystemPages(PartitionAddressSpace::RegularPoolBase(),
                      sizeof(QuarantineCardTable),
                      PageAccessibilityConfiguration(
                          PageAccessibilityConfiguration::kReadWrite),
                      PageAccessibilityDisposition::kRequireUpdate);
#endif
}

template <class Function>
void IterateNonEmptySlotSpans(uintptr_t super_page,
                              size_t nonempty_slot_spans,
                              Function function) {
  PA_SCAN_DCHECK(!(super_page % kSuperPageAlignment));
  PA_SCAN_DCHECK(nonempty_slot_spans);

  size_t slot_spans_to_visit = nonempty_slot_spans;
#if PA_SCAN_DCHECK_IS_ON()
  size_t visited = 0;
#endif

  IterateSlotSpans(super_page, true /*with_quarantine*/,
                   [&function, &slot_spans_to_visit
#if PA_SCAN_DCHECK_IS_ON()
                    ,
                    &visited
#endif
  ](SlotSpanMetadata* slot_span) {
                     if (slot_span->is_empty() || slot_span->is_decommitted()) {
                       // Skip empty/decommitted slot spans.
                       return false;
                     }
                     function(slot_span);
                     --slot_spans_to_visit;
#if PA_SCAN_DCHECK_IS_ON()
                     // In debug builds, scan all the slot spans to check that
                     // number of visited slot spans is equal to the number of
                     // nonempty_slot_spans.
                     ++visited;
                     return false;
#else
        return slot_spans_to_visit == 0;
#endif
                   });
#if PA_SCAN_DCHECK_IS_ON()
  // Check that exactly all non-empty slot spans have been visited.
  PA_DCHECK(nonempty_slot_spans == visited);
#endif
}

// SuperPageSnapshot is used to record all slot spans that contain live slots.
// The class avoids dynamic allocations and is designed to be instantiated on
// stack. To avoid stack overflow, internal data structures are kept packed.
class SuperPageSnapshot final {
  // The following constants are used to define a conservative estimate for
  // maximum number of slot spans in a super page.
  //
  // For systems with runtime-defined page size, assume partition page size is
  // at least 16kiB.
  static constexpr size_t kMinPartitionPageSize =
      __builtin_constant_p(PartitionPageSize()) ? PartitionPageSize() : 1 << 14;
  static constexpr size_t kStateBitmapMinReservedSize =
      __builtin_constant_p(ReservedStateBitmapSize())
          ? ReservedStateBitmapSize()
          : partition_alloc::internal::base::bits::AlignUp(
                sizeof(AllocationStateMap),
                kMinPartitionPageSize);
  // Take into account guard partition page at the end of super-page.
  static constexpr size_t kGuardPagesSize = 2 * kMinPartitionPageSize;

  static constexpr size_t kPayloadMaxSize =
      kSuperPageSize - kStateBitmapMinReservedSize - kGuardPagesSize;
  static_assert(kPayloadMaxSize % kMinPartitionPageSize == 0,
                "kPayloadMaxSize must be multiple of kMinPartitionPageSize");

  static constexpr size_t kMaxSlotSpansInSuperPage =
      kPayloadMaxSize / kMinPartitionPageSize;

 public:
  struct ScanArea {
    // Use packed integer types to save stack space. In theory, kAlignment could
    // be used instead of words, but it doesn't seem to bring savings.
    uint32_t offset_within_page_in_words;
    uint32_t size_in_words;
    uint32_t slot_size_in_words;
  };

  class ScanAreas : private std::array<ScanArea, kMaxSlotSpansInSuperPage> {
    using Base = std::array<ScanArea, kMaxSlotSpansInSuperPage>;

   public:
    using iterator = Base::iterator;
    using const_iterator = Base::const_iterator;
    using Base::operator[];

    iterator begin() { return Base::begin(); }
    const_iterator begin() const { return Base::begin(); }

    iterator end() { return std::next(begin(), size_); }
    const_iterator end() const { return std::next(begin(), size_); }

    void set_size(size_t new_size) { size_ = new_size; }

   private:
    size_t size_;
  };

  static_assert(std::is_trivially_default_constructible_v<ScanAreas>,
                "ScanAreas must be trivially default constructible to ensure "
                "that no memsets are generated by the compiler as a "
                "result of value-initialization (or zero-initialization)");

  void* operator new(size_t) = delete;
  void operator delete(void*) = delete;

  // Creates snapshot for a single super page. In theory, we could simply
  // iterate over slot spans without taking a snapshot. However, we do this to
  // minimize the mutex locking time. The mutex must be acquired to make sure
  // that no mutator is concurrently changing any of the slot spans.
  explicit SuperPageSnapshot(uintptr_t super_page_base);

  const ScanAreas& scan_areas() const { return scan_areas_; }

 private:
  ScanAreas scan_areas_;
};

static_assert(
    sizeof(SuperPageSnapshot) <= 2048,
    "SuperPageSnapshot must stay relatively small to be allocated on stack");

SuperPageSnapshot::SuperPageSnapshot(uintptr_t super_page) {
  auto* extent_entry = PartitionSuperPageToExtent(super_page);

  ::partition_alloc::internal::ScopedGuard lock(
      ::partition_alloc::internal::PartitionRootLock(extent_entry->root));

  const size_t nonempty_slot_spans =
      extent_entry->number_of_nonempty_slot_spans;
  if (!nonempty_slot_spans) {
#if PA_SCAN_DCHECK_IS_ON()
    // Check that quarantine bitmap is empty for super-pages that contain
    // only empty/decommitted slot-spans.
    PA_CHECK(IsQuarantineEmptyOnSuperPage(super_page));
#endif
    scan_areas_.set_size(0);
    return;
  }

  size_t current = 0;

  IterateNonEmptySlotSpans(
      super_page, nonempty_slot_spans,
      [this, &current](SlotSpanMetadata* slot_span) {
        const uintptr_t payload_begin =
            SlotSpanMetadata::ToSlotSpanStart(slot_span);
        // For single-slot slot-spans, scan only utilized slot part.
        const size_t provisioned_size =
            PA_UNLIKELY(slot_span->CanStoreRawSize())
                ? slot_span->GetRawSize()
                : slot_span->GetProvisionedSize();
        // Free & decommitted slot spans are skipped.
        PA_SCAN_DCHECK(provisioned_size > 0);
        const uintptr_t payload_end = payload_begin + provisioned_size;
        auto& area = scan_areas_[current];

        const size_t offset_in_words =
            (payload_begin & kSuperPageOffsetMask) / sizeof(uintptr_t);
        const size_t size_in_words =
            (payload_end - payload_begin) / sizeof(uintptr_t);
        const size_t slot_size_in_words =
            slot_span->bucket->slot_size / sizeof(uintptr_t);

#if PA_SCAN_DCHECK_IS_ON()
        PA_DCHECK(offset_in_words <=
                  std::numeric_limits<
                      decltype(area.offset_within_page_in_words)>::max());
        PA_DCHECK(size_in_words <=
                  std::numeric_limits<decltype(area.size_in_words)>::max());
        PA_DCHECK(
            slot_size_in_words <=
            std::numeric_limits<decltype(area.slot_size_in_words)>::max());
#endif

        area.offset_within_page_in_words = offset_in_words;
        area.size_in_words = size_in_words;
        area.slot_size_in_words = slot_size_in_words;

        ++current;
      });

  PA_SCAN_DCHECK(kMaxSlotSpansInSuperPage >= current);
  scan_areas_.set_size(current);
}

}  // namespace

class PCScanScanLoop;

// This class is responsible for performing the entire PCScan task.
// TODO(bikineev): Move PCScan algorithm out of PCScanTask.
class PCScanTask final : public base::RefCountedThreadSafe<PCScanTask>,
                         public InternalPartitionAllocated {
 public:
  // Creates and initializes a PCScan state.
  PCScanTask(PCScan& pcscan, size_t quarantine_last_size);

  PCScanTask(PCScanTask&&) noexcept = delete;
  PCScanTask& operator=(PCScanTask&&) noexcept = delete;

  // Execute PCScan from mutator inside safepoint.
  void RunFromMutator();

  // Execute PCScan from the scanner thread. Must be called only once from the
  // scanner thread.
  void RunFromScanner();

  PCScanScheduler& scheduler() const { return pcscan_.scheduler(); }

 private:
  class StackVisitor;
  friend class PCScanScanLoop;

  using Root = PCScan::Root;
  using SlotSpan = SlotSpanMetadata;

  // This is used:
  // - to synchronize all scanning threads (mutators and the scanner);
  // - for the scanner, to transition through the state machine
  //   (kScheduled -> kScanning (ctor) -> kSweepingAndFinishing (dtor).
  template <Context context>
  class SyncScope final {
   public:
    explicit SyncScope(PCScanTask& task) : task_(task) {
      task_.number_of_scanning_threads_.fetch_add(1, std::memory_order_relaxed);
      if (context == Context::kScanner) {
        task_.pcscan_.state_.store(PCScan::State::kScanning,
                                   std::memory_order_relaxed);
        task_.pcscan_.SetJoinableIfSafepointEnabled(true);
      }
    }
    ~SyncScope() {
      // First, notify the scanning thread that this thread is done.
      NotifyThreads();
      if (context == Context::kScanner) {
        // The scanner thread must wait here until all safepoints leave.
        // Otherwise, sweeping may free a page that can later be accessed by a
        // descheduled mutator.
        WaitForOtherThreads();
        task_.pcscan_.state_.store(PCScan::State::kSweepingAndFinishing,
                                   std::memory_order_relaxed);
      }
    }

   private:
    void NotifyThreads() {
      {
        // The lock is required as otherwise there is a race between
        // fetch_sub/notify in the mutator and checking
        // number_of_scanning_threads_/waiting in the scanner.
        std::lock_guard<std::mutex> lock(task_.mutex_);
        task_.number_of_scanning_threads_.fetch_sub(1,
                                                    std::memory_order_relaxed);
        {
          // Notify that scan is done and there is no need to enter
          // the safepoint. This also helps a mutator to avoid repeating
          // entering. Since the scanner thread waits for all threads to finish,
          // there is no ABA problem here.
          task_.pcscan_.SetJoinableIfSafepointEnabled(false);
        }
      }
      task_.condvar_.notify_all();
    }

    void WaitForOtherThreads() {
      std::unique_lock<std::mutex> lock(task_.mutex_);
      task_.condvar_.wait(lock, [this] {
        return !task_.number_of_scanning_threads_.load(
            std::memory_order_relaxed);
      });
    }

    PCScanTask& task_;
  };

  friend class base::RefCountedThreadSafe<PCScanTask>;
  ~PCScanTask() = default;

  PA_SCAN_INLINE AllocationStateMap* TryFindScannerBitmapForPointer(
      uintptr_t maybe_ptr) const;

  // Lookup and marking functions. Return size of the slot if marked, or zero
  // otherwise.
  PA_SCAN_INLINE size_t TryMarkSlotInNormalBuckets(uintptr_t maybe_ptr) const;

  // Scans stack, only called from safepoints.
  void ScanStack();

  // Scan individual areas.
  void ScanNormalArea(PCScanInternal& pcscan,
                      PCScanScanLoop& scan_loop,
                      uintptr_t begin,
                      uintptr_t end);
  void ScanLargeArea(PCScanInternal& pcscan,
                     PCScanScanLoop& scan_loop,
                     uintptr_t begin,
                     uintptr_t end,
                     size_t slot_size);

  // Scans all registered partitions and marks reachable quarantined slots.
  void ScanPartitions();

  // Clear quarantined slots and prepare card table for fast lookup
  void ClearQuarantinedSlotsAndPrepareCardTable();

  // Unprotect all slot spans from all partitions.
  void UnprotectPartitions();

  // Sweeps (frees) unreachable quarantined entries.
  void SweepQuarantine();

  // Finishes the scanner (updates limits, UMA, etc).
  void FinishScanner();

  // Cache the pcscan epoch to avoid the compiler loading the atomic
  // QuarantineData::epoch_ on each access.
  const size_t pcscan_epoch_;
  std::unique_ptr<StarScanSnapshot> snapshot_;
  StatsCollector stats_;
  // Mutex and codvar that are used to synchronize scanning threads.
  std::mutex mutex_;
  std::condition_variable condvar_;
  std::atomic<size_t> number_of_scanning_threads_{0u};
  // We can unprotect only once to reduce context-switches.
  std::once_flag unprotect_once_flag_;
  bool immediatelly_free_slots_{false};
  PCScan& pcscan_;
};

PA_SCAN_INLINE AllocationStateMap* PCScanTask::TryFindScannerBitmapForPointer(
    uintptr_t maybe_ptr) const {
  PA_SCAN_DCHECK(IsManagedByPartitionAllocRegularPool(maybe_ptr));
  // First, check if |maybe_ptr| points to a valid super page or a quarantined
  // card.
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#if PA_CONFIG(STARSCAN_USE_CARD_TABLE)
  // Check if |maybe_ptr| points to a quarantined card.
  if (PA_LIKELY(
          !QuarantineCardTable::GetFrom(maybe_ptr).IsQuarantined(maybe_ptr))) {
    return nullptr;
  }
#else   // PA_CONFIG(STARSCAN_USE_CARD_TABLE)
  // Without the card table, use the reservation offset table to check if
  // |maybe_ptr| points to a valid super-page. It's not as precise (meaning that
  // we may have hit the slow path more frequently), but reduces the memory
  // overhead.  Since we are certain here, that |maybe_ptr| refers to the
  // regular pool, it's okay to use non-checking version of
  // ReservationOffsetPointer().
  const uintptr_t offset =
      maybe_ptr & ~PartitionAddressSpace::RegularPoolBaseMask();
  if (PA_LIKELY(*ReservationOffsetPointer(kRegularPoolHandle, offset) !=
                kOffsetTagNormalBuckets)) {
    return nullptr;
  }
#endif  // PA_CONFIG(STARSCAN_USE_CARD_TABLE)
#else   // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  if (PA_LIKELY(!IsManagedByPartitionAllocRegularPool(maybe_ptr))) {
    return nullptr;
  }
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

  // We are certain here that |maybe_ptr| points to an allocated super-page.
  return StateBitmapFromAddr(maybe_ptr);
}

// Looks up and marks a potential dangling pointer. Returns the size of the slot
// (which is then accounted as quarantined), or zero if no slot is found.
// For normal bucket super pages, PCScan uses two quarantine bitmaps, the
// mutator and the scanner one. The former is used by mutators when slots are
// freed, while the latter is used concurrently by the PCScan thread. The
// bitmaps are swapped as soon as PCScan is triggered. Once a dangling pointer
// (which points to a slot in the scanner bitmap) is found,
// TryMarkSlotInNormalBuckets() marks it again in the bitmap and clears
// from the scanner bitmap. This way, when scanning is done, all uncleared
// entries in the scanner bitmap correspond to unreachable slots.
PA_SCAN_INLINE size_t
PCScanTask::TryMarkSlotInNormalBuckets(uintptr_t maybe_ptr) const {
  // Check if |maybe_ptr| points somewhere to the heap.
  // The caller has to make sure that |maybe_ptr| isn't MTE-tagged.
  auto* state_map = TryFindScannerBitmapForPointer(maybe_ptr);
  if (!state_map) {
    return 0;
  }

  // Beyond this point, we know that |maybe_ptr| is a pointer within a
  // normal-bucket super page.
  PA_SCAN_DCHECK(IsManagedByNormalBuckets(maybe_ptr));

#if !PA_CONFIG(STARSCAN_USE_CARD_TABLE)
  // Pointer from a normal bucket is always in the first superpage.
  auto* root = Root::FromAddrInFirstSuperpage(maybe_ptr);
  // Without the card table, we must make sure that |maybe_ptr| doesn't point to
  // metadata partition.
  // TODO(bikineev): To speed things up, consider removing the check and
  // committing quarantine bitmaps for metadata partition.
  // TODO(bikineev): Marking an entry in the reservation-table is not a
  // publishing operation, meaning that the |root| pointer may not be assigned
  // yet. This can happen as arbitrary pointers may point into a super-page
  // during its set up. Make sure to check |root| is not null before
  // dereferencing it.
  if (PA_UNLIKELY(!root || !root->IsQuarantineEnabled())) {
    return 0;
  }
#endif  // !PA_CONFIG(STARSCAN_USE_CARD_TABLE)

  // Check if pointer was in the quarantine bitmap.
  const GetSlotStartResult slot_start_result =
      GetSlotStartInSuperPage(maybe_ptr);
  if (!slot_start_result.is_found()) {
    return 0;
  }

  const uintptr_t slot_start = slot_start_result.slot_start;
  if (PA_LIKELY(!state_map->IsQuarantined(slot_start))) {
    return 0;
  }

  PA_SCAN_DCHECK((maybe_ptr & kSuperPageBaseMask) ==
                 (slot_start & kSuperPageBaseMask));

  if (PA_UNLIKELY(immediatelly_free_slots_)) {
    return 0;
  }

  // Now we are certain that |maybe_ptr| is a dangling pointer. Mark it again in
  // the mutator bitmap and clear from the scanner bitmap. Note that since
  // PCScan has exclusive access to the scanner bitmap, we can avoid atomic rmw
  // operation for it.
  if (PA_LIKELY(
          state_map->MarkQuarantinedAsReachable(slot_start, pcscan_epoch_))) {
    return slot_start_result.slot_size;
  }

  return 0;
}

void PCScanTask::ClearQuarantinedSlotsAndPrepareCardTable() {
  const PCScan::ClearType clear_type = pcscan_.clear_type_;

#if !PA_CONFIG(STARSCAN_USE_CARD_TABLE)
  if (clear_type == PCScan::ClearType::kEager) {
    return;
  }
#endif

  StarScanSnapshot::ClearingView view(*snapshot_);
  view.VisitConcurrently([clear_type](uintptr_t super_page) {
    auto* bitmap = StateBitmapFromAddr(super_page);
    auto* root = Root::FromFirstSuperPage(super_page);
    bitmap->IterateQuarantined([root, clear_type](uintptr_t slot_start) {
      auto* slot_span = SlotSpanMetadata::FromSlotStart(slot_start);
      // Use zero as a zapping value to speed up the fast bailout check in
      // ScanPartitions.
      const size_t size = root->GetSlotUsableSize(slot_span);
      if (clear_type == PCScan::ClearType::kLazy) {
        void* object = root->SlotStartToObject(slot_start);
        memset(object, 0, size);
      }
#if PA_CONFIG(STARSCAN_USE_CARD_TABLE)
      // Set card(s) for this quarantined slot.
      QuarantineCardTable::GetFrom(slot_start).Quarantine(slot_start, size);
#endif
    });
  });
}

void PCScanTask::UnprotectPartitions() {
  auto& pcscan = PCScanInternal::Instance();
  if (!pcscan.WriteProtectionEnabled()) {
    return;
  }

  StarScanSnapshot::UnprotectingView unprotect_view(*snapshot_);
  unprotect_view.VisitConcurrently([&pcscan](uintptr_t super_page) {
    SuperPageSnapshot super_page_snapshot(super_page);

    for (const auto& scan_area : super_page_snapshot.scan_areas()) {
      const uintptr_t begin =
          super_page |
          (scan_area.offset_within_page_in_words * sizeof(uintptr_t));
      const uintptr_t end =
          begin + (scan_area.size_in_words * sizeof(uintptr_t));

      pcscan.UnprotectPages(begin, end - begin);
    }
  });
}

class PCScanScanLoop final : public ScanLoop<PCScanScanLoop> {
  friend class ScanLoop<PCScanScanLoop>;

 public:
  explicit PCScanScanLoop(const PCScanTask& task)
      : ScanLoop(PCScanInternal::Instance().simd_support()), task_(task) {}

  size_t quarantine_size() const { return quarantine_size_; }

 private:
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  PA_ALWAYS_INLINE static uintptr_t RegularPoolBase() {
    return PartitionAddressSpace::RegularPoolBase();
  }
  PA_ALWAYS_INLINE static uintptr_t RegularPoolMask() {
    return PartitionAddressSpace::RegularPoolBaseMask();
  }
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

  PA_SCAN_INLINE void CheckPointer(uintptr_t maybe_ptr_maybe_tagged) {
    // |maybe_ptr| may have an MTE tag, so remove it first.
    quarantine_size_ +=
        task_.TryMarkSlotInNormalBuckets(UntagAddr(maybe_ptr_maybe_tagged));
  }

  const PCScanTask& task_;
  DisableMTEScope disable_mte_;
  size_t quarantine_size_ = 0;
};

class PCScanTask::StackVisitor final : public internal::StackVisitor {
 public:
  explicit StackVisitor(const PCScanTask& task) : task_(task) {}

  void VisitStack(uintptr_t* stack_ptr, uintptr_t* stack_top) override {
    static constexpr size_t kMinimalAlignment = 32;
    uintptr_t begin =
        reinterpret_cast<uintptr_t>(stack_ptr) & ~(kMinimalAlignment - 1);
    uintptr_t end =
        (reinterpret_cast<uintptr_t>(stack_top) + kMinimalAlignment - 1) &
        ~(kMinimalAlignment - 1);
    PA_CHECK(begin < end);
    PCScanScanLoop loop(task_);
    loop.Run(begin, end);
    quarantine_size_ += loop.quarantine_size();
  }

  // Returns size of quarantined slots that are reachable from the current
  // stack.
  size_t quarantine_size() const { return quarantine_size_; }

 private:
  const PCScanTask& task_;
  size_t quarantine_size_ = 0;
};

PCScanTask::PCScanTask(PCScan& pcscan, size_t quarantine_last_size)
    : pcscan_epoch_(pcscan.epoch() - 1),
      snapshot_(StarScanSnapshot::Create(PCScanInternal::Instance())),
      stats_(PCScanInternal::Instance().process_name(), quarantine_last_size),
      immediatelly_free_slots_(
          PCScanInternal::Instance().IsImmediateFreeingEnabled()),
      pcscan_(pcscan) {}

void PCScanTask::ScanStack() {
  const auto& pcscan = PCScanInternal::Instance();
  if (!pcscan.IsStackScanningEnabled()) {
    return;
  }
  // Check if the stack top was registered. It may happen that it's not if the
  // current allocation happens from pthread trampolines.
  void* stack_top = StackTopRegistry::Get().GetCurrentThreadStackTop();
  if (PA_UNLIKELY(!stack_top)) {
    return;
  }

  Stack stack_scanner(stack_top);
  StackVisitor visitor(*this);
  stack_scanner.IteratePointers(&visitor);
  stats_.IncreaseSurvivedQuarantineSize(visitor.quarantine_size());
}

void PCScanTask::ScanNormalArea(PCScanInternal& pcscan,
                                PCScanScanLoop& scan_loop,
                                uintptr_t begin,
                                uintptr_t end) {
  // Protect slot span before scanning it.
  pcscan.ProtectPages(begin, end - begin);
  scan_loop.Run(begin, end);
}

void PCScanTask::ScanLargeArea(PCScanInternal& pcscan,
                               PCScanScanLoop& scan_loop,
                               uintptr_t begin,
                               uintptr_t end,
                               size_t slot_size) {
  // For scanning large areas, it's worthwhile checking whether the range that
  // is scanned contains allocated slots. It also helps to skip discarded
  // freed slots.
  // Protect slot span before scanning it.
  pcscan.ProtectPages(begin, end - begin);

  auto* bitmap = StateBitmapFromAddr(begin);

  for (uintptr_t current_slot = begin; current_slot < end;
       current_slot += slot_size) {
    // It is okay to skip slots as the object they hold has been zapped at this
    // point, which means that the pointers no longer retain other slots.
    if (!bitmap->IsAllocated(current_slot)) {
      continue;
    }
    uintptr_t current_slot_end = current_slot + slot_size;
    // |slot_size| may be larger than |raw_size| for single-slot slot spans.
    scan_loop.Run(current_slot, std::min(current_slot_end, end));
  }
}

void PCScanTask::ScanPartitions() {
  // Threshold for which bucket size it is worthwhile in checking whether the
  // slot is allocated and needs to be scanned. PartitionPurgeSlotSpan()
  // purges only slots >= page-size, this helps us to avoid faulting in
  // discarded pages. We actually lower it further to 1024, to take advantage of
  // skipping unallocated slots, but don't want to go any lower, as this comes
  // at a cost of expensive bitmap checking.
  static constexpr size_t kLargeScanAreaThresholdInWords =
      1024 / sizeof(uintptr_t);

  PCScanScanLoop scan_loop(*this);
  auto& pcscan = PCScanInternal::Instance();

  StarScanSnapshot::ScanningView snapshot_view(*snapshot_);
  snapshot_view.VisitConcurrently([this, &pcscan,
                                   &scan_loop](uintptr_t super_page) {
    SuperPageSnapshot super_page_snapshot(super_page);

    for (const auto& scan_area : super_page_snapshot.scan_areas()) {
      const uintptr_t begin =
          super_page |
          (scan_area.offset_within_page_in_words * sizeof(uintptr_t));
      PA_SCAN_DCHECK(begin ==
                     super_page + (scan_area.offset_within_page_in_words *
                                   sizeof(uintptr_t)));
      const uintptr_t end = begin + scan_area.size_in_words * sizeof(uintptr_t);

      if (PA_UNLIKELY(scan_area.slot_size_in_words >=
                      kLargeScanAreaThresholdInWords)) {
        ScanLargeArea(pcscan, scan_loop, begin, end,
                      scan_area.slot_size_in_words * sizeof(uintptr_t));
      } else {
        ScanNormalArea(pcscan, scan_loop, begin, end);
      }
    }
  });

  stats_.IncreaseSurvivedQuarantineSize(scan_loop.quarantine_size());
}

namespace {

struct SweepStat {
  // Bytes that were really swept (by calling free()).
  size_t swept_bytes = 0;
  // Bytes of marked quarantine memory that were discarded (by calling
  // madvice(DONT_NEED)).
  size_t discarded_bytes = 0;
};

void UnmarkInCardTable(uintptr_t slot_start, SlotSpanMetadata* slot_span) {
#if PA_CONFIG(STARSCAN_USE_CARD_TABLE)
  // Reset card(s) for this quarantined slot. Please note that the cards may
  // still contain quarantined slots (which were promoted in this scan cycle),
  // but ClearQuarantinedSlotsAndPrepareCardTable() will set them again in the
  // next PCScan cycle.
  QuarantineCardTable::GetFrom(slot_start)
      .Unquarantine(slot_start, slot_span->GetUtilizedSlotSize());
#endif  // PA_CONFIG(STARSCAN_USE_CARD_TABLE)
}

[[maybe_unused]] size_t FreeAndUnmarkInCardTable(PartitionRoot* root,
                                                 SlotSpanMetadata* slot_span,
                                                 uintptr_t slot_start) {
  void* object = root->SlotStartToObject(slot_start);
  root->FreeNoHooksImmediate(object, slot_span, slot_start);
  UnmarkInCardTable(slot_start, slot_span);
  return slot_span->bucket->slot_size;
}

[[maybe_unused]] void SweepSuperPage(PartitionRoot* root,
                                     uintptr_t super_page,
                                     size_t epoch,
                                     SweepStat& stat) {
  auto* bitmap = StateBitmapFromAddr(super_page);
  PartitionRoot::FromFirstSuperPage(super_page);
  bitmap->IterateUnmarkedQuarantined(epoch, [root,
                                             &stat](uintptr_t slot_start) {
    auto* slot_span = SlotSpanMetadata::FromSlotStart(slot_start);
    stat.swept_bytes += FreeAndUnmarkInCardTable(root, slot_span, slot_start);
  });
}

[[maybe_unused]] void SweepSuperPageAndDiscardMarkedQuarantine(
    PartitionRoot* root,
    uintptr_t super_page,
    size_t epoch,
    SweepStat& stat) {
  auto* bitmap = StateBitmapFromAddr(super_page);
  bitmap->IterateQuarantined(epoch, [root, &stat](uintptr_t slot_start,
                                                  bool is_marked) {
    auto* slot_span = SlotSpanMetadata::FromSlotStart(slot_start);
    if (PA_LIKELY(!is_marked)) {
      stat.swept_bytes += FreeAndUnmarkInCardTable(root, slot_span, slot_start);
      return;
    }
    // Otherwise, try to discard pages for marked quarantine. Since no data is
    // stored in quarantined slots (e.g. the |next| pointer), this can be
    // freely done.
    const size_t slot_size = slot_span->bucket->slot_size;
    if (slot_size >= SystemPageSize()) {
      const uintptr_t discard_end =
          base::bits::AlignDown(slot_start + slot_size, SystemPageSize());
      const uintptr_t discard_begin =
          base::bits::AlignUp(slot_start, SystemPageSize());
      const intptr_t discard_size = discard_end - discard_begin;
      if (discard_size > 0) {
        DiscardSystemPages(discard_begin, discard_size);
        stat.discarded_bytes += discard_size;
      }
    }
  });
}

[[maybe_unused]] void SweepSuperPageWithBatchedFree(PartitionRoot* root,
                                                    uintptr_t super_page,
                                                    size_t epoch,
                                                    SweepStat& stat) {
  auto* bitmap = StateBitmapFromAddr(super_page);
  SlotSpanMetadata* previous_slot_span = nullptr;
  internal::PartitionFreelistEntry* freelist_tail = nullptr;
  internal::PartitionFreelistEntry* freelist_head = nullptr;
  size_t freelist_entries = 0;

  const auto bitmap_iterator = [&](uintptr_t slot_start) {
    SlotSpanMetadata* current_slot_span =
        SlotSpanMetadata::FromSlotStart(slot_start);
    const internal::PartitionFreelistDispatcher* freelist_dispatcher =
        root->get_freelist_dispatcher();
    auto* entry = freelist_dispatcher->EmplaceAndInitNull(slot_start);

    if (current_slot_span != previous_slot_span) {
      // We started scanning a new slot span. Flush the accumulated freelist to
      // the slot-span's freelist. This is a single lock acquired per slot span.
      if (previous_slot_span && freelist_entries) {
        root->RawFreeBatch(freelist_head, freelist_tail, freelist_entries,
                           previous_slot_span);
      }
      freelist_head = entry;
      freelist_tail = nullptr;
      freelist_entries = 0;
      previous_slot_span = current_slot_span;
    }

    if (freelist_tail) {
      freelist_dispatcher->SetNext(freelist_tail, entry);
    }
    freelist_tail = entry;
    ++freelist_entries;

    UnmarkInCardTable(slot_start, current_slot_span);

    stat.swept_bytes += current_slot_span->bucket->slot_size;
  };

  bitmap->IterateUnmarkedQuarantinedAndFree(epoch, bitmap_iterator);

  if (previous_slot_span && freelist_entries) {
    root->RawFreeBatch(freelist_head, freelist_tail, freelist_entries,
                       previous_slot_span);
  }
}

}  // namespace

void PCScanTask::SweepQuarantine() {
  // Check that scan is unjoinable by this time.
  PA_DCHECK(!pcscan_.IsJoinable());
  // Discard marked quarantine memory on every Nth scan.
  // TODO(bikineev): Find a better signal (e.g. memory pressure, high
  // survival rate, etc).
  static constexpr size_t kDiscardMarkedQuarantineFrequency = 16;
  const bool should_discard =
      (pcscan_epoch_ % kDiscardMarkedQuarantineFrequency == 0) &&
      (pcscan_.clear_type_ == PCScan::ClearType::kEager);

  SweepStat stat;
  StarScanSnapshot::SweepingView sweeping_view(*snapshot_);
  sweeping_view.VisitNonConcurrently(
      [this, &stat, should_discard](uintptr_t super_page) {
        auto* root = PartitionRoot::FromFirstSuperPage(super_page);

#if PA_CONFIG(STARSCAN_BATCHED_FREE)
        SweepSuperPageWithBatchedFree(root, super_page, pcscan_epoch_, stat);
        (void)should_discard;
#else
        if (PA_UNLIKELY(should_discard && !root->settings.use_cookie)) {
          SweepSuperPageAndDiscardMarkedQuarantine(root, super_page,
                                                   pcscan_epoch_, stat);
        } else {
          SweepSuperPage(root, super_page, pcscan_epoch_, stat);
        }
#endif  // PA_CONFIG(STARSCAN_BATCHED_FREE)
      });

  stats_.IncreaseSweptSize(stat.swept_bytes);
  stats_.IncreaseDiscardedQuarantineSize(stat.discarded_bytes);

#if PA_CONFIG(THREAD_CACHE_SUPPORTED)
  // Sweeping potentially frees into the current thread's thread cache. Purge
  // releases the cache back to the global allocator.
  auto* current_thread_tcache = ThreadCache::Get();
  if (ThreadCache::IsValid(current_thread_tcache)) {
    current_thread_tcache->Purge();
  }
#endif  // PA_CONFIG(THREAD_CACHE_SUPPORTED)
}

void PCScanTask::FinishScanner() {
  stats_.ReportTracesAndHists(PCScanInternal::Instance().GetReporter());

  pcscan_.scheduler_.scheduling_backend().UpdateScheduleAfterScan(
      stats_.survived_quarantine_size(), stats_.GetOverallTime(),
      PCScanInternal::Instance().CalculateTotalHeapSize());

  PCScanInternal::Instance().ResetCurrentPCScanTask();
  // Change the state and check that concurrent task can't be scheduled twice.
  PA_CHECK(pcscan_.state_.exchange(PCScan::State::kNotRunning,
                                   std::memory_order_acq_rel) ==
           PCScan::State::kSweepingAndFinishing);
}

void PCScanTask::RunFromMutator() {
  ReentrantScannerGuard reentrancy_guard;
  StatsCollector::MutatorScope overall_scope(
      stats_, StatsCollector::MutatorId::kOverall);
  {
    SyncScope<Context::kMutator> sync_scope(*this);
    // Mutator might start entering the safepoint while scanning was already
    // finished.
    if (!pcscan_.IsJoinable()) {
      return;
    }
    {
      // Clear all quarantined slots and prepare card table.
      StatsCollector::MutatorScope clear_scope(
          stats_, StatsCollector::MutatorId::kClear);
      ClearQuarantinedSlotsAndPrepareCardTable();
    }
    {
      // Scan the thread's stack to find dangling references.
      StatsCollector::MutatorScope scan_scope(
          stats_, StatsCollector::MutatorId::kScanStack);
      ScanStack();
    }
    {
      // Unprotect all scanned pages, if needed.
      UnprotectPartitions();
    }
    {
      // Scan heap for dangling references.
      StatsCollector::MutatorScope scan_scope(stats_,
                                              StatsCollector::MutatorId::kScan);
      ScanPartitions();
    }
  }
}

void PCScanTask::RunFromScanner() {
  ReentrantScannerGuard reentrancy_guard;
  {
    StatsCollector::ScannerScope overall_scope(
        stats_, StatsCollector::ScannerId::kOverall);
    {
      SyncScope<Context::kScanner> sync_scope(*this);
      {
        // Clear all quarantined slots and prepare the card table.
        StatsCollector::ScannerScope clear_scope(
            stats_, StatsCollector::ScannerId::kClear);
        ClearQuarantinedSlotsAndPrepareCardTable();
      }
      {
        // Scan heap for dangling references.
        StatsCollector::ScannerScope scan_scope(
            stats_, StatsCollector::ScannerId::kScan);
        ScanPartitions();
      }
      {
        // Unprotect all scanned pages, if needed.
        UnprotectPartitions();
      }
    }
    {
      // Sweep unreachable quarantined slots.
      StatsCollector::ScannerScope sweep_scope(
          stats_, StatsCollector::ScannerId::kSweep);
      SweepQuarantine();
    }
  }
  FinishScanner();
}

class PCScan::PCScanThread final {
 public:
  using TaskHandle = PCScanInternal::TaskHandle;

  static PCScanThread& Instance() {
    // Lazily instantiate the scanning thread.
    static internal::base::NoDestructor<PCScanThread> instance;
    return *instance;
  }

  void PostTask(TaskHandle task) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      PA_DCHECK(!posted_task_.get());
      posted_task_ = std::move(task);
      wanted_delay_ = base::TimeDelta();
    }
    condvar_.notify_one();
  }

  void PostDelayedTask(base::TimeDelta delay) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (posted_task_.get()) {
        return;
      }
      wanted_delay_ = delay;
    }
    condvar_.notify_one();
  }

 private:
  friend class internal::base::NoDestructor<PCScanThread>;

  PCScanThread() {
    ScopedAllowAllocations allow_allocations_within_std_thread;
    std::thread{[](PCScanThread* instance) {
                  static constexpr const char* kThreadName = "PCScan";
                  // Ideally we should avoid mixing base:: and std:: API for
                  // threading, but this is useful for visualizing the pcscan
                  // thread in chrome://tracing.
                  internal::base::PlatformThread::SetName(kThreadName);
                  instance->TaskLoop();
                },
                this}
        .detach();
  }

  // Waits and returns whether the delay should be recomputed.
  bool Wait(std::unique_lock<std::mutex>& lock) {
    PA_DCHECK(lock.owns_lock());
    if (wanted_delay_.is_zero()) {
      condvar_.wait(lock, [this] {
        // Re-evaluate if either delay changed, or a task was
        // enqueued.
        return !wanted_delay_.is_zero() || posted_task_.get();
      });
      // The delay has already been set up and should not be queried again.
      return false;
    }
    condvar_.wait_for(
        lock, std::chrono::microseconds(wanted_delay_.InMicroseconds()));
    // If no task has been posted, the delay should be recomputed at this point.
    return !posted_task_.get();
  }

  void TaskLoop() {
    while (true) {
      TaskHandle current_task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        // Scheduling.
        while (!posted_task_.get()) {
          if (Wait(lock)) {
            wanted_delay_ =
                scheduler().scheduling_backend().UpdateDelayedSchedule();
            if (wanted_delay_.is_zero()) {
              break;
            }
          }
        }
        // Differentiate between a posted task and a delayed task schedule.
        if (posted_task_.get()) {
          std::swap(current_task, posted_task_);
          wanted_delay_ = base::TimeDelta();
        } else {
          PA_DCHECK(wanted_delay_.is_zero());
        }
      }
      // Differentiate between a posted task and a delayed task schedule.
      if (current_task.get()) {
        current_task->RunFromScanner();
      } else {
        PCScan::Instance().PerformScan(PCScan::InvocationMode::kNonBlocking);
      }
    }
  }

  PCScanScheduler& scheduler() const { return PCScan::Instance().scheduler(); }

  std::mutex mutex_;
  std::condition_variable condvar_;
  TaskHandle posted_task_;
  base::TimeDelta wanted_delay_;
};

PCScanInternal::PCScanInternal() : simd_support_(DetectSimdSupport()) {}

PCScanInternal::~PCScanInternal() = default;

void PCScanInternal::Initialize(PCScan::InitConfig config) {
  PA_DCHECK(!is_initialized_);
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  // Make sure that pools are initialized.
  PartitionAddressSpace::Init();
#endif
  CommitCardTable();
#if PA_CONFIG(STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
  if (config.write_protection ==
      PCScan::InitConfig::WantedWriteProtectionMode::kEnabled) {
    write_protector_ = std::make_unique<UserFaultFDWriteProtector>();
  } else {
    write_protector_ = std::make_unique<NoWriteProtector>();
  }
#else
  write_protector_ = std::make_unique<NoWriteProtector>();
#endif  // PA_CONFIG(STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
  PCScan::SetClearType(write_protector_->SupportedClearType());

  if (config.safepoint == PCScan::InitConfig::SafepointMode::kEnabled) {
    PCScan::Instance().EnableSafepoints();
  }
  scannable_roots_ = RootsMap();
  nonscannable_roots_ = RootsMap();

  static partition_alloc::StatsReporter s_no_op_reporter;
  PCScan::Instance().RegisterStatsReporter(&s_no_op_reporter);

  // Don't initialize PCScanThread::Instance() as otherwise sandbox complains
  // about multiple threads running on sandbox initialization.
  is_initialized_ = true;
}

void PCScanInternal::PerformScan(PCScan::InvocationMode invocation_mode) {
#if PA_SCAN_DCHECK_IS_ON()
  PA_DCHECK(is_initialized());
  PA_DCHECK(scannable_roots().size() > 0);
  PA_DCHECK(std::all_of(
      scannable_roots().begin(), scannable_roots().end(),
      [](const auto& pair) { return pair.first->IsScanEnabled(); }));
  PA_DCHECK(std::all_of(
      nonscannable_roots().begin(), nonscannable_roots().end(),
      [](const auto& pair) { return pair.first->IsQuarantineEnabled(); }));
#endif

  PCScan& frontend = PCScan::Instance();
  {
    // If scanning is already in progress, bail out.
    PCScan::State expected = PCScan::State::kNotRunning;
    if (!frontend.state_.compare_exchange_strong(
            expected, PCScan::State::kScheduled, std::memory_order_acq_rel,
            std::memory_order_relaxed)) {
      return;
    }
  }

  const size_t last_quarantine_size =
      frontend.scheduler_.scheduling_backend().ScanStarted();

  // Create PCScan task and set it as current.
  auto task = base::MakeRefCounted<PCScanTask>(frontend, last_quarantine_size);
  PCScanInternal::Instance().SetCurrentPCScanTask(task);

  if (PA_UNLIKELY(invocation_mode ==
                  PCScan::InvocationMode::kScheduleOnlyForTesting)) {
    // Immediately change the state to enable safepoint testing.
    frontend.state_.store(PCScan::State::kScanning, std::memory_order_release);
    frontend.SetJoinableIfSafepointEnabled(true);
    return;
  }

  // Post PCScan task.
  if (PA_LIKELY(invocation_mode == PCScan::InvocationMode::kNonBlocking)) {
    PCScan::PCScanThread::Instance().PostTask(std::move(task));
  } else {
    PA_SCAN_DCHECK(PCScan::InvocationMode::kBlocking == invocation_mode ||
                   PCScan::InvocationMode::kForcedBlocking == invocation_mode);
    std::move(*task).RunFromScanner();
  }
}

void PCScanInternal::PerformScanIfNeeded(
    PCScan::InvocationMode invocation_mode) {
  if (!scannable_roots().size()) {
    return;
  }
  PCScan& frontend = PCScan::Instance();
  if (invocation_mode == PCScan::InvocationMode::kForcedBlocking ||
      frontend.scheduler_.scheduling_backend()
          .GetQuarantineData()
          .MinimumScanningThresholdReached()) {
    PerformScan(invocation_mode);
  }
}

void PCScanInternal::PerformDelayedScan(base::TimeDelta delay) {
  PCScan::PCScanThread::Instance().PostDelayedTask(delay);
}

void PCScanInternal::JoinScan() {
  // Current task can be destroyed by the scanner. Check that it's valid.
  if (auto current_task = CurrentPCScanTask()) {
    current_task->RunFromMutator();
  }
}

PCScanInternal::TaskHandle PCScanInternal::CurrentPCScanTask() const {
  std::lock_guard<std::mutex> lock(current_task_mutex_);
  return current_task_;
}

void PCScanInternal::SetCurrentPCScanTask(TaskHandle task) {
  std::lock_guard<std::mutex> lock(current_task_mutex_);
  current_task_ = std::move(task);
}

void PCScanInternal::ResetCurrentPCScanTask() {
  std::lock_guard<std::mutex> lock(current_task_mutex_);
  current_task_.reset();
}

namespace {
PCScanInternal::SuperPages GetSuperPagesAndCommitStateBitmaps(
    PCScan::Root& root) {
  const size_t state_bitmap_size_to_commit = CommittedStateBitmapSize();
  PCScanInternal::SuperPages super_pages;
  for (auto* super_page_extent = root.first_extent; super_page_extent;
       super_page_extent = super_page_extent->next) {
    for (uintptr_t super_page = SuperPagesBeginFromExtent(super_page_extent),
                   super_page_end = SuperPagesEndFromExtent(super_page_extent);
         super_page != super_page_end; super_page += kSuperPageSize) {
      // Make sure the metadata is committed.
      // TODO(bikineev): Remove once this is known to work.
      const volatile char* metadata =
          reinterpret_cast<char*>(PartitionSuperPageToMetadataArea(super_page));
      *metadata;
      RecommitSystemPages(SuperPageStateBitmapAddr(super_page),
                          state_bitmap_size_to_commit,
                          PageAccessibilityConfiguration(
                              PageAccessibilityConfiguration::kReadWrite),
                          PageAccessibilityDisposition::kRequireUpdate);
      super_pages.push_back(super_page);
    }
  }
  return super_pages;
}
}  // namespace

void PCScanInternal::RegisterScannableRoot(Root* root) {
  PA_DCHECK(is_initialized());
  PA_DCHECK(root);
  // Avoid nesting locks and store super_pages in a temporary vector.
  SuperPages super_pages;
  {
    ::partition_alloc::internal::ScopedGuard guard(
        ::partition_alloc::internal::PartitionRootLock(root));
    PA_CHECK(root->IsQuarantineAllowed());
    if (root->IsScanEnabled()) {
      return;
    }
    PA_CHECK(!root->IsQuarantineEnabled());
    super_pages = GetSuperPagesAndCommitStateBitmaps(*root);
    root->settings.scan_mode = Root::ScanMode::kEnabled;
    root->settings.quarantine_mode = Root::QuarantineMode::kEnabled;
  }
  std::lock_guard<std::mutex> lock(roots_mutex_);
  PA_DCHECK(!scannable_roots_.count(root));
  auto& root_super_pages = scannable_roots_[root];
  root_super_pages.insert(root_super_pages.end(), super_pages.begin(),
                          super_pages.end());
}

void PCScanInternal::RegisterNonScannableRoot(Root* root) {
  PA_DCHECK(is_initialized());
  PA_DCHECK(root);
  // Avoid nesting locks and store super_pages in a temporary vector.
  SuperPages super_pages;
  {
    ::partition_alloc::internal::ScopedGuard guard(
        ::partition_alloc::internal::PartitionRootLock(root));
    PA_CHECK(root->IsQuarantineAllowed());
    PA_CHECK(!root->IsScanEnabled());
    if (root->IsQuarantineEnabled()) {
      return;
    }
    super_pages = GetSuperPagesAndCommitStateBitmaps(*root);
    root->settings.quarantine_mode = Root::QuarantineMode::kEnabled;
  }
  std::lock_guard<std::mutex> lock(roots_mutex_);
  PA_DCHECK(!nonscannable_roots_.count(root));
  auto& root_super_pages = nonscannable_roots_[root];
  root_super_pages.insert(root_super_pages.end(), super_pages.begin(),
                          super_pages.end());
}

void PCScanInternal::RegisterNewSuperPage(Root* root,
                                          uintptr_t super_page_base) {
  PA_DCHECK(is_initialized());
  PA_DCHECK(root);
  PA_CHECK(root->IsQuarantineAllowed());
  PA_DCHECK(!(super_page_base % kSuperPageAlignment));
  // Make sure the metadata is committed.
  // TODO(bikineev): Remove once this is known to work.
  const volatile char* metadata = reinterpret_cast<char*>(
      PartitionSuperPageToMetadataArea(super_page_base));
  *metadata;

  std::lock_guard<std::mutex> lock(roots_mutex_);

  // Dispatch based on whether root is scannable or not.
  if (root->IsScanEnabled()) {
    PA_DCHECK(scannable_roots_.count(root));
    auto& super_pages = scannable_roots_[root];
    PA_DCHECK(std::find(super_pages.begin(), super_pages.end(),
                        super_page_base) == super_pages.end());
    super_pages.push_back(super_page_base);
  } else {
    PA_DCHECK(root->IsQuarantineEnabled());
    PA_DCHECK(nonscannable_roots_.count(root));
    auto& super_pages = nonscannable_roots_[root];
    PA_DCHECK(std::find(super_pages.begin(), super_pages.end(),
                        super_page_base) == super_pages.end());
    super_pages.push_back(super_page_base);
  }
}

void PCScanInternal::SetProcessName(const char* process_name) {
  PA_DCHECK(is_initialized());
  PA_DCHECK(process_name);
  PA_DCHECK(!process_name_);
  process_name_ = process_name;
}

size_t PCScanInternal::CalculateTotalHeapSize() const {
  PA_DCHECK(is_initialized());
  std::lock_guard<std::mutex> lock(roots_mutex_);
  const auto acc = [](size_t size, const auto& pair) {
    return size + pair.first->get_total_size_of_committed_pages();
  };
  return std::accumulate(scannable_roots_.begin(), scannable_roots_.end(), 0u,
                         acc) +
         std::accumulate(nonscannable_roots_.begin(), nonscannable_roots_.end(),
                         0u, acc);
}

void PCScanInternal::EnableStackScanning() {
  PA_DCHECK(!stack_scanning_enabled_);
  stack_scanning_enabled_ = true;
}
void PCScanInternal::DisableStackScanning() {
  PA_DCHECK(stack_scanning_enabled_);
  stack_scanning_enabled_ = false;
}
bool PCScanInternal::IsStackScanningEnabled() const {
  return stack_scanning_enabled_;
}

bool PCScanInternal::WriteProtectionEnabled() const {
  return write_protector_->IsEnabled();
}

void PCScanInternal::ProtectPages(uintptr_t begin, size_t size) {
  // Slot-span sizes are multiple of system page size. However, the ranges that
  // are recorded are not, since in the snapshot we only record the used
  // payload. Therefore we align up the incoming range by 4k. The unused part of
  // slot-spans doesn't need to be protected (the allocator will enter the
  // safepoint before trying to allocate from it).
  PA_SCAN_DCHECK(write_protector_.get());
  write_protector_->ProtectPages(
      begin,
      partition_alloc::internal::base::bits::AlignUp(size, SystemPageSize()));
}

void PCScanInternal::UnprotectPages(uintptr_t begin, size_t size) {
  PA_SCAN_DCHECK(write_protector_.get());
  write_protector_->UnprotectPages(
      begin,
      partition_alloc::internal::base::bits::AlignUp(size, SystemPageSize()));
}

void PCScanInternal::ClearRootsForTesting() {
  std::lock_guard<std::mutex> lock(roots_mutex_);
  // Set all roots as non-scannable and non-quarantinable.
  for (auto& pair : scannable_roots_) {
    Root* root = pair.first;
    root->settings.scan_mode = Root::ScanMode::kDisabled;
    root->settings.quarantine_mode = Root::QuarantineMode::kDisabledByDefault;
  }
  for (auto& pair : nonscannable_roots_) {
    Root* root = pair.first;
    root->settings.quarantine_mode = Root::QuarantineMode::kDisabledByDefault;
  }
  // Make sure to destroy maps so that on the following ReinitForTesting() call
  // the maps don't attempt to destroy the backing.
  scannable_roots_.clear();
  scannable_roots_.~RootsMap();
  nonscannable_roots_.clear();
  nonscannable_roots_.~RootsMap();
  // Destroy write protector object, so that there is no double free on the next
  // call to ReinitForTesting();
  write_protector_.reset();
}

void PCScanInternal::ReinitForTesting(PCScan::InitConfig config) {
  is_initialized_ = false;
  auto* new_this = new (this) PCScanInternal;
  new_this->Initialize(config);
}

void PCScanInternal::FinishScanForTesting() {
  auto current_task = CurrentPCScanTask();
  PA_CHECK(current_task.get());
  current_task->RunFromScanner();
}

void PCScanInternal::RegisterStatsReporter(
    partition_alloc::StatsReporter* reporter) {
  PA_DCHECK(reporter);
  stats_reporter_ = reporter;
}

partition_alloc::StatsReporter& PCScanInternal::GetReporter() {
  PA_DCHECK(stats_reporter_);
  return *stats_reporter_;
}

}  // namespace partition_alloc::internal
