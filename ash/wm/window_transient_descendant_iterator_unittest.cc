// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_transient_descendant_iterator.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class WindowTransientDescendantIteratorTest : public AshTestBase {
 public:
  WindowTransientDescendantIteratorTest() = default;
  ~WindowTransientDescendantIteratorTest() override = default;

  // Creates a test set of windows parented like a linked list. The result
  // vector will have window in order: ABCD.
  void CreateTestLinkedList(
      std::vector<std::unique_ptr<aura::Window>>* out_result) {
    ASSERT_TRUE(out_result->empty());
    for (char c : {'A', 'B', 'C', 'D'}) {
      auto window = CreateTestWindow();
      window->SetName(std::string(1, c));
      if (!out_result->empty())
        ::wm::AddTransientChild(out_result->back().get(), window.get());
      out_result->push_back(std::move(window));
    }
  }

  //          A
  //         /  \
  //        B    F
  //       /  \
  //      C    E
  //     /
  //    D
  // Creates a test set of windows parented like in the diagram above. The
  // result vector will have the windows in preorder: ABCDEF.
  void CreateTestInOrderTree(
      std::vector<std::unique_ptr<aura::Window>>* out_result) {
    ASSERT_TRUE(out_result->empty());
    auto window_a = CreateTestWindow();
    auto window_b = CreateTestWindow();
    auto window_c = CreateTestWindow();
    auto window_d = CreateTestWindow();
    auto window_e = CreateTestWindow();
    auto window_f = CreateTestWindow();

    window_a->SetName("A");
    window_b->SetName("B");
    window_c->SetName("C");
    window_d->SetName("D");
    window_e->SetName("E");
    window_f->SetName("F");

    // Create the tree structure.
    ::wm::AddTransientChild(window_a.get(), window_b.get());
    ::wm::AddTransientChild(window_a.get(), window_f.get());
    ::wm::AddTransientChild(window_b.get(), window_c.get());
    ::wm::AddTransientChild(window_b.get(), window_e.get());
    ::wm::AddTransientChild(window_c.get(), window_d.get());

    // Insert windows in preorder traversal: ABCDEF.
    out_result->push_back(std::move(window_a));
    out_result->push_back(std::move(window_b));
    out_result->push_back(std::move(window_c));
    out_result->push_back(std::move(window_d));
    out_result->push_back(std::move(window_e));
    out_result->push_back(std::move(window_f));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowTransientDescendantIteratorTest);
};

// Tests that case that windows a parented transiently like a linked list.
TEST_F(WindowTransientDescendantIteratorTest, LinkedList) {
  std::vector<std::unique_ptr<aura::Window>> windows;
  CreateTestLinkedList(&windows);

  int index = 0;
  std::string str;
  for (auto* window : GetTransientTreeIterator(windows[0].get())) {
    EXPECT_EQ(windows[index].get(), window);
    str += window->GetName();
    ++index;
  }
  EXPECT_EQ("ABCD", str);
}

// Tests that case that windows a parented transiently like a tree. The iterator
// should go through the windows with preorder traversal.
TEST_F(WindowTransientDescendantIteratorTest, Tree) {
  std::vector<std::unique_ptr<aura::Window>> windows;
  CreateTestInOrderTree(&windows);

  // The windows in |window| are added with preorder traversal, so they should
  // match exactly the window order from the transient iterator.
  int index = 0;
  std::string str;
  for (auto* window : GetTransientTreeIterator(windows[0].get())) {
    EXPECT_EQ(windows[index].get(), window);
    str += window->GetName();
    ++index;
  }
  EXPECT_EQ("ABCDEF", str);
}

// Tests that windows that affected by a given predicate do not show up when
// iterating.
TEST_F(WindowTransientDescendantIteratorTest, LinkedListWithPredicate) {
  std::vector<std::unique_ptr<aura::Window>> windows;
  CreateTestLinkedList(&windows);

  auto predicate = [](aura::Window* w) { return w->GetName() == "C"; };

  std::string str;
  for (auto* window : GetTransientTreeIterator(
           windows[0].get(), base::BindRepeating(predicate))) {
    str += window->GetName();
  }
  EXPECT_EQ("ABD", str);
}

TEST_F(WindowTransientDescendantIteratorTest, TreeWithPredicate) {
  std::vector<std::unique_ptr<aura::Window>> windows;
  CreateTestInOrderTree(&windows);

  auto predicate = [](aura::Window* w) {
    return w->GetName() == "B" || w->GetName() == "E";
  };

  std::string str;
  for (auto* window : GetTransientTreeIterator(
           windows[0].get(), base::BindRepeating(predicate))) {
    str += window->GetName();
  }
  EXPECT_EQ("ACDF", str);
}

}  // namespace ash
