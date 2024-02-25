// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/mock_timer.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void CallMeMaybe(int *number) {
  (*number)++;
}

TEST(MockTimerTest, FiresOnce) {
  int calls = 0;
  base::MockOneShotTimer timer;
  base::TimeDelta delay = base::Seconds(2);
  timer.Start(FROM_HERE, delay,
              base::BindOnce(&CallMeMaybe, base::Unretained(&calls)));
  EXPECT_EQ(delay, timer.GetCurrentDelay());
  EXPECT_TRUE(timer.IsRunning());
  timer.Fire();
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_EQ(1, calls);
}

TEST(MockTimerTest, FiresRepeatedly) {
  int calls = 0;
  base::MockRepeatingTimer timer;
  base::TimeDelta delay = base::Seconds(2);
  timer.Start(FROM_HERE, delay,
              base::BindRepeating(&CallMeMaybe, base::Unretained(&calls)));
  timer.Fire();
  EXPECT_TRUE(timer.IsRunning());
  timer.Fire();
  timer.Fire();
  EXPECT_TRUE(timer.IsRunning());
  EXPECT_EQ(3, calls);
}

TEST(MockTimerTest, Stops) {
  int calls = 0;
  base::MockRepeatingTimer timer;
  base::TimeDelta delay = base::Seconds(2);
  timer.Start(FROM_HERE, delay,
              base::BindRepeating(&CallMeMaybe, base::Unretained(&calls)));
  EXPECT_TRUE(timer.IsRunning());
  timer.Stop();
  EXPECT_FALSE(timer.IsRunning());
}

TEST(MockOneShotTimerTest, FireNow) {
  int calls = 0;
  base::MockOneShotTimer timer;
  base::TimeDelta delay = base::Seconds(2);
  timer.Start(FROM_HERE, delay,
              base::BindOnce(&CallMeMaybe, base::Unretained(&calls)));
  EXPECT_EQ(delay, timer.GetCurrentDelay());
  EXPECT_TRUE(timer.IsRunning());
  timer.FireNow();
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_EQ(1, calls);
}

class HasWeakPtr {
 public:
  HasWeakPtr() = default;

  HasWeakPtr(const HasWeakPtr&) = delete;
  HasWeakPtr& operator=(const HasWeakPtr&) = delete;

  virtual ~HasWeakPtr() = default;

  base::WeakPtr<HasWeakPtr> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<HasWeakPtr> weak_ptr_factory_{this};
};

TEST(MockTimerTest, DoesNotRetainClosure) {
  HasWeakPtr *has_weak_ptr = new HasWeakPtr();
  base::WeakPtr<HasWeakPtr> weak_ptr(has_weak_ptr->AsWeakPtr());
  base::MockOneShotTimer timer;
  base::TimeDelta delay = base::Seconds(2);
  ASSERT_TRUE(weak_ptr.get());
  timer.Start(FROM_HERE, delay,
              base::BindOnce([](HasWeakPtr*) {}, base::Owned(has_weak_ptr)));
  ASSERT_TRUE(weak_ptr.get());
  timer.Fire();
  ASSERT_FALSE(weak_ptr.get());
}

}  // namespace
