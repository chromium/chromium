// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

#include "base/allocator/partition_allocator/starscan/pcscan.h"

#include "base/allocator/partition_allocator/partition_alloc-inl.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/cpu.h"
#include "base/allocator/partition_allocator/partition_alloc_base/logging.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_for_testing.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/starscan/stack/stack.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_STARSCAN)

namespace partition_alloc::internal {

namespace {

struct DisableStackScanningScope final {
  DisableStackScanningScope() {
    if (PCScan::IsStackScanningEnabled()) {
      PCScan::DisableStackScanning();
      changed_ = true;
    }
  }
  ~DisableStackScanningScope() {
    if (changed_) {
      PCScan::EnableStackScanning();
    }
  }

 private:
  bool changed_ = false;
};

}  // namespace

class PartitionAllocPCScanTestBase : public testing::Test {
 public:
  PartitionAllocPCScanTestBase()
      : allocator_(PartitionOptions{
            .aligned_alloc = PartitionOptions::kAllowed,
            .star_scan_quarantine = PartitionOptions::kAllowed,
            .memory_tagging = {
                .enabled =
                    base::CPU::GetInstanceNoAllocation().has_mte()
                        ? partition_alloc::PartitionOptions::kEnabled
                        : partition_alloc::PartitionOptions::kDisabled}}) {
    PartitionAllocGlobalInit([](size_t) { PA_LOG(FATAL) << "Out of memory"; });
    // Previous test runs within the same process decommit pools, therefore
    // we need to make sure that the card table is recommitted for each run.
    PCScan::ReinitForTesting(
        {PCScan::InitConfig::WantedWriteProtectionMode::kDisabled,
         PCScan::InitConfig::SafepointMode::kEnabled});
    allocator_.root()->UncapEmptySlotSpanMemoryForTesting();
    allocator_.root()->SwitchToDenserBucketDistribution();

    PCScan::RegisterScannableRoot(allocator_.root());
  }

  ~PartitionAllocPCScanTestBase() override {
    allocator_.root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans |
                                   PurgeFlags::kDiscardUnusedSystemPages);
    PartitionAllocGlobalUninitForTesting();
  }

  void RunPCScan() {
    PCScan::Instance().PerformScan(PCScan::InvocationMode::kBlocking);
  }

  void SchedulePCScan() {
    PCScan::Instance().PerformScan(
        PCScan::InvocationMode::kScheduleOnlyForTesting);
  }

  void JoinPCScanAsMutator() {
    auto& instance = PCScan::Instance();
    PA_CHECK(instance.IsJoinable());
    instance.JoinScan();
  }

  void FinishPCScanAsScanner() { PCScan::FinishScanForTesting(); }

  bool IsInQuarantine(void* object) const {
    uintptr_t slot_start = root().ObjectToSlotStart(object);
    return StateBitmapFromAddr(slot_start)->IsQuarantined(slot_start);
  }

  PartitionRoot& root() { return *allocator_.root(); }
  const PartitionRoot& root() const { return *allocator_.root(); }

 private:
  // Leverage the already-templated version outside `internal::`.
  partition_alloc::PartitionAllocatorAllowLeaksForTesting allocator_;
};

namespace {

// The test that expects free() being quarantined only when tag overflow occurs.
using PartitionAllocPCScanWithMTETest = PartitionAllocPCScanTestBase;

// The test that expects every free() being quarantined.
class PartitionAllocPCScanTest : public PartitionAllocPCScanTestBase {
 public:
  PartitionAllocPCScanTest() { root().SetQuarantineAlwaysForTesting(true); }
  ~PartitionAllocPCScanTest() override {
    root().SetQuarantineAlwaysForTesting(false);
  }
};

using SlotSpan = PartitionRoot::SlotSpan;

struct FullSlotSpanAllocation {
  SlotSpan* slot_span;
  void* first;
  void* last;
};

// Assumes heap is purged.
FullSlotSpanAllocation GetFullSlotSpan(PartitionRoot& root,
                                       size_t object_size) {
  PA_CHECK(0u == root.get_total_size_of_committed_pages());

  const size_t raw_size = root.AdjustSizeForExtrasAdd(object_size);
  const size_t bucket_index =
      root.SizeToBucketIndex(raw_size, root.GetBucketDistribution());
  PartitionRoot::Bucket& bucket = root.buckets[bucket_index];
  const size_t num_slots = (bucket.get_bytes_per_span()) / bucket.slot_size;

  uintptr_t first = 0;
  uintptr_t last = 0;
  for (size_t i = 0; i < num_slots; ++i) {
    void* ptr = root.Alloc<partition_alloc::AllocFlags::kNoHooks>(object_size);
    EXPECT_TRUE(ptr);
    if (i == 0) {
      first = root.ObjectToSlotStart(ptr);
    } else if (i == num_slots - 1) {
      last = root.ObjectToSlotStart(ptr);
    }
  }

  EXPECT_EQ(SlotSpan::FromSlotStart(first), SlotSpan::FromSlotStart(last));
  if (bucket.num_system_pages_per_slot_span ==
      NumSystemPagesPerPartitionPage()) {
    // Pointers are expected to be in the same partition page, but have a
    // different MTE-tag.
    EXPECT_EQ(UntagAddr(first & PartitionPageBaseMask()),
              UntagAddr(last & PartitionPageBaseMask()));
  }
  EXPECT_EQ(num_slots, bucket.active_slot_spans_head->num_allocated_slots);
  EXPECT_EQ(nullptr, bucket.active_slot_spans_head->get_freelist_head());
  EXPECT_TRUE(bucket.is_valid());
  EXPECT_TRUE(bucket.active_slot_spans_head !=
              SlotSpan::get_sentinel_slot_span());

  return {bucket.active_slot_spans_head, root.SlotStartToObject(first),
          root.SlotStartToObject(last)};
}

bool IsInFreeList(uintptr_t slot_start) {
  // slot_start isn't MTE-tagged, whereas pointers in the freelist are.
  void* slot_start_tagged = SlotStartAddr2Ptr(slot_start);
  auto* slot_span = SlotSpan::FromSlotStart(slot_start);
  for (auto* entry = slot_span->get_freelist_head(); entry;
       entry = entry->GetNext(slot_span->bucket->slot_size)) {
    if (entry == slot_start_tagged) {
      return true;
    }
  }
  return false;
}

struct ListBase {
  // Volatile to prevent the compiler from doing dead store elimination.
  ListBase* volatile next = nullptr;
};

template <size_t Size, size_t Alignment = 0>
struct List final : ListBase {
  char buffer[Size];

  static List* Create(PartitionRoot& root, ListBase* next = nullptr) {
    List* list;
    if (Alignment) {
      list = static_cast<List*>(root.AlignedAlloc(Alignment, sizeof(List)));
    } else {
      list = static_cast<List*>(root.Alloc(sizeof(List), nullptr));
    }
    list->next = next;
    return list;
  }

  static void Destroy(PartitionRoot& root, List* list) { root.Free(list); }
};

TEST_F(PartitionAllocPCScanTest, ArbitraryObjectInQuarantine) {
  using ListType = List<8>;

  auto* obj1 = ListType::Create(root());
  auto* obj2 = ListType::Create(root());
  EXPECT_FALSE(IsInQuarantine(obj1));
  EXPECT_FALSE(IsInQuarantine(obj2));

  ListType::Destroy(root(), obj2);
  EXPECT_FALSE(IsInQuarantine(obj1));
  EXPECT_TRUE(IsInQuarantine(obj2));
}

TEST_F(PartitionAllocPCScanTest, FirstObjectInQuarantine) {
  static constexpr size_t kAllocationSize = 16;

  FullSlotSpanAllocation full_slot_span =
      GetFullSlotSpan(root(), kAllocationSize);
  EXPECT_FALSE(IsInQuarantine(full_slot_span.first));

  root().Free<FreeFlags::kNoHooks>(full_slot_span.first);
  EXPECT_TRUE(IsInQuarantine(full_slot_span.first));
}

TEST_F(PartitionAllocPCScanTest, LastObjectInQuarantine) {
  static constexpr size_t kAllocationSize = 16;

  FullSlotSpanAllocation full_slot_span =
      GetFullSlotSpan(root(), kAllocationSize);
  EXPECT_FALSE(IsInQuarantine(full_slot_span.last));

  root().Free<FreeFlags::kNoHooks>(full_slot_span.last);
  EXPECT_TRUE(IsInQuarantine(full_slot_span.last));
}

template <typename SourceList, typename ValueList>
void TestDanglingReference(PartitionAllocPCScanTest& test,
                           SourceList* source,
                           ValueList* value,
                           PartitionRoot& value_root) {
  {
    // Free |value| and leave the dangling reference in |source|.
    ValueList::Destroy(value_root, value);
    // Check that |value| is in the quarantine now.
    EXPECT_TRUE(test.IsInQuarantine(value));
    // Run PCScan.
    test.RunPCScan();
    // Check that the object is still quarantined since it's referenced by
    // |source|.
    EXPECT_TRUE(test.IsInQuarantine(value));
  }
  {
    // Get rid of the dangling reference.
    source->next = nullptr;
    // Run PCScan again.
    test.RunPCScan();
    // Check that the object is no longer in the quarantine.
    EXPECT_FALSE(test.IsInQuarantine(value));
    // Check that the object is in the freelist now.
    EXPECT_TRUE(IsInFreeList(value_root.ObjectToSlotStart(value)));
  }
}

void TestDanglingReferenceNotVisited(PartitionAllocPCScanTest& test,
                                     void* value,
                                     PartitionRoot& value_root) {
  value_root.Free(value);
  // Check that |value| is in the quarantine now.
  EXPECT_TRUE(test.IsInQuarantine(value));
  // Run PCScan.
  test.RunPCScan();
  // Check that the object is no longer in the quarantine since the pointer to
  // it was not scanned from the non-scannable partition.
  EXPECT_FALSE(test.IsInQuarantine(value));
  // Check that the object is in the freelist now.
  EXPECT_TRUE(IsInFreeList(value_root.ObjectToSlotStart(value)));
}

TEST_F(PartitionAllocPCScanTest, DanglingReferenceSameBucket) {
  using SourceList = List<8>;
  using ValueList = SourceList;

  // Create two objects, where |source| references |value|.
  auto* value = ValueList::Create(root(), nullptr);
  auto* source = SourceList::Create(root(), value);

  TestDanglingReference(*this, source, value, root());
}

TEST_F(PartitionAllocPCScanTest, DanglingReferenceDifferentBuckets) {
  using SourceList = List<8>;
  using ValueList = List<128>;

  // Create two objects, where |source| references |value|.
  auto* value = ValueList::Create(root(), nullptr);
  auto* source = SourceList::Create(root(), value);

  TestDanglingReference(*this, source, value, root());
}

TEST_F(PartitionAllocPCScanTest, DanglingReferenceDifferentBucketsAligned) {
  // Choose a high alignment that almost certainly will cause a gap between slot
  // spans. But make it less than kMaxSupportedAlignment, or else two
  // allocations will end up on different super pages.
  constexpr size_t alignment = kMaxSupportedAlignment / 2;
  using SourceList = List<8, alignment>;
  using ValueList = List<128, alignment>;

  // Create two objects, where |source| references |value|.
  auto* value = ValueList::Create(root(), nullptr);
  auto* source = SourceList::Create(root(), value);

  // Double check the setup -- make sure that exactly two slot spans were
  // allocated, within the same super page, with a gap in between.
  {
    ::partition_alloc::internal::ScopedGuard guard{root().lock_};

    uintptr_t value_slot_start = root().ObjectToSlotStart(value);
    uintptr_t source_slot_start = root().ObjectToSlotStart(source);
    auto super_page = value_slot_start & kSuperPageBaseMask;
    ASSERT_EQ(super_page, source_slot_start & kSuperPageBaseMask);
    size_t i = 0;
    uintptr_t first_slot_span_end = 0;
    uintptr_t second_slot_span_start = 0;
    IterateSlotSpans(
        super_page, true, [&](SlotSpan* slot_span) -> bool {
          if (i == 0) {
            first_slot_span_end = SlotSpan::ToSlotSpanStart(slot_span) +
                                  slot_span->bucket->get_pages_per_slot_span() *
                                      PartitionPageSize();
          } else {
            second_slot_span_start = SlotSpan::ToSlotSpanStart(slot_span);
          }
          ++i;
          return false;
        });
    ASSERT_EQ(i, 2u);
    ASSERT_GT(second_slot_span_start, first_slot_span_end);
  }

  TestDanglingReference(*this, source, value, root());
}

TEST_F(PartitionAllocPCScanTest,
       DanglingReferenceSameSlotSpanButDifferentPages) {
  using SourceList = List<8>;
  using ValueList = SourceList;

  static const size_t kObjectSizeForSlotSpanConsistingOfMultiplePartitionPages =
      static_cast<size_t>(PartitionPageSize() * 0.75);

  FullSlotSpanAllocation full_slot_span = GetFullSlotSpan(
      root(), root().AdjustSizeForExtrasSubtract(
                  kObjectSizeForSlotSpanConsistingOfMultiplePartitionPages));

  // Assert that the first and the last objects are in the same slot span but on
  // different partition pages.
  // Converting to slot start also takes care of the MTE-tag difference.
  ASSERT_EQ(SlotSpan::FromObject(full_slot_span.first),
            SlotSpan::FromObject(full_slot_span.last));
  uintptr_t first_slot_start = root().ObjectToSlotStart(full_slot_span.first);
  uintptr_t last_slot_start = root().ObjectToSlotStart(full_slot_span.last);
  ASSERT_NE(first_slot_start & PartitionPageBaseMask(),
            last_slot_start & PartitionPageBaseMask());

  // Create two objects, on different partition pages.
  auto* value = new (full_slot_span.first) ValueList;
  auto* source = new (full_slot_span.last) SourceList;
  source->next = value;

  TestDanglingReference(*this, source, value, root());
}

TEST_F(PartitionAllocPCScanTest, DanglingReferenceFromFullPage) {
  using SourceList = List<64>;
  using ValueList = SourceList;

  FullSlotSpanAllocation full_slot_span =
      GetFullSlotSpan(root(), sizeof(SourceList));
  void* source_buffer = full_slot_span.first;
  // This allocation must go through the slow path and call SetNewActivePage(),
  // which will flush the full page from the active page list.
  void* value_buffer =
      root().Alloc<partition_alloc::AllocFlags::kNoHooks>(sizeof(ValueList));

  // Assert that the first and the last objects are in different slot spans but
  // in the same bucket.
  SlotSpan* source_slot_span =
      PartitionRoot::SlotSpan::FromObject(source_buffer);
  SlotSpan* value_slot_span = PartitionRoot::SlotSpan::FromObject(value_buffer);
  ASSERT_NE(source_slot_span, value_slot_span);
  ASSERT_EQ(source_slot_span->bucket, value_slot_span->bucket);

  // Create two objects, where |source| is in a full detached page.
  auto* value = new (value_buffer) ValueList;
  auto* source = new (source_buffer) SourceList;
  source->next = value;

  TestDanglingReference(*this, source, value, root());
}

template <size_t Size>
struct ListWithInnerReference {
  char buffer1[Size];
  // Volatile to prevent the compiler from doing dead store elimination.
  char* volatile next = nullptr;
  char buffer2[Size];

  static ListWithInnerReference* Create(PartitionRoot& root) {
    auto* list = static_cast<ListWithInnerReference*>(
        root.Alloc(sizeof(ListWithInnerReference), nullptr));
    return list;
  }

  static void Destroy(PartitionRoot& root, ListWithInnerReference* list) {
    root.Free(list);
  }
};

// Disabled due to consistent failure http://crbug.com/1242407
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DanglingInnerReference DISABLED_DanglingInnerReference
#else
#define MAYBE_DanglingInnerReference DanglingInnerReference
#endif
TEST_F(PartitionAllocPCScanTest, MAYBE_DanglingInnerReference) {
  using SourceList = ListWithInnerReference<64>;
  using ValueList = SourceList;

  auto* source = SourceList::Create(root());
  auto* value = ValueList::Create(root());
  source->next = value->buffer2;

  TestDanglingReference(*this, source, value, root());
}

TEST_F(PartitionAllocPCScanTest, DanglingReferenceFromSingleSlotSlotSpan) {
  using SourceList = List<kMaxBucketed - 4096>;
  using ValueList = SourceList;

  auto* source = SourceList::Create(root());
  auto* slot_span = SlotSpanMetadata::FromObject(source);
  ASSERT_TRUE(slot_span->CanStoreRawSize());

  auto* value = ValueList::Create(root());
  source->next = value;

  TestDanglingReference(*this, source, value, root());
}

TEST_F(PartitionAllocPCScanTest, DanglingInterPartitionReference) {
  using SourceList = List<64>;
  using ValueList = SourceList;

  PartitionRoot source_root(PartitionOptions{
      .star_scan_quarantine = PartitionOptions::kAllowed,
  });
  source_root.UncapEmptySlotSpanMemoryForTesting();
  PartitionRoot value_root(PartitionOptions{
      .star_scan_quarantine = PartitionOptions::kAllowed,
  });
  value_root.UncapEmptySlotSpanMemoryForTesting();

  PCScan::RegisterScannableRoot(&source_root);
  source_root.SetQuarantineAlwaysForTesting(true);
  PCScan::RegisterScannableRoot(&value_root);
  value_root.SetQuarantineAlwaysForTesting(true);

  auto* source = SourceList::Create(source_root);
  auto* value = ValueList::Create(value_root);
  source->next = value;

  TestDanglingReference(*this, source, value, value_root);
}

TEST_F(PartitionAllocPCScanTest, DanglingReferenceToNonScannablePartition) {
  using SourceList = List<64>;
  using ValueList = SourceList;

  PartitionRoot source_root(PartitionOptions{
      .star_scan_quarantine = PartitionOptions::kAllowed,
  });
  source_root.UncapEmptySlotSpanMemoryForTesting();
  PartitionRoot value_root(PartitionOptions{
      .star_scan_quarantine = PartitionOptions::kAllowed,
  });
  value_root.UncapEmptySlotSpanMemoryForTesting();

  PCScan::RegisterScannableRoot(&source_root);
  source_root.SetQuarantineAlwaysForTesting(true);
  PCScan::RegisterNonScannableRoot(&value_root);
  value_root.SetQuarantineAlwaysForTesting(true);

  auto* source = SourceList::Create(source_root);
  auto* value = ValueList::Create(value_root);
  source->next = value;

  TestDanglingReference(*this, source, value, value_root);
}

TEST_F(PartitionAllocPCScanTest, DanglingReferenceFromNonScannablePartition) {
  using SourceList = List<64>;
  using ValueList = SourceList;

  PartitionRoot source_root(PartitionOptions{
      .star_scan_quarantine = PartitionOptions::kAllowed,
  });
  source_root.UncapEmptySlotSpanMemoryForTesting();
  PartitionRoot value_root(PartitionOptions{
      .star_scan_quarantine = PartitionOptions::kAllowed,
  });
  value_root.UncapEmptySlotSpanMemoryForTesting();

  PCScan::RegisterNonScannableRoot(&source_root);
  value_root.SetQuarantineAlwaysForTesting(true);
  PCScan::RegisterScannableRoot(&value_root);
  source_root.SetQuarantineAlwaysForTesting(true);

  auto* source = SourceList::Create(source_root);
  auto* value = ValueList::Create(value_root);
  source->next = value;

  TestDanglingReferenceNotVisited(*this, value, value_root);
}

// Death tests misbehave on Android, http://crbug.com/643760.
#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
#if PA_CONFIG(STARSCAN_EAGER_DOUBLE_FREE_DETECTION_ENABLED)
TEST_F(PartitionAllocPCScanTest, DoubleFree) {
  auto* list = List<1>::Create(root());
  List<1>::Destroy(root(), list);
  EXPECT_DEATH(List<1>::Destroy(root(), list), "");
}
#endif  // PA_CONFIG(STARSCAN_EAGER_DOUBLE_FREE_DETECTION_ENABLED)
#endif  // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

template <typename SourceList, typename ValueList>
void TestDanglingReferenceWithSafepoint(PartitionAllocPCScanTest& test,
                                        SourceList* source,
                                        ValueList* value,
                                        PartitionRoot& value_root) {
  {
    // Free |value| and leave the dangling reference in |source|.
    ValueList::Destroy(value_root, value);
    // Check that |value| is in the quarantine now.
    EXPECT_TRUE(test.IsInQuarantine(value));
    // Schedule PCScan but don't scan.
    test.SchedulePCScan();
    // Enter safepoint and scan from mutator.
    test.JoinPCScanAsMutator();
    // Check that the object is still quarantined since it's referenced by
    // |source|.
    EXPECT_TRUE(test.IsInQuarantine(value));
    // Check that |value| is not in the freelist.
    EXPECT_FALSE(IsInFreeList(test.root().ObjectToSlotStart(value)));
    // Run sweeper.
    test.FinishPCScanAsScanner();
    // Check that |value| still exists.
    EXPECT_FALSE(IsInFreeList(test.root().ObjectToSlotStart(value)));
  }
  {
    // Get rid of the dangling reference.
    source->next = nullptr;
    // Schedule PCScan but don't scan.
    test.SchedulePCScan();
    // Enter safepoint and scan from mutator.
    test.JoinPCScanAsMutator();
    // Check that |value| is not in the freelist yet, since sweeper didn't run.
    EXPECT_FALSE(IsInFreeList(test.root().ObjectToSlotStart(value)));
    test.FinishPCScanAsScanner();
    // Check that the object is no longer in the quarantine.
    EXPECT_FALSE(test.IsInQuarantine(value));
    // Check that |value| is in the freelist now.
    EXPECT_TRUE(IsInFreeList(test.root().ObjectToSlotStart(value)));
  }
}

TEST_F(PartitionAllocPCScanTest, Safepoint) {
  using SourceList = List<64>;
  using ValueList = SourceList;

  DisableStackScanningScope no_stack_scanning;

  auto* source = SourceList::Create(root());
  auto* value = ValueList::Create(root());
  source->next = value;

  TestDanglingReferenceWithSafepoint(*this, source, value, root());
}

class PartitionAllocPCScanStackScanningTest : public PartitionAllocPCScanTest {
 protected:
  // Creates and sets a dangling reference in `dangling_reference_`.
  PA_NOINLINE void CreateDanglingReference() {
    using ValueList = List<8>;
    auto* value = ValueList::Create(root(), nullptr);
    ValueList::Destroy(root(), value);
    dangling_reference_ = value;
  }

  PA_NOINLINE void SetupAndRunTest() {
    // Register the top of the stack to be the current pointer.
    PCScan::NotifyThreadCreated(GetStackPointer());
    RunTest();
  }

  PA_NOINLINE void RunTest() {
    // This writes the pointer to the stack.
    [[maybe_unused]] auto* volatile stack_ref = dangling_reference_;
    // Call the non-inline function that would scan the stack. Don't execute
    // the rest of the actions inside the function, since otherwise it would
    // be tail-call optimized and the parent frame's stack with the dangling
    // pointer would be missed.
    ScanStack();
    // Check that the object is still quarantined since it's referenced by
    // |dangling_reference_|.
    EXPECT_TRUE(IsInQuarantine(dangling_reference_));
    // Check that value is not in the freelist.
    EXPECT_FALSE(IsInFreeList(root().ObjectToSlotStart(dangling_reference_)));
    // Run sweeper.
    FinishPCScanAsScanner();
    // Check that |dangling_reference_| still exists.
    EXPECT_FALSE(IsInFreeList(root().ObjectToSlotStart(dangling_reference_)));
  }

  PA_NOINLINE void ScanStack() {
    // Schedule PCScan but don't scan.
    SchedulePCScan();
    // Enter safepoint and scan from mutator. This will scan the stack.
    JoinPCScanAsMutator();
  }

  static void* dangling_reference_;
};

// static
void* PartitionAllocPCScanStackScanningTest::dangling_reference_ = nullptr;

// The test currently fails on some platform due to the stack dangling reference
// not being found.
TEST_F(PartitionAllocPCScanStackScanningTest, DISABLED_StackScanning) {
  PCScan::EnableStackScanning();

  // Set to nullptr if the test is retried.
  dangling_reference_ = nullptr;

  CreateDanglingReference();

  SetupAndRunTest();
}

TEST_F(PartitionAllocPCScanTest, DontScanUnusedRawSize) {
  using ValueList = List<8>;

  // Make sure to commit more memory than requested to have slack for storing
  // dangling reference outside of the raw size.
  const size_t big_size = kMaxBucketed - SystemPageSize() + 1;
  void* ptr = root().Alloc(big_size);

  uintptr_t slot_start = root().ObjectToSlotStart(ptr);
  auto* slot_span = SlotSpanMetadata::FromSlotStart(slot_start);
  ASSERT_TRUE(big_size + sizeof(void*) <=
              root().AllocationCapacityFromSlotStart(slot_start));
  ASSERT_TRUE(slot_span->CanStoreRawSize());

  auto* value = ValueList::Create(root());

  // This not only points past the object, but past all extras around it.
  // However, there should be enough space between this and the end of slot, to
  // store some data.
  uintptr_t source_end = slot_start + slot_span->GetRawSize();
  // Write the pointer.
  // Since we stripped the MTE-tag to get |slot_start|, we need to retag it.
  *static_cast<ValueList**>(TagAddr(source_end)) = value;

  TestDanglingReferenceNotVisited(*this, value, root());
}

TEST_F(PartitionAllocPCScanTest, PointersToGuardPages) {
  struct Pointers {
    void* super_page;
    void* metadata_page;
    void* guard_page1;
    void* scan_bitmap;
    void* guard_page2;
  };
  auto* const pointers = static_cast<Pointers*>(
      root().Alloc<partition_alloc::AllocFlags::kNoHooks>(sizeof(Pointers)));

  // Converting to slot start strips MTE tag.
  const uintptr_t super_page =
      root().ObjectToSlotStart(pointers) & kSuperPageBaseMask;

  // Initialize scannable pointers with addresses of guard pages and metadata.
  // None of these point to an MTE-tagged area, so no need for retagging.
  pointers->super_page = reinterpret_cast<void*>(super_page);
  pointers->metadata_page = PartitionSuperPageToMetadataArea(super_page);
  pointers->guard_page1 =
      static_cast<char*>(pointers->metadata_page) + SystemPageSize();
  pointers->scan_bitmap = SuperPageStateBitmap(super_page);
  pointers->guard_page2 = reinterpret_cast<void*>(super_page + kSuperPageSize -
                                                  PartitionPageSize());

  // Simply run PCScan and expect no crashes.
  RunPCScan();
}

TEST_F(PartitionAllocPCScanTest, TwoDanglingPointersToSameObject) {
  using SourceList = List<8>;
  using ValueList = List<128>;

  auto* value = ValueList::Create(root(), nullptr);
  // Create two source objects referring to |value|.
  SourceList::Create(root(), value);
  SourceList::Create(root(), value);

  // Destroy |value| and run PCScan.
  ValueList::Destroy(root(), value);
  RunPCScan();
  EXPECT_TRUE(IsInQuarantine(value));

  // Check that accounted size after the cycle is only sizeof ValueList.
  auto* slot_span_metadata = SlotSpan::FromObject(value);
  const auto& quarantine =
      PCScan::scheduler().scheduling_backend().GetQuarantineData();
  EXPECT_EQ(slot_span_metadata->bucket->slot_size, quarantine.current_size);
}

TEST_F(PartitionAllocPCScanTest, DanglingPointerToInaccessibleArea) {
  static const size_t kObjectSizeForSlotSpanConsistingOfMultiplePartitionPages =
      static_cast<size_t>(PartitionPageSize() * 1.25);

  FullSlotSpanAllocation full_slot_span = GetFullSlotSpan(
      root(), root().AdjustSizeForExtrasSubtract(
                  kObjectSizeForSlotSpanConsistingOfMultiplePartitionPages));

  // Assert that number of allocatable bytes for this bucket is smaller or equal
  // to all allocated partition pages.
  auto* bucket = full_slot_span.slot_span->bucket;
  ASSERT_LE(bucket->get_bytes_per_span(),
            bucket->get_pages_per_slot_span() * PartitionPageSize());

  // Let the first object point past the end of the last one + some random
  // offset.
  // It should fall within the same slot, so no need for MTE-retagging.
  static constexpr size_t kOffsetPastEnd = 7;
  *reinterpret_cast<uint8_t**>(full_slot_span.first) =
      reinterpret_cast<uint8_t*>(full_slot_span.last) +
      kObjectSizeForSlotSpanConsistingOfMultiplePartitionPages + kOffsetPastEnd;

  // Destroy the last object and put it in quarantine.
  root().Free(full_slot_span.last);
  EXPECT_TRUE(IsInQuarantine(full_slot_span.last));

  // Run PCScan. After it, the quarantined object should not be promoted.
  RunPCScan();
  EXPECT_FALSE(IsInQuarantine(full_slot_span.last));
}

TEST_F(PartitionAllocPCScanTest, DanglingPointerOutsideUsablePart) {
  using ValueList = List<kMaxBucketed - 4096>;
  using SourceList = List<64>;

  auto* value = ValueList::Create(root());
  auto* slot_span = SlotSpanMetadata::FromObject(value);
  ASSERT_TRUE(slot_span->CanStoreRawSize());

  auto* source = SourceList::Create(root());

  // Let the |source| object point to the unused area of |value| and expect
  // |value| to be nevertheless marked during scanning.
  // It should fall within the same slot, so no need for MTE-retagging.
  static constexpr size_t kOffsetPastEnd = 7;
  source->next = reinterpret_cast<ListBase*>(
      reinterpret_cast<uint8_t*>(value + 1) + kOffsetPastEnd);

  TestDanglingReference(*this, source, value, root());
}

#if PA_CONFIG(HAS_MEMORY_TAGGING)
TEST_F(PartitionAllocPCScanWithMTETest, QuarantineOnlyOnTagOverflow) {
  using ListType = List<64>;

  if (!base::CPU::GetInstanceNoAllocation().has_mte()) {
    return;
  }

  {
    auto* obj1 = ListType::Create(root());
    ListType::Destroy(root(), obj1);
    auto* obj2 = ListType::Create(root());
    // The test relies on unrandomized freelist! If the slot was not moved to
    // quarantine, assert that the obj2 is the same as obj1 and the tags are
    // different.
    // MTE-retag |obj1|, as the tag changed when freeing it.
    if (!HasOverflowTag(TagPtr(obj1))) {
      // Assert that the pointer is the same.
      ASSERT_EQ(UntagPtr(obj1), UntagPtr(obj2));
      // Assert that the tag is different.
      ASSERT_NE(obj1, obj2);
    }
  }

  for (size_t i = 0; i < 16; ++i) {
    auto* obj = ListType::Create(root());
    ListType::Destroy(root(), obj);
    // MTE-retag |obj|, as the tag changed when freeing it.
    obj = TagPtr(obj);
    // Check if the tag overflows. If so, the object must be in quarantine.
    if (HasOverflowTag(obj)) {
      EXPECT_TRUE(IsInQuarantine(obj));
      EXPECT_FALSE(IsInFreeList(root().ObjectToSlotStart(obj)));
      return;
    } else {
      EXPECT_FALSE(IsInQuarantine(obj));
      EXPECT_TRUE(IsInFreeList(root().ObjectToSlotStart(obj)));
    }
  }

  EXPECT_FALSE(true && "Should never be reached");
}
#endif  // PA_CONFIG(HAS_MEMORY_TAGGING)

}  // namespace

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(USE_STARSCAN)
#endif  // defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
