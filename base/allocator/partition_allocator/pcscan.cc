// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/pcscan.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <set>
#include <thread>
#include <vector>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#include "base/allocator/partition_allocator/object_bitmap.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/compiler_specific.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_X86_64)
// Include order is important, so we disable formatting.
// clang-format off
#include <immintrin.h>
// Including these headers directly should generally be avoided. For the
// scanning loop, we check at runtime which SIMD extension we can use. Since
// Chrome is compiled with -msse3 (the minimal requirement), we include the
// headers directly to make the intrinsics available. Another option could be to
// use inline assembly, but that would hinder compiler optimization for
// vectorized instructions.
#include <avxintrin.h>
#include <avx2intrin.h>
// clang-format on
#endif

namespace base {
namespace internal {

namespace {

// Similar to std::bitset, but doesn't perform out-of-bounds checks.
template <size_t Size>
class SimpleBitset final {
 public:
  SimpleBitset() { memset(bitset_.data(), 0, bitset_.size()); }

  ALWAYS_INLINE void Set(size_t position) {
    const auto ib = GetIndexAndBit(position);
    PA_DCHECK(!Test(position));
    bitset_[ib.index] |= (1 << ib.bit);
  }

  ALWAYS_INLINE bool Test(size_t position) const {
    const auto ib = GetIndexAndBit(position);
    return bitset_[ib.index] & (1 << ib.bit);
  }

 private:
  struct IndexAndBit {
    size_t index;
    size_t bit;
  };

  ALWAYS_INLINE static constexpr IndexAndBit GetIndexAndBit(size_t position) {
    PA_DCHECK(kBitmapSize > (position / kCellSize));
    static_assert(
        base::bits::IsPowerOfTwo(kCellSize),
        "Cell size must be power of two for the compiler to optimize division");
    return {position / kCellSize, position % kCellSize};
  }

  static constexpr size_t kCellSize = sizeof(uint8_t) * CHAR_BIT;
  static constexpr size_t kBitmapSize = Size / kCellSize;
  std::array<uint8_t, kBitmapSize> bitset_;
};

ThreadSafePartitionRoot& PCScanMetadataAllocator() {
  static base::NoDestructor<ThreadSafePartitionRoot> allocator{
      PartitionOptions{PartitionOptions::Alignment::kRegular,
                       PartitionOptions::ThreadCache::kDisabled,
                       PartitionOptions::Quarantine::kDisallowed,
                       PartitionOptions::RefCount::kDisabled}};
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
  VLOG(2) << "quarantine size: " << last_size << " -> " << new_size
          << ", swept bytes: " << swept_bytes
          << ", survival rate: " << static_cast<double>(new_size) / last_size;
}

template <bool thread_safe>
ALWAYS_INLINE uintptr_t
GetObjectStartInSuperPage(uintptr_t maybe_ptr,
                          const PartitionRoot<thread_safe>& root) {
  char* allocation_start =
      GetSlotStartInSuperPage<thread_safe>(reinterpret_cast<char*>(maybe_ptr));
  if (!allocation_start) {
    // |maybe_ptr| refers to a garbage or is outside of the payload region.
    return 0;
  }
  return reinterpret_cast<uintptr_t>(
      root.AdjustPointerForExtrasAdd(allocation_start));
}

#if DCHECK_IS_ON()
bool IsScannerQuarantineBitmapEmpty(char* super_page, size_t epoch) {
  auto* bitmap = QuarantineBitmapFromPointer(QuarantineBitmapType::kScanner,
                                             epoch, super_page);
  size_t visited = 0;
  bitmap->Iterate([&visited](auto) { ++visited; });
  return !visited;
}
#endif

namespace scopes {
constexpr char kPCScan[] = "PCScan";
constexpr char kClear[] = "PCScan.Clear";
constexpr char kScan[] = "PCScan.Scan";
constexpr char kSweep[] = "PCScan.Sweep";
}  // namespace scopes
constexpr char kTraceCategory[] = "partition_alloc";
#define PCSCAN_EVENT(scope) TRACE_EVENT0(kTraceCategory, (scope))

}  // namespace

// This class is responsible for performing the entire PCScan task.
template <bool thread_safe>
class PCScan<thread_safe>::PCScanTask final {
 public:
  static void* operator new(size_t size) {
    return PCScanMetadataAllocator().AllocFlagsNoHooks(0, size);
  }

  static void operator delete(void* ptr) {
    PCScanMetadataAllocator().FreeNoHooks(ptr);
  }

  // Creates and initializes a PCScan state.
  explicit PCScanTask(PCScan& pcscan);

  // Only allow moving to make sure that the state is not redundantly copied.
  PCScanTask(PCScanTask&&) noexcept = default;
  PCScanTask& operator=(PCScanTask&&) noexcept = default;

  // Execute PCScan. Must be executed only once.
  void RunOnce() &&;

 private:
  class ScanLoop;

  using SlotSpan = SlotSpanMetadata<thread_safe>;

  struct ScanArea {
    ScanArea(uintptr_t* begin, uintptr_t* end) : begin(begin), end(end) {}

    uintptr_t* begin;
    uintptr_t* end;
  };
  using ScanAreas = std::vector<ScanArea, MetadataAllocator<ScanArea>>;

  // Large scan areas have their slot size recorded which allows to iterate
  // based on objects, potentially skipping over objects if possible.
  struct LargeScanArea : public ScanArea {
    LargeScanArea(uintptr_t* begin, uintptr_t* end, size_t slot_size)
        : ScanArea(begin, end), slot_size(slot_size) {}

    size_t slot_size = 0;
  };
  using LargeScanAreas =
      std::vector<LargeScanArea, MetadataAllocator<LargeScanArea>>;

  // Super pages only correspond to normal buckets.
  // TODO(bikineev): Consider flat containers since the number of elements is
  // relatively small. This requires making base containers allocator-aware.
  using SuperPages =
      std::set<uintptr_t, std::less<>, MetadataAllocator<uintptr_t>>;

  class SuperPagesBitmap final {
   public:
    void Populate(const SuperPages& super_pages) {
      for (uintptr_t super_page_base : super_pages) {
#if DCHECK_IS_ON()
        PA_DCHECK(!(super_page_base % kSuperPageAlignment));
        PA_DCHECK(IsManagedByPartitionAllocNormalBuckets(
            reinterpret_cast<char*>(super_page_base)));
#endif
        bitset_.Set(NormalBucketPoolOffset(super_page_base));
      }
    }

    ALWAYS_INLINE bool Test(uintptr_t maybe_ptr) const {
#if DCHECK_IS_ON()
      PA_DCHECK(IsManagedByPartitionAllocNormalBuckets(
          reinterpret_cast<char*>(maybe_ptr)));
#endif
      return bitset_.Test(NormalBucketPoolOffset(maybe_ptr));
    }

   private:
    static constexpr size_t kBitmapSize =
        AddressPoolManager::kNormalBucketMaxSize >> kSuperPageShift;

    ALWAYS_INLINE static constexpr size_t NormalBucketPoolOffset(
        uintptr_t ptr) {
      constexpr uintptr_t kNormalBucketPoolMask =
#if defined(PA_HAS_64_BITS_POINTERS)
          PartitionAddressSpace::NormalBucketPoolBaseMask();
#else
          0;
#endif
      return static_cast<size_t>((ptr & ~kNormalBucketPoolMask) >>
                                 kSuperPageShift);
    }

    SimpleBitset<kBitmapSize> bitset_;
  };

  struct BitmapLookupPolicy {
    ALWAYS_INLINE bool TestOnHeapPointer(uintptr_t maybe_ptr) const {
#if DCHECK_IS_ON()
      PA_DCHECK(IsManagedByPartitionAllocNormalBuckets(
          reinterpret_cast<void*>(maybe_ptr)));
#endif
      return task_.super_pages_bitmap_.Test(maybe_ptr);
    }
    const PCScanTask& task_;
  };

  struct BinaryLookupPolicy {
    ALWAYS_INLINE bool TestOnHeapPointer(uintptr_t maybe_ptr) const {
      const auto super_page_base = maybe_ptr & kSuperPageBaseMask;
      auto it = task_.super_pages_.lower_bound(super_page_base);
      return it != task_.super_pages_.end() && *it == super_page_base;
    }
    const PCScanTask& task_;
  };

  template <typename LookupPolicy>
  ALWAYS_INLINE QuarantineBitmap* TryFindScannerBitmapForPointer(
      uintptr_t maybe_ptr) const;

  // Lookup and marking functions. Return size of the object if marked or zero
  // otherwise.
  template <typename LookupPolicy>
  ALWAYS_INLINE size_t
  TryMarkObjectInNormalBucketPool(uintptr_t maybe_ptr) const;

  // Scans all registeres partitions and marks reachable quarantined objects.
  // Returns the size of marked objects.
  size_t ScanPartitions();

  // Clear quarantined objects inside the PCScan task.
  void ClearQuarantinedObjects() const;

  // Sweeps (frees) unreachable quarantined entries. Returns the size of swept
  // objects.
  size_t SweepQuarantine();

  PCScan<thread_safe>& pcscan_;

  ScanAreas scan_areas_;
  LargeScanAreas large_scan_areas_;
  SuperPages super_pages_;
  SuperPagesBitmap super_pages_bitmap_;
};

template <bool thread_safe>
template <typename LookupPolicy>
ALWAYS_INLINE QuarantineBitmap*
PCScan<thread_safe>::PCScanTask::TryFindScannerBitmapForPointer(
    uintptr_t maybe_ptr) const {
  // First, check if |maybe_ptr| points to a valid super page.
  LookupPolicy lookup{*this};
  if (!lookup.TestOnHeapPointer(maybe_ptr))
    return nullptr;
  // Check if we are not pointing to metadata/guard pages.
  if (!IsWithinSuperPagePayload(reinterpret_cast<char*>(maybe_ptr),
                                true /*with quarantine*/))
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
template <typename LookupPolicy>
ALWAYS_INLINE size_t
PCScan<thread_safe>::PCScanTask::TryMarkObjectInNormalBucketPool(
    uintptr_t maybe_ptr) const {
  using AccessType = QuarantineBitmap::AccessType;
  // Check if maybe_ptr points somewhere to the heap.
  auto* scanner_bitmap =
      TryFindScannerBitmapForPointer<LookupPolicy>(maybe_ptr);
  if (!scanner_bitmap)
    return 0;

  auto* root =
      Root::FromPointerInNormalBucketPool(reinterpret_cast<char*>(maybe_ptr));

  // Check if pointer was in the quarantine bitmap.
  const uintptr_t base =
      GetObjectStartInSuperPage<thread_safe>(maybe_ptr, *root);
  if (!base || !scanner_bitmap->template CheckBit<AccessType::kNonAtomic>(base))
    return 0;

  PA_DCHECK((maybe_ptr & kSuperPageBaseMask) == (base & kSuperPageBaseMask));

  auto target_slot_span =
      SlotSpan::FromSlotInnerPtr(reinterpret_cast<void*>(base));
  PA_DCHECK(root == PartitionRoot<thread_safe>::FromSlotSpan(target_slot_span));

  const size_t usable_size = target_slot_span->GetUsableSize(root);
  // Range check for inner pointers.
  if (maybe_ptr >= base + usable_size)
    return 0;

  // Now we are certain that |maybe_ptr| is a dangling pointer. Mark it again in
  // the mutator bitmap and clear from the scanner bitmap. Note that since
  // PCScan has exclusive access to the scanner bitmap, we can avoid atomic rmw
  // operation for it.
  scanner_bitmap->template ClearBit<AccessType::kNonAtomic>(base);
  QuarantineBitmapFromPointer(QuarantineBitmapType::kMutator,
                              pcscan_.quarantine_data_.epoch(),
                              reinterpret_cast<char*>(base))
      ->template SetBit<AccessType::kAtomic>(base);
  return target_slot_span->bucket->slot_size;
}

template <bool thread_safe>
void PCScan<thread_safe>::PCScanTask::ClearQuarantinedObjects() const {
  using AccessType = QuarantineBitmap::AccessType;

  PCSCAN_EVENT(scopes::kClear);

  for (auto super_page : super_pages_) {
    auto* bitmap = QuarantineBitmapFromPointer(
        QuarantineBitmapType::kScanner, pcscan_.quarantine_data_.epoch(),
        reinterpret_cast<char*>(super_page));
    auto* root = Root::FromSuperPage(reinterpret_cast<char*>(super_page));
    bitmap->template Iterate<AccessType::kNonAtomic>([root](uintptr_t ptr) {
      auto* object = reinterpret_cast<void*>(ptr);
      auto* slot_span = SlotSpan::FromSlotInnerPtr(object);
      // Use zero as a zapping value to speed up the fast bailout check in
      // ScanPartitions.
      memset(object, 0, slot_span->GetUsableSize(root));
    });
  }
}

// Class used to perform actual scanning. Dispatches at runtime based on
// supported SIMD extensions.
template <bool thread_safe>
class PCScan<thread_safe>::PCScanTask::ScanLoop final {
 public:
  explicit ScanLoop(const PCScanTask& pcscan_task)
      : scan_function_(GetScanFunction()),
        pcscan_task_(pcscan_task)
#if defined(PA_HAS_64_BITS_POINTERS)
        ,
        normal_bucket_pool_base_(PartitionAddressSpace::NormalBucketPoolBase())
#endif
  {
  }

  ScanLoop(const ScanLoop&) = delete;
  ScanLoop& operator=(const ScanLoop&) = delete;

  // Scans a range of addresses and marks reachable quarantined objects. Returns
  // the size of marked objects. The function race-fully reads the heap and
  // therefore TSAN is disabled for the dispatch functions.
  size_t Run(uintptr_t* begin, uintptr_t* end) const {
    static_assert(alignof(uintptr_t) % alignof(void*) == 0,
                  "Alignment of uintptr_t must be at least as strict as "
                  "alignment of a pointer type.");
    return (this->*scan_function_)(begin, end);
  }

 private:
  // This is to support polymorphic behavior and to avoid virtual calls.
  using ScanFunction = size_t (ScanLoop::*)(uintptr_t*, uintptr_t*) const;

  static ScanFunction GetScanFunction() {
    if (UNLIKELY(!features::IsPartitionAllocGigaCageEnabled())) {
      return &ScanLoop::RunUnvectorizedNoGigaCage;
    }
    // We define vectorized versions of the scanning loop only for 64bit since
    // they require support of the 64bit GigaCage, and only for x86 because
    // a special instruction set is required.
#if defined(ARCH_CPU_X86_64)
    base::CPU cpu;
    if (cpu.has_avx2())
      return &ScanLoop::RunAVX2;
    if (cpu.has_sse3())
      return &ScanLoop::RunSSE3;
#endif  // defined(ARCH_CPU_X86_64)
    return &ScanLoop::RunUnvectorized;
  }

#if defined(PA_HAS_64_BITS_POINTERS)
  ALWAYS_INLINE bool IsInNormalBucketPool(uintptr_t maybe_ptr) const {
    return (maybe_ptr & PartitionAddressSpace::NormalBucketPoolBaseMask()) ==
           normal_bucket_pool_base_;
  }
#endif

#if defined(ARCH_CPU_X86_64)
  __attribute__((target("sse3"))) NO_SANITIZE("thread") size_t
      RunSSE3(uintptr_t* begin, uintptr_t* end) const {
    static constexpr size_t kAlignmentRequirement = 16;
    static constexpr size_t kWordsInVector = 2;
    PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % kAlignmentRequirement));
    PA_DCHECK(
        !((reinterpret_cast<char*>(end) - reinterpret_cast<char*>(begin)) %
          kAlignmentRequirement));
    // For SSE3, since some integer instructions are not yet available (e.g.
    // _mm_cmpeq_epi64), use packed doubles (not integers). Sticking to doubles
    // helps to avoid latency caused by "domain crossing penalties" (see bypass
    // delays in https://agner.org/optimize/microarchitecture.pdf).
    const __m128d vbase =
        _mm_castsi128_pd(_mm_set1_epi64x(normal_bucket_pool_base_));
    const __m128d cage_mask = _mm_castsi128_pd(
        _mm_set1_epi64x(PartitionAddressSpace::NormalBucketPoolBaseMask()));

    size_t quarantine_size = 0;
    for (uintptr_t* payload = begin; payload < end; payload += kWordsInVector) {
      const __m128d maybe_ptrs =
          _mm_load_pd(reinterpret_cast<double*>(payload));
      const __m128d vand = _mm_and_pd(maybe_ptrs, cage_mask);
      const __m128d vcmp = _mm_cmpeq_pd(vand, vbase);
      const int mask = _mm_movemask_pd(vcmp);
      if (LIKELY(!mask))
        continue;
      // It's important to extract pointers from the already loaded vector to
      // avoid racing with the mutator.
      if (mask & 0b01) {
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBucketPool<BitmapLookupPolicy>(
                _mm_cvtsi128_si64(_mm_castpd_si128(maybe_ptrs)));
      }
      if (mask & 0b10) {
        // Extraction intrinsics for qwords are only supported in SSE4.1, so
        // instead we reshuffle dwords with pshufd. The mask is used to move the
        // 4th and 3rd dwords into the second and first position.
        static constexpr int kSecondWordMask = (3 << 2) | (2 << 0);
        const __m128i shuffled =
            _mm_shuffle_epi32(_mm_castpd_si128(maybe_ptrs), kSecondWordMask);
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBucketPool<BitmapLookupPolicy>(
                _mm_cvtsi128_si64(shuffled));
      }
    }
    return quarantine_size;
  }

  __attribute__((target("avx2"))) NO_SANITIZE("thread") size_t
      RunAVX2(uintptr_t* begin, uintptr_t* end) const {
    static constexpr size_t kAlignmentRequirement = 32;
    static constexpr size_t kWordsInVector = 4;
    PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % kAlignmentRequirement));
    // For AVX2, stick to integer instructions. This brings slightly better
    // throughput. For example, according to the Intel docs, on Broadwell and
    // Haswell the CPI of vmovdqa (_mm256_load_si256) is twice smaller (0.25)
    // than that of vmovapd (_mm256_load_pd).
    const __m256i vbase = _mm256_set1_epi64x(normal_bucket_pool_base_);
    const __m256i cage_mask =
        _mm256_set1_epi64x(PartitionAddressSpace::NormalBucketPoolBaseMask());

    size_t quarantine_size = 0;
    uintptr_t* payload = begin;
    for (; payload < (end - kWordsInVector); payload += kWordsInVector) {
      const __m256i maybe_ptrs =
          _mm256_load_si256(reinterpret_cast<__m256i*>(payload));
      const __m256i vand = _mm256_and_si256(maybe_ptrs, cage_mask);
      const __m256i vcmp = _mm256_cmpeq_epi64(vand, vbase);
      const int mask = _mm256_movemask_pd(_mm256_castsi256_pd(vcmp));
      if (LIKELY(!mask))
        continue;
      // It's important to extract pointers from the already loaded vector to
      // avoid racing with the mutator.
      if (mask & 0b0001)
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBucketPool<BitmapLookupPolicy>(
                _mm256_extract_epi64(maybe_ptrs, 0));
      if (mask & 0b0010)
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBucketPool<BitmapLookupPolicy>(
                _mm256_extract_epi64(maybe_ptrs, 1));
      if (mask & 0b0100)
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBucketPool<BitmapLookupPolicy>(
                _mm256_extract_epi64(maybe_ptrs, 2));
      if (mask & 0b1000)
        quarantine_size +=
            pcscan_task_.TryMarkObjectInNormalBucketPool<BitmapLookupPolicy>(
                _mm256_extract_epi64(maybe_ptrs, 3));
    }

    quarantine_size += RunUnvectorized(payload, end);
    return quarantine_size;
  }
#endif  // defined(ARCH_CPU_X86_64)

  ALWAYS_INLINE NO_SANITIZE("thread") size_t
      RunUnvectorized(uintptr_t* begin, uintptr_t* end) const {
    PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % sizeof(uintptr_t)));
    size_t quarantine_size = 0;
    for (; begin < end; ++begin) {
      uintptr_t maybe_ptr = *begin;
#if defined(PA_HAS_64_BITS_POINTERS)
      // On 64bit architectures, call IsInNormalBucketPool instead of
      // IsManagedByPartitionAllocNormalBuckets to avoid redundant load of
      // PartitionAddressSpace::normal_bucket_pool_base_address_.
      if (LIKELY(!IsInNormalBucketPool(maybe_ptr)))
#else
      if (LIKELY(!IsManagedByPartitionAllocNormalBuckets(
              reinterpret_cast<void*>(maybe_ptr))))
#endif
        continue;
      quarantine_size +=
          pcscan_task_.TryMarkObjectInNormalBucketPool<BitmapLookupPolicy>(
              maybe_ptr);
    }
    return quarantine_size;
  }

  ALWAYS_INLINE NO_SANITIZE("thread") size_t
      RunUnvectorizedNoGigaCage(uintptr_t* begin, uintptr_t* end) const {
    PA_DCHECK(!(reinterpret_cast<uintptr_t>(begin) % sizeof(uintptr_t)));
    size_t quarantine_size = 0;
    for (; begin < end; ++begin) {
      uintptr_t maybe_ptr = *begin;
      if (!maybe_ptr)
        continue;
      quarantine_size +=
          pcscan_task_.TryMarkObjectInNormalBucketPool<BinaryLookupPolicy>(
              maybe_ptr);
    }
    return quarantine_size;
  }

  // Keep this constant so that the compiler can remove redundant loads for
  // the base of the normal bucket pool and hoist them out of the loops.
  const ScanFunction scan_function_;
  const PCScanTask& pcscan_task_;
#if defined(PA_HAS_64_BITS_POINTERS)
  const uintptr_t normal_bucket_pool_base_;
#endif
};

template <bool thread_safe>
size_t PCScan<thread_safe>::PCScanTask::ScanPartitions() {
  PCSCAN_EVENT(scopes::kScan);

  const ScanLoop scan_loop(*this);

  size_t new_quarantine_size = 0;

  // For scanning large areas, it's worthwhile checking whether the range that
  // is scanned contains quarantined objects.
  for (auto scan_area : large_scan_areas_) {
    // The bitmap is (a) always guaranteed to exist and (b) the same for all
    // objects in a given slot span.
    // TODO(chromium:1129751): Check mutator bitmap as well if performance
    // allows.
    auto* bitmap = QuarantineBitmapFromPointer(
        QuarantineBitmapType::kScanner, pcscan_.quarantine_data_.epoch(),
        reinterpret_cast<char*>(scan_area.begin));
    for (uintptr_t* current_slot = scan_area.begin;
         current_slot < scan_area.end;
         current_slot += (scan_area.slot_size / sizeof(uintptr_t))) {
      // It is okay to skip objects as their payload has been zapped at this
      // point which means that the pointers no longer retain other objects.
      if (bitmap->CheckBit(reinterpret_cast<uintptr_t>(current_slot))) {
        continue;
      }
      uintptr_t* current_slot_end =
          current_slot + (scan_area.slot_size / sizeof(uintptr_t));
      PA_DCHECK(current_slot_end <= scan_area.end);
      new_quarantine_size += scan_loop.Run(current_slot, current_slot_end);
    }
  }
  for (auto scan_area : scan_areas_) {
    new_quarantine_size += scan_loop.Run(scan_area.begin, scan_area.end);
  }
  return new_quarantine_size;
}

template <bool thread_safe>
size_t PCScan<thread_safe>::PCScanTask::SweepQuarantine() {
  using AccessType = QuarantineBitmap::AccessType;

  PCSCAN_EVENT(scopes::kSweep);
  size_t swept_bytes = 0;

  for (auto super_page : super_pages_) {
    auto* bitmap = QuarantineBitmapFromPointer(
        QuarantineBitmapType::kScanner, pcscan_.quarantine_data_.epoch(),
        reinterpret_cast<char*>(super_page));
    auto* root = Root::FromSuperPage(reinterpret_cast<char*>(super_page));
    bitmap->template IterateAndClear<AccessType::kNonAtomic>(
        [root, &swept_bytes](uintptr_t ptr) {
          auto* object = reinterpret_cast<void*>(ptr);
          auto* slot_span = SlotSpan::FromSlotInnerPtr(object);
          swept_bytes += slot_span->bucket->slot_size;
          root->FreeNoHooksImmediate(object, slot_span);
        });
  }

  return swept_bytes;
}

template <bool thread_safe>
PCScan<thread_safe>::PCScanTask::PCScanTask(PCScan& pcscan) : pcscan_(pcscan) {
  // Threshold for which bucket size it is worthwhile in checking whether the
  // object is a quarantined object and can be skipped.
  static constexpr size_t kLargeScanAreaThreshold = 8192;
  // Take a snapshot of all allocated non-empty slot spans.
  static constexpr size_t kScanAreasReservationSize = 128;
  scan_areas_.reserve(kScanAreasReservationSize);

  for (Root* root : pcscan.scannable_roots_) {
    typename Root::ScopedGuard guard(root->lock_);

    // Take a snapshot of all super pages and scannable slot spans.
    // TODO(bikineev): Consider making current_extent lock-free and moving it to
    // the concurrent thread.
    for (auto* super_page_extent = root->first_extent; super_page_extent;
         super_page_extent = super_page_extent->next) {
      for (char* super_page = super_page_extent->super_page_base;
           super_page != super_page_extent->super_pages_end;
           super_page += kSuperPageSize) {
        // TODO(bikineev): Consider following freelists instead of slot spans.
        const size_t visited_slot_spans =
            IterateActiveAndFullSlotSpans<thread_safe>(
                super_page, true /*with_quarantine*/,
                [this](SlotSpan* slot_span) {
                  auto* payload_begin = static_cast<uintptr_t*>(
                      SlotSpan::ToSlotSpanStartPtr(slot_span));
                  size_t provisioned_size = slot_span->GetProvisionedSize();
                  // Free & decommitted slot spans are skipped.
                  PA_DCHECK(provisioned_size > 0);
                  auto* payload_end =
                      payload_begin + (provisioned_size / sizeof(uintptr_t));
                  if (slot_span->bucket->slot_size >= kLargeScanAreaThreshold) {
                    large_scan_areas_.push_back({payload_begin, payload_end,
                                                 slot_span->bucket->slot_size});
                  } else {
                    scan_areas_.push_back({payload_begin, payload_end});
                  }
                });
        // If we haven't visited any slot spans, all the slot spans in the
        // super-page are either empty or decommitted. This means that all the
        // objects are freed and there are no quarantined objects.
        if (LIKELY(visited_slot_spans)) {
          super_pages_.insert(reinterpret_cast<uintptr_t>(super_page));
        } else {
#if DCHECK_IS_ON()
          PA_CHECK(IsScannerQuarantineBitmapEmpty(
              super_page, pcscan_.quarantine_data_.epoch()));
#endif
        }
      }
    }
  }
  for (Root* root : pcscan.nonscannable_roots_) {
    typename Root::ScopedGuard guard(root->lock_);
    // Take a snapshot of all super pages and scannable slot spans.
    for (auto* super_page_extent = root->first_extent; super_page_extent;
         super_page_extent = super_page_extent->next) {
      for (char* super_page = super_page_extent->super_page_base;
           super_page != super_page_extent->super_pages_end;
           super_page += kSuperPageSize) {
        super_pages_.insert(reinterpret_cast<uintptr_t>(super_page));
      }
    }
  }
}

template <bool thread_safe>
void PCScan<thread_safe>::PCScanTask::RunOnce() && {
  PCSCAN_EVENT(scopes::kPCScan);

  const bool is_with_gigacage = features::IsPartitionAllocGigaCageEnabled();
  if (is_with_gigacage) {
    // Prepare super page bitmap for fast scanning.
    super_pages_bitmap_.Populate(super_pages_);
  }

  // Clear all quarantined objects.
  ClearQuarantinedObjects();

  // Mark and sweep the quarantine list.
  const size_t new_quarantine_size = ScanPartitions();
  const size_t swept_bytes = SweepQuarantine();

  ReportStats(swept_bytes, pcscan_.quarantine_data_.last_size(),
              new_quarantine_size);

  const size_t total_pa_heap_size = pcscan_.CalculateTotalHeapSize();

  pcscan_.quarantine_data_.Account(new_quarantine_size);
  pcscan_.quarantine_data_.GrowLimitIfNeeded(total_pa_heap_size);

  // Check that concurrent task can't be scheduled twice.
  PA_CHECK(pcscan_.in_progress_.exchange(false, std::memory_order_acq_rel));
}

template <bool thread_safe>
class PCScan<thread_safe>::PCScanThread final {
 public:
  using TaskHandle = std::unique_ptr<PCScanTask>;

  static PCScanThread& Instance() {
    // Lazily instantiate the scanning thread.
    static base::NoDestructor<PCScanThread> instance;
    return *instance;
  }

  void PostTask(TaskHandle task) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      PA_DCHECK(!posted_task_.get());
      posted_task_ = std::move(task);
    }
    condvar_.notify_all();
  }

 private:
  friend class base::NoDestructor<PCScanThread>;

  PCScanThread() {
    std::thread{[this] {
      static constexpr const char* kThreadName = "PCScan";
      // Ideally we should avoid mixing base:: and std:: API for threading, but
      // this is useful for visualizing the pcscan thread in chrome://tracing.
      base::PlatformThread::SetName(kThreadName);
      TaskLoop();
    }}.detach();
  }

  void TaskLoop() {
    while (true) {
      TaskHandle current_task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        condvar_.wait(lock, [this] { return posted_task_.get(); });
        std::swap(current_task, posted_task_);
      }
      std::move(*current_task).RunOnce();
    }
  }

  std::mutex mutex_;
  std::condition_variable condvar_;
  TaskHandle posted_task_;
};

template <bool thread_safe>
constexpr size_t PCScan<thread_safe>::QuarantineData::kQuarantineSizeMinLimit;

template <bool thread_safe>
void PCScan<thread_safe>::QuarantineData::ResetAndAdvanceEpoch() {
  last_size_ = current_size_.exchange(0, std::memory_order_relaxed);
  epoch_.fetch_add(1, std::memory_order_relaxed);
}

template <bool thread_safe>
void PCScan<thread_safe>::QuarantineData::GrowLimitIfNeeded(size_t heap_size) {
  static constexpr double kQuarantineSizeFraction = 0.1;
  // |heap_size| includes the current quarantine size, we intentionally leave
  // some slack till hitting the limit.
  size_limit_.store(
      std::max(kQuarantineSizeMinLimit,
               static_cast<size_t>(kQuarantineSizeFraction * heap_size)),
      std::memory_order_relaxed);
}

template <bool thread_safe>
void PCScan<thread_safe>::Roots::Add(Root* root) {
  PA_CHECK(std::find(begin(), end(), root) == end());
  (*this)[current_] = root;
  ++current_;
  PA_CHECK(current_ != kMaxNumberOfPartitions)
      << "Exceeded number of allowed partitions";
}

template <bool thread_safe>
void PCScan<thread_safe>::Roots::ClearForTesting() {
  std::fill(begin(), end(), nullptr);
  current_ = 0;
}

template <bool thread_safe>
void PCScan<thread_safe>::PerformScan(InvocationMode invocation_mode) {
  PA_DCHECK(scannable_roots_.size() > 0);
  PA_DCHECK(std::all_of(scannable_roots_.begin(), scannable_roots_.end(),
                        [](Root* root) { return root->IsScanEnabled(); }));
  PA_DCHECK(
      std::all_of(nonscannable_roots_.begin(), nonscannable_roots_.end(),
                  [](Root* root) { return root->IsQuarantineEnabled(); }));

  if (in_progress_.exchange(true, std::memory_order_acq_rel)) {
    // Bail out if PCScan is already in progress.
    return;
  }

  quarantine_data_.ResetAndAdvanceEpoch();

  // Initialize PCScan task.
  auto task = std::make_unique<PCScanTask>(*this);

  // Post PCScan task.
  if (LIKELY(invocation_mode == InvocationMode::kNonBlocking)) {
    PCScanThread::Instance().PostTask(std::move(task));
  } else {
    PA_DCHECK(InvocationMode::kBlocking == invocation_mode ||
              InvocationMode::kForcedBlocking == invocation_mode);
    std::move(*task).RunOnce();
  }
}

template <bool thread_safe>
void PCScan<thread_safe>::PerformScanIfNeeded(InvocationMode invocation_mode) {
  if (!scannable_roots_.size())
    return;
  if (invocation_mode == InvocationMode::kForcedBlocking ||
      quarantine_data_.MinimumScanningThresholdReached())
    PerformScan(invocation_mode);
}

template <bool thread_safe>
size_t PCScan<thread_safe>::CalculateTotalHeapSize() const {
  const auto acc = [](size_t size, Root* root) {
    return size + root->get_total_size_of_committed_pages();
  };
  return std::accumulate(scannable_roots_.begin(), scannable_roots_.end(), 0u,
                         acc) +
         std::accumulate(nonscannable_roots_.begin(), nonscannable_roots_.end(),
                         0u, acc);
}

namespace {
template <bool thread_safe>
void CommitQuarantineBitmaps(PartitionRoot<thread_safe>& root) {
  size_t quarantine_bitmaps_size_to_commit = CommittedQuarantineBitmapsSize();
  for (auto* super_page_extent = root.first_extent; super_page_extent;
       super_page_extent = super_page_extent->next) {
    for (char* super_page = super_page_extent->super_page_base;
         super_page != super_page_extent->super_pages_end;
         super_page += kSuperPageSize) {
      RecommitSystemPages(internal::SuperPageQuarantineBitmaps(super_page),
                          quarantine_bitmaps_size_to_commit, PageReadWrite,
                          PageUpdatePermissions);
    }
  }
}
}  // namespace

template <bool thread_safe>
void PCScan<thread_safe>::RegisterScannableRoot(Root* root) {
  PA_DCHECK(root);
  PA_CHECK(root->IsQuarantineAllowed());
  typename Root::ScopedGuard guard(root->lock_);
  if (root->IsScanEnabled())
    return;
  PA_CHECK(!root->IsQuarantineEnabled());
  CommitQuarantineBitmaps(*root);
  root->scan_mode = Root::ScanMode::kEnabled;
  root->quarantine_mode = Root::QuarantineMode::kEnabled;
  scannable_roots_.Add(root);
}

template <bool thread_safe>
void PCScan<thread_safe>::RegisterNonScannableRoot(Root* root) {
  PA_DCHECK(root);
  PA_CHECK(root->IsQuarantineAllowed());
  typename Root::ScopedGuard guard(root->lock_);
  if (root->IsQuarantineEnabled())
    return;
  CommitQuarantineBitmaps(*root);
  root->quarantine_mode = Root::QuarantineMode::kEnabled;
  nonscannable_roots_.Add(root);
}

template <bool thread_safe>
void PCScan<thread_safe>::ClearRootsForTesting() {
  scannable_roots_.ClearForTesting();     // IN-TEST
  nonscannable_roots_.ClearForTesting();  // IN-TEST
}

template <bool thread_safe>
PCScan<thread_safe> PCScan<thread_safe>::instance_ PA_CONSTINIT;

template class PCScan<ThreadSafe>;
template class PCScan<NotThreadSafe>;

}  // namespace internal
}  // namespace base
