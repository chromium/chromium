// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/linked_list.h"

#include <concepts>
#include <list>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class UnsafeNode : public UnsafeLinkNode<UnsafeNode> {
 public:
  explicit UnsafeNode(int id) : id_(id) {}
  UnsafeNode(UnsafeNode&&) = default;

  int id() const { return id_; }

 private:
  int id_;
};

class SafeNode : public SafeLinkNode<SafeNode> {
 public:
  explicit SafeNode(int id) : id_(id) {}
  SafeNode(SafeNode&&) = default;

  int id() const { return id_; }

 private:
  int id_;
};

using UnsafeList = UnsafeLinkedList<UnsafeNode>;
using SafeList = SafeLinkedList<SafeNode>;

// Ensure that simple/unsafe list types are trivially destructible.

static_assert(std::is_trivially_destructible_v<UnsafeNode>,
              "Node must be trivially-destructible.");

static_assert(std::is_trivially_destructible_v<UnsafeLinkedList<UnsafeNode>>,
              "List must be trivially-destructible.");

template <typename List>
void ExpectListContentsForDirection(List* list,
                                    base::span<const int> node_ids,
                                    bool forward) {
  size_t i = 0;
  for (auto* node = (forward ? list->head() : list->tail());
       node != list->end();
       node = (forward ? node->next() : node->previous())) {
    ASSERT_LT(i, node_ids.size());
    EXPECT_EQ(node_ids[forward ? i : node_ids.size() - i - 1],
              node->value()->id());
    ++i;
  }
  EXPECT_EQ(node_ids.size(), i);
}

template <typename List>
void ExpectListContents(List* list, base::span<const int> node_ids) {
  {
    SCOPED_TRACE("Iterating forward (from head to tail)");
    ExpectListContentsForDirection(list, node_ids, true);
  }
  {
    SCOPED_TRACE("Iterating backward (from tail to head)");
    ExpectListContentsForDirection(list, node_ids, false);
  }
}

template <typename List>
class LinkedListTest : public testing::Test {
 public:
  using Node = typename List::NodeType;

  void TearDown() override {
    if constexpr (std::same_as<List, SafeList>) {
      for (auto& node : nodes_) {
        if (node.next()) {
          node.RemoveFromList();
        }
      }
    }
  }

  List* CreateList() { return &lists_.emplace_back(); }

  List* MoveList(List* other) {
    return &lists_.emplace_back(std::move(*other));
  }

  Node* CreateNode(int id) { return &nodes_.emplace_back(id); }

  Node* MoveNode(Node* other) {
    return &nodes_.emplace_back(std::move(*other));
  }

 private:
  std::list<List> lists_;
  std::list<Node> nodes_;
};

struct LinkedListTestNameGenerator {
  template <typename T>
  static std::string GetName(int) {
    if constexpr (std::is_same_v<T, UnsafeList>) {
      return "Unsafe";
    }
    if constexpr (std::is_same_v<T, SafeList>) {
      return "Safe";
    }
  }
};

using TestTypes = testing::Types<UnsafeList, SafeList>;

TYPED_TEST_SUITE(LinkedListTest, TestTypes, LinkedListTestNameGenerator);

TYPED_TEST(LinkedListTest, Empty) {
  auto* list = this->CreateList();
  EXPECT_EQ(list->end(), list->head());
  EXPECT_EQ(list->end(), list->tail());
  ExpectListContents(list, {});
}

TYPED_TEST(LinkedListTest, Append) {
  auto* list = this->CreateList();
  ExpectListContents(list, {});

  auto* n1 = this->CreateNode(1);
  list->Append(n1);

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n1, list->tail());
  {
    const int expected[] = {1};
    ExpectListContents(list, expected);
  }

  auto* n2 = this->CreateNode(2);
  list->Append(n2);

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n2, list->tail());
  {
    const int expected[] = {1, 2};
    ExpectListContents(list, expected);
  }

  auto* n3 = this->CreateNode(3);
  list->Append(n3);

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n3, list->tail());
  {
    const int expected[] = {1, 2, 3};
    ExpectListContents(list, expected);
  }
}

TYPED_TEST(LinkedListTest, RemoveFromList) {
  auto* list = this->CreateList();

  auto* n1 = this->CreateNode(1);
  auto* n2 = this->CreateNode(2);
  auto* n3 = this->CreateNode(3);
  auto* n4 = this->CreateNode(4);
  auto* n5 = this->CreateNode(5);

  list->Append(n1);
  list->Append(n2);
  list->Append(n3);
  list->Append(n4);
  list->Append(n5);

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n5, list->tail());
  {
    const int expected[] = {1, 2, 3, 4, 5};
    ExpectListContents(list, expected);
  }

  // Remove from the middle.
  n3->RemoveFromList();

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n5, list->tail());
  {
    const int expected[] = {1, 2, 4, 5};
    ExpectListContents(list, expected);
  }

  // Remove from the tail.
  n5->RemoveFromList();

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n4, list->tail());
  {
    const int expected[] = {1, 2, 4};
    ExpectListContents(list, expected);
  }

  // Remove from the head.
  n1->RemoveFromList();

  EXPECT_EQ(n2, list->head());
  EXPECT_EQ(n4, list->tail());
  {
    const int expected[] = {2, 4};
    ExpectListContents(list, expected);
  }

  // Empty the list.
  n2->RemoveFromList();
  n4->RemoveFromList();

  ExpectListContents(list, {});
  EXPECT_EQ(list->end(), list->head());
  EXPECT_EQ(list->end(), list->tail());

  // Fill the list once again.
  list->Append(n1);
  list->Append(n2);
  list->Append(n3);
  list->Append(n4);
  list->Append(n5);

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n5, list->tail());
  {
    const int expected[] = {1, 2, 3, 4, 5};
    ExpectListContents(list, expected);
  }
}

TYPED_TEST(LinkedListTest, InsertBefore) {
  auto* list = this->CreateList();

  auto* n1 = this->CreateNode(1);
  auto* n2 = this->CreateNode(2);
  auto* n3 = this->CreateNode(3);
  auto* n4 = this->CreateNode(4);

  list->Append(n1);
  list->Append(n2);

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n2, list->tail());
  {
    const int expected[] = {1, 2};
    ExpectListContents(list, expected);
  }

  n3->InsertBefore(n2);

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n2, list->tail());
  {
    const int expected[] = {1, 3, 2};
    ExpectListContents(list, expected);
  }

  n4->InsertBefore(n1);

  EXPECT_EQ(n4, list->head());
  EXPECT_EQ(n2, list->tail());
  {
    const int expected[] = {4, 1, 3, 2};
    ExpectListContents(list, expected);
  }
}

TYPED_TEST(LinkedListTest, InsertAfter) {
  auto* list = this->CreateList();

  auto* n1 = this->CreateNode(1);
  auto* n2 = this->CreateNode(2);
  auto* n3 = this->CreateNode(3);
  auto* n4 = this->CreateNode(4);

  list->Append(n1);
  list->Append(n2);

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n2, list->tail());
  {
    const int expected[] = {1, 2};
    ExpectListContents(list, expected);
  }

  n3->InsertAfter(n2);

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n3, list->tail());
  {
    const int expected[] = {1, 2, 3};
    ExpectListContents(list, expected);
  }

  n4->InsertAfter(n1);

  EXPECT_EQ(n1, list->head());
  EXPECT_EQ(n3, list->tail());
  {
    const int expected[] = {1, 4, 2, 3};
    ExpectListContents(list, expected);
  }
}

TYPED_TEST(LinkedListTest, EmptyListIsEmpty) {
  auto* list = this->CreateList();
  EXPECT_TRUE(list->empty());
}

TYPED_TEST(LinkedListTest, NonEmptyListIsNotEmpty) {
  auto* list = this->CreateList();

  auto* n = this->CreateNode(1);
  list->Append(n);

  EXPECT_FALSE(list->empty());
}

TYPED_TEST(LinkedListTest, EmptiedListIsEmptyAgain) {
  auto* list = this->CreateList();

  auto* n = this->CreateNode(1);
  list->Append(n);
  n->RemoveFromList();

  EXPECT_TRUE(list->empty());
}

TYPED_TEST(LinkedListTest, NodesCanBeReused) {
  auto* list1 = this->CreateList();
  auto* list2 = this->CreateList();

  auto* n = this->CreateNode(1);
  list1->Append(n);
  n->RemoveFromList();
  list2->Append(n);

  EXPECT_EQ(list2->head(), n);
  EXPECT_TRUE(list1->empty());
}

TYPED_TEST(LinkedListTest, RemovedNodeHasNullNextPrevious) {
  auto* list = this->CreateList();

  auto* n = this->CreateNode(1);
  list->Append(n);
  n->RemoveFromList();

  EXPECT_EQ(nullptr, n->next());
  EXPECT_EQ(nullptr, n->previous());
}

TYPED_TEST(LinkedListTest, ChecksOnInsertBeforeWhenInList) {
  auto* list1 = this->CreateList();
  auto* list2 = this->CreateList();

  auto* n1 = this->CreateNode(1);
  auto* n2 = this->CreateNode(2);
  auto* n3 = this->CreateNode(3);

  list1->Append(n1);

  list2->Append(n2);
  list2->Append(n3);

  EXPECT_CHECK_DEATH(n1->InsertBefore(n3));
}

TYPED_TEST(LinkedListTest, ChecksOnInsertAfterWhenInList) {
  auto* list1 = this->CreateList();
  auto* list2 = this->CreateList();

  auto* n1 = this->CreateNode(1);
  auto* n2 = this->CreateNode(2);
  auto* n3 = this->CreateNode(3);

  list1->Append(n1);

  list2->Append(n2);
  list2->Append(n3);

  EXPECT_CHECK_DEATH(n1->InsertAfter(n2));
}

TYPED_TEST(LinkedListTest, NodeMoveConstructor) {
  auto* list = this->CreateList();

  auto* n1 = this->CreateNode(1);
  auto* n2 = this->CreateNode(2);
  auto* n3 = this->CreateNode(3);

  list->Append(n1);
  list->Append(n2);
  list->Append(n3);

  EXPECT_EQ(n1, n2->previous());
  EXPECT_EQ(n2, n1->next());
  EXPECT_EQ(n3, n2->next());
  EXPECT_EQ(n2, n3->previous());
  EXPECT_EQ(2, n2->id());

  auto* n2_new = this->MoveNode(n2);

  EXPECT_EQ(nullptr, n2->next());      // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(nullptr, n2->previous());  // NOLINT(bugprone-use-after-move)

  EXPECT_EQ(n1, n2_new->previous());
  EXPECT_EQ(n2_new, n1->next());
  EXPECT_EQ(n3, n2_new->next());
  EXPECT_EQ(n2_new, n3->previous());
  EXPECT_EQ(2, n2_new->id());
}

TYPED_TEST(LinkedListTest, LinkedListMoveConstructor) {
  // Moving list sizes 0 (head==end), 1 (head==tail), 2 (head!=tail) all stress
  // different cases. Also test size 3 in case it does something weird.
  for (size_t size = 0; size <= 3; ++size) {
    SCOPED_TRACE(StringPrintf("List size %zu", size));
    auto* original_list = this->CreateList();

    std::vector<decltype(this->CreateNode(0))> nodes;
    std::vector<int> expected_contents;
    nodes.reserve(size);
    for (int id = 0; id < size; ++id) {
      nodes.push_back(this->CreateNode(id));
      original_list->Append(nodes.back());
      expected_contents.push_back(id);
    }

    auto* new_list = this->MoveList(original_list);

    ExpectListContents(new_list, expected_contents);

    EXPECT_TRUE(original_list->empty());  // NOLINT(bugprone-use-after-move)
    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_EQ(original_list->head(), original_list->tail());
    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_EQ(original_list->tail(), original_list->end());
  }
}

TEST(SafeLinkedListTest, ClearRemovesNodesAndDoesNotCrash) {
  SafeLinkedList<SafeNode> list;
  SafeNode n1(1);
  SafeNode n2(2);
  list.Append(&n1);
  list.Append(&n2);
  list.clear();
  EXPECT_EQ(nullptr, n1.next());
  EXPECT_EQ(nullptr, n2.next());
  EXPECT_TRUE(list.empty());
}

TEST(SafeLinkedListTest, CanRemoveNodesSeparatelyWithoutCrashing) {
  SafeLinkedList<SafeNode> list;
  SafeNode n1(1);
  SafeNode n2(2);
  list.Append(&n1);
  list.Append(&n2);
  n1.RemoveFromList();
  n2.RemoveFromList();
  EXPECT_EQ(nullptr, n1.next());
  EXPECT_EQ(nullptr, n2.next());
  EXPECT_TRUE(list.empty());
}

TEST(SafeLinkedListTest, NodeCrashesIfNodeNotRemovedOnDestruction) {
  SafeLinkedList<SafeNode> list;
  auto n1 = std::make_unique<SafeNode>(1);
  list.Append(n1.get());
  EXPECT_CHECK_DEATH(n1.reset());

  // This prevents additional crashes after the reset fails.
  // (The pointer is not cleared out and will crash again on destruction.)
  list.clear();
}

TEST(SafeLinkedListTest, ListCrashesIfNodeNotRemovedOnDestruction) {
  auto list = std::make_unique<SafeLinkedList<SafeNode>>();
  SafeNode n1(1);
  list->Append(&n1);
  EXPECT_CHECK_DEATH(list.reset());

  // This prevents additional crashes after the reset fails.
  // (The pointer is not cleared out and will crash again on destruction.)
  list->clear();
}

class MultipleInheritanceNodeBase {
 public:
  MultipleInheritanceNodeBase() = default;
  int field_taking_up_space_ = 0;
};

class UnsafeMultipleInheritanceNode
    : public MultipleInheritanceNodeBase,
      public UnsafeLinkNode<UnsafeMultipleInheritanceNode> {
 public:
  UnsafeMultipleInheritanceNode() = default;
};

class SafeMultipleInheritanceNode
    : public MultipleInheritanceNodeBase,
      public SafeLinkNode<SafeMultipleInheritanceNode> {
 public:
  SafeMultipleInheritanceNode() = default;
};

TEST(LinkedListMultipleInheritanceTest, Unsafe) {
  UnsafeMultipleInheritanceNode node;
  EXPECT_EQ(&node, node.value());
}

TEST(LinkedListMultipleInheritanceTest, Safe) {
  SafeMultipleInheritanceNode node;
  EXPECT_EQ(&node, node.value());
}

}  // namespace
}  // namespace base
