// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_callback.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(BarrierCallbackTest, RunsImmediatelyForZeroCallbacks) {
  bool done = true;
  auto barrier_callback = base::BarrierCallback<int>(
      0, base::BindLambdaForTesting([&done](std::vector<int> results) {
        EXPECT_THAT(results, testing::IsEmpty());
        done = true;
      }));
  EXPECT_TRUE(done);
}

TEST(BarrierCallbackTest, ErrorToCallCallbackWithZeroCallbacks) {
  auto barrier_callback =
      base::BarrierCallback<int>(0, base::DoNothing::Once<std::vector<int>>());
  EXPECT_FALSE(barrier_callback.is_null());

  EXPECT_CHECK_DEATH(barrier_callback.Run(3));
}

TEST(BarrierCallbackTest, RunAfterNumCallbacks) {
  bool done = false;
  auto barrier_callback = base::BarrierCallback<int>(
      3, base::BindLambdaForTesting([&done](std::vector<int> results) {
        EXPECT_THAT(results, testing::ElementsAre(1, 3, 2));
        done = true;
      }));
  EXPECT_FALSE(done);

  barrier_callback.Run(1);
  EXPECT_FALSE(done);

  barrier_callback.Run(3);
  EXPECT_FALSE(done);

  barrier_callback.Run(2);
  EXPECT_TRUE(done);
}

template <typename... Args>
class DestructionIndicator {
 public:
  // Sets `*destructed` to true in destructor.
  explicit DestructionIndicator(bool* destructed) : destructed_(destructed) {
    *destructed_ = false;
  }

  ~DestructionIndicator() { *destructed_ = true; }

  void DoNothing(Args...) {}

 private:
  bool* destructed_;
};

TEST(BarrierCallbackTest, ReleasesDoneCallbackWhenDone) {
  bool done_destructed = false;
  auto barrier_callback = base::BarrierCallback<bool>(
      1,
      base::BindOnce(&DestructionIndicator<std::vector<bool>>::DoNothing,
                     std::make_unique<DestructionIndicator<std::vector<bool>>>(
                         &done_destructed)));
  EXPECT_FALSE(done_destructed);
  barrier_callback.Run(true);
  EXPECT_TRUE(done_destructed);
}

// Tests a case when `done_callback` resets the `barrier_callback`.
// `barrier_callback` is a RepeatingCallback holding the `done_callback`.
// `done_callback` holds a reference back to the `barrier_callback`. When
// `barrier_callback` is Run() it calls `done_callback` which erases the
// `barrier_callback` while still inside of its Run(). The Run() implementation
// (in base::BarrierCallback) must not try use itself after executing
// ResetBarrierCallback() or this test would crash inside Run().
TEST(BarrierCallbackTest, KeepingCallbackAliveUntilDone) {
  base::RepeatingCallback<void(bool)> barrier_callback;
  barrier_callback = base::BarrierCallback<bool>(
      1, base::BindLambdaForTesting(
             [&barrier_callback](std::vector<bool> results) {
               barrier_callback = base::RepeatingCallback<void(bool)>();
               EXPECT_THAT(results, testing::ElementsAre(true));
             }));
  barrier_callback.Run(true);
  EXPECT_TRUE(barrier_callback.is_null());
}

TEST(BarrierCallbackTest, SupportsMoveonlyTypes) {
  class MoveOnly {
   public:
    MoveOnly() = default;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
  };

  // No need to assert anything here, since if BarrierCallback didn't work with
  // move-only types, this wouldn't compile.
  auto barrier_callback = base::BarrierCallback<MoveOnly>(
      1, base::DoNothing::Once<std::vector<MoveOnly>>());
  barrier_callback.Run(MoveOnly());

  auto barrier_callback2 = base::BarrierCallback<MoveOnly>(
      1, base::DoNothing::Once<const std::vector<MoveOnly>&>());
  barrier_callback2.Run(MoveOnly());
}

TEST(BarrierCallbackTest, SupportsConstRefResults) {
  auto barrier_callback = base::BarrierCallback<int>(
      1, base::DoNothing::Once<const std::vector<int>&>());

  barrier_callback.Run(1);
}

TEST(BarrierCallbackTest, SupportsReferenceTypes) {
  class Referenceable {
    // Must be copyable.
  };
  Referenceable ref;

  // No need to assert anything here, since if BarrierCallback didn't work with
  // by-reference args, this wouldn't compile.
  auto barrier_callback = base::BarrierCallback<const Referenceable&>(
      1, base::DoNothing::Once<std::vector<Referenceable>>());
  barrier_callback.Run(ref);

  auto barrier_callback2 = base::BarrierCallback<const Referenceable&>(
      1, base::DoNothing::Once<const std::vector<Referenceable>&>());
  barrier_callback2.Run(ref);
}

}  // namespace
