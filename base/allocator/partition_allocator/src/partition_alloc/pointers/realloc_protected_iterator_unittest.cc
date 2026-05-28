// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/pointers/realloc_protected_iterator.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <string>
#include <vector>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#include "partition_alloc/address_pool_manager_bitmap.h"
#include "partition_alloc/partition_address_space.h"
#endif

namespace base {

using IntIter = ReallocProtectedIterator<std::vector<int>::iterator>;

// --- Hot path forwarding ---------------------------------------------------

TEST(ReallocProtectedIteratorTest, DerefAndAdvance) {
  std::vector<int> v = {10, 20, 30, 40, 50};
  auto begin = realloc_protected_begin(v);
  auto end = realloc_protected_end(v);

  EXPECT_EQ(*begin, 10);
  EXPECT_EQ(begin[2], 30);
  ++begin;
  EXPECT_EQ(*begin, 20);
  begin += 2;
  EXPECT_EQ(*begin, 40);
  --begin;
  EXPECT_EQ(*begin, 30);

  EXPECT_EQ(end - realloc_protected_begin(v), 5);
}

TEST(ReallocProtectedIteratorTest, ComparisonOperators) {
  std::vector<int> v = {1, 2, 3};
  auto a = realloc_protected_begin(v);
  auto b = realloc_protected_begin(v);
  auto e = realloc_protected_end(v);

  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
  EXPECT_TRUE(a < e);
  EXPECT_TRUE(e > a);
  EXPECT_TRUE(a <= b);
  EXPECT_TRUE(a >= b);
}

TEST(ReallocProtectedIteratorTest, RangeForLoop) {
  std::vector<int> v = {1, 2, 3, 4, 5};
  int sum = 0;
  for (int x : realloc_protected_range(v)) {
    sum += x;
  }
  EXPECT_EQ(sum, 15);
}

TEST(ReallocProtectedIteratorTest, MutationThroughIterator) {
  std::vector<int> v = {0, 0, 0};
  for (int& x : realloc_protected_range(v)) {
    x = 7;
  }
  EXPECT_EQ(v, (std::vector<int>{7, 7, 7}));
}

TEST(ReallocProtectedIteratorTest, WorksWithStdAlgorithms) {
  std::vector<int> v = {5, 2, 8, 1, 9, 3};
  auto it = std::find(realloc_protected_begin(v), realloc_protected_end(v), 8);
  ASSERT_NE(it, realloc_protected_end(v));
  EXPECT_EQ(*it, 8);

  int total =
      std::accumulate(realloc_protected_begin(v), realloc_protected_end(v), 0);
  EXPECT_EQ(total, 28);
}

// --- Wrapper lifecycle / copy & move ---------------------------------------

TEST(ReallocProtectedIteratorTest, CopyAndMove) {
  std::vector<int> v = {11, 22, 33};

  IntIter a(v.begin());
  IntIter b(a);  // Copy.
  EXPECT_EQ(*b, 11);

  IntIter c(std::move(a));  // Move.
  EXPECT_EQ(*c, 11);

  IntIter d;
  d = b;  // Copy-assign.
  EXPECT_EQ(*d, 11);

  IntIter e;
  e = std::move(c);  // Move-assign.
  EXPECT_EQ(*e, 11);
}

TEST(ReallocProtectedIteratorTest, DefaultConstructed) {
  IntIter it;  // Doesn't crash on destruction.
  IntIter it2;
  EXPECT_TRUE(it == it2);
}

TEST(ReallocProtectedIteratorTest, SelfAssignment) {
  std::vector<int> v = {1};
  IntIter it(v.begin());
  // Alias prevents -Wself-assign-overloaded / -Wself-move while still
  // exercising the same code path.
  IntIter& alias = it;
  it = alias;  // Self copy-assign should not double-release.
  EXPECT_EQ(*it, 1);

  it = std::move(alias);  // Self move-assign.
  EXPECT_EQ(*it, 1);
}

// --- Non-BRP backings should be no-ops -------------------------------------

TEST(ReallocProtectedIteratorTest, StackArrayIsNoOp) {
  // Stack-allocated container's backing is not in any BRP pool. The wrapper
  // must detect this and degrade to a no-op (no crash, no spurious work).
  std::array<int, 4> arr = {1, 2, 3, 4};
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  EXPECT_FALSE(partition_alloc::IsManagedByPartitionAllocBRPPool(
      reinterpret_cast<uintptr_t>(arr.data())));
#endif
  int sum = 0;
  for (int x : realloc_protected_range(arr)) {
    sum += x;
  }
  EXPECT_EQ(sum, 10);
}

// --- Quarantine behaviour --------------------------------------------------

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
namespace {

// True iff `std::vector`'s backing for this build lands in PA's BRP pool.
// On PA-Everywhere + BRP builds this is yes; otherwise the wrapper is a no-op
// and the quarantine/CHECK behaviour cannot be tested.
bool VectorBackingIsInBrpPool() {
  std::vector<uint64_t> probe(1);
  return partition_alloc::IsManagedByPartitionAllocBRPPool(
      reinterpret_cast<uintptr_t>(probe.data()));
}

}  // namespace

// When a wrapped iterator is alive and the container reallocates, the old
// backing's slot is freed but kept alive by BRP quarantine. At wrapper
// destruction `UnwrapBackingSlot` notices the slot's "allocated" bit is
// clear and CHECK-fails -- converting a quiet UAF into a fail-fast crash.
TEST(ReallocProtectedIteratorDeathTest,
     ReallocDuringIterationCrashesAtDestruction) {
  if (!VectorBackingIsInBrpPool()) {
    GTEST_SKIP() << "std::vector backing is not in the BRP pool in this build";
  }
  PA_EXPECT_CHECK_DEATH({
    std::vector<uint64_t> v;
    v.reserve(8);
    for (uint64_t i = 0; i < 8; ++i) {
      v.push_back(0xAAAAAAAAAAAAAAAAULL + i);
    }
    auto wrapped = realloc_protected_begin(v);
    // Force a reallocation. Old backing is freed, but the wrapper's BRP
    // ref keeps the slot alive (zapped, quarantined).
    v.reserve(1024);
    // At end of this block `wrapped` destructs. UnwrapBackingSlot sees the
    // slot is no longer marked alive and PA_BASE_CHECKs.
  });
}

#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

// --- Other contiguous-iterator containers ----------------------------------
//
// The wrapper is templated on any contiguous iterator, so containers that
// expose one are protected without code changes. These tests verify the
// forwarding works (always) and the realloc-CHECK fires (when the backing
// lives in PA's BRP pool).

TEST(ReallocProtectedIteratorTest, StringForwarding) {
  std::string s = "hello";
  std::string out;
  for (char c : realloc_protected_range(s)) {
    out.push_back(c);
  }
  EXPECT_EQ(out, "hello");
}

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
// std::string: realloc on heap-grown string triggers the CHECK.
TEST(ReallocProtectedIteratorDeathTest, StringReallocCrashes) {
  // Use a string large enough to definitely be on the heap (past SSO).
  std::string seed(64, 'a');
  std::string probe = seed;
  if (!partition_alloc::IsManagedByPartitionAllocBRPPool(
          reinterpret_cast<uintptr_t>(probe.data()))) {
    GTEST_SKIP() << "std::string backing is not in the BRP pool in this build";
  }
  PA_EXPECT_CHECK_DEATH({
    std::string s(64, 'a');
    s.reserve(64);
    auto wrapped = realloc_protected_begin(s);
    s.reserve(8192);  // realloc, frees old heap backing
  });
}

#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

// --- Static type properties ------------------------------------------------

TEST(ReallocProtectedIteratorTest, ContiguousIteratorConcept) {
  static_assert(std::contiguous_iterator<IntIter>);
  static_assert(std::random_access_iterator<IntIter>);
  static_assert(std::is_same_v<std::iterator_traits<IntIter>::value_type, int>);
}

}  // namespace base
