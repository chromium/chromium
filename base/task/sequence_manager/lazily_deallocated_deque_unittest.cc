// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/lazily_deallocated_deque.h"

#include "base/test/scoped_mock_clock_override.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
namespace sequence_manager {
namespace internal {

class LazilyDeallocatedDequeTest : public testing::Test {};

TEST_F(LazilyDeallocatedDequeTest, InitiallyEmpty) {
  LazilyDeallocatedDeque<int> d;

  EXPECT_TRUE(d.empty());
  EXPECT_EQ(0u, d.size());
}

TEST_F(LazilyDeallocatedDequeTest, PushBackAndPopFront1) {
  LazilyDeallocatedDeque<int> d;

  d.push_back(123);

  EXPECT_FALSE(d.empty());
  EXPECT_EQ(1u, d.size());

  EXPECT_EQ(123, d.front());

  d.pop_front();
  EXPECT_TRUE(d.empty());
  EXPECT_EQ(0u, d.size());
}

TEST_F(LazilyDeallocatedDequeTest, PushBackAndPopFront1000) {
  LazilyDeallocatedDeque<int> d;

  for (int i = 0; i < 1000; i++) {
    d.push_back(i);
  }

  EXPECT_EQ(0, d.front());
  EXPECT_EQ(999, d.back());
  EXPECT_EQ(1000u, d.size());

  for (int i = 0; i < 1000; i++) {
    EXPECT_EQ(i, d.front());
    d.pop_front();
  }

  EXPECT_EQ(0u, d.size());
}

TEST_F(LazilyDeallocatedDequeTest, PushFrontBackAndPopFront1) {
  LazilyDeallocatedDeque<int> d;

  d.push_front(123);

  EXPECT_FALSE(d.empty());
  EXPECT_EQ(1u, d.size());

  EXPECT_EQ(123, d.front());

  d.pop_front();
  EXPECT_TRUE(d.empty());
  EXPECT_EQ(0u, d.size());
}

TEST_F(LazilyDeallocatedDequeTest, PushFrontAndPopFront1000) {
  LazilyDeallocatedDeque<int> d;

  for (int i = 0; i < 1000; i++) {
    d.push_front(i);
  }

  EXPECT_EQ(999, d.front());
  EXPECT_EQ(0, d.back());
  EXPECT_EQ(1000u, d.size());

  for (int i = 0; i < 1000; i++) {
    EXPECT_EQ(999 - i, d.front());
    d.pop_front();
  }

  EXPECT_EQ(0u, d.size());
}

TEST_F(LazilyDeallocatedDequeTest, MaybeShrinkQueueWithLargeSizeDrop) {
  LazilyDeallocatedDeque<int> d;

  for (int i = 0; i < 1000; i++) {
    d.push_back(i);
  }
  EXPECT_EQ(1000u, d.size());
  EXPECT_EQ(1305u, d.capacity());
  EXPECT_EQ(1000u, d.max_size());

  // Drop most elements.
  for (int i = 0; i < 990; i++) {
    d.pop_front();
  }
  EXPECT_EQ(10u, d.size());
  EXPECT_EQ(450u, d.capacity());
  EXPECT_EQ(1000u, d.max_size());

  // This won't do anything since the max size is greater than the current
  // capacity.
  d.MaybeShrinkQueue();
  EXPECT_EQ(450u, d.capacity());
  EXPECT_EQ(10u, d.max_size());

  // This will shrink because the max size is now much less than the current
  // capacity.
  d.MaybeShrinkQueue();
  EXPECT_EQ(11u, d.capacity());
}

TEST_F(LazilyDeallocatedDequeTest, MaybeShrinkQueueWithSmallSizeDrop) {
  LazilyDeallocatedDeque<int> d;

  for (int i = 0; i < 1010; i++) {
    d.push_back(i);
  }
  EXPECT_EQ(1010u, d.size());
  EXPECT_EQ(1305u, d.capacity());
  EXPECT_EQ(1010u, d.max_size());

  // Drop a couple of elements.
  d.pop_front();
  d.pop_front();
  EXPECT_EQ(1008u, d.size());
  EXPECT_EQ(1305u, d.capacity());
  EXPECT_EQ(1010u, d.max_size());

  // This won't do anything since the max size is only slightly lower than the
  // capacity.
  EXPECT_EQ(1305u, d.capacity());
  EXPECT_EQ(1010u, d.max_size());

  // Ditto. Nothing changed so no point shrinking.
  d.MaybeShrinkQueue();
  EXPECT_EQ(1008u, d.max_size());
  EXPECT_EQ(1011u, d.capacity());
}

TEST_F(LazilyDeallocatedDequeTest, MaybeShrinkQueueToEmpty) {
  LazilyDeallocatedDeque<int> d;

  for (int i = 0; i < 1000; i++) {
    d.push_front(i);
  }

  for (int i = 0; i < 1000; i++) {
    d.pop_front();
  }

  d.MaybeShrinkQueue();
  EXPECT_EQ(0u, d.max_size());
  EXPECT_EQ(LazilyDeallocatedDeque<int>::kMinimumRingSize, d.capacity());
}

TEST_F(LazilyDeallocatedDequeTest, MaybeShrinkQueueRateLimiting) {
  ScopedMockClockOverride clock;
  LazilyDeallocatedDeque<int> d;

  for (int i = 0; i < 1000; i++) {
    d.push_back(i);
  }
  EXPECT_EQ(1000u, d.size());
  EXPECT_EQ(1305u, d.capacity());
  EXPECT_EQ(1000u, d.max_size());

  // Drop some elements.
  for (int i = 0; i < 400; i++) {
    d.pop_front();
  }
  EXPECT_EQ(600u, d.size());
  EXPECT_EQ(947u, d.capacity());
  EXPECT_EQ(1000u, d.max_size());

  // This won't do anything since the max size is greater than the current
  // capacity.
  d.MaybeShrinkQueue();
  EXPECT_EQ(947u, d.capacity());
  EXPECT_EQ(600u, d.max_size());

  // This will shrink to fit.
  d.MaybeShrinkQueue();
  EXPECT_EQ(601u, d.capacity());
  EXPECT_EQ(600u, d.max_size());

  // Drop some more elements.
  for (int i = 0; i < 100; i++) {
    d.pop_front();
  }
  EXPECT_EQ(500u, d.size());
  EXPECT_EQ(601u, d.capacity());
  EXPECT_EQ(600u, d.max_size());

  // Not enough time has passed so max_size is untouched and not shrunk.
  d.MaybeShrinkQueue();
  EXPECT_EQ(601u, d.capacity());
  EXPECT_EQ(600u, d.max_size());

  // After time passes we re-sample max_size.
  clock.Advance(
      Seconds(LazilyDeallocatedDeque<int>::kMinimumShrinkIntervalInSeconds));
  d.MaybeShrinkQueue();
  EXPECT_EQ(601u, d.capacity());
  EXPECT_EQ(500u, d.max_size());

  // And The next call to MaybeShrinkQueue actually shrinks the queue.
  d.MaybeShrinkQueue();
  EXPECT_EQ(501u, d.capacity());
  EXPECT_EQ(500u, d.max_size());
}

TEST_F(LazilyDeallocatedDequeTest, Iterators) {
  LazilyDeallocatedDeque<int> d;

  d.push_back(1);
  d.push_back(2);
  d.push_back(3);

  auto iter = d.begin();
  EXPECT_EQ(1, *iter);
  EXPECT_NE(++iter, d.end());

  EXPECT_EQ(2, *iter);
  EXPECT_NE(++iter, d.end());

  EXPECT_EQ(3, *iter);
  EXPECT_EQ(++iter, d.end());
}

TEST_F(LazilyDeallocatedDequeTest, PushBackAndFront) {
  LazilyDeallocatedDeque<int> d;

  int j = 1;
  for (int i = 0; i < 1000; i++) {
    d.push_back(j++);
    d.push_back(j++);
    d.push_back(j++);
    d.push_back(j++);
    d.push_front(-i);
  }

  for (int i = -999; i < 4000; i++) {
    EXPECT_EQ(d.front(), i);
    d.pop_front();
  }
}

TEST_F(LazilyDeallocatedDequeTest, PushBackThenSetCapacity) {
  LazilyDeallocatedDeque<int> d;
  for (int i = 0; i < 1000; i++) {
    d.push_back(i);
  }

  EXPECT_EQ(1305u, d.capacity());

  // We need 1 more spot than the size due to the way the Ring works.
  d.SetCapacity(1001);

  EXPECT_EQ(1000u, d.size());
  EXPECT_EQ(0, d.front());
  EXPECT_EQ(999, d.back());

  for (int i = 0; i < 1000; i++) {
    EXPECT_EQ(d.front(), i);
    d.pop_front();
  }
}

TEST_F(LazilyDeallocatedDequeTest, PushFrontThenSetCapacity) {
  LazilyDeallocatedDeque<int> d;
  for (int i = 0; i < 1000; i++) {
    d.push_front(i);
  }

  EXPECT_EQ(1336u, d.capacity());

  // We need 1 more spot than the size due to the way the Ring works.
  d.SetCapacity(1001);

  EXPECT_EQ(1000u, d.size());
  EXPECT_EQ(999, d.front());
  EXPECT_EQ(0, d.back());

  for (int i = 0; i < 1000; i++) {
    EXPECT_EQ(d.front(), 999 - i);
    d.pop_front();
  }
}

TEST_F(LazilyDeallocatedDequeTest, PushFrontThenSetCapacity2) {
  LazilyDeallocatedDeque<std::unique_ptr<int>> d;
  for (int i = 0; i < 1000; i++) {
    d.push_front(std::make_unique<int>(i));
  }

  EXPECT_EQ(1336u, d.capacity());

  // We need 1 more spot than the size due to the way the Ring works.
  d.SetCapacity(1001);

  EXPECT_EQ(1000u, d.size());
  EXPECT_EQ(999, *d.front());
  EXPECT_EQ(0, *d.back());

  for (int i = 0; i < 1000; i++) {
    EXPECT_EQ(*d.front(), 999 - i);
    d.pop_front();
  }
}

TEST_F(LazilyDeallocatedDequeTest, PushBackAndFrontThenSetCapacity) {
  LazilyDeallocatedDeque<int> d;

  int j = 1;
  for (int i = 0; i < 1000; i++) {
    d.push_back(j++);
    d.push_back(j++);
    d.push_back(j++);
    d.push_back(j++);
    d.push_front(-i);
  }

  d.SetCapacity(5001);

  EXPECT_EQ(5000u, d.size());
  EXPECT_EQ(-999, d.front());
  EXPECT_EQ(4000, d.back());

  for (int i = -999; i < 4000; i++) {
    EXPECT_EQ(d.front(), i);
    d.pop_front();
  }
}

TEST_F(LazilyDeallocatedDequeTest, RingPushFront) {
  LazilyDeallocatedDeque<int>::Ring r(4);

  r.push_front(1);
  r.push_front(2);
  r.push_front(3);

  EXPECT_EQ(3, r.front());
  EXPECT_EQ(1, r.back());
}

TEST_F(LazilyDeallocatedDequeTest, RingPushBack) {
  LazilyDeallocatedDeque<int>::Ring r(4);

  r.push_back(1);
  r.push_back(2);
  r.push_back(3);

  EXPECT_EQ(1, r.front());
  EXPECT_EQ(3, r.back());
}

TEST_F(LazilyDeallocatedDequeTest, RingCanPush) {
  LazilyDeallocatedDeque<int>::Ring r1(4);
  LazilyDeallocatedDeque<int>::Ring r2(4);

  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(r1.CanPush());
    r1.push_back(0);

    EXPECT_TRUE(r2.CanPush());
    r2.push_back(0);
  }

  EXPECT_FALSE(r1.CanPush());
  EXPECT_FALSE(r2.CanPush());
}

TEST_F(LazilyDeallocatedDequeTest, RingPushPopPushPop) {
  LazilyDeallocatedDeque<int>::Ring r(4);

  EXPECT_FALSE(r.CanPop());
  EXPECT_TRUE(r.CanPush());
  r.push_back(1);
  EXPECT_TRUE(r.CanPop());
  EXPECT_TRUE(r.CanPush());
  r.push_back(2);
  EXPECT_TRUE(r.CanPush());
  r.push_back(3);
  EXPECT_FALSE(r.CanPush());

  EXPECT_TRUE(r.CanPop());
  EXPECT_EQ(1, r.front());
  r.pop_front();
  EXPECT_TRUE(r.CanPop());
  EXPECT_EQ(2, r.front());
  r.pop_front();
  EXPECT_TRUE(r.CanPop());
  EXPECT_EQ(3, r.front());
  r.pop_front();
  EXPECT_FALSE(r.CanPop());

  EXPECT_TRUE(r.CanPush());
  r.push_back(10);
  EXPECT_TRUE(r.CanPush());
  r.push_back(20);
  EXPECT_TRUE(r.CanPush());
  r.push_back(30);
  EXPECT_FALSE(r.CanPush());

  EXPECT_TRUE(r.CanPop());
  EXPECT_EQ(10, r.front());
  r.pop_front();
  EXPECT_TRUE(r.CanPop());
  EXPECT_EQ(20, r.front());
  r.pop_front();
  EXPECT_TRUE(r.CanPop());
  EXPECT_EQ(30, r.front());
  r.pop_front();

  EXPECT_FALSE(r.CanPop());
}

TEST_F(LazilyDeallocatedDequeTest, PushAndIterate) {
  LazilyDeallocatedDeque<int> d;

  for (int i = 0; i < 1000; i++) {
    d.push_front(i);
  }

  int expected = 999;
  for (int value : d) {
    EXPECT_EQ(expected, value);
    expected--;
  }
}

TEST_F(LazilyDeallocatedDequeTest, Swap) {
  LazilyDeallocatedDeque<int> a;
  LazilyDeallocatedDeque<int> b;

  a.push_back(1);
  a.push_back(2);

  for (int i = 0; i < 1000; i++) {
    b.push_back(i);
  }

  EXPECT_EQ(2u, a.size());
  EXPECT_EQ(1, a.front());
  EXPECT_EQ(2, a.back());
  EXPECT_EQ(1000u, b.size());
  EXPECT_EQ(0, b.front());
  EXPECT_EQ(999, b.back());

  a.swap(b);

  EXPECT_EQ(1000u, a.size());
  EXPECT_EQ(0, a.front());
  EXPECT_EQ(999, a.back());
  EXPECT_EQ(2u, b.size());
  EXPECT_EQ(1, b.front());
  EXPECT_EQ(2, b.back());
}

class DestructorTestItem {
 public:
  DestructorTestItem() : v_(-1) {}

  DestructorTestItem(int v) : v_(v) {}

  ~DestructorTestItem() { destructor_count_++; }

  int v_;
  static int destructor_count_;
};

int DestructorTestItem::destructor_count_ = 0;

TEST_F(LazilyDeallocatedDequeTest, PopFrontCallsDestructor) {
  LazilyDeallocatedDeque<DestructorTestItem> a;

  a.push_front(DestructorTestItem(123));

  DestructorTestItem::destructor_count_ = 0;
  a.pop_front();
  EXPECT_EQ(1, DestructorTestItem::destructor_count_);
}

TEST_F(LazilyDeallocatedDequeTest, ExpectedNumberOfDestructorsCalled) {
  {
    LazilyDeallocatedDeque<DestructorTestItem> a;

    for (int i = 0; i < 100; i++) {
      a.push_front(DestructorTestItem(i));
    }

    DestructorTestItem::destructor_count_ = 0;
  }

  EXPECT_EQ(100, DestructorTestItem::destructor_count_);
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
