// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/allocator/buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/memory/advanced_memory_safety_checks.h"
#include "base/trace_event/process_memory_dump.h"
#include "partition_alloc/partition_root.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace base::trace_event {

#if BUILDFLAG(IS_WIN)

namespace {

class ScopedTestHeap {
 public:
  ScopedTestHeap() : handle_(::HeapCreate(0, 0, 0)) { CHECK(handle_); }
  ~ScopedTestHeap() { CHECK(::HeapDestroy(handle_)); }

  ScopedTestHeap(const ScopedTestHeap&) = delete;
  ScopedTestHeap& operator=(const ScopedTestHeap&) = delete;

  HANDLE handle() { return handle_; }

 private:
  HANDLE handle_;
};

// Above the historical HeapAlloc->VirtualAlloc threshold (~512 KB), so the
// allocation is guaranteed to appear as an orphan busy entry.
constexpr size_t kLargeAllocBytes = 2 * 1024 * 1024;

}  // namespace

TEST(MallocDumpProviderTest, WinHeapInfo_EmptyHeap) {
  ScopedTestHeap heap;

  auto info = internal::WinHeapInfo::FromHandleForTesting(heap.handle());

  EXPECT_EQ(info.allocated_size, 0u);
  EXPECT_EQ(info.block_count, 0u);
  // A fresh heap has at least one reserved region.
  EXPECT_GT(info.committed_size + info.uncommitted_size, 0u);
}

TEST(MallocDumpProviderTest, WinHeapInfo_SmallAllocStaysInRegion) {
  ScopedTestHeap heap;
  void* p = ::HeapAlloc(heap.handle(), 0, 64);
  ASSERT_TRUE(p);

  auto info = internal::WinHeapInfo::FromHandleForTesting(heap.handle());

  EXPECT_GE(info.allocated_size, 64u);
  EXPECT_GE(info.block_count, 1u);
  // The block lives inside a region whose committed bytes already include it,
  // so committed_size must dominate allocated_size.
  EXPECT_GE(info.committed_size, info.allocated_size);

  ::HeapFree(heap.handle(), 0, p);
}

TEST(MallocDumpProviderTest, WinHeapInfo_LargeAllocBecomesOrphanBusy) {
  ScopedTestHeap heap;
  void* p = ::HeapAlloc(heap.handle(), 0, kLargeAllocBytes);
  ASSERT_TRUE(p);

  auto info = internal::WinHeapInfo::FromHandleForTesting(heap.handle());

  EXPECT_GE(info.allocated_size, kLargeAllocBytes);
  EXPECT_GE(info.block_count, 1u);
  // Regression assertion for the orphan-busy-entry fix: committed_size must
  // grow with the large allocation. Before the fix, large blocks lived
  // outside any PROCESS_HEAP_REGION and were not counted as committed, so
  // committed_size would have been (much) less than allocated_size.
  EXPECT_GE(info.committed_size, info.allocated_size);

  ::HeapFree(heap.handle(), 0, p);
}

#endif  // BUILDFLAG(IS_WIN)

// The malloc/win_heap dump is only created when PartitionAlloc is the malloc
// implementation. Without it, ReportWinHeapStats folds the WinHeap numbers
// into the malloc totals and is passed no dump to populate.
#if BUILDFLAG(IS_WIN) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace {

constexpr char kWinHeapWasteDumpName[] =
    "malloc/win_heap/metadata_fragmentation_caches";
constexpr char kMallocWasteDumpName[] = "malloc/metadata_fragmentation_caches";
constexpr char kPartitionsDumpName[] = "malloc/partitions";

const MemoryAllocatorDump* FindAllocatorDump(const ProcessMemoryDump& pmd,
                                             std::string_view name) {
  auto it = pmd.allocator_dumps().find(std::string(name));
  return it == pmd.allocator_dumps().cend() ? nullptr : it->second.get();
}

std::optional<uint64_t> GetScalarEntry(const MemoryAllocatorDump& dump,
                                       std::string_view name,
                                       std::string_view units) {
  for (const auto& entry : dump.entries()) {
    if (entry.name == name) {
      CHECK_EQ(MemoryAllocatorDump::Entry::EntryType::kUint64,
               entry.entry_type);
      CHECK_EQ(units, entry.units);
      return entry.value_uint64;
    }
  }
  return std::nullopt;
}

std::optional<uint64_t> GetBytesEntry(const MemoryAllocatorDump& dump,
                                      std::string_view name) {
  return GetScalarEntry(dump, name, MemoryAllocatorDump::kUnitsBytes);
}

}  // namespace

// malloc/win_heap reports the resident footprint of the heap. The objects
// allocated out of it are reported by malloc/allocated_objects/win_heap.
TEST(MallocDumpProviderTest, WinHeapDumpReportsFootprint) {
  std::unique_ptr<MallocDumpProvider> mdp =
      MallocDumpProvider::CreateForTesting();
  const MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::kDetailed};
  ProcessMemoryDump pmd(dump_args);
  ASSERT_TRUE(mdp->OnMemoryDump(dump_args, &pmd));

  const MemoryAllocatorDump* win_heap_dump =
      FindAllocatorDump(pmd, MallocDumpProvider::kWinHeap);
  const MemoryAllocatorDump* win_heap_objects_dump =
      FindAllocatorDump(pmd, MallocDumpProvider::kWinHeapAllocatedObjects);
  ASSERT_TRUE(win_heap_dump);
  ASSERT_TRUE(win_heap_objects_dump);

  std::optional<uint64_t> size =
      GetBytesEntry(*win_heap_dump, MemoryAllocatorDump::kNameSize);
  std::optional<uint64_t> committed_size =
      GetBytesEntry(*win_heap_dump, "virtual_committed_size");
  std::optional<uint64_t> virtual_size =
      GetBytesEntry(*win_heap_dump, "virtual_size");
  std::optional<uint64_t> allocated_size =
      GetBytesEntry(*win_heap_objects_dump, MemoryAllocatorDump::kNameSize);
  std::optional<uint64_t> wasted = GetBytesEntry(*win_heap_dump, "wasted");
  std::optional<uint64_t> fragmentation =
      GetScalarEntry(*win_heap_dump, "fragmentation", "percent");
  ASSERT_TRUE(size.has_value());
  ASSERT_TRUE(committed_size.has_value());
  ASSERT_TRUE(virtual_size.has_value());
  ASSERT_TRUE(allocated_size.has_value());
  ASSERT_TRUE(wasted.has_value());
  ASSERT_TRUE(fragmentation.has_value());

  // Resident size is approximated with the committed heap size.
  EXPECT_EQ(*size, *committed_size);
  // virtual_size is committed + uncommitted, and the committed bytes of a
  // region already include the blocks allocated inside it.
  EXPECT_GE(*virtual_size, *committed_size);
  EXPECT_GE(*committed_size, *allocated_size);
  // The committed bytes are either handed out to a live allocation or wasted.
  EXPECT_EQ(*committed_size, *allocated_size + *wasted);
  EXPECT_LE(*fragmentation, 100u);
}

// The wasted bytes are reported under malloc/win_heap so that its children
// account for the whole committed heap, and are excluded from the malloc-wide
// malloc/metadata_fragmentation_caches to avoid counting them twice.
TEST(MallocDumpProviderTest, WinHeapWasteReportedUnderHeapDump) {
  std::unique_ptr<MallocDumpProvider> mdp =
      MallocDumpProvider::CreateForTesting();
  const MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::kDetailed};
  ProcessMemoryDump pmd(dump_args);
  ASSERT_TRUE(mdp->OnMemoryDump(dump_args, &pmd));

  const MemoryAllocatorDump* win_heap_dump =
      FindAllocatorDump(pmd, MallocDumpProvider::kWinHeap);
  ASSERT_TRUE(win_heap_dump);
  std::optional<uint64_t> wasted = GetBytesEntry(*win_heap_dump, "wasted");
  ASSERT_TRUE(wasted.has_value());

  // The WinHeap waste is the only contributor to the malloc-wide waste dump on
  // Windows, so excluding it leaves nothing to report there.
  EXPECT_FALSE(FindAllocatorDump(pmd, kMallocWasteDumpName));

  const MemoryAllocatorDump* waste_dump =
      FindAllocatorDump(pmd, kWinHeapWasteDumpName);
  if (*wasted == 0) {
    // A heap whose committed bytes are all handed out gets no waste dump.
    EXPECT_FALSE(waste_dump);
    return;
  }
  ASSERT_TRUE(waste_dump);
  EXPECT_EQ(GetBytesEntry(*waste_dump, MemoryAllocatorDump::kNameSize), wasted);
}

// The objects allocated out of the WinHeap are accounted for under the system
// allocator pool, and reported as suballocated from malloc/win_heap so that the
// heap dump does not account for them a second time.
TEST(MallocDumpProviderTest, WinHeapAllocatedObjectsAreSuballocatedFromHeap) {
  std::unique_ptr<MallocDumpProvider> mdp =
      MallocDumpProvider::CreateForTesting();
  const MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::kDetailed};
  ProcessMemoryDump pmd(dump_args);
  ASSERT_TRUE(mdp->OnMemoryDump(dump_args, &pmd));

  const MemoryAllocatorDump* win_heap_objects_dump =
      FindAllocatorDump(pmd, MallocDumpProvider::kWinHeapAllocatedObjects);
  ASSERT_TRUE(win_heap_objects_dump);
  EXPECT_TRUE(
      GetBytesEntry(*win_heap_objects_dump, MemoryAllocatorDump::kNameSize)
          .has_value());
  EXPECT_TRUE(GetScalarEntry(*win_heap_objects_dump,
                             MemoryAllocatorDump::kNameObjectCount,
                             MemoryAllocatorDump::kUnitsObjects)
                  .has_value());

  // AddSuballocation() names the child after the owner's guid and leaves it
  // without a size of its own: the UI groups nodes named this way under a
  // synthetic "suballocations" entry of the parent, and takes their size from
  // the owner.
  const std::string suballocation_name =
      std::string(MallocDumpProvider::kWinHeap) + "/__" +
      win_heap_objects_dump->guid().ToString();
  const MemoryAllocatorDump* suballocation_dump =
      FindAllocatorDump(pmd, suballocation_name);
  ASSERT_TRUE(suballocation_dump);
  EXPECT_FALSE(
      GetBytesEntry(*suballocation_dump, MemoryAllocatorDump::kNameSize)
          .has_value());

  const auto& edges = pmd.allocator_dumps_edges();
  auto edge = edges.find(win_heap_objects_dump->guid());
  ASSERT_TRUE(edge != edges.cend());
  EXPECT_EQ(edge->second.target.ToUint64(),
            suballocation_dump->guid().ToUint64());
}

// An allocator dump can only own a single target, and malloc/allocated_objects
// already owns malloc/partitions. Attributing the WinHeap objects with an
// ownership edge instead of a child dump would DCHECK in AddOwnershipEdge and
// drop one of the two.
TEST(MallocDumpProviderTest, SystemAllocatorPoolStillOwnsPartitions) {
  std::unique_ptr<MallocDumpProvider> mdp =
      MallocDumpProvider::CreateForTesting();
  const MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::kDetailed};
  ProcessMemoryDump pmd(dump_args);
  ASSERT_TRUE(mdp->OnMemoryDump(dump_args, &pmd));

  const MemoryAllocatorDump* allocated_objects_dump =
      FindAllocatorDump(pmd, MallocDumpProvider::kAllocatedObjects);
  const MemoryAllocatorDump* partitions_dump =
      FindAllocatorDump(pmd, kPartitionsDumpName);
  ASSERT_TRUE(allocated_objects_dump);
  ASSERT_TRUE(partitions_dump);

  const auto& edges = pmd.allocator_dumps_edges();
  auto edge = edges.find(allocated_objects_dump->guid());
  ASSERT_TRUE(edge != edges.cend());
  EXPECT_EQ(edge->second.target.ToUint64(), partitions_dump->guid().ToUint64());
}

// Walking the heap is too expensive for the lighter levels of detail, so no
// dumps are created for them at all.
TEST(MallocDumpProviderTest, WinHeapDumpsOmittedBelowDetailedLevel) {
  std::unique_ptr<MallocDumpProvider> mdp =
      MallocDumpProvider::CreateForTesting();
  const MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::kBackground};
  ProcessMemoryDump pmd(dump_args);
  ASSERT_TRUE(mdp->OnMemoryDump(dump_args, &pmd));

  EXPECT_FALSE(FindAllocatorDump(pmd, MallocDumpProvider::kWinHeap));
  EXPECT_FALSE(
      FindAllocatorDump(pmd, MallocDumpProvider::kWinHeapAllocatedObjects));
  EXPECT_FALSE(FindAllocatorDump(pmd, kWinHeapWasteDumpName));
}

#endif  // BUILDFLAG(IS_WIN) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace {

class NormalTestClass1 {
 public:
  NormalTestClass1() = default;

  uint8_t unused_padding[15];
};

class LeakedTestClass1 {
  LEAKED_SANITIZED_OBJECT();

 public:
  LeakedTestClass1() = default;

  uint8_t unused_padding[15];
};

class LeakedTestClass2 {
  LEAKED_SANITIZED_OBJECT();

 public:
  LeakedTestClass2() = default;

  uint8_t unused_padding[2047];
};

std::pair<bool, size_t> GetIntendedLeakSize() {
  constexpr std::string_view kIntendedLeakSize = "intended_leak_size";
  constexpr std::string_view kAllocatorDumpName = "malloc/partitions/leaked";

  std::unique_ptr<MallocDumpProvider> mdp =
      MallocDumpProvider::CreateForTesting();
  const MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::kBackground};
  ProcessMemoryDump pmd(dump_args);
  mdp->OnMemoryDump(dump_args, &pmd);

  auto iterator = pmd.allocator_dumps().find(std::string(kAllocatorDumpName));
  if (pmd.allocator_dumps().cend() == iterator) {
    return std::make_pair(false, 0u);
  }
  for (const auto& entry : iterator->second->entries()) {
    if (entry.name == kIntendedLeakSize) {
      CHECK_EQ(MemoryAllocatorDump::Entry::EntryType::kUint64,
               entry.entry_type);
      CHECK_EQ(MemoryAllocatorDump::kUnitsBytes, entry.units);
      return std::make_pair(true, entry.value_uint64);
    }
  }
  return std::make_pair(false, 0u);
}

}  // namespace

TEST(MallocDumpProviderTest, DumpIntendedLeakedSize) {
  // To avoid flakiness, firstly we will measure current `intended_leak_size`.
  // The flakiness will be caused by `safety_checks_unittests` because the tests
  // leak some objects at free().
  size_t expected_intended_leak_size;
  expected_intended_leak_size = GetIntendedLeakSize().second;

  const auto* leaked_security_object_root =
      base::internal::LeakedSecurityObjectAllocator();
  ASSERT_NE(leaked_security_object_root, nullptr);

  // Allocate and deallocate normal object. This doesn't cause any memory leaks.
  {
    std::unique_ptr<NormalTestClass1> normal_obj1 =
        std::make_unique<NormalTestClass1>();
    ASSERT_NE(normal_obj1, nullptr);
    EXPECT_NE(
        leaked_security_object_root,
        partition_alloc::PartitionRoot::GetRootFromAddress(normal_obj1.get()));
  }
  {
    auto intended_leak_size = GetIntendedLeakSize();
    EXPECT_TRUE(intended_leak_size.first);
    EXPECT_EQ(expected_intended_leak_size, intended_leak_size.second);
  }

  // Allocate and deallocate leaked security object. This will cause memory
  // leak.
  {
    std::unique_ptr<LeakedTestClass1> leaked_obj1 =
        std::make_unique<LeakedTestClass1>();
    ASSERT_NE(leaked_obj1, nullptr);
    EXPECT_EQ(
        leaked_security_object_root,
        partition_alloc::PartitionRoot::GetRootFromAddress(leaked_obj1.get()));
    // `intended_leaked_size` is calculated based on `slot_size`.
    expected_intended_leak_size +=
        leaked_security_object_root->GetSlotSizeForTesting(leaked_obj1.get());
  }

  {
    auto intended_leak_size = GetIntendedLeakSize();
    EXPECT_TRUE(intended_leak_size.first);
    EXPECT_EQ(expected_intended_leak_size, intended_leak_size.second);
  }

  {
    std::unique_ptr<LeakedTestClass2> leaked_obj2 =
        std::make_unique<LeakedTestClass2>();
    ASSERT_NE(leaked_obj2, nullptr);
    EXPECT_EQ(
        leaked_security_object_root,
        partition_alloc::PartitionRoot::GetRootFromAddress(leaked_obj2.get()));
    expected_intended_leak_size +=
        leaked_security_object_root->GetSlotSizeForTesting(leaked_obj2.get());
  }

  {
    auto intended_leak_size = GetIntendedLeakSize();
    EXPECT_TRUE(intended_leak_size.first);
    EXPECT_EQ(expected_intended_leak_size, intended_leak_size.second);
  }
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace base::trace_event
