// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/intrusive_heap.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

struct TestElement {
  int key;
  HeapHandle* handle;

  bool operator<=(const TestElement& other) const { return key <= other.key; }

  void SetHeapHandle(HeapHandle h) {
    if (handle)
      *handle = h;
  }

  void ClearHeapHandle() {
    if (handle)
      *handle = HeapHandle();
  }
};

}  // namespace

class IntrusiveHeapTest : public testing::Test {
 protected:
  static bool CompareNodes(const TestElement& a, const TestElement& b) {
    return IntrusiveHeap<TestElement>::CompareNodes(a, b);
  }
};

TEST_F(IntrusiveHeapTest, Basic) {
  IntrusiveHeap<TestElement> heap;

  EXPECT_TRUE(heap.empty());
  EXPECT_EQ(0u, heap.size());
}

TEST_F(IntrusiveHeapTest, Clear) {
  IntrusiveHeap<TestElement> heap;
  HeapHandle index1;

  heap.insert({11, &index1});
  EXPECT_EQ(1u, heap.size());
  EXPECT_TRUE(index1.IsValid());

  heap.Clear();
  EXPECT_EQ(0u, heap.size());
  EXPECT_FALSE(index1.IsValid());
}

TEST_F(IntrusiveHeapTest, Destructor) {
  HeapHandle index1;

  {
    IntrusiveHeap<TestElement> heap;

    heap.insert({11, &index1});
    EXPECT_EQ(1u, heap.size());
    EXPECT_TRUE(index1.IsValid());
  }

  EXPECT_FALSE(index1.IsValid());
}

TEST_F(IntrusiveHeapTest, Min) {
  IntrusiveHeap<TestElement> heap;

  heap.insert({9, nullptr});
  heap.insert({10, nullptr});
  heap.insert({8, nullptr});
  heap.insert({2, nullptr});
  heap.insert({7, nullptr});
  heap.insert({15, nullptr});
  heap.insert({22, nullptr});
  heap.insert({3, nullptr});

  EXPECT_FALSE(heap.empty());
  EXPECT_EQ(8u, heap.size());
  EXPECT_EQ(2, heap.Min().key);
}

TEST_F(IntrusiveHeapTest, InsertAscending) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 50; i++)
    heap.insert({i, nullptr});

  EXPECT_EQ(0, heap.Min().key);
  EXPECT_EQ(50u, heap.size());
}

TEST_F(IntrusiveHeapTest, InsertDescending) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 50; i++)
    heap.insert({50 - i, nullptr});

  EXPECT_EQ(1, heap.Min().key);
  EXPECT_EQ(50u, heap.size());
}

TEST_F(IntrusiveHeapTest, HeapIndex) {
  HeapHandle index5;
  HeapHandle index4;
  HeapHandle index3;
  HeapHandle index2;
  HeapHandle index1;
  IntrusiveHeap<TestElement> heap;

  EXPECT_FALSE(index1.IsValid());
  EXPECT_FALSE(index2.IsValid());
  EXPECT_FALSE(index3.IsValid());
  EXPECT_FALSE(index4.IsValid());
  EXPECT_FALSE(index5.IsValid());

  heap.insert({15, &index5});
  heap.insert({14, &index4});
  heap.insert({13, &index3});
  heap.insert({12, &index2});
  heap.insert({11, &index1});

  EXPECT_TRUE(index1.IsValid());
  EXPECT_TRUE(index2.IsValid());
  EXPECT_TRUE(index3.IsValid());
  EXPECT_TRUE(index4.IsValid());
  EXPECT_TRUE(index5.IsValid());

  EXPECT_FALSE(heap.empty());
}

TEST_F(IntrusiveHeapTest, Pop) {
  IntrusiveHeap<TestElement> heap;
  HeapHandle index1;
  HeapHandle index2;

  heap.insert({11, &index1});
  heap.insert({12, &index2});
  EXPECT_EQ(2u, heap.size());
  EXPECT_TRUE(index1.IsValid());
  EXPECT_TRUE(index2.IsValid());

  heap.Pop();
  EXPECT_EQ(1u, heap.size());
  EXPECT_FALSE(index1.IsValid());
  EXPECT_TRUE(index2.IsValid());

  heap.Pop();
  EXPECT_EQ(0u, heap.size());
  EXPECT_FALSE(index1.IsValid());
  EXPECT_FALSE(index2.IsValid());
}

TEST_F(IntrusiveHeapTest, PopMany) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 500; i++)
    heap.insert({i, nullptr});

  EXPECT_FALSE(heap.empty());
  EXPECT_EQ(500u, heap.size());
  for (int i = 0; i < 500; i++) {
    EXPECT_EQ(i, heap.Min().key);
    heap.Pop();
  }
  EXPECT_TRUE(heap.empty());
}

TEST_F(IntrusiveHeapTest, Erase) {
  IntrusiveHeap<TestElement> heap;

  HeapHandle index12;

  heap.insert({15, nullptr});
  heap.insert({14, nullptr});
  heap.insert({13, nullptr});
  heap.insert({12, &index12});
  heap.insert({11, nullptr});

  EXPECT_EQ(5u, heap.size());
  EXPECT_TRUE(index12.IsValid());
  heap.erase(index12);
  EXPECT_EQ(4u, heap.size());
  EXPECT_FALSE(index12.IsValid());

  EXPECT_EQ(11, heap.Min().key);
  heap.Pop();
  EXPECT_EQ(13, heap.Min().key);
  heap.Pop();
  EXPECT_EQ(14, heap.Min().key);
  heap.Pop();
  EXPECT_EQ(15, heap.Min().key);
  heap.Pop();
  EXPECT_TRUE(heap.empty());
}

TEST_F(IntrusiveHeapTest, ReplaceMin) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 500; i++)
    heap.insert({500 - i, nullptr});

  EXPECT_EQ(1, heap.Min().key);

  for (int i = 0; i < 500; i++)
    heap.ReplaceMin({1000 + i, nullptr});

  EXPECT_EQ(1000, heap.Min().key);
}

TEST_F(IntrusiveHeapTest, ReplaceMinWithNonLeafNode) {
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 50; i++) {
    heap.insert({i, nullptr});
    heap.insert({200 + i, nullptr});
  }

  EXPECT_EQ(0, heap.Min().key);

  for (int i = 0; i < 50; i++)
    heap.ReplaceMin({100 + i, nullptr});

  for (int i = 0; i < 50; i++) {
    EXPECT_EQ((100 + i), heap.Min().key);
    heap.Pop();
  }
  for (int i = 0; i < 50; i++) {
    EXPECT_EQ((200 + i), heap.Min().key);
    heap.Pop();
  }
  EXPECT_TRUE(heap.empty());
}

TEST_F(IntrusiveHeapTest, ReplaceMinCheckAllFinalPositions) {
  HeapHandle index[100];

  for (int j = -1; j <= 201; j += 2) {
    IntrusiveHeap<TestElement> heap;
    for (size_t i = 0; i < 100; i++) {
      heap.insert({static_cast<int>(i) * 2, &index[i]});
    }

    heap.ReplaceMin({j, &index[40]});

    int prev = -2;
    while (!heap.empty()) {
      DCHECK_GT(heap.Min().key, prev);
      DCHECK(heap.Min().key == j || (heap.Min().key % 2) == 0);
      DCHECK_NE(heap.Min().key, 0);
      prev = heap.Min().key;
      heap.Pop();
    }
  }
}

TEST_F(IntrusiveHeapTest, ChangeKeyUp) {
  IntrusiveHeap<TestElement> heap;
  HeapHandle index[10];

  for (size_t i = 0; i < 10; i++) {
    heap.insert({static_cast<int>(i) * 2, &index[i]});
  }

  heap.ChangeKey(index[5], {17, &index[5]});

  std::vector<int> results;
  while (!heap.empty()) {
    results.push_back(heap.Min().key);
    heap.Pop();
  }

  EXPECT_THAT(results, testing::ElementsAre(0, 2, 4, 6, 8, 12, 14, 16, 17, 18));
}

TEST_F(IntrusiveHeapTest, ChangeKeyUpButDoesntMove) {
  IntrusiveHeap<TestElement> heap;
  HeapHandle index[10];

  for (size_t i = 0; i < 10; i++) {
    heap.insert({static_cast<int>(i) * 2, &index[i]});
  }

  heap.ChangeKey(index[5], {11, &index[5]});

  std::vector<int> results;
  while (!heap.empty()) {
    results.push_back(heap.Min().key);
    heap.Pop();
  }

  EXPECT_THAT(results, testing::ElementsAre(0, 2, 4, 6, 8, 11, 12, 14, 16, 18));
}

TEST_F(IntrusiveHeapTest, ChangeKeyDown) {
  IntrusiveHeap<TestElement> heap;
  HeapHandle index[10];

  for (size_t i = 0; i < 10; i++) {
    heap.insert({static_cast<int>(i) * 2, &index[i]});
  }

  heap.ChangeKey(index[5], {1, &index[5]});

  std::vector<int> results;
  while (!heap.empty()) {
    results.push_back(heap.Min().key);
    heap.Pop();
  }

  EXPECT_THAT(results, testing::ElementsAre(0, 1, 2, 4, 6, 8, 12, 14, 16, 18));
}

TEST_F(IntrusiveHeapTest, ChangeKeyDownButDoesntMove) {
  IntrusiveHeap<TestElement> heap;
  HeapHandle index[10];

  for (size_t i = 0; i < 10; i++) {
    heap.insert({static_cast<int>(i) * 2, &index[i]});
  }

  heap.ChangeKey(index[5], {9, &index[5]});

  std::vector<int> results;
  while (!heap.empty()) {
    results.push_back(heap.Min().key);
    heap.Pop();
  }

  EXPECT_THAT(results, testing::ElementsAre(0, 2, 4, 6, 8, 9, 12, 14, 16, 18));
}

TEST_F(IntrusiveHeapTest, ChangeKeyCheckAllFinalPositions) {
  HeapHandle index[100];

  for (int j = -1; j <= 201; j += 2) {
    IntrusiveHeap<TestElement> heap;
    for (size_t i = 0; i < 100; i++) {
      heap.insert({static_cast<int>(i) * 2, &index[i]});
    }

    heap.ChangeKey(index[40], {j, &index[40]});

    int prev = -2;
    while (!heap.empty()) {
      DCHECK_GT(heap.Min().key, prev);
      DCHECK(heap.Min().key == j || (heap.Min().key % 2) == 0);
      DCHECK_NE(heap.Min().key, 80);
      prev = heap.Min().key;
      heap.Pop();
    }
  }
}

TEST_F(IntrusiveHeapTest, CompareNodes) {
  TestElement five{5, nullptr}, six{6, nullptr};

  // Check that we have a strict comparator, otherwise std::is_heap()
  // (used in DCHECK) may fail. See http://crbug.com/661080.
  EXPECT_FALSE(IntrusiveHeapTest::CompareNodes(six, six));

  EXPECT_FALSE(IntrusiveHeapTest::CompareNodes(five, six));
  EXPECT_TRUE(IntrusiveHeapTest::CompareNodes(six, five));
}

TEST_F(IntrusiveHeapTest, At) {
  HeapHandle index[10];
  IntrusiveHeap<TestElement> heap;

  for (int i = 0; i < 10; i++)
    heap.insert({static_cast<int>(i ^ (i + 1)), &index[i]});

  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(heap.at(index[i]).key, i ^ (i + 1));
    EXPECT_EQ(heap.at(index[i]).handle, &index[i]);
  }
}

}  // namespace internal
}  // namespace base
