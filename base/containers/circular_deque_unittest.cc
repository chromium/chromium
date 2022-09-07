// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/circular_deque.h"

#include "base/test/copy_only_int.h"
#include "base/test/move_only_int.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::internal::VectorBuffer;

namespace base {

namespace {

circular_deque<int> MakeSequence(size_t max) {
  circular_deque<int> ret;
  for (size_t i = 0; i < max; i++)
    ret.push_back(i);
  return ret;
}

// Cycles through the queue, popping items from the back and pushing items
// at the front to validate behavior across different configurations of the
// queue in relation to the underlying buffer. The tester closure is run for
// each cycle.
template <class QueueT, class Tester>
void CycleTest(circular_deque<QueueT>& queue, const Tester& tester) {
  size_t steps = queue.size() * 2;
  for (size_t i = 0; i < steps; i++) {
    tester(queue, i);
    queue.pop_back();
    queue.push_front(QueueT());
  }
}

class DestructorCounter {
 public:
  DestructorCounter(int* counter) : counter_(counter) {}
  ~DestructorCounter() { ++(*counter_); }

 private:
  int* counter_;
};

}  // namespace

TEST(CircularDeque, FillConstructor) {
  constexpr size_t num_elts = 9;

  std::vector<int> foo(15);
  EXPECT_EQ(15u, foo.size());

  // Fill with default constructor.
  {
    circular_deque<int> buf(num_elts);

    EXPECT_EQ(num_elts, buf.size());
    EXPECT_EQ(num_elts, static_cast<size_t>(buf.end() - buf.begin()));

    for (size_t i = 0; i < num_elts; i++)
      EXPECT_EQ(0, buf[i]);
  }

  // Fill with explicit value.
  {
    int value = 199;
    circular_deque<int> buf(num_elts, value);

    EXPECT_EQ(num_elts, buf.size());
    EXPECT_EQ(num_elts, static_cast<size_t>(buf.end() - buf.begin()));

    for (size_t i = 0; i < num_elts; i++)
      EXPECT_EQ(value, buf[i]);
  }
}

TEST(CircularDeque, CopyAndRangeConstructor) {
  int values[] = {1, 2, 3, 4, 5, 6};
  circular_deque<CopyOnlyInt> first(std::begin(values), std::end(values));

  circular_deque<CopyOnlyInt> second(first);
  EXPECT_EQ(6u, second.size());
  for (int i = 0; i < 6; i++)
    EXPECT_EQ(i + 1, second[i].data());
}

TEST(CircularDeque, MoveConstructor) {
  int values[] = {1, 2, 3, 4, 5, 6};
  circular_deque<MoveOnlyInt> first(std::begin(values), std::end(values));

  circular_deque<MoveOnlyInt> second(std::move(first));
  EXPECT_TRUE(first.empty());
  EXPECT_EQ(6u, second.size());
  for (int i = 0; i < 6; i++)
    EXPECT_EQ(i + 1, second[i].data());
}

TEST(CircularDeque, InitializerListConstructor) {
  circular_deque<int> empty({});
  ASSERT_TRUE(empty.empty());

  circular_deque<int> first({1, 2, 3, 4, 5, 6});
  EXPECT_EQ(6u, first.size());
  for (int i = 0; i < 6; i++)
    EXPECT_EQ(i + 1, first[i]);
}

TEST(CircularDeque, Destructor) {
  int destruct_count = 0;

  // Contiguous buffer.
  {
    circular_deque<DestructorCounter> q;
    q.resize(5, DestructorCounter(&destruct_count));

    EXPECT_EQ(1, destruct_count);  // The temporary in the call to resize().
    destruct_count = 0;
  }
  EXPECT_EQ(5, destruct_count);  // One call for each.

  // Force a wraparound buffer.
  {
    circular_deque<DestructorCounter> q;
    q.reserve(7);
    q.resize(5, DestructorCounter(&destruct_count));

    // Cycle throught some elements in our buffer to force a wraparound.
    destruct_count = 0;
    for (int i = 0; i < 4; i++) {
      q.emplace_back(&destruct_count);
      q.pop_front();
    }
    EXPECT_EQ(4, destruct_count);  // One for each cycle.
    destruct_count = 0;
  }
  EXPECT_EQ(5, destruct_count);  // One call for each.
}

TEST(CircularDeque, EqualsCopy) {
  circular_deque<int> first = {1, 2, 3, 4, 5, 6};
  circular_deque<int> copy;
  EXPECT_TRUE(copy.empty());
  copy = first;
  EXPECT_EQ(6u, copy.size());
  for (int i = 0; i < 6; i++) {
    EXPECT_EQ(i + 1, first[i]);
    EXPECT_EQ(i + 1, copy[i]);
    EXPECT_NE(&first[i], &copy[i]);
  }
}

TEST(CircularDeque, EqualsMove) {
  circular_deque<int> first = {1, 2, 3, 4, 5, 6};
  circular_deque<int> move;
  EXPECT_TRUE(move.empty());
  move = std::move(first);
  EXPECT_TRUE(first.empty());
  EXPECT_EQ(6u, move.size());
  for (int i = 0; i < 6; i++)
    EXPECT_EQ(i + 1, move[i]);
}

// Tests that self-assignment is a no-op.
TEST(CircularDeque, EqualsSelf) {
  circular_deque<int> q = {1, 2, 3, 4, 5, 6};
  q = *&q;  // The *& defeats Clang's -Wself-assign warning.
  EXPECT_EQ(6u, q.size());
  for (int i = 0; i < 6; i++)
    EXPECT_EQ(i + 1, q[i]);
}

TEST(CircularDeque, EqualsInitializerList) {
  circular_deque<int> q;
  EXPECT_TRUE(q.empty());
  q = {1, 2, 3, 4, 5, 6};
  EXPECT_EQ(6u, q.size());
  for (int i = 0; i < 6; i++)
    EXPECT_EQ(i + 1, q[i]);
}

TEST(CircularDeque, AssignCountValue) {
  circular_deque<int> empty;
  empty.assign(0, 52);
  EXPECT_EQ(0u, empty.size());

  circular_deque<int> full;
  size_t count = 13;
  int value = 12345;
  full.assign(count, value);
  EXPECT_EQ(count, full.size());

  for (size_t i = 0; i < count; i++)
    EXPECT_EQ(value, full[i]);
}

TEST(CircularDeque, AssignIterator) {
  int range[8] = {11, 12, 13, 14, 15, 16, 17, 18};

  circular_deque<int> empty;
  empty.assign(std::begin(range), std::begin(range));
  EXPECT_TRUE(empty.empty());

  circular_deque<int> full;
  full.assign(std::begin(range), std::end(range));
  EXPECT_EQ(8u, full.size());
  for (size_t i = 0; i < 8; i++)
    EXPECT_EQ(range[i], full[i]);
}

TEST(CircularDeque, AssignInitializerList) {
  circular_deque<int> empty;
  empty.assign({});
  EXPECT_TRUE(empty.empty());

  circular_deque<int> full;
  full.assign({11, 12, 13, 14, 15, 16, 17, 18});
  EXPECT_EQ(8u, full.size());
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(11 + i, full[i]);
}

// Tests [] and .at().
TEST(CircularDeque, At) {
  circular_deque<int> q = MakeSequence(10);
  CycleTest(q, [](const circular_deque<int>& q, size_t cycle) {
    size_t expected_size = 10;
    EXPECT_EQ(expected_size, q.size());

    // A sequence of 0's.
    size_t index = 0;
    size_t num_zeros = std::min(expected_size, cycle);
    for (size_t i = 0; i < num_zeros; i++, index++) {
      EXPECT_EQ(0, q[index]);
      EXPECT_EQ(0, q.at(index));
    }

    // Followed by a sequence of increasing ints.
    size_t num_ints = expected_size - num_zeros;
    for (int i = 0; i < static_cast<int>(num_ints); i++, index++) {
      EXPECT_EQ(i, q[index]);
      EXPECT_EQ(i, q.at(index));
    }
  });
}

// This also tests the copy constructor with lots of different types of
// input configurations.
TEST(CircularDeque, FrontBackPushPop) {
  circular_deque<int> q = MakeSequence(10);

  int expected_front = 0;
  int expected_back = 9;

  // Go in one direction.
  for (int i = 0; i < 100; i++) {
    const circular_deque<int> const_q(q);

    EXPECT_EQ(expected_front, q.front());
    EXPECT_EQ(expected_back, q.back());
    EXPECT_EQ(expected_front, const_q.front());
    EXPECT_EQ(expected_back, const_q.back());

    expected_front++;
    expected_back++;

    q.pop_front();
    q.push_back(expected_back);
  }

  // Go back in reverse.
  for (int i = 0; i < 100; i++) {
    const circular_deque<int> const_q(q);

    EXPECT_EQ(expected_front, q.front());
    EXPECT_EQ(expected_back, q.back());
    EXPECT_EQ(expected_front, const_q.front());
    EXPECT_EQ(expected_back, const_q.back());

    expected_front--;
    expected_back--;

    q.pop_back();
    q.push_front(expected_front);
  }
}

TEST(CircularDeque, ReallocateWithSplitBuffer) {
  // Tests reallocating a deque with an internal buffer that looks like this:
  //   4   5   x   x   0   1   2   3
  //       end-^       ^-begin
  circular_deque<int> q;
  q.reserve(7);  // Internal buffer is always 1 larger than requested.
  q.push_back(-1);
  q.push_back(-1);
  q.push_back(-1);
  q.push_back(-1);
  q.push_back(0);
  q.pop_front();
  q.pop_front();
  q.pop_front();
  q.pop_front();
  q.push_back(1);
  q.push_back(2);
  q.push_back(3);
  q.push_back(4);
  q.push_back(5);

  q.shrink_to_fit();
  EXPECT_EQ(6u, q.size());

  EXPECT_EQ(0, q[0]);
  EXPECT_EQ(1, q[1]);
  EXPECT_EQ(2, q[2]);
  EXPECT_EQ(3, q[3]);
  EXPECT_EQ(4, q[4]);
  EXPECT_EQ(5, q[5]);
}

TEST(CircularDeque, Swap) {
  circular_deque<int> a = MakeSequence(10);
  circular_deque<int> b = MakeSequence(100);

  a.swap(b);
  EXPECT_EQ(100u, a.size());
  for (int i = 0; i < 100; i++)
    EXPECT_EQ(i, a[i]);

  EXPECT_EQ(10u, b.size());
  for (int i = 0; i < 10; i++)
    EXPECT_EQ(i, b[i]);
}

TEST(CircularDeque, Iteration) {
  circular_deque<int> q = MakeSequence(10);

  int expected_front = 0;
  int expected_back = 9;

  // This loop causes various combinations of begin and end to be tested.
  for (int i = 0; i < 30; i++) {
    // Range-based for loop going forward.
    int current_expected = expected_front;
    for (int cur : q) {
      EXPECT_EQ(current_expected, cur);
      current_expected++;
    }

    // Manually test reverse iterators.
    current_expected = expected_back;
    for (auto cur = q.crbegin(); cur < q.crend(); cur++) {
      EXPECT_EQ(current_expected, *cur);
      current_expected--;
    }

    expected_front++;
    expected_back++;

    q.pop_front();
    q.push_back(expected_back);
  }

  // Go back in reverse.
  for (int i = 0; i < 100; i++) {
    const circular_deque<int> const_q(q);

    EXPECT_EQ(expected_front, q.front());
    EXPECT_EQ(expected_back, q.back());
    EXPECT_EQ(expected_front, const_q.front());
    EXPECT_EQ(expected_back, const_q.back());

    expected_front--;
    expected_back--;

    q.pop_back();
    q.push_front(expected_front);
  }
}

TEST(CircularDeque, IteratorComparisons) {
  circular_deque<int> q = MakeSequence(10);

  // This loop causes various combinations of begin and end to be tested.
  for (int i = 0; i < 30; i++) {
    EXPECT_LT(q.begin(), q.end());
    EXPECT_LE(q.begin(), q.end());
    EXPECT_LE(q.begin(), q.begin());

    EXPECT_GT(q.end(), q.begin());
    EXPECT_GE(q.end(), q.begin());
    EXPECT_GE(q.end(), q.end());

    EXPECT_EQ(q.begin(), q.begin());
    EXPECT_NE(q.begin(), q.end());

    q.push_front(10);
    q.pop_back();
  }
}

TEST(CircularDeque, IteratorIncDec) {
  circular_deque<int> q;

  // No-op offset computations with no capacity.
  EXPECT_EQ(q.end(), q.end() + 0);
  EXPECT_EQ(q.end(), q.end() - 0);

  q = MakeSequence(10);

  // Mutable preincrement, predecrement.
  {
    circular_deque<int>::iterator it = q.begin();
    circular_deque<int>::iterator op_result = ++it;
    EXPECT_EQ(1, *op_result);
    EXPECT_EQ(1, *it);

    op_result = --it;
    EXPECT_EQ(0, *op_result);
    EXPECT_EQ(0, *it);
  }

  // Const preincrement, predecrement.
  {
    circular_deque<int>::const_iterator it = q.begin();
    circular_deque<int>::const_iterator op_result = ++it;
    EXPECT_EQ(1, *op_result);
    EXPECT_EQ(1, *it);

    op_result = --it;
    EXPECT_EQ(0, *op_result);
    EXPECT_EQ(0, *it);
  }

  // Mutable postincrement, postdecrement.
  {
    circular_deque<int>::iterator it = q.begin();
    circular_deque<int>::iterator op_result = it++;
    EXPECT_EQ(0, *op_result);
    EXPECT_EQ(1, *it);

    op_result = it--;
    EXPECT_EQ(1, *op_result);
    EXPECT_EQ(0, *it);
  }

  // Const postincrement, postdecrement.
  {
    circular_deque<int>::const_iterator it = q.begin();
    circular_deque<int>::const_iterator op_result = it++;
    EXPECT_EQ(0, *op_result);
    EXPECT_EQ(1, *it);

    op_result = it--;
    EXPECT_EQ(1, *op_result);
    EXPECT_EQ(0, *it);
  }
}

TEST(CircularDeque, IteratorIntegerOps) {
  circular_deque<int> q = MakeSequence(10);

  int expected_front = 0;
  int expected_back = 9;

  for (int i = 0; i < 30; i++) {
    EXPECT_EQ(0, q.begin() - q.begin());
    EXPECT_EQ(0, q.end() - q.end());
    EXPECT_EQ(q.size(), static_cast<size_t>(q.end() - q.begin()));

    // +=
    circular_deque<int>::iterator eight = q.begin();
    eight += 8;
    EXPECT_EQ(8, eight - q.begin());
    EXPECT_EQ(expected_front + 8, *eight);

    // -=
    eight -= 8;
    EXPECT_EQ(q.begin(), eight);

    // +
    eight = eight + 8;
    EXPECT_EQ(8, eight - q.begin());

    // -
    eight = eight - 8;
    EXPECT_EQ(q.begin(), eight);

    expected_front++;
    expected_back++;

    q.pop_front();
    q.push_back(expected_back);
  }
}

TEST(CircularDeque, IteratorArrayAccess) {
  circular_deque<int> q = MakeSequence(10);

  circular_deque<int>::iterator begin = q.begin();
  EXPECT_EQ(0, begin[0]);
  EXPECT_EQ(9, begin[9]);

  circular_deque<int>::iterator end = q.end();
  EXPECT_EQ(0, end[-10]);
  EXPECT_EQ(9, end[-1]);

  begin[0] = 100;
  EXPECT_EQ(100, end[-10]);
}

TEST(CircularDeque, ReverseIterator) {
  circular_deque<int> q;
  q.push_back(4);
  q.push_back(3);
  q.push_back(2);
  q.push_back(1);

  circular_deque<int>::reverse_iterator iter = q.rbegin();
  EXPECT_EQ(1, *iter);
  iter++;
  EXPECT_EQ(2, *iter);
  ++iter;
  EXPECT_EQ(3, *iter);
  iter++;
  EXPECT_EQ(4, *iter);
  ++iter;
  EXPECT_EQ(q.rend(), iter);
}

TEST(CircularDeque, CapacityReserveShrink) {
  circular_deque<int> q;

  // A default constructed queue should have no capacity since it should waste
  // no space.
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(0u, q.size());
  EXPECT_EQ(0u, q.capacity());

  size_t new_capacity = 100;
  q.reserve(new_capacity);
  EXPECT_EQ(new_capacity, q.capacity());

  // Adding that many items should not cause a resize.
  for (size_t i = 0; i < new_capacity; i++)
    q.push_back(i);
  EXPECT_EQ(new_capacity, q.size());
  EXPECT_EQ(new_capacity, q.capacity());

  // Shrink to fit to a smaller size.
  size_t capacity_2 = new_capacity / 2;
  q.resize(capacity_2);
  q.shrink_to_fit();
  EXPECT_EQ(capacity_2, q.size());
  EXPECT_EQ(capacity_2, q.capacity());
}

TEST(CircularDeque, CapacityAutoShrink) {
  size_t big_size = 1000u;
  circular_deque<int> q;
  q.resize(big_size);

  size_t big_capacity = q.capacity();

  // Delete 3/4 of the items.
  for (size_t i = 0; i < big_size / 4 * 3; i++)
    q.pop_back();

  // The capacity should have shrunk by deleting that many items.
  size_t medium_capacity = q.capacity();
  EXPECT_GT(big_capacity, medium_capacity);

  // Using resize to shrink should keep some extra capacity.
  q.resize(1);
  EXPECT_LT(1u, q.capacity());

  q.resize(0);
  EXPECT_LT(0u, q.capacity());

  // Using clear() should delete everything.
  q.clear();
  EXPECT_EQ(0u, q.capacity());
}

TEST(CircularDeque, ClearAndEmpty) {
  circular_deque<int> q;
  EXPECT_TRUE(q.empty());

  q.resize(10);
  EXPECT_EQ(10u, q.size());
  EXPECT_FALSE(q.empty());

  q.clear();
  EXPECT_EQ(0u, q.size());
  EXPECT_TRUE(q.empty());

  // clear() also should reset the capacity.
  EXPECT_EQ(0u, q.capacity());
}

TEST(CircularDeque, Resize) {
  circular_deque<int> q;

  // Resize with default constructor.
  size_t first_size = 10;
  q.resize(first_size);
  EXPECT_EQ(first_size, q.size());
  for (size_t i = 0; i < first_size; i++)
    EXPECT_EQ(0, q[i]);

  // Resize with different value.
  size_t second_expand = 10;
  q.resize(first_size + second_expand, 3);
  EXPECT_EQ(first_size + second_expand, q.size());
  for (size_t i = 0; i < first_size; i++)
    EXPECT_EQ(0, q[i]);
  for (size_t i = 0; i < second_expand; i++)
    EXPECT_EQ(3, q[i + first_size]);

  // Erase from the end and add to the beginning so resize is forced to cross
  // a circular buffer wrap boundary.
  q.shrink_to_fit();
  for (int i = 0; i < 5; i++) {
    q.pop_back();
    q.push_front(6);
  }
  q.resize(10);

  EXPECT_EQ(6, q[0]);
  EXPECT_EQ(6, q[1]);
  EXPECT_EQ(6, q[2]);
  EXPECT_EQ(6, q[3]);
  EXPECT_EQ(6, q[4]);
  EXPECT_EQ(0, q[5]);
  EXPECT_EQ(0, q[6]);
  EXPECT_EQ(0, q[7]);
  EXPECT_EQ(0, q[8]);
  EXPECT_EQ(0, q[9]);
}

// Tests destructor behavior of resize.
TEST(CircularDeque, ResizeDelete) {
  int counter = 0;
  circular_deque<DestructorCounter> q;
  q.resize(10, DestructorCounter(&counter));
  // The one temporary when calling resize() should be deleted, that's it.
  EXPECT_EQ(1, counter);

  // The loops below assume the capacity will be set by resize().
  EXPECT_EQ(10u, q.capacity());

  // Delete some via resize(). This is done so that the wasted items are
  // still greater than the size() so that auto-shrinking is not triggered
  // (which will mess up our destructor counting).
  counter = 0;
  q.resize(8, DestructorCounter(&counter));
  // 2 deleted ones + the one temporary in the resize() call.
  EXPECT_EQ(3, counter);

  // Cycle through some items so two items will cross the boundary in the
  // 11-item buffer (one more than the capacity).
  //   Before: x x x x x x x x . . .
  //   After:  x . . . x x x x x x x
  counter = 0;
  for (int i = 0; i < 4; i++) {
    q.emplace_back(&counter);
    q.pop_front();
  }
  EXPECT_EQ(4, counter);  // Loop should have deleted 7 items.

  // Delete two items with resize, these should be on either side of the
  // buffer wrap point.
  counter = 0;
  q.resize(6, DestructorCounter(&counter));
  // 2 deleted ones + the one temporary in the resize() call.
  EXPECT_EQ(3, counter);
}

TEST(CircularDeque, InsertEraseSingle) {
  circular_deque<int> q;
  q.push_back(1);
  q.push_back(2);

  // Insert at the beginning.
  auto result = q.insert(q.begin(), 0);
  EXPECT_EQ(q.begin(), result);
  EXPECT_EQ(3u, q.size());
  EXPECT_EQ(0, q[0]);
  EXPECT_EQ(1, q[1]);
  EXPECT_EQ(2, q[2]);

  // Erase at the beginning.
  result = q.erase(q.begin());
  EXPECT_EQ(q.begin(), result);
  EXPECT_EQ(2u, q.size());
  EXPECT_EQ(1, q[0]);
  EXPECT_EQ(2, q[1]);

  // Insert at the end.
  result = q.insert(q.end(), 3);
  EXPECT_EQ(q.end() - 1, result);
  EXPECT_EQ(1, q[0]);
  EXPECT_EQ(2, q[1]);
  EXPECT_EQ(3, q[2]);

  // Erase at the end.
  result = q.erase(q.end() - 1);
  EXPECT_EQ(q.end(), result);
  EXPECT_EQ(1, q[0]);
  EXPECT_EQ(2, q[1]);

  // Insert in the middle.
  result = q.insert(q.begin() + 1, 10);
  EXPECT_EQ(q.begin() + 1, result);
  EXPECT_EQ(1, q[0]);
  EXPECT_EQ(10, q[1]);
  EXPECT_EQ(2, q[2]);

  // Erase in the middle.
  result = q.erase(q.begin() + 1);
  EXPECT_EQ(q.begin() + 1, result);
  EXPECT_EQ(1, q[0]);
  EXPECT_EQ(2, q[1]);
}

TEST(CircularDeque, InsertFill) {
  circular_deque<int> q;

  // Fill when empty.
  q.insert(q.begin(), 2, 1);

  // 0's at the beginning.
  q.insert(q.begin(), 2, 0);

  // 50's in the middle (now at offset 3).
  q.insert(q.begin() + 3, 2, 50);

  // 200's at the end.
  q.insert(q.end(), 2, 200);

  ASSERT_EQ(8u, q.size());
  EXPECT_EQ(0, q[0]);
  EXPECT_EQ(0, q[1]);
  EXPECT_EQ(1, q[2]);
  EXPECT_EQ(50, q[3]);
  EXPECT_EQ(50, q[4]);
  EXPECT_EQ(1, q[5]);
  EXPECT_EQ(200, q[6]);
  EXPECT_EQ(200, q[7]);
}

TEST(CircularDeque, InsertEraseRange) {
  circular_deque<int> q;

  // Erase nothing from an empty deque should work.
  q.erase(q.begin(), q.end());

  // Loop index used below to shift the used items in the buffer.
  for (int i = 0; i < 10; i++) {
    circular_deque<int> source;

    // Fill empty range.
    q.insert(q.begin(), source.begin(), source.end());

    // Have some stuff to insert.
    source.push_back(1);
    source.push_back(2);

    q.insert(q.begin(), source.begin(), source.end());

    // Shift the used items in the buffer by i which will place the two used
    // elements in different places in the buffer each time through this loop.
    for (int shift_i = 0; shift_i < i; shift_i++) {
      q.push_back(0);
      q.pop_front();
    }

    // Set the two items to notable values so we can check for them later.
    ASSERT_EQ(2u, q.size());
    q[0] = 100;
    q[1] = 101;

    // Insert at the beginning, middle (now at offset 3), and end.
    q.insert(q.begin(), source.begin(), source.end());
    q.insert(q.begin() + 3, source.begin(), source.end());
    q.insert(q.end(), source.begin(), source.end());

    ASSERT_EQ(8u, q.size());
    EXPECT_EQ(1, q[0]);
    EXPECT_EQ(2, q[1]);
    EXPECT_EQ(100, q[2]);  // First inserted one.
    EXPECT_EQ(1, q[3]);
    EXPECT_EQ(2, q[4]);
    EXPECT_EQ(101, q[5]);  // First inserted second one.
    EXPECT_EQ(1, q[6]);
    EXPECT_EQ(2, q[7]);

    // Now erase the inserted ranges. Try each subsection also with no items
    // being erased, which should be a no-op.
    auto result = q.erase(q.begin(), q.begin());  // No-op.
    EXPECT_EQ(q.begin(), result);
    result = q.erase(q.begin(), q.begin() + 2);
    EXPECT_EQ(q.begin(), result);

    result = q.erase(q.begin() + 1, q.begin() + 1);  // No-op.
    EXPECT_EQ(q.begin() + 1, result);
    result = q.erase(q.begin() + 1, q.begin() + 3);
    EXPECT_EQ(q.begin() + 1, result);

    result = q.erase(q.end() - 2, q.end() - 2);  // No-op.
    EXPECT_EQ(q.end() - 2, result);
    result = q.erase(q.end() - 2, q.end());
    EXPECT_EQ(q.end(), result);

    ASSERT_EQ(2u, q.size());
    EXPECT_EQ(100, q[0]);
    EXPECT_EQ(101, q[1]);

    // Erase everything.
    result = q.erase(q.begin(), q.end());
    EXPECT_EQ(q.end(), result);
    EXPECT_TRUE(q.empty());
  }
}

TEST(CircularDeque, EmplaceMoveOnly) {
  int values[] = {1, 3};
  circular_deque<MoveOnlyInt> q(std::begin(values), std::end(values));

  q.emplace(q.begin(), MoveOnlyInt(0));
  q.emplace(q.begin() + 2, MoveOnlyInt(2));
  q.emplace(q.end(), MoveOnlyInt(4));

  ASSERT_EQ(5u, q.size());
  EXPECT_EQ(0, q[0].data());
  EXPECT_EQ(1, q[1].data());
  EXPECT_EQ(2, q[2].data());
  EXPECT_EQ(3, q[3].data());
  EXPECT_EQ(4, q[4].data());
}

TEST(CircularDeque, EmplaceFrontBackReturnsReference) {
  circular_deque<int> q;
  q.reserve(2);

  int& front = q.emplace_front(1);
  int& back = q.emplace_back(2);
  ASSERT_EQ(2u, q.size());
  EXPECT_EQ(1, q[0]);
  EXPECT_EQ(2, q[1]);

  EXPECT_EQ(&front, &q.front());
  EXPECT_EQ(&back, &q.back());

  front = 3;
  back = 4;

  ASSERT_EQ(2u, q.size());
  EXPECT_EQ(3, q[0]);
  EXPECT_EQ(4, q[1]);

  EXPECT_EQ(&front, &q.front());
  EXPECT_EQ(&back, &q.back());
}

/*
This test should assert in a debug build. It tries to dereference an iterator
after mutating the container. Uncomment to double-check that this works.
TEST(CircularDeque, UseIteratorAfterMutate) {
  circular_deque<int> q;
  q.push_back(0);

  auto old_begin = q.begin();
  EXPECT_EQ(0, *old_begin);

  q.push_back(1);
  EXPECT_EQ(0, *old_begin);  // Should DCHECK.
}
*/

}  // namespace base
