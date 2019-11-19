// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/dependent_list.h"

#include <cstdint>
#include <limits>

#include "base/memory/scoped_refptr.h"
#include "base/task/promise/abstract_promise.h"
#include "base/test/do_nothing_promise.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

class PushBackVisitor : public DependentList::Visitor {
 public:
  void Visit(scoped_refptr<AbstractPromise> dependent) override {
    dependents_.push_back(dependent.get());
  }

  const std::vector<AbstractPromise*> visited_dependents() const {
    return dependents_;
  }

 private:
  std::vector<AbstractPromise*> dependents_;
};

class FailTestVisitor : public DependentList::Visitor {
 public:
  void Visit(scoped_refptr<AbstractPromise> dependent) override {
    ADD_FAILURE();
  }
};

}  // namespace

using ::testing::ElementsAre;

TEST(DependentList, ConstructUnresolved) {
  DependentList list(DependentList::ConstructUnresolved{});
  DependentList::Node node;
  EXPECT_EQ(DependentList::InsertResult::SUCCESS, list.Insert(&node));
  EXPECT_FALSE(list.IsRejectedForTesting());
  EXPECT_FALSE(list.IsCanceled());
  EXPECT_FALSE(list.IsResolvedForTesting());
  EXPECT_FALSE(list.IsSettled());
}

TEST(DependentList, ConstructResolved) {
  DependentList list(DependentList::ConstructResolved{});
  DependentList::Node node;
  EXPECT_EQ(DependentList::InsertResult::FAIL_PROMISE_RESOLVED,
            list.Insert(&node));
  EXPECT_TRUE(list.IsResolved());
  EXPECT_FALSE(list.IsRejected());
  EXPECT_FALSE(list.IsCanceled());
  EXPECT_TRUE(list.IsSettled());
}

TEST(DependentList, ConstructRejected) {
  DependentList list(DependentList::ConstructRejected{});
  DependentList::Node node;
  EXPECT_EQ(DependentList::InsertResult::FAIL_PROMISE_REJECTED,
            list.Insert(&node));
  EXPECT_TRUE(list.IsRejected());
  EXPECT_FALSE(list.IsCanceled());
  EXPECT_FALSE(list.IsResolved());
  EXPECT_TRUE(list.IsSettled());
}

TEST(DependentList, ResolveAndConsumeAllDependents) {
  DependentList list(DependentList::ConstructUnresolved{});
  DependentList::Node node1;
  DependentList::Node node2;
  DependentList::Node node3;
  EXPECT_EQ(DependentList::InsertResult::SUCCESS, list.Insert(&node1));
  EXPECT_EQ(DependentList::InsertResult::SUCCESS, list.Insert(&node2));
  EXPECT_EQ(DependentList::InsertResult::SUCCESS, list.Insert(&node3));

  EXPECT_FALSE(list.IsResolvedForTesting());
  EXPECT_FALSE(list.IsSettled());

  std::vector<AbstractPromise*> expected_dependants = {node1.dependent().get(),
                                                       node2.dependent().get(),
                                                       node3.dependent().get()};

  PushBackVisitor visitor;
  list.ResolveAndConsumeAllDependents(&visitor);
  EXPECT_TRUE(list.IsResolved());
  EXPECT_TRUE(list.IsSettled());

  EXPECT_EQ(expected_dependants, visitor.visited_dependents());

  // Can't insert any more nodes.
  DependentList::Node node4;
  EXPECT_EQ(DependentList::InsertResult::FAIL_PROMISE_RESOLVED,
            list.Insert(&node4));
}

TEST(DependentList, RejectAndConsumeAllDependents) {
  DependentList list(DependentList::ConstructUnresolved{});
  DependentList::Node node1;
  DependentList::Node node2;
  DependentList::Node node3;
  EXPECT_EQ(DependentList::InsertResult::SUCCESS, list.Insert(&node1));
  EXPECT_EQ(DependentList::InsertResult::SUCCESS, list.Insert(&node2));
  EXPECT_EQ(DependentList::InsertResult::SUCCESS, list.Insert(&node3));

  EXPECT_FALSE(list.IsResolvedForTesting());
  EXPECT_FALSE(list.IsSettled());
  std::vector<AbstractPromise*> expected_dependants = {node1.dependent().get(),
                                                       node2.dependent().get(),
                                                       node3.dependent().get()};

  PushBackVisitor visitor;
  list.RejectAndConsumeAllDependents(&visitor);
  EXPECT_TRUE(list.IsRejected());
  EXPECT_TRUE(list.IsSettled());

  EXPECT_EQ(expected_dependants, visitor.visited_dependents());

  // Can't insert any more nodes.
  DependentList::Node node4;
  EXPECT_EQ(DependentList::InsertResult::FAIL_PROMISE_REJECTED,
            list.Insert(&node4));
}

TEST(DependentList, CancelAndConsumeAllDependents) {
  DependentList list(DependentList::ConstructUnresolved{});
  DependentList::Node node1;
  DependentList::Node node2;
  DependentList::Node node3;
  EXPECT_EQ(DependentList::InsertResult::SUCCESS, list.Insert(&node1));
  EXPECT_EQ(DependentList::InsertResult::SUCCESS, list.Insert(&node2));
  EXPECT_EQ(DependentList::InsertResult::SUCCESS, list.Insert(&node3));

  EXPECT_FALSE(list.IsResolvedForTesting());
  EXPECT_FALSE(list.IsSettled());
  std::vector<AbstractPromise*> expected_dependants = {node1.dependent().get(),
                                                       node2.dependent().get(),
                                                       node3.dependent().get()};

  PushBackVisitor visitor;
  EXPECT_TRUE(list.CancelAndConsumeAllDependents(&visitor));
  EXPECT_TRUE(list.IsCanceled());
  EXPECT_TRUE(list.IsSettled());

  EXPECT_EQ(expected_dependants, visitor.visited_dependents());

  // Can't insert any more nodes.
  DependentList::Node node4;
  EXPECT_EQ(DependentList::InsertResult::FAIL_PROMISE_CANCELED,
            list.Insert(&node4));
}

TEST(DependentList, CancelAndConsumeAllDependentsFailsIfAlreadySettled) {
  DependentList list(DependentList::ConstructUnresolved{});

  FailTestVisitor visitor;
  list.ResolveAndConsumeAllDependents(&visitor);

  EXPECT_FALSE(list.CancelAndConsumeAllDependents(&visitor));

  EXPECT_FALSE(list.IsCanceled());
  EXPECT_TRUE(list.IsResolved());
}

TEST(DependentList, NextPowerOfTwo) {
  static_assert(NextPowerOfTwo(0) == 1u, "");
  static_assert(NextPowerOfTwo(1) == 2u, "");
  static_assert(NextPowerOfTwo(2) == 4u, "");
  static_assert(NextPowerOfTwo(3) == 4u, "");
  static_assert(NextPowerOfTwo(4) == 8u, "");
  static_assert(NextPowerOfTwo((1ull << 21) + (1ull << 19)) == 1ull << 22, "");
  static_assert(NextPowerOfTwo(std::numeric_limits<uintptr_t>::max() >> 1) ==
                    1ull << (sizeof(uintptr_t) * 8 - 1),
                "");
  static_assert(NextPowerOfTwo(std::numeric_limits<uintptr_t>::max()) == 0u,
                "");
}

TEST(DependentListNode, Simple) {
  DependentList::Node node;
  EXPECT_EQ(nullptr, node.prerequisite());

  scoped_refptr<AbstractPromise> p = DoNothingPromiseBuilder(FROM_HERE);
  EXPECT_TRUE(p->HasOneRef());
  node.SetPrerequisite(p.get());
  EXPECT_EQ(p.get(), node.prerequisite());
  EXPECT_TRUE(p->HasOneRef());

  EXPECT_TRUE(p->HasOneRef());
  node.RetainSettledPrerequisite();
  EXPECT_EQ(p.get(), node.prerequisite());
  EXPECT_FALSE(p->HasOneRef());

  node.ClearPrerequisite();
  EXPECT_EQ(nullptr, node.prerequisite());
  EXPECT_TRUE(p->HasOneRef());
}

}  // namespace internal
}  // namespace base
