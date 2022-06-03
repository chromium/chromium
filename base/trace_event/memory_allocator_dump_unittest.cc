// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_dump.h"

#include <stdint.h>

#include "base/format_macros.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Eq;
using testing::ByRef;
using testing::IsEmpty;

namespace base {
namespace trace_event {

namespace {

class FakeMemoryAllocatorDumpProvider : public MemoryDumpProvider {
 public:
  bool OnMemoryDump(const MemoryDumpArgs& args,
                    ProcessMemoryDump* pmd) override {
    MemoryAllocatorDump* root_heap =
        pmd->CreateAllocatorDump("foobar_allocator");

    root_heap->AddScalar(MemoryAllocatorDump::kNameSize,
                         MemoryAllocatorDump::kUnitsBytes, 4096);
    root_heap->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                         MemoryAllocatorDump::kUnitsObjects, 42);
    root_heap->AddScalar("attr1", "units1", 1234);
    root_heap->AddString("attr2", "units2", "string_value");

    MemoryAllocatorDump* sub_heap =
        pmd->CreateAllocatorDump("foobar_allocator/sub_heap");
    sub_heap->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes, 1);
    sub_heap->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                        MemoryAllocatorDump::kUnitsObjects, 3);

    pmd->CreateAllocatorDump("foobar_allocator/sub_heap/empty");
    // Leave the rest of sub heap deliberately uninitialized, to check that
    // CreateAllocatorDump returns a properly zero-initialized object.

    return true;
  }
};

void CheckString(const MemoryAllocatorDump* dump,
                 const std::string& name,
                 const char* expected_units,
                 const std::string& expected_value) {
  MemoryAllocatorDump::Entry expected(name, expected_units, expected_value);
  EXPECT_THAT(dump->entries(), testing::Contains(Eq(ByRef(expected))));
}

void CheckScalar(const MemoryAllocatorDump* dump,
                 const std::string& name,
                 const char* expected_units,
                 uint64_t expected_value) {
  MemoryAllocatorDump::Entry expected(name, expected_units, expected_value);
  EXPECT_THAT(dump->entries(), testing::Contains(Eq(ByRef(expected))));
}

}  // namespace

TEST(MemoryAllocatorDumpTest, GuidGeneration) {
  std::unique_ptr<MemoryAllocatorDump> mad(new MemoryAllocatorDump(
      "foo", MemoryDumpLevelOfDetail::FIRST, MemoryAllocatorDumpGuid(0x42u)));
  ASSERT_EQ("42", mad->guid().ToString());
}

TEST(MemoryAllocatorDumpTest, DumpIntoProcessMemoryDump) {
  FakeMemoryAllocatorDumpProvider fmadp;
  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::DETAILED};
  ProcessMemoryDump pmd(dump_args);

  fmadp.OnMemoryDump(dump_args, &pmd);

  ASSERT_EQ(3u, pmd.allocator_dumps().size());

  const MemoryAllocatorDump* root_heap =
      pmd.GetAllocatorDump("foobar_allocator");
  ASSERT_NE(nullptr, root_heap);
  EXPECT_EQ("foobar_allocator", root_heap->absolute_name());
  CheckScalar(root_heap, MemoryAllocatorDump::kNameSize,
              MemoryAllocatorDump::kUnitsBytes, 4096);
  CheckScalar(root_heap, MemoryAllocatorDump::kNameObjectCount,
              MemoryAllocatorDump::kUnitsObjects, 42);
  CheckScalar(root_heap, "attr1", "units1", 1234);
  CheckString(root_heap, "attr2", "units2", "string_value");

  const MemoryAllocatorDump* sub_heap =
      pmd.GetAllocatorDump("foobar_allocator/sub_heap");
  ASSERT_NE(nullptr, sub_heap);
  EXPECT_EQ("foobar_allocator/sub_heap", sub_heap->absolute_name());
  CheckScalar(sub_heap, MemoryAllocatorDump::kNameSize,
              MemoryAllocatorDump::kUnitsBytes, 1);
  CheckScalar(sub_heap, MemoryAllocatorDump::kNameObjectCount,
              MemoryAllocatorDump::kUnitsObjects, 3);
  const MemoryAllocatorDump* empty_sub_heap =
      pmd.GetAllocatorDump("foobar_allocator/sub_heap/empty");
  ASSERT_NE(nullptr, empty_sub_heap);
  EXPECT_EQ("foobar_allocator/sub_heap/empty", empty_sub_heap->absolute_name());

  EXPECT_THAT(empty_sub_heap->entries(), IsEmpty());

  // Check that calling serialization routines doesn't cause a crash.
  std::unique_ptr<TracedValue> traced_value(new TracedValue);
  pmd.SerializeAllocatorDumpsInto(traced_value.get());
}

TEST(MemoryAllocatorDumpTest, GetSize) {
  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::DETAILED};
  ProcessMemoryDump pmd(dump_args);
  MemoryAllocatorDump* dump = pmd.CreateAllocatorDump("allocator_for_size");
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, 1);
  dump->AddScalar("foo", MemoryAllocatorDump::kUnitsBytes, 2);
  EXPECT_EQ(1u, dump->GetSizeInternal());
}

TEST(MemoryAllocatorDumpTest, ReadValues) {
  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::DETAILED};
  ProcessMemoryDump pmd(dump_args);
  MemoryAllocatorDump* dump = pmd.CreateAllocatorDump("allocator_for_size");
  dump->AddScalar("one", "byte", 1);
  dump->AddString("one", "object", "one");

  MemoryAllocatorDump::Entry expected_scalar("one", "byte", 1);
  MemoryAllocatorDump::Entry expected_string("one", "object", "one");
  EXPECT_THAT(dump->entries(), ElementsAre(Eq(ByRef(expected_scalar)),
                                           Eq(ByRef(expected_string))));
}

TEST(MemoryAllocatorDumpTest, MovingAnEntry) {
  MemoryAllocatorDump::Entry expected_entry("one", "byte", 1);
  MemoryAllocatorDump::Entry from_entry("one", "byte", 1);
  MemoryAllocatorDump::Entry to_entry = std::move(from_entry);
  EXPECT_EQ(expected_entry, to_entry);
}

// DEATH tests are not supported in Android/iOS/Fuchsia.
#if !defined(NDEBUG) && !defined(OS_ANDROID) && !defined(OS_IOS) && \
    !defined(OS_FUCHSIA)
TEST(MemoryAllocatorDumpTest, ForbidDuplicatesDeathTest) {
  FakeMemoryAllocatorDumpProvider fmadp;
  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::DETAILED};
  ProcessMemoryDump pmd(dump_args);
  pmd.CreateAllocatorDump("foo_allocator");
  pmd.CreateAllocatorDump("bar_allocator/heap");
  ASSERT_DEATH(pmd.CreateAllocatorDump("foo_allocator"), "");
  ASSERT_DEATH(pmd.CreateAllocatorDump("bar_allocator/heap"), "");
  ASSERT_DEATH(pmd.CreateAllocatorDump(""), "");
}

TEST(MemoryAllocatorDumpTest, ForbidStringsInBackgroundModeDeathTest) {
  MemoryDumpArgs dump_args = {MemoryDumpLevelOfDetail::BACKGROUND};
  ProcessMemoryDump pmd(dump_args);
  MemoryAllocatorDump* dump = pmd.CreateAllocatorDump("malloc");
  ASSERT_DEATH(dump->AddString("foo", "bar", "baz"), "");
}
#endif

}  // namespace trace_event
}  // namespace base
