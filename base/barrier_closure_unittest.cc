// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(BarrierClosureTest, RunImmediatelyForZeroClosures) {
  int count = 0;
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      0, base::BindLambdaForTesting([&count]() { ++count; }));
  EXPECT_EQ(1, count);
}

TEST(BarrierClosureTest, ChecksIfCalledForZeroClosures) {
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(0, base::DoNothing());
  EXPECT_FALSE(barrier_closure.is_null());

  EXPECT_CHECK_DEATH(barrier_closure.Run());
}

TEST(BarrierClosureTest, RunAfterNumClosures) {
  int count = 0;
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2, base::BindLambdaForTesting([&count]() { ++count; }));
  EXPECT_EQ(0, count);

  barrier_closure.Run();
  EXPECT_EQ(0, count);

  barrier_closure.Run();
  EXPECT_EQ(1, count);
}

class DestructionIndicator {
 public:
  // Sets |*destructed| to true in destructor.
  DestructionIndicator(bool* destructed) : destructed_(destructed) {
    *destructed_ = false;
  }

  ~DestructionIndicator() { *destructed_ = true; }

  void DoNothing() {}

 private:
  bool* destructed_;
};

TEST(BarrierClosureTest, ReleasesDoneClosureWhenDone) {
  bool done_destructed = false;
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      1,
      base::BindOnce(&DestructionIndicator::DoNothing,
                     base::Owned(new DestructionIndicator(&done_destructed))));
  EXPECT_FALSE(done_destructed);
  barrier_closure.Run();
  EXPECT_TRUE(done_destructed);
}

// Tests a case when |done_closure| resets a |barrier_closure|.
// |barrier_closure| is a RepeatingClosure holding the |done_closure|.
// |done_closure| holds a pointer back to the |barrier_closure|. When
// |barrier_closure| is Run() it calls ResetBarrierClosure() which erases the
// |barrier_closure| while still inside of its Run(). The Run() implementation
// (in base::BarrierClosure) must not try use itself after executing
// ResetBarrierClosure() or this test would crash inside Run().
TEST(BarrierClosureTest, KeepingClosureAliveUntilDone) {
  base::RepeatingClosure barrier_closure;
  barrier_closure =
      base::BarrierClosure(1, base::BindLambdaForTesting([&barrier_closure]() {
                             barrier_closure = base::RepeatingClosure();
                           }));
  barrier_closure.Run();
  EXPECT_TRUE(barrier_closure.is_null());
}

}  // namespace
