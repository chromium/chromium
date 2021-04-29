// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_root.h"
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

#include "base/allocator/partition_allocator/starscan/pcscan.h"

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/starscan/stack/stack.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_ALLOW_PCSCAN

namespace base {
namespace internal {

namespace {
struct DisableStackScanningScope final {
  DisableStackScanningScope() {
    if (PCScan::IsStackScanningEnabled()) {
      PCScan::DisableStackScanning();
      changed_ = true;
    }
  }
  ~DisableStackScanningScope() {
    if (changed_)
      PCScan::EnableStackScanning();
  }

 private:
  bool changed_ = false;
};
}  // namespace

class PCScanTest : public testing::Test {
 public:
  PCScanTest() {
    PartitionAllocGlobalInit([](size_t) { LOG(FATAL) << "Out of memory"; });
    // Previous test runs within the same process decommit GigaCage, therefore
    // we need to make sure that the card table is recommitted for each run.
    PCScan::ReinitForTesting();
    allocator_.init({PartitionOptions::AlignedAlloc::kDisallowed,
                     PartitionOptions::ThreadCache::kDisabled,
                     PartitionOptions::Quarantine::kAllowed,
                     PartitionOptions::Cookies::kAllowed,
                     PartitionOptions::RefCount::kDisallowed});
    PCScan::RegisterScannableRoot(allocator_.root());
  }
  ~PCScanTest() override {
    allocator_.root()->PurgeMemory(PartitionPurgeDecommitEmptySlotSpans |
                                   PartitionPurgeDiscardUnusedSystemPages);
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

  bool IsInQuarantine(void* ptr) const {
    return QuarantineBitmapFromPointer(QuarantineBitmapType::kMutator,
                                       PCScan::Instance().epoch(), ptr)
        ->CheckBit(reinterpret_cast<uintptr_t>(ptr));
  }

  ThreadSafePartitionRoot& root() { return *allocator_.root(); }
  const ThreadSafePartitionRoot& root() const { return *allocator_.root(); }

 private:
  PartitionAllocator<ThreadSafe> allocator_;
};

namespace {

using SlotSpan = ThreadSafePartitionRoot::SlotSpan;

struct FullSlotSpanAllocation {
  SlotSpan* slot_span;
  void* first;
  void* last;
};

// Assumes heap is purged.
FullSlotSpanAllocation GetFullSlotSpan(ThreadSafePartitionRoot& root,
                                       size_t object_size) {
  CHECK_EQ(0u, root.get_total_size_of_committed_pages());

  const size_t raw_size = root.AdjustSizeForExtrasAdd(object_size);
  const size_t bucket_index = root.SizeToBucketIndex(raw_size);
  ThreadSafePartitionRoot::Bucket& bucket = root.buckets[bucket_index];
  const size_t num_slots = (bucket.get_bytes_per_span()) / bucket.slot_size;

  void* first = nullptr;
  void* last = nullptr;
  for (size_t i = 0; i < num_slots; ++i) {
    void* ptr = root.AllocFlagsNoHooks(0, object_size);
    EXPECT_TRUE(ptr);
    if (i == 0)
      first = root.AdjustPointerForExtrasSubtract(ptr);
    else if (i == num_slots - 1)
      last = root.AdjustPointerForExtrasSubtract(ptr);
  }

  EXPECT_EQ(SlotSpan::FromSlotStartPtr(first),
            SlotSpan::FromSlotStartPtr(last));
  if (bucket.num_system_pages_per_slot_span == NumSystemPagesPerPartitionPage())
    EXPECT_EQ(reinterpret_cast<size_t>(first) & PartitionPageBaseMask(),
              reinterpret_cast<size_t>(last) & PartitionPageBaseMask());
  EXPECT_EQ(num_slots, static_cast<size_t>(
                           bucket.active_slot_spans_head->num_allocated_slots));
  EXPECT_EQ(nullptr, bucket.active_slot_spans_head->freelist_head);
  EXPECT_TRUE(bucket.is_valid());
  EXPECT_TRUE(bucket.active_slot_spans_head !=
              SlotSpan::get_sentinel_slot_span());

  return {bucket.active_slot_spans_head, root.AdjustPointerForExtrasAdd(first),
          root.AdjustPointerForExtrasAdd(last)};
}

bool IsInFreeList(void* slot_start) {
  auto* slot_span = SlotSpan::FromSlotStartPtr(slot_start);
  for (auto* entry = slot_span->freelist_head; entry;
       entry = entry->GetNext()) {
    if (entry == slot_start)
      return true;
  }
  return false;
}

struct ListBase {
  ListBase* next = nullptr;
};

template <size_t Size>
struct List final : ListBase {
  char buffer[Size];

  static List* Create(ThreadSafePartitionRoot& root, ListBase* next = nullptr) {
    auto* list = static_cast<List*>(root.Alloc(sizeof(List), nullptr));
    list->next = next;
    return list;
  }

  static void Destroy(ThreadSafePartitionRoot& root, List* list) {
    root.Free(list);
  }
};

}  // namespace

TEST_F(PCScanTest, ArbitraryObjectInQuarantine) {
  using ListType = List<8>;

  auto* obj1 = ListType::Create(root());
  auto* obj2 = ListType::Create(root());
  EXPECT_FALSE(IsInQuarantine(obj1));
  EXPECT_FALSE(IsInQuarantine(obj2));

  ListType::Destroy(root(), obj2);
  EXPECT_FALSE(IsInQuarantine(obj1));
  EXPECT_TRUE(IsInQuarantine(obj2));
}

TEST_F(PCScanTest, FirstObjectInQuarantine) {
  static constexpr size_t kAllocationSize = 16;

  FullSlotSpanAllocation full_slot_span =
      GetFullSlotSpan(root(), kAllocationSize);
  EXPECT_FALSE(IsInQuarantine(full_slot_span.first));

  root().FreeNoHooks(full_slot_span.first);
  EXPECT_TRUE(IsInQuarantine(full_slot_span.first));
}

TEST_F(PCScanTest, LastObjectInQuarantine) {
  static constexpr size_t kAllocationSize = 16;

  FullSlotSpanAllocation full_slot_span =
      GetFullSlotSpan(root(), kAllocationSize);
  EXPECT_FALSE(IsInQuarantine(full_slot_span.last));

  root().FreeNoHooks(full_slot_span.last);
  EXPECT_TRUE(IsInQuarantine(full_slot_span.last));
}

namespace {

template <typename SourceList, typename ValueList>
void TestDanglingReference(PCScanTest& test,
                           SourceList* source,
                           ValueList* value) {
  auto* value_root = ThreadSafePartitionRoot::FromPointerInNormalBuckets(
      reinterpret_cast<char*>(value));
  {
    // Free |value| and leave the dangling reference in |source|.
    ValueList::Destroy(*value_root, value);
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
    EXPECT_TRUE(
        IsInFreeList(value_root->AdjustPointerForExtrasSubtract(value)));
  }
}
}  // namespace

TEST_F(PCScanTest, DanglingReferenceSameBucket) {
  using SourceList = List<8>;
  using ValueList = SourceList;

  // Create two objects, where |source| references |value|.
  auto* value = ValueList::Create(root(), nullptr);
  auto* source = SourceList::Create(root(), value);

  TestDanglingReference(*this, source, value);
}

TEST_F(PCScanTest, DanglingReferenceDifferentBuckets) {
  using SourceList = List<8>;
  using ValueList = List<128>;

  // Create two objects, where |source| references |value|.
  auto* value = ValueList::Create(root(), nullptr);
  auto* source = SourceList::Create(root(), value);

  TestDanglingReference(*this, source, value);
}

TEST_F(PCScanTest, DanglingReferenceSameSlotSpanButDifferentPages) {
  using SourceList = List<8>;
  using ValueList = SourceList;

  static const size_t kObjectSizeForSlotSpanConsistingOfMultiplePartitionPages =
      static_cast<size_t>(PartitionPageSize() * 0.75);

  FullSlotSpanAllocation full_slot_span = GetFullSlotSpan(
      root(), root().AdjustSizeForExtrasSubtract(
                  kObjectSizeForSlotSpanConsistingOfMultiplePartitionPages));

  // Assert that the first and the last objects are in the same slot span but on
  // different partition pages.
  ASSERT_EQ(SlotSpan::FromSlotInnerPtr(full_slot_span.first),
            SlotSpan::FromSlotInnerPtr(full_slot_span.last));
  ASSERT_NE(
      reinterpret_cast<size_t>(full_slot_span.first) & PartitionPageBaseMask(),
      reinterpret_cast<size_t>(full_slot_span.last) & PartitionPageBaseMask());

  // Create two objects, on different partition pages.
  auto* value = new (full_slot_span.first) ValueList;
  auto* source = new (full_slot_span.last) SourceList;
  source->next = value;

  TestDanglingReference(*this, source, value);
}

TEST_F(PCScanTest, DanglingReferenceFromFullPage) {
  using SourceList = List<64>;
  using ValueList = SourceList;

  FullSlotSpanAllocation full_slot_span =
      GetFullSlotSpan(root(), sizeof(SourceList));
  void* source_addr = full_slot_span.first;
  // This allocation must go through the slow path and call SetNewActivePage(),
  // which will flush the full page from the active page list.
  void* value_addr = root().AllocFlagsNoHooks(0, sizeof(ValueList));

  // Assert that the first and the last objects are in different slot spans but
  // in the same bucket.
  SlotSpan* source_slot_span =
      ThreadSafePartitionRoot::SlotSpan::FromSlotInnerPtr(source_addr);
  SlotSpan* value_slot_span =
      ThreadSafePartitionRoot::SlotSpan::FromSlotInnerPtr(value_addr);
  ASSERT_NE(source_slot_span, value_slot_span);
  ASSERT_EQ(source_slot_span->bucket, value_slot_span->bucket);

  // Create two objects, where |source| is in a full detached page.
  auto* value = new (value_addr) ValueList;
  auto* source = new (source_addr) SourceList;
  source->next = value;

  TestDanglingReference(*this, source, value);
}

namespace {

template <size_t Size>
struct ListWithInnerReference {
  char buffer1[Size];
  char* next = nullptr;
  char buffer2[Size];

  static ListWithInnerReference* Create(ThreadSafePartitionRoot& root) {
    auto* list = static_cast<ListWithInnerReference*>(
        root.Alloc(sizeof(ListWithInnerReference), nullptr));
    return list;
  }

  static void Destroy(ThreadSafePartitionRoot& root,
                      ListWithInnerReference* list) {
    root.Free(list);
  }
};

}  // namespace

TEST_F(PCScanTest, DanglingInnerReference) {
  using SourceList = ListWithInnerReference<64>;
  using ValueList = SourceList;

  auto* source = SourceList::Create(root());
  auto* value = ValueList::Create(root());
  source->next = value->buffer2;

  TestDanglingReference(*this, source, value);
}

TEST_F(PCScanTest, DanglingInterPartitionReference) {
  using SourceList = List<64>;
  using ValueList = SourceList;

  ThreadSafePartitionRoot source_root(
      {PartitionOptions::AlignedAlloc::kDisallowed,
       PartitionOptions::ThreadCache::kDisabled,
       PartitionOptions::Quarantine::kAllowed,
       PartitionOptions::Cookies::kAllowed,
       PartitionOptions::RefCount::kDisallowed});
  ThreadSafePartitionRoot value_root(
      {PartitionOptions::AlignedAlloc::kDisallowed,
       PartitionOptions::ThreadCache::kDisabled,
       PartitionOptions::Quarantine::kAllowed,
       PartitionOptions::Cookies::kAllowed,
       PartitionOptions::RefCount::kDisallowed});

  PCScan::RegisterScannableRoot(&source_root);
  PCScan::RegisterScannableRoot(&value_root);

  auto* source = SourceList::Create(source_root);
  auto* value = ValueList::Create(value_root);
  source->next = value;

  TestDanglingReference(*this, source, value);
}

TEST_F(PCScanTest, DanglingReferenceToNonScannablePartition) {
  using SourceList = List<64>;
  using ValueList = SourceList;

  ThreadSafePartitionRoot source_root(
      {PartitionOptions::AlignedAlloc::kDisallowed,
       PartitionOptions::ThreadCache::kDisabled,
       PartitionOptions::Quarantine::kAllowed,
       PartitionOptions::Cookies::kAllowed,
       PartitionOptions::RefCount::kDisallowed});
  ThreadSafePartitionRoot value_root(
      {PartitionOptions::AlignedAlloc::kDisallowed,
       PartitionOptions::ThreadCache::kDisabled,
       PartitionOptions::Quarantine::kAllowed,
       PartitionOptions::Cookies::kAllowed,
       PartitionOptions::RefCount::kDisallowed});

  PCScan::RegisterScannableRoot(&source_root);
  PCScan::RegisterNonScannableRoot(&value_root);

  auto* source = SourceList::Create(source_root);
  auto* value = ValueList::Create(value_root);
  source->next = value;

  TestDanglingReference(*this, source, value);
}

TEST_F(PCScanTest, DanglingReferenceFromNonScannablePartition) {
  using SourceList = List<64>;
  using ValueList = SourceList;

  ThreadSafePartitionRoot source_root(
      {PartitionOptions::AlignedAlloc::kDisallowed,
       PartitionOptions::ThreadCache::kDisabled,
       PartitionOptions::Quarantine::kAllowed,
       PartitionOptions::Cookies::kAllowed,
       PartitionOptions::RefCount::kDisallowed});
  ThreadSafePartitionRoot value_root(
      {PartitionOptions::AlignedAlloc::kDisallowed,
       PartitionOptions::ThreadCache::kDisabled,
       PartitionOptions::Quarantine::kAllowed,
       PartitionOptions::Cookies::kAllowed,
       PartitionOptions::RefCount::kDisallowed});

  PCScan::RegisterNonScannableRoot(&source_root);
  PCScan::RegisterScannableRoot(&value_root);

  auto* source = SourceList::Create(source_root);
  auto* value = ValueList::Create(value_root);
  source->next = value;

  // Free |value| and leave the dangling reference in |source|.
  ValueList::Destroy(source_root, value);
  // Check that |value| is in the quarantine now.
  EXPECT_TRUE(IsInQuarantine(value));
  // Run PCScan.
  RunPCScan();
  // Check that the object is no longer in the quarantine since the pointer to
  // it was not scanned from the non-scannable partition.
  EXPECT_FALSE(IsInQuarantine(value));
  // Check that the object is in the freelist now.
  EXPECT_TRUE(IsInFreeList(value_root.AdjustPointerForExtrasSubtract(value)));
}

// Death tests misbehave on Android, http://crbug.com/643760.
#if defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)
TEST_F(PCScanTest, DoubleFree) {
  auto* list = List<1>::Create(root());
  List<1>::Destroy(root(), list);
  EXPECT_DEATH(List<1>::Destroy(root(), list), "");
}
#endif

#if !PCSCAN_DISABLE_SAFEPOINTS
namespace {
template <typename SourceList, typename ValueList>
void TestDanglingReferenceWithSafepoint(PCScanTest& test,
                                        SourceList* source,
                                        ValueList* value) {
  auto* value_root = ThreadSafePartitionRoot::FromPointerInNormalBuckets(
      reinterpret_cast<char*>(value));
  {
    // Free |value| and leave the dangling reference in |source|.
    ValueList::Destroy(*value_root, value);
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
    EXPECT_FALSE(
        IsInFreeList(test.root().AdjustPointerForExtrasSubtract(value)));
    // Run sweeper.
    test.FinishPCScanAsScanner();
    // Check that |value| still exists.
    EXPECT_FALSE(
        IsInFreeList(test.root().AdjustPointerForExtrasSubtract(value)));
  }
  {
    // Get rid of the dangling reference.
    source->next = nullptr;
    // Schedule PCScan but don't scan.
    test.SchedulePCScan();
    // Enter safepoint and scan from mutator.
    test.JoinPCScanAsMutator();
    // Check that the object is no longer in the quarantine.
    EXPECT_FALSE(test.IsInQuarantine(value));
    // Check that |value| is not in the freelist yet, since sweeper didn't run.
    EXPECT_FALSE(
        IsInFreeList(test.root().AdjustPointerForExtrasSubtract(value)));
    test.FinishPCScanAsScanner();
    // Check that |value| is in the freelist now.
    EXPECT_TRUE(
        IsInFreeList(test.root().AdjustPointerForExtrasSubtract(value)));
  }
}
}  // namespace

TEST_F(PCScanTest, Safepoint) {
  using SourceList = List<64>;
  using ValueList = SourceList;

  DisableStackScanningScope no_stack_scanning;

  auto* source = SourceList::Create(root());
  auto* value = ValueList::Create(root());
  source->next = value;

  TestDanglingReferenceWithSafepoint(*this, source, value);
}
#endif  // PCSCAN_DISABLE_SAFEPOINTS

TEST_F(PCScanTest, StackScanning) {
  using ValueList = List<8>;

  PCScan::EnableStackScanning();

  static void* dangling_reference = nullptr;
  // Set to nullptr if the test is retried.
  dangling_reference = nullptr;

  // Create and set dangling reference in the global.
  [this]() NOINLINE {
    auto* value = ValueList::Create(root(), nullptr);
    ValueList::Destroy(root(), value);
    dangling_reference = value;
  }();

  [this]() NOINLINE {
    // Register the top of the stack to be the current pointer.
    PCScan::NotifyThreadCreated(GetStackPointer());
    [this]() NOINLINE {
      // This writes the pointer to the stack.
      auto* volatile stack_ref = dangling_reference;
      ALLOW_UNUSED_LOCAL(stack_ref);
      [this]() NOINLINE {
        // Schedule PCScan but don't scan.
        SchedulePCScan();
        // Enter safepoint and scan from mutator. This will scan the stack.
        JoinPCScanAsMutator();
        // Check that the object is still quarantined since it's referenced by
        // |dangling_reference|.
        EXPECT_TRUE(IsInQuarantine(dangling_reference));
        // Check that value is not in the freelist.
        EXPECT_FALSE(IsInFreeList(
            root().AdjustPointerForExtrasSubtract(dangling_reference)));
        // Run sweeper.
        FinishPCScanAsScanner();
        // Check that |dangling_reference| still exists.
        EXPECT_FALSE(IsInFreeList(
            root().AdjustPointerForExtrasSubtract(dangling_reference)));
      }();
    }();
  }();
}

}  // namespace internal
}  // namespace base

#endif  // PA_ALLOW_PCSCAN
#endif  // defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
