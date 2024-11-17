// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/atomic_flag_set.h"

#include <set>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;
using testing::IsNull;
using testing::NotNull;

namespace base {
namespace sequence_manager {
namespace internal {

class AtomicFlagSetForTest : public AtomicFlagSet {
 public:
  explicit AtomicFlagSetForTest(
      scoped_refptr<AssociatedThreadId> associated_thread)
      : AtomicFlagSet(std::move(associated_thread)) {}

  using AtomicFlagSet::GetAllocListForTesting;
  using AtomicFlagSet::GetPartiallyFreeListForTesting;
  using AtomicFlagSet::Group;
};

class AtomicFlagSetTest : public testing::Test {
 public:
  void CreateFlags(size_t number_of_flags,
                   RepeatingCallback<void(size_t index)> callback) {
    atomic_flags_.reserve(number_of_flags);
    for (size_t i = 0; i < number_of_flags; i++) {
      atomic_flags_.push_back(atomic_flag_set_.AddFlag(
          base::BindRepeating([](RepeatingCallback<void(size_t index)> callback,
                                 size_t i) { callback.Run(i); },
                              callback, i)));
    }
  }

  AtomicFlagSetForTest atomic_flag_set_{AssociatedThreadId::CreateBound()};
  std::vector<AtomicFlagSet::AtomicFlag> atomic_flags_;
};

TEST_F(AtomicFlagSetTest, VisitEmptyAtomicFlagSet) {
  atomic_flag_set_.RunActiveCallbacks();  // Shouldn't crash.
}

TEST_F(AtomicFlagSetTest, SetActiveOneFlag) {
  std::vector<size_t> flags_visited;

  CreateFlags(3, BindLambdaForTesting(
                     [&](size_t index) { flags_visited.push_back(index); }));

  atomic_flags_[1].SetActive(true);
  EXPECT_TRUE(flags_visited.empty());

  atomic_flag_set_.RunActiveCallbacks();
  EXPECT_THAT(flags_visited, ElementsAre(1));

  // A subsequent call to RunActiveCallbacks should not visit anything.
  flags_visited.clear();
  atomic_flag_set_.RunActiveCallbacks();
  EXPECT_TRUE(flags_visited.empty());
}

TEST_F(AtomicFlagSetTest, SetActiveManyFlags) {
  constexpr size_t num_flags = 1000;
  std::set<size_t> flags_visited;

  CreateFlags(num_flags, BindLambdaForTesting([&](size_t index) {
                flags_visited.insert(index);
              }));

  // Set active all even numbered flags.
  for (size_t i = 0; i < num_flags; i += 2) {
    atomic_flags_[i].SetActive(true);
  }

  atomic_flag_set_.RunActiveCallbacks();

  ASSERT_EQ(flags_visited.size(), num_flags / 2);
  for (size_t i = 0; i < flags_visited.size(); i++) {
    ASSERT_EQ(flags_visited.count(i * 2), 1u);
  }
}

TEST_F(AtomicFlagSetTest, SetActiveFalse) {
  std::vector<size_t> flags_visited;

  CreateFlags(3, BindLambdaForTesting(
                     [&](size_t index) { flags_visited.push_back(index); }));

  atomic_flags_[1].SetActive(true);
  atomic_flags_[1].SetActive(false);

  atomic_flag_set_.RunActiveCallbacks();
  EXPECT_TRUE(flags_visited.empty());
}

TEST_F(AtomicFlagSetTest, ReleaseAtomicFlag) {
  constexpr size_t num_flags = 1000;
  constexpr size_t half_num_flags = num_flags / 2;
  std::set<size_t> flags_visited;

  CreateFlags(num_flags, BindLambdaForTesting([&](size_t index) {
                flags_visited.insert(index);
              }));

  // Set active all flags.
  for (size_t i = 0; i < num_flags; i++) {
    atomic_flags_[i].SetActive(true);
  }

  // Release half the AtomicFlags.
  for (size_t i = 0; i < half_num_flags; i++) {
    atomic_flags_[i].ReleaseAtomicFlag();
  }

  atomic_flag_set_.RunActiveCallbacks();

  // We should only have visited the unreleased half.
  ASSERT_EQ(flags_visited.size(), half_num_flags);
  for (size_t i = 0; i < flags_visited.size(); i++) {
    ASSERT_EQ(flags_visited.count(i + half_num_flags), 1u);
  }
}

TEST_F(AtomicFlagSetTest, GroupBecomesFull) {
  CreateFlags(AtomicFlagSetForTest::Group::kNumFlags - 1,
              BindLambdaForTesting([](size_t index) {}));
  AtomicFlagSetForTest::Group* group1 =
      atomic_flag_set_.GetAllocListForTesting();
  EXPECT_THAT(group1->next.get(), IsNull());
  EXPECT_EQ(group1, atomic_flag_set_.GetPartiallyFreeListForTesting());
  EXPECT_FALSE(group1->IsFull());
  EXPECT_FALSE(group1->IsEmpty());

  // Add an extra flag to fill up the group.
  atomic_flags_.push_back(atomic_flag_set_.AddFlag(base::BindRepeating([] {})));

  EXPECT_TRUE(group1->IsFull());
  EXPECT_THAT(atomic_flag_set_.GetPartiallyFreeListForTesting(), IsNull());
}

TEST_F(AtomicFlagSetTest, GroupBecomesEmptyOnlyEntryInPartiallyFreeList) {
  CreateFlags(AtomicFlagSetForTest::Group::kNumFlags + 1,
              BindLambdaForTesting([](size_t index) {}));

  AtomicFlagSetForTest::Group* group1 =
      atomic_flag_set_.GetAllocListForTesting();
  ASSERT_THAT(group1, NotNull());
  EXPECT_FALSE(group1->IsFull());
  EXPECT_FALSE(group1->IsEmpty());
  EXPECT_EQ(group1, atomic_flag_set_.GetPartiallyFreeListForTesting());
  AtomicFlagSetForTest::Group* group2 = group1->next.get();
  ASSERT_THAT(group2, NotNull());
  EXPECT_THAT(group2->next.get(), IsNull());
  EXPECT_TRUE(group2->IsFull());

  // This will release |group1|.
  atomic_flags_[AtomicFlagSetForTest::Group::kNumFlags].ReleaseAtomicFlag();

  EXPECT_THAT(group2->next.get(), IsNull());
  EXPECT_THAT(atomic_flag_set_.GetPartiallyFreeListForTesting(), IsNull());
}

TEST_F(AtomicFlagSetTest, GroupBecomesEmptyHeadOfPartiallyFreeList) {
  CreateFlags(AtomicFlagSetForTest::Group::kNumFlags * 2 + 1,
              BindLambdaForTesting([](size_t index) {}));

  AtomicFlagSetForTest::Group* group1 =
      atomic_flag_set_.GetAllocListForTesting();
  ASSERT_THAT(group1, NotNull());
  EXPECT_FALSE(group1->IsFull());
  EXPECT_FALSE(group1->IsEmpty());
  AtomicFlagSetForTest::Group* group2 = group1->next.get();
  ASSERT_THAT(group2, NotNull());
  EXPECT_TRUE(group2->IsFull());
  AtomicFlagSetForTest::Group* group3 = group2->next.get();
  ASSERT_THAT(group3, NotNull());
  EXPECT_THAT(group3->next.get(), IsNull());
  EXPECT_TRUE(group3->IsFull());

  // |group1| is on the head of the partially free list, now add groups 2 and 3.
  atomic_flags_[AtomicFlagSetForTest::Group::kNumFlags].ReleaseAtomicFlag();
  EXPECT_FALSE(group2->IsFull());
  atomic_flags_[0].ReleaseAtomicFlag();
  EXPECT_FALSE(group3->IsFull());

  EXPECT_EQ(group3, atomic_flag_set_.GetPartiallyFreeListForTesting());
  EXPECT_EQ(group3->partially_free_list_prev, nullptr);
  EXPECT_EQ(group3->partially_free_list_next, group2);
  EXPECT_EQ(group2->partially_free_list_prev, group3);
  EXPECT_EQ(group2->partially_free_list_next, group1);
  EXPECT_EQ(group1->partially_free_list_prev, group2);
  EXPECT_EQ(group1->partially_free_list_next, nullptr);
  EXPECT_EQ(group1->prev, nullptr);
  EXPECT_EQ(group2->prev, group1);
  EXPECT_EQ(group3->prev, group2);

  // This will release |group3|.
  for (size_t i = 0; i < AtomicFlagSetForTest::Group::kNumFlags; i++) {
    atomic_flags_[i].ReleaseAtomicFlag();
  }
  EXPECT_EQ(group2, atomic_flag_set_.GetPartiallyFreeListForTesting());
  EXPECT_EQ(group2->partially_free_list_prev, nullptr);
  EXPECT_EQ(group2->partially_free_list_next, group1);
  EXPECT_EQ(group1->partially_free_list_prev, group2);
  EXPECT_EQ(group1->partially_free_list_next, nullptr);
  EXPECT_EQ(group1, atomic_flag_set_.GetAllocListForTesting());
  EXPECT_EQ(group1->next.get(), group2);
  EXPECT_EQ(group1->prev, nullptr);
  EXPECT_EQ(group2->prev, group1);
  EXPECT_EQ(group2->next.get(), nullptr);
}

TEST_F(AtomicFlagSetTest, GroupBecomesEmptyMiddleOfPartiallyFreeList) {
  CreateFlags(AtomicFlagSetForTest::Group::kNumFlags * 2 + 1,
              BindLambdaForTesting([](size_t index) {}));

  AtomicFlagSetForTest::Group* group1 =
      atomic_flag_set_.GetAllocListForTesting();
  ASSERT_THAT(group1, NotNull());
  EXPECT_FALSE(group1->IsFull());
  EXPECT_FALSE(group1->IsEmpty());
  AtomicFlagSetForTest::Group* group2 = group1->next.get();
  ASSERT_THAT(group2, NotNull());
  EXPECT_TRUE(group2->IsFull());
  AtomicFlagSetForTest::Group* group3 = group2->next.get();
  ASSERT_THAT(group3, NotNull());
  EXPECT_THAT(group3->next.get(), IsNull());
  EXPECT_TRUE(group3->IsFull());

  // |group1| is on the head of the partially free list, now add groups 2 and 3.
  atomic_flags_[AtomicFlagSetForTest::Group::kNumFlags].ReleaseAtomicFlag();
  EXPECT_FALSE(group2->IsFull());
  atomic_flags_[0].ReleaseAtomicFlag();
  EXPECT_FALSE(group3->IsFull());

  EXPECT_EQ(group3, atomic_flag_set_.GetPartiallyFreeListForTesting());
  EXPECT_EQ(group3->partially_free_list_prev, nullptr);
  EXPECT_EQ(group3->partially_free_list_next, group2);
  EXPECT_EQ(group2->partially_free_list_prev, group3);
  EXPECT_EQ(group2->partially_free_list_next, group1);
  EXPECT_EQ(group1->partially_free_list_prev, group2);
  EXPECT_EQ(group1->partially_free_list_next, nullptr);
  EXPECT_EQ(group1->prev, nullptr);
  EXPECT_EQ(group2->prev, group1);
  EXPECT_EQ(group3->prev, group2);

  // This will release |group2|.
  for (size_t i = AtomicFlagSetForTest::Group::kNumFlags;
       i < AtomicFlagSetForTest::Group::kNumFlags * 2; i++) {
    atomic_flags_[i].ReleaseAtomicFlag();
  }
  EXPECT_EQ(group3, atomic_flag_set_.GetPartiallyFreeListForTesting());
  EXPECT_EQ(group3->partially_free_list_prev, nullptr);
  EXPECT_EQ(group3->partially_free_list_next, group1);
  EXPECT_EQ(group1->partially_free_list_prev, group3);
  EXPECT_EQ(group1->partially_free_list_next, nullptr);
  EXPECT_EQ(group1, atomic_flag_set_.GetAllocListForTesting());
  EXPECT_EQ(group1->prev, nullptr);
  EXPECT_EQ(group1->next.get(), group3);
  EXPECT_EQ(group3->prev, group1);
  EXPECT_EQ(group3->next.get(), nullptr);
}

TEST_F(AtomicFlagSetTest, GroupBecomesEmptyTailOfPartiallyFreeList) {
  CreateFlags(AtomicFlagSetForTest::Group::kNumFlags * 2 + 1,
              BindLambdaForTesting([](size_t index) {}));

  AtomicFlagSetForTest::Group* group1 =
      atomic_flag_set_.GetAllocListForTesting();
  ASSERT_THAT(group1, NotNull());
  EXPECT_FALSE(group1->IsFull());
  EXPECT_FALSE(group1->IsEmpty());
  AtomicFlagSetForTest::Group* group2 = group1->next.get();
  ASSERT_THAT(group2, NotNull());
  EXPECT_TRUE(group2->IsFull());
  AtomicFlagSetForTest::Group* group3 = group2->next.get();
  ASSERT_THAT(group3, NotNull());
  EXPECT_THAT(group3->next.get(), IsNull());
  EXPECT_TRUE(group3->IsFull());

  // |group1| is on the head of the partially free list, now add groups 2 and 3.
  atomic_flags_[AtomicFlagSetForTest::Group::kNumFlags].ReleaseAtomicFlag();
  EXPECT_FALSE(group2->IsFull());
  atomic_flags_[0].ReleaseAtomicFlag();
  EXPECT_FALSE(group3->IsFull());

  EXPECT_EQ(group3, atomic_flag_set_.GetPartiallyFreeListForTesting());
  EXPECT_EQ(group3->partially_free_list_prev, nullptr);
  EXPECT_EQ(group3->partially_free_list_next, group2);
  EXPECT_EQ(group2->partially_free_list_prev, group3);
  EXPECT_EQ(group2->partially_free_list_next, group1);
  EXPECT_EQ(group1->partially_free_list_prev, group2);
  EXPECT_EQ(group1->partially_free_list_next, nullptr);
  EXPECT_EQ(group1->prev, nullptr);
  EXPECT_EQ(group2->prev, group1);
  EXPECT_EQ(group3->prev, group2);

  // This will release |group1|.
  atomic_flags_[AtomicFlagSetForTest::Group::kNumFlags * 2].ReleaseAtomicFlag();
  EXPECT_EQ(group3, atomic_flag_set_.GetPartiallyFreeListForTesting());
  EXPECT_EQ(group3->partially_free_list_prev, nullptr);
  EXPECT_EQ(group3->partially_free_list_next, group2);
  EXPECT_EQ(group2->partially_free_list_prev, group3);
  EXPECT_EQ(group2->partially_free_list_next, nullptr);
  EXPECT_EQ(group2, atomic_flag_set_.GetAllocListForTesting());
  EXPECT_EQ(group2->prev, nullptr);
  EXPECT_EQ(group2->next.get(), group3);
  EXPECT_EQ(group3->prev, group2);
  EXPECT_EQ(group3->next.get(), nullptr);
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
