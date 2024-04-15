// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/persistent_repeating_timer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using dips::PersistentRepeatingTimer;

// How long calls to SlowStorage::{Get,Set}LastFired() take.
constexpr base::TimeDelta kStorageDelay = base::Minutes(1);
// The delay between executions of the PersistentRepeatingTimer.
constexpr base::TimeDelta kTimerDelay = base::Hours(2);

// Storage that takes `kStorageDelay` for reads/writes to happen.
class SlowStorage : public PersistentRepeatingTimer::Storage {
 public:
  explicit SlowStorage(std::optional<base::Time> time = std::nullopt)
      : time_(time) {}

  void GetLastFired(TimeCallback callback) const override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SlowStorage::GetImpl, base::Unretained(this),
                       std::move(callback)),
        kStorageDelay);
  }

  void SetLastFired(base::Time time) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SlowStorage::SetImpl, base::Unretained(this), time),
        kStorageDelay);
  }

 private:
  void SetImpl(base::Time time) { time_ = time; }

  void GetImpl(TimeCallback callback) const { std::move(callback).Run(time_); }

  std::optional<base::Time> time_;
};

}  // namespace

class PersistentRepeatingTimerTest : public ::testing::Test {
 public:
  void RunTask() { ++call_count_; }

  void CheckCallCount(int call_count) {
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(call_count, call_count_);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  int call_count_ = 0;
};

// Checks that the missing pref is treated like an old one.
TEST_F(PersistentRepeatingTimerTest, MissingPref) {
  PersistentRepeatingTimer timer(
      std::make_unique<SlowStorage>(), kTimerDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  CheckCallCount(0);

  // The task is run immediately on start.
  timer.Start();
  CheckCallCount(0);
  task_environment_.FastForwardBy(kStorageDelay);
  CheckCallCount(1);

  task_environment_.FastForwardBy(base::Minutes(1));
  CheckCallCount(1);

  // And after the delay.
  task_environment_.FastForwardBy(kTimerDelay);
  CheckCallCount(2);
}

// Checks that spurious calls to Start() have no effect.
TEST_F(PersistentRepeatingTimerTest, MultipleStarts) {
  PersistentRepeatingTimer timer(
      std::make_unique<SlowStorage>(), kTimerDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  CheckCallCount(0);

  // The task is run immediately on start.
  timer.Start();
  CheckCallCount(0);
  timer.Start();
  CheckCallCount(0);
  task_environment_.FastForwardBy(kStorageDelay);
  CheckCallCount(1);

  task_environment_.FastForwardBy(base::Minutes(1));
  CheckCallCount(1);
  task_environment_.FastForwardBy(base::Minutes(1));
  timer.Start();
  CheckCallCount(1);

  // And after the delay.
  task_environment_.FastForwardBy(kTimerDelay);
  CheckCallCount(2);
  timer.Start();
  CheckCallCount(2);
}

TEST_F(PersistentRepeatingTimerTest, RecentPref) {
  PersistentRepeatingTimer timer(
      std::make_unique<SlowStorage>(base::Time::Now() - base::Hours(1)),
      kTimerDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  CheckCallCount(0);

  // The task is NOT run immediately on start.
  timer.Start();
  CheckCallCount(0);
  task_environment_.FastForwardBy(kStorageDelay);
  CheckCallCount(0);

  task_environment_.FastForwardBy(base::Minutes(1));
  CheckCallCount(0);

  // It is run after te delay.
  task_environment_.FastForwardBy(base::Hours(1));
  CheckCallCount(1);
  task_environment_.FastForwardBy(base::Hours(1));
  CheckCallCount(1);

  task_environment_.FastForwardBy(base::Hours(1));
  CheckCallCount(2);
}

TEST_F(PersistentRepeatingTimerTest, OldPref) {
  PersistentRepeatingTimer timer(
      std::make_unique<SlowStorage>(base::Time::Now() - base::Hours(10)),
      kTimerDelay,
      base::BindRepeating(&PersistentRepeatingTimerTest::RunTask,
                          base::Unretained(this)));
  CheckCallCount(0);

  // The task is run immediately on start.
  timer.Start();
  CheckCallCount(0);
  task_environment_.FastForwardBy(kStorageDelay);
  CheckCallCount(1);

  task_environment_.FastForwardBy(base::Minutes(1));
  CheckCallCount(1);

  // And after the delay.
  task_environment_.FastForwardBy(kTimerDelay);
  CheckCallCount(2);
}
