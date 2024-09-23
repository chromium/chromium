// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_dump.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/aligned_memory.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/process/process_metrics.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_infra_background_allowlist.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "winbase.h"
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <sys/mman.h>
#endif

namespace base::trace_event {

namespace {

const MemoryDumpArgs kDetailedDumpArgs = {MemoryDumpLevelOfDetail::kDetailed};
constexpr std::string_view kTestDumpNameAllowlist[] = {
    "Allowlisted/TestName", "Allowlisted/TestName_0x?",
    "Allowlisted/0x?/TestName", "Allowlisted/0x?"};

void* Map(size_t size) {
#if BUILDFLAG(IS_WIN)
  return ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT,
                        PAGE_READWRITE);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
                0, 0);
#endif
}

void Unmap(void* addr, size_t size) {
#if BUILDFLAG(IS_WIN)
  ::VirtualFree(addr, 0, MEM_DECOMMIT);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  ::munmap(addr, size);
#else
#error This architecture is not (yet) supported.
#endif
}

std::optional<size_t> CountResidentBytesInSharedMemory(
    const WritableSharedMemoryMapping& mapping) {
  // SAFETY: We need the actual mapped memory size here. There's no public
  // method to get this as a span, so we need to construct it unsafely. The
  // mapped_size() is larger than `mem.size()` but represents the actual memory
  // segment size in the SharedMemoryMapping.
  auto mapped =
      UNSAFE_BUFFERS(base::span(mapping.data(), mapping.mapped_size()));
  return ProcessMemoryDump::CountResidentBytesInSharedMemory(mapped.data(),
                                                             mapped.size());
}

}  // namespace

TEST(ProcessMemoryDumpTest, MoveConstructor) {
  ProcessMemoryDump pmd1 = ProcessMemoryDump(kDetailedDumpArgs);
  pmd1.CreateAllocatorDump("mad1");
  pmd1.CreateAllocatorDump("mad2");
  pmd1.AddOwnershipEdge(MemoryAllocatorDumpGuid(42),
                        MemoryAllocatorDumpGuid(4242));

  ProcessMemoryDump pmd2(std::move(pmd1));

  EXPECT_EQ(1u, pmd2.allocator_dumps().count("mad1"));
  EXPECT_EQ(1u, pmd2.allocator_dumps().count("mad2"));
  EXPECT_EQ(MemoryDumpLevelOfDetail::kDetailed,
            pmd2.dump_args().level_of_detail);
  EXPECT_EQ(1u, pmd2.allocator_dumps_edges().size());

  // Check that calling serialization routines doesn't cause a crash.
  auto traced_value = std::make_unique<TracedValue>();
  pmd2.SerializeAllocatorDumpsInto(traced_value.get());
}

TEST(ProcessMemoryDumpTest, MoveAssignment) {
  ProcessMemoryDump pmd1 = ProcessMemoryDump(kDetailedDumpArgs);
  pmd1.CreateAllocatorDump("mad1");
  pmd1.CreateAllocatorDump("mad2");
  pmd1.AddOwnershipEdge(MemoryAllocatorDumpGuid(42),
                        MemoryAllocatorDumpGuid(4242));

  ProcessMemoryDump pmd2({MemoryDumpLevelOfDetail::kBackground});
  pmd2.CreateAllocatorDump("malloc");

  pmd2 = std::move(pmd1);
  EXPECT_EQ(1u, pmd2.allocator_dumps().count("mad1"));
  EXPECT_EQ(1u, pmd2.allocator_dumps().count("mad2"));
  EXPECT_EQ(0u, pmd2.allocator_dumps().count("mad3"));
  EXPECT_EQ(MemoryDumpLevelOfDetail::kDetailed,
            pmd2.dump_args().level_of_detail);
  EXPECT_EQ(1u, pmd2.allocator_dumps_edges().size());

  // Check that calling serialization routines doesn't cause a crash.
  auto traced_value = std::make_unique<TracedValue>();
  pmd2.SerializeAllocatorDumpsInto(traced_value.get());
}

TEST(ProcessMemoryDumpTest, Clear) {
  std::unique_ptr<ProcessMemoryDump> pmd1(
      new ProcessMemoryDump(kDetailedDumpArgs));
  pmd1->CreateAllocatorDump("mad1");
  pmd1->CreateAllocatorDump("mad2");
  ASSERT_FALSE(pmd1->allocator_dumps().empty());

  pmd1->AddOwnershipEdge(MemoryAllocatorDumpGuid(42),
                         MemoryAllocatorDumpGuid(4242));

  MemoryAllocatorDumpGuid shared_mad_guid1(1);
  MemoryAllocatorDumpGuid shared_mad_guid2(2);
  pmd1->CreateSharedGlobalAllocatorDump(shared_mad_guid1);
  pmd1->CreateSharedGlobalAllocatorDump(shared_mad_guid2);

  pmd1->Clear();
  ASSERT_TRUE(pmd1->allocator_dumps().empty());
  ASSERT_TRUE(pmd1->allocator_dumps_edges().empty());
  ASSERT_EQ(nullptr, pmd1->GetAllocatorDump("mad1"));
  ASSERT_EQ(nullptr, pmd1->GetAllocatorDump("mad2"));
  ASSERT_EQ(nullptr, pmd1->GetSharedGlobalAllocatorDump(shared_mad_guid1));
  ASSERT_EQ(nullptr, pmd1->GetSharedGlobalAllocatorDump(shared_mad_guid2));

  // Check that calling serialization routines doesn't cause a crash.
  auto traced_value = std::make_unique<TracedValue>();
  pmd1->SerializeAllocatorDumpsInto(traced_value.get());

  // Check that the pmd can be reused and behaves as expected.
  auto* mad1 = pmd1->CreateAllocatorDump("mad1");
  auto* mad3 = pmd1->CreateAllocatorDump("mad3");
  auto* shared_mad1 = pmd1->CreateSharedGlobalAllocatorDump(shared_mad_guid1);
  auto* shared_mad2 =
      pmd1->CreateWeakSharedGlobalAllocatorDump(shared_mad_guid2);
  ASSERT_EQ(4u, pmd1->allocator_dumps().size());
  ASSERT_EQ(mad1, pmd1->GetAllocatorDump("mad1"));
  ASSERT_EQ(nullptr, pmd1->GetAllocatorDump("mad2"));
  ASSERT_EQ(mad3, pmd1->GetAllocatorDump("mad3"));
  ASSERT_EQ(shared_mad1, pmd1->GetSharedGlobalAllocatorDump(shared_mad_guid1));
  ASSERT_EQ(MemoryAllocatorDump::Flags::DEFAULT, shared_mad1->flags());
  ASSERT_EQ(shared_mad2, pmd1->GetSharedGlobalAllocatorDump(shared_mad_guid2));
  ASSERT_EQ(MemoryAllocatorDump::Flags::WEAK, shared_mad2->flags());

  traced_value = std::make_unique<TracedValue>();
  pmd1->SerializeAllocatorDumpsInto(traced_value.get());

  pmd1.reset();
}

TEST(ProcessMemoryDumpTest, TakeAllDumpsFrom) {
  std::unique_ptr<TracedValue> traced_value(new TracedValue);
  std::unordered_map<AllocationContext, AllocationMetrics> metrics_by_context;
  metrics_by_context[AllocationContext()] = {1, 1};
  TraceEventMemoryOverhead overhead;

  std::unique_ptr<ProcessMemoryDump> pmd1(
      new ProcessMemoryDump(kDetailedDumpArgs));
  auto* mad1_1 = pmd1->CreateAllocatorDump("pmd1/mad1");
  auto* mad1_2 = pmd1->CreateAllocatorDump("pmd1/mad2");
  pmd1->AddOwnershipEdge(mad1_1->guid(), mad1_2->guid());
  pmd1->DumpHeapUsage(metrics_by_context, overhead, "pmd1/heap_dump1");
  pmd1->DumpHeapUsage(metrics_by_context, overhead, "pmd1/heap_dump2");

  std::unique_ptr<ProcessMemoryDump> pmd2(
      new ProcessMemoryDump(kDetailedDumpArgs));
  auto* mad2_1 = pmd2->CreateAllocatorDump("pmd2/mad1");
  auto* mad2_2 = pmd2->CreateAllocatorDump("pmd2/mad2");
  pmd2->AddOwnershipEdge(mad2_1->guid(), mad2_2->guid());
  pmd2->DumpHeapUsage(metrics_by_context, overhead, "pmd2/heap_dump1");
  pmd2->DumpHeapUsage(metrics_by_context, overhead, "pmd2/heap_dump2");

  MemoryAllocatorDumpGuid shared_mad_guid1(1);
  MemoryAllocatorDumpGuid shared_mad_guid2(2);
  auto* shared_mad1 = pmd2->CreateSharedGlobalAllocatorDump(shared_mad_guid1);
  auto* shared_mad2 =
      pmd2->CreateWeakSharedGlobalAllocatorDump(shared_mad_guid2);

  pmd1->TakeAllDumpsFrom(pmd2.get());

  // Make sure that pmd2 is empty but still usable after it has been emptied.
  ASSERT_TRUE(pmd2->allocator_dumps().empty());
  ASSERT_TRUE(pmd2->allocator_dumps_edges().empty());
  pmd2->CreateAllocatorDump("pmd2/this_mad_stays_with_pmd2");
  ASSERT_EQ(1u, pmd2->allocator_dumps().size());
  ASSERT_EQ(1u, pmd2->allocator_dumps().count("pmd2/this_mad_stays_with_pmd2"));
  pmd2->AddOwnershipEdge(MemoryAllocatorDumpGuid(42),
                         MemoryAllocatorDumpGuid(4242));

  // Check that calling serialization routines doesn't cause a crash.
  pmd2->SerializeAllocatorDumpsInto(traced_value.get());

  // Free the |pmd2| to check that the memory ownership of the two MAD(s)
  // has been transferred to |pmd1|.
  pmd2.reset();

  // Now check that |pmd1| has been effectively merged.
  ASSERT_EQ(6u, pmd1->allocator_dumps().size());
  ASSERT_EQ(1u, pmd1->allocator_dumps().count("pmd1/mad1"));
  ASSERT_EQ(1u, pmd1->allocator_dumps().count("pmd1/mad2"));
  ASSERT_EQ(1u, pmd1->allocator_dumps().count("pmd2/mad1"));
  ASSERT_EQ(1u, pmd1->allocator_dumps().count("pmd1/mad2"));
  ASSERT_EQ(2u, pmd1->allocator_dumps_edges().size());
  ASSERT_EQ(shared_mad1, pmd1->GetSharedGlobalAllocatorDump(shared_mad_guid1));
  ASSERT_EQ(shared_mad2, pmd1->GetSharedGlobalAllocatorDump(shared_mad_guid2));
  ASSERT_TRUE(MemoryAllocatorDump::Flags::WEAK & shared_mad2->flags());

  // Check that calling serialization routines doesn't cause a crash.
  traced_value = std::make_unique<TracedValue>();
  pmd1->SerializeAllocatorDumpsInto(traced_value.get());

  pmd1.reset();
}

TEST(ProcessMemoryDumpTest, OverrideOwnershipEdge) {
  std::unique_ptr<ProcessMemoryDump> pmd(
      new ProcessMemoryDump(kDetailedDumpArgs));

  auto* shm_dump1 = pmd->CreateAllocatorDump("shared_mem/seg1");
  auto* shm_dump2 = pmd->CreateAllocatorDump("shared_mem/seg2");
  auto* shm_dump3 = pmd->CreateAllocatorDump("shared_mem/seg3");
  auto* shm_dump4 = pmd->CreateAllocatorDump("shared_mem/seg4");

  // Create one allocation with an auto-assigned guid and mark it as a
  // suballocation of "fakealloc/allocated_objects".
  auto* child1_dump = pmd->CreateAllocatorDump("shared_mem/child/seg1");
  pmd->AddOverridableOwnershipEdge(child1_dump->guid(), shm_dump1->guid(),
                                   0 /* importance */);
  auto* child2_dump = pmd->CreateAllocatorDump("shared_mem/child/seg2");
  pmd->AddOwnershipEdge(child2_dump->guid(), shm_dump2->guid(),
                        3 /* importance */);
  MemoryAllocatorDumpGuid shared_mad_guid(1);
  pmd->CreateSharedGlobalAllocatorDump(shared_mad_guid);
  pmd->AddOverridableOwnershipEdge(shm_dump3->guid(), shared_mad_guid,
                                   0 /* importance */);
  auto* child4_dump = pmd->CreateAllocatorDump("shared_mem/child/seg4");
  pmd->AddOverridableOwnershipEdge(child4_dump->guid(), shm_dump4->guid(),
                                   4 /* importance */);

  const ProcessMemoryDump::AllocatorDumpEdgesMap& edges =
      pmd->allocator_dumps_edges();
  EXPECT_EQ(4u, edges.size());
  EXPECT_EQ(shm_dump1->guid(), edges.find(child1_dump->guid())->second.target);
  EXPECT_EQ(0, edges.find(child1_dump->guid())->second.importance);
  EXPECT_TRUE(edges.find(child1_dump->guid())->second.overridable);
  EXPECT_EQ(shm_dump2->guid(), edges.find(child2_dump->guid())->second.target);
  EXPECT_EQ(3, edges.find(child2_dump->guid())->second.importance);
  EXPECT_FALSE(edges.find(child2_dump->guid())->second.overridable);
  EXPECT_EQ(shared_mad_guid, edges.find(shm_dump3->guid())->second.target);
  EXPECT_EQ(0, edges.find(shm_dump3->guid())->second.importance);
  EXPECT_TRUE(edges.find(shm_dump3->guid())->second.overridable);
  EXPECT_EQ(shm_dump4->guid(), edges.find(child4_dump->guid())->second.target);
  EXPECT_EQ(4, edges.find(child4_dump->guid())->second.importance);
  EXPECT_TRUE(edges.find(child4_dump->guid())->second.overridable);

  // These should override old edges:
  pmd->AddOwnershipEdge(child1_dump->guid(), shm_dump1->guid(),
                        1 /* importance */);
  pmd->AddOwnershipEdge(shm_dump3->guid(), shared_mad_guid, 2 /* importance */);
  // This should not change the old edges.
  pmd->AddOverridableOwnershipEdge(child2_dump->guid(), shm_dump2->guid(),
                                   0 /* importance */);
  pmd->AddOwnershipEdge(child4_dump->guid(), shm_dump4->guid(),
                        0 /* importance */);

  EXPECT_EQ(4u, edges.size());
  EXPECT_EQ(shm_dump1->guid(), edges.find(child1_dump->guid())->second.target);
  EXPECT_EQ(1, edges.find(child1_dump->guid())->second.importance);
  EXPECT_FALSE(edges.find(child1_dump->guid())->second.overridable);
  EXPECT_EQ(shm_dump2->guid(), edges.find(child2_dump->guid())->second.target);
  EXPECT_EQ(3, edges.find(child2_dump->guid())->second.importance);
  EXPECT_FALSE(edges.find(child2_dump->guid())->second.overridable);
  EXPECT_EQ(shared_mad_guid, edges.find(shm_dump3->guid())->second.target);
  EXPECT_EQ(2, edges.find(shm_dump3->guid())->second.importance);
  EXPECT_FALSE(edges.find(shm_dump3->guid())->second.overridable);
  EXPECT_EQ(shm_dump4->guid(), edges.find(child4_dump->guid())->second.target);
  EXPECT_EQ(4, edges.find(child4_dump->guid())->second.importance);
  EXPECT_FALSE(edges.find(child4_dump->guid())->second.overridable);
}

TEST(ProcessMemoryDumpTest, Suballocations) {
  std::unique_ptr<ProcessMemoryDump> pmd(
      new ProcessMemoryDump(kDetailedDumpArgs));
  const std::string allocator_dump_name = "fakealloc/allocated_objects";
  pmd->CreateAllocatorDump(allocator_dump_name);

  // Create one allocation with an auto-assigned guid and mark it as a
  // suballocation of "fakealloc/allocated_objects".
  auto* pic1_dump = pmd->CreateAllocatorDump("picturemanager/picture1");
  pmd->AddSuballocation(pic1_dump->guid(), allocator_dump_name);

  // Same here, but this time create an allocation with an explicit guid.
  auto* pic2_dump = pmd->CreateAllocatorDump("picturemanager/picture2",
                                            MemoryAllocatorDumpGuid(0x42));
  pmd->AddSuballocation(pic2_dump->guid(), allocator_dump_name);

  // Now check that AddSuballocation() has created anonymous child dumps under
  // "fakealloc/allocated_objects".
  auto anon_node_1_it = pmd->allocator_dumps().find(
      allocator_dump_name + "/__" + pic1_dump->guid().ToString());
  ASSERT_NE(pmd->allocator_dumps().end(), anon_node_1_it);

  auto anon_node_2_it =
      pmd->allocator_dumps().find(allocator_dump_name + "/__42");
  ASSERT_NE(pmd->allocator_dumps().end(), anon_node_2_it);

  // Finally check that AddSuballocation() has created also the
  // edges between the pictures and the anonymous allocator child dumps.
  bool found_edge[2]{false, false};
  for (const auto& e : pmd->allocator_dumps_edges()) {
    found_edge[0] |= (e.first == pic1_dump->guid() &&
                      e.second.target == anon_node_1_it->second->guid());
    found_edge[1] |= (e.first == pic2_dump->guid() &&
                      e.second.target == anon_node_2_it->second->guid());
  }
  ASSERT_TRUE(found_edge[0]);
  ASSERT_TRUE(found_edge[1]);

  // Check that calling serialization routines doesn't cause a crash.
  std::unique_ptr<TracedValue> traced_value(new TracedValue);
  pmd->SerializeAllocatorDumpsInto(traced_value.get());

  pmd.reset();
}

TEST(ProcessMemoryDumpTest, GlobalAllocatorDumpTest) {
  std::unique_ptr<ProcessMemoryDump> pmd(
      new ProcessMemoryDump(kDetailedDumpArgs));
  MemoryAllocatorDumpGuid shared_mad_guid(1);
  auto* shared_mad1 = pmd->CreateWeakSharedGlobalAllocatorDump(shared_mad_guid);
  ASSERT_EQ(shared_mad_guid, shared_mad1->guid());
  ASSERT_EQ(MemoryAllocatorDump::Flags::WEAK, shared_mad1->flags());

  auto* shared_mad2 = pmd->GetSharedGlobalAllocatorDump(shared_mad_guid);
  ASSERT_EQ(shared_mad1, shared_mad2);
  ASSERT_EQ(MemoryAllocatorDump::Flags::WEAK, shared_mad1->flags());

  auto* shared_mad3 = pmd->CreateWeakSharedGlobalAllocatorDump(shared_mad_guid);
  ASSERT_EQ(shared_mad1, shared_mad3);
  ASSERT_EQ(MemoryAllocatorDump::Flags::WEAK, shared_mad1->flags());

  auto* shared_mad4 = pmd->CreateSharedGlobalAllocatorDump(shared_mad_guid);
  ASSERT_EQ(shared_mad1, shared_mad4);
  ASSERT_EQ(MemoryAllocatorDump::Flags::DEFAULT, shared_mad1->flags());

  auto* shared_mad5 = pmd->CreateWeakSharedGlobalAllocatorDump(shared_mad_guid);
  ASSERT_EQ(shared_mad1, shared_mad5);
  ASSERT_EQ(MemoryAllocatorDump::Flags::DEFAULT, shared_mad1->flags());
}

TEST(ProcessMemoryDumpTest, SharedMemoryOwnershipTest) {
  std::unique_ptr<ProcessMemoryDump> pmd(
      new ProcessMemoryDump(kDetailedDumpArgs));
  const ProcessMemoryDump::AllocatorDumpEdgesMap& edges =
      pmd->allocator_dumps_edges();

  auto* client_dump2 = pmd->CreateAllocatorDump("discardable/segment2");
  auto shm_token2 = UnguessableToken::Create();
  MemoryAllocatorDumpGuid shm_local_guid2 =
      pmd->GetDumpId(SharedMemoryTracker::GetDumpNameForTracing(shm_token2));
  MemoryAllocatorDumpGuid shm_global_guid2 =
      SharedMemoryTracker::GetGlobalDumpIdForTracing(shm_token2);
  pmd->AddOverridableOwnershipEdge(shm_local_guid2, shm_global_guid2,
                                   0 /* importance */);

  pmd->CreateSharedMemoryOwnershipEdge(client_dump2->guid(), shm_token2,
                                       1 /* importance */);
  EXPECT_EQ(2u, edges.size());

  EXPECT_EQ(shm_global_guid2, edges.find(shm_local_guid2)->second.target);
  EXPECT_EQ(1, edges.find(shm_local_guid2)->second.importance);
  EXPECT_FALSE(edges.find(shm_local_guid2)->second.overridable);
  EXPECT_EQ(shm_local_guid2, edges.find(client_dump2->guid())->second.target);
  EXPECT_EQ(1, edges.find(client_dump2->guid())->second.importance);
  EXPECT_FALSE(edges.find(client_dump2->guid())->second.overridable);
}

TEST(ProcessMemoryDumpTest, BackgroundModeTest) {
  MemoryDumpArgs background_args = {MemoryDumpLevelOfDetail::kBackground};
  std::unique_ptr<ProcessMemoryDump> pmd(
      new ProcessMemoryDump(background_args));
  ProcessMemoryDump::is_black_hole_non_fatal_for_testing_ = true;
  SetAllocatorDumpNameAllowlistForTesting(kTestDumpNameAllowlist);
  MemoryAllocatorDump* black_hole_mad = pmd->GetBlackHoleMad(std::string());

  // GetAllocatorDump works for uncreated dumps.
  EXPECT_EQ(nullptr, pmd->GetAllocatorDump("NotAllowlisted/TestName"));
  EXPECT_EQ(nullptr, pmd->GetAllocatorDump("Allowlisted/TestName"));

  // Invalid dump names.
  EXPECT_EQ(black_hole_mad,
            pmd->CreateAllocatorDump("NotAllowlisted/TestName"));
  EXPECT_EQ(black_hole_mad, pmd->CreateAllocatorDump("TestName"));
  EXPECT_EQ(black_hole_mad, pmd->CreateAllocatorDump("Allowlisted/Test"));
  EXPECT_EQ(black_hole_mad,
            pmd->CreateAllocatorDump("Not/Allowlisted/TestName"));
  EXPECT_EQ(black_hole_mad,
            pmd->CreateAllocatorDump("Allowlisted/TestName/Google"));
  EXPECT_EQ(black_hole_mad,
            pmd->CreateAllocatorDump("Allowlisted/TestName/0x1a2Google"));
  EXPECT_EQ(black_hole_mad,
            pmd->CreateAllocatorDump("Allowlisted/TestName/__12/Google"));

  // Suballocations.
  MemoryAllocatorDumpGuid guid(1);
  pmd->AddSuballocation(guid, "malloc/allocated_objects");
  EXPECT_EQ(0u, pmd->allocator_dumps_edges_.size());
  EXPECT_EQ(0u, pmd->allocator_dumps_.size());

  // Global dumps.
  EXPECT_NE(black_hole_mad, pmd->CreateSharedGlobalAllocatorDump(guid));
  EXPECT_NE(black_hole_mad, pmd->CreateWeakSharedGlobalAllocatorDump(guid));
  EXPECT_NE(black_hole_mad, pmd->GetSharedGlobalAllocatorDump(guid));

  // Valid dump names.
  EXPECT_NE(black_hole_mad, pmd->CreateAllocatorDump("Allowlisted/TestName"));
  EXPECT_NE(black_hole_mad,
            pmd->CreateAllocatorDump("Allowlisted/TestName_0xA1b2"));
  EXPECT_NE(black_hole_mad,
            pmd->CreateAllocatorDump("Allowlisted/0xaB/TestName"));

  // GetAllocatorDump is consistent.
  EXPECT_EQ(nullptr, pmd->GetAllocatorDump("NotAllowlisted/TestName"));
  EXPECT_NE(black_hole_mad, pmd->GetAllocatorDump("Allowlisted/TestName"));

  // Test allowed entries.
  ASSERT_TRUE(IsMemoryAllocatorDumpNameInAllowlist("Allowlisted/TestName"));

  // Global dumps should be allowed.
  ASSERT_TRUE(IsMemoryAllocatorDumpNameInAllowlist("global/13456"));

  // Global dumps with non-guids should not be.
  ASSERT_FALSE(IsMemoryAllocatorDumpNameInAllowlist("global/random"));

  // Random names should not.
  ASSERT_FALSE(IsMemoryAllocatorDumpNameInAllowlist("NotAllowlisted/TestName"));

  // Check hex processing.
  ASSERT_TRUE(IsMemoryAllocatorDumpNameInAllowlist("Allowlisted/0xA1b2"));
}

TEST(ProcessMemoryDumpTest, GuidsTest) {
  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::kDetailed};

  const auto process_token_one = UnguessableToken::Create();
  const auto process_token_two = UnguessableToken::Create();

  ProcessMemoryDump pmd1(dump_args);
  pmd1.set_process_token_for_testing(process_token_one);
  MemoryAllocatorDump* mad1 = pmd1.CreateAllocatorDump("foo");

  ProcessMemoryDump pmd2(dump_args);
  pmd2.set_process_token_for_testing(process_token_one);
  MemoryAllocatorDump* mad2 = pmd2.CreateAllocatorDump("foo");

  // If we don't pass the argument we get a random PMD:
  ProcessMemoryDump pmd3(dump_args);
  MemoryAllocatorDump* mad3 = pmd3.CreateAllocatorDump("foo");

  // PMD's for different processes produce different GUIDs even for the same
  // names:
  ProcessMemoryDump pmd4(dump_args);
  pmd4.set_process_token_for_testing(process_token_two);
  MemoryAllocatorDump* mad4 = pmd4.CreateAllocatorDump("foo");

  ASSERT_EQ(mad1->guid(), mad2->guid());

  ASSERT_NE(mad2->guid(), mad3->guid());
  ASSERT_NE(mad3->guid(), mad4->guid());
  ASSERT_NE(mad4->guid(), mad2->guid());

  ASSERT_EQ(mad1->guid(), pmd1.GetDumpId("foo"));
}

#if defined(COUNT_RESIDENT_BYTES_SUPPORTED)
#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/42050620): Counting resident bytes is not supported on
// Fuchsia.
#define MAYBE_CountResidentBytes DISABLED_CountResidentBytes
#else
#define MAYBE_CountResidentBytes CountResidentBytes
#endif
TEST(ProcessMemoryDumpTest, MAYBE_CountResidentBytes) {
  const size_t page_size = ProcessMemoryDump::GetSystemPageSize();

  // Allocate few page of dirty memory and check if it is resident.
  const size_t size1 = 5 * page_size;
  void* memory1 = Map(size1);
  memset(memory1, 0, size1);
  std::optional<size_t> res1 =
      ProcessMemoryDump::CountResidentBytes(memory1, size1);
  ASSERT_TRUE(res1.has_value());
  ASSERT_EQ(res1.value(), size1);
  Unmap(memory1, size1);

  // Allocate a large memory segment (> 8Mib).
  const size_t kVeryLargeMemorySize = 15 * 1024 * 1024;
  void* memory2 = Map(kVeryLargeMemorySize);
  memset(memory2, 0, kVeryLargeMemorySize);
  std::optional<size_t> res2 =
      ProcessMemoryDump::CountResidentBytes(memory2, kVeryLargeMemorySize);
  ASSERT_TRUE(res2.has_value());
  ASSERT_EQ(res2.value(), kVeryLargeMemorySize);
  Unmap(memory2, kVeryLargeMemorySize);
}

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/42050620): Counting resident bytes is not supported on
// Fuchsia.
#define MAYBE_CountResidentBytesInSharedMemory \
  DISABLED_CountResidentBytesInSharedMemory
#else
#define MAYBE_CountResidentBytesInSharedMemory CountResidentBytesInSharedMemory
#endif
TEST(ProcessMemoryDumpTest, MAYBE_CountResidentBytesInSharedMemory) {
  const size_t page_size = ProcessMemoryDump::GetSystemPageSize();

  // Allocate few page of dirty memory and check if it is resident.
  {
    const size_t kDirtyMemorySize = 5 * page_size;
    auto region = base::WritableSharedMemoryRegion::Create(kDirtyMemorySize);
    base::WritableSharedMemoryMapping mapping = region.Map();
    base::span<uint8_t> mapping_mem(mapping);
    std::ranges::fill(mapping_mem, 0u);
    std::optional<size_t> res1 = CountResidentBytesInSharedMemory(mapping);
    ASSERT_TRUE(res1.has_value());
    ASSERT_EQ(res1.value(), kDirtyMemorySize);
  }

  // Allocate a shared memory segment but map at a non-page-aligned offset.
  {
    const size_t kDirtyMemorySize = 5 * page_size;
    auto region =
        base::WritableSharedMemoryRegion::Create(kDirtyMemorySize + page_size);
    base::WritableSharedMemoryMapping mapping =
        region.MapAt(page_size / 2, kDirtyMemorySize);
    base::span<uint8_t> mapping_mem(mapping);
    std::ranges::fill(mapping_mem, 0u);
    std::optional<size_t> res1 = CountResidentBytesInSharedMemory(mapping);
    ASSERT_TRUE(res1.has_value());
    ASSERT_EQ(res1.value(), kDirtyMemorySize + page_size);
  }

  // Allocate a large memory segment (> 8Mib).
  {
    const size_t kVeryLargeMemorySize = 15 * 1024 * 1024;
    auto region =
        base::WritableSharedMemoryRegion::Create(kVeryLargeMemorySize);
    base::WritableSharedMemoryMapping mapping = region.Map();
    base::span<uint8_t> mapping_mem(mapping);
    std::ranges::fill(mapping_mem, 0u);
    std::optional<size_t> res2 = CountResidentBytesInSharedMemory(mapping);
    ASSERT_TRUE(res2.has_value());
    ASSERT_EQ(res2.value(), kVeryLargeMemorySize);
  }

  // Allocate a large memory segment, but touch about half of all pages.
  {
    const size_t kTouchedMemorySize = 7 * 1024 * 1024;
    auto region = base::WritableSharedMemoryRegion::Create(kTouchedMemorySize);
    base::WritableSharedMemoryMapping mapping = region.Map();
    base::span<uint8_t> mapping_mem(mapping);
    std::ranges::fill(mapping_mem, 0u);
    std::optional<size_t> res3 = CountResidentBytesInSharedMemory(mapping);
    ASSERT_TRUE(res3.has_value());
    ASSERT_EQ(res3.value(), kTouchedMemorySize);
  }
}
#endif  // defined(COUNT_RESIDENT_BYTES_SUPPORTED)

}  // namespace base::trace_event
