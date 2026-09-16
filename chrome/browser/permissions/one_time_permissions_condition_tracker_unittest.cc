// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_condition_tracker.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ::testing::StrictMock;

constexpr base::TimeDelta kDelay = base::Seconds(10);

TEST(OneTimePermissionsConditionTrackerTest,
     DestructionCallbackInvokedOnRelease) {
  base::test::TaskEnvironment task_environment;
  base::MockOnceClosure destruction_callback;

  auto tracker = base::MakeRefCounted<OneTimePermissionsConditionTracker>(
      destruction_callback.Get());

  auto second_ref = tracker;
  tracker.reset();

  EXPECT_CALL(destruction_callback, Run());
  second_ref.reset();
}

TEST(OneTimePermissionsConditionTrackerTest,
     NullCallbackOnDestructionDoesNotCrash) {
  base::test::TaskEnvironment task_environment;
  auto tracker = base::MakeRefCounted<OneTimePermissionsConditionTracker>(
      base::OnceClosure());
  tracker.reset();
}

class OneTimePermissionsConditionTrackerFactoryTest : public testing::Test {
 public:
  OneTimePermissionsConditionTrackerFactoryTest() = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  StrictMock<base::MockRepeatingCallback<void(const url::Origin&)>> callback_;

  const url::Origin origin_a_ = url::Origin::Create(GURL("https://a.com"));
  const url::Origin origin_b_ = url::Origin::Create(GURL("https://b.com"));
};

TEST_F(OneTimePermissionsConditionTrackerFactoryTest,
       SingleReferenceZeroDelay) {
  OneTimePermissionsConditionTracker::Factory factory(callback_.Get(),
                                                      base::Seconds(0));

  auto tracker = factory.New(origin_a_);
  ASSERT_TRUE(tracker);

  tracker.reset();

  // The callback should not be invoked synchronously, but on the task runner.
  EXPECT_CALL(callback_, Run(origin_a_));
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(OneTimePermissionsConditionTrackerFactoryTest,
       SingleReferenceWithDelay) {
  OneTimePermissionsConditionTracker::Factory factory(callback_.Get(), kDelay);

  auto tracker = factory.New(origin_a_);
  ASSERT_TRUE(tracker);

  tracker.reset();

  task_environment_.FastForwardBy(kDelay / 2);

  EXPECT_CALL(callback_, Run(origin_a_));
  task_environment_.FastForwardBy(kDelay / 2);
}

TEST_F(OneTimePermissionsConditionTrackerFactoryTest,
       MultipleReferencesToSameOriginShareTracker) {
  OneTimePermissionsConditionTracker::Factory factory(callback_.Get(), kDelay);

  auto tracker1 = factory.New(origin_a_);
  auto tracker2 = factory.New(origin_a_);
  EXPECT_EQ(tracker1.get(), tracker2.get());

  tracker1.reset();
  // Timer should not start while tracker2 is still alive.
  task_environment_.FastForwardBy(kDelay * 2);

  tracker2.reset();
  task_environment_.FastForwardBy(kDelay / 2);

  EXPECT_CALL(callback_, Run(origin_a_));
  task_environment_.FastForwardBy(kDelay / 2);
}

TEST_F(OneTimePermissionsConditionTrackerFactoryTest,
       RenewReferenceBeforeTimerExpiresCancelsTimer) {
  OneTimePermissionsConditionTracker::Factory factory(callback_.Get(), kDelay);

  auto tracker1 = factory.New(origin_a_);
  tracker1.reset();

  // Half of the delay passes.
  task_environment_.FastForwardBy(kDelay / 2);

  // Acquire a new reference for the same origin. This should cancel the pending
  // timer.
  auto tracker2 = factory.New(origin_a_);

  // Fast-forward past the original expiration time. Callback should not be
  // invoked.
  task_environment_.FastForwardBy(kDelay);

  // Now release tracker2. A new timer should start.
  tracker2.reset();
  task_environment_.FastForwardBy(kDelay / 2);

  EXPECT_CALL(callback_, Run(origin_a_));
  task_environment_.FastForwardBy(kDelay / 2);
}

TEST_F(OneTimePermissionsConditionTrackerFactoryTest,
       MultipleOriginsTrackedIndependently) {
  OneTimePermissionsConditionTracker::Factory factory(callback_.Get(), kDelay);

  auto tracker_a = factory.New(origin_a_);
  auto tracker_b = factory.New(origin_b_);
  EXPECT_NE(tracker_a.get(), tracker_b.get());

  tracker_a.reset();

  // After kDelay, only origin A should be notified.
  EXPECT_CALL(callback_, Run(origin_a_));
  task_environment_.FastForwardBy(kDelay);

  tracker_b.reset();

  // After kDelay, origin B should be notified.
  EXPECT_CALL(callback_, Run(origin_b_));
  task_environment_.FastForwardBy(kDelay);
}

TEST_F(OneTimePermissionsConditionTrackerFactoryTest,
       FactoryDestructionCancelsPendingTimers) {
  auto factory = std::make_unique<OneTimePermissionsConditionTracker::Factory>(
      callback_.Get(), kDelay);

  auto tracker = factory->New(origin_a_);
  tracker.reset();

  // Destroy the factory while the timer is running.
  factory.reset();

  // Callback should not be invoked even after delay passes.
  task_environment_.FastForwardBy(kDelay);
}

TEST_F(OneTimePermissionsConditionTrackerFactoryTest,
       FactoryDestructionWhileReferenceAliveDoesNotCrash) {
  auto factory = std::make_unique<OneTimePermissionsConditionTracker::Factory>(
      callback_.Get(), kDelay);

  auto tracker = factory->New(origin_a_);

  // Destroy the factory first.
  factory.reset();

  // Releasing the tracker afterward should not crash or invoke the callback.
  tracker.reset();
  task_environment_.FastForwardBy(kDelay);
}

TEST_F(OneTimePermissionsConditionTrackerFactoryTest,
       RecreateTrackerAfterTimerExpires) {
  OneTimePermissionsConditionTracker::Factory factory(callback_.Get(), kDelay);

  auto tracker = factory.New(origin_a_);
  tracker.reset();

  EXPECT_CALL(callback_, Run(origin_a_));
  task_environment_.FastForwardBy(kDelay);

  // Create a new tracker for the same origin after the previous one expired.
  tracker = factory.New(origin_a_);
  ASSERT_TRUE(tracker);
  tracker.reset();

  EXPECT_CALL(callback_, Run(origin_a_));
  task_environment_.FastForwardBy(kDelay);
}

TEST_F(OneTimePermissionsConditionTrackerFactoryTest,
       RecreateTrackerInCallback) {
  OneTimePermissionsConditionTracker::Factory factory(callback_.Get(), kDelay);

  auto tracker = factory.New(origin_a_);
  tracker.reset();

  EXPECT_CALL(callback_, Run(origin_a_)).WillOnce([&] {
    tracker = factory.New(origin_a_);
  });
  task_environment_.FastForwardBy(kDelay);

  ASSERT_TRUE(tracker);
  tracker.reset();

  EXPECT_CALL(callback_, Run(origin_a_));
  task_environment_.FastForwardBy(kDelay);
}

TEST_F(OneTimePermissionsConditionTrackerFactoryTest, SetTaskRunnerForTesting) {
  OneTimePermissionsConditionTracker::Factory factory(callback_.Get(), kDelay);
  factory.SetTaskRunnerForTesting(task_environment_.GetMainThreadTaskRunner());

  auto tracker = factory.New(origin_a_);
  tracker.reset();

  EXPECT_CALL(callback_, Run(origin_a_));
  task_environment_.FastForwardBy(kDelay);
}

}  // namespace
