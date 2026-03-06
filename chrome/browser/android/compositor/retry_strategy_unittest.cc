// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/retry_strategy.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/android/compositor/retryable_task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android {

namespace {

class MockRetryableTask : public RetryableTask {
 public:
  explicit MockRetryableTask(int success_at_retry)
      : success_at_retry_(success_at_retry) {}

  void Run(base::OnceCallback<void(bool)> should_retry_callback) override {
    run_count_++;
    bool should_retry = run_count_ <= success_at_retry_;
    std::move(should_retry_callback).Run(should_retry);
  }

  base::WeakPtr<RetryableTask> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  int run_count() const { return run_count_; }

 private:
  int run_count_ = 0;
  int success_at_retry_;
  base::WeakPtrFactory<MockRetryableTask> weak_factory_{this};
};

class ManualRetryableTask : public RetryableTask {
 public:
  ManualRetryableTask() = default;

  void Run(base::OnceCallback<void(bool)> should_retry_callback) override {
    run_count_++;
    should_retry_callback_ = std::move(should_retry_callback);
  }

  base::WeakPtr<RetryableTask> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  void Finish(bool should_retry) {
    std::move(should_retry_callback_).Run(should_retry);
  }

  int run_count() const { return run_count_; }

 private:
  int run_count_ = 0;
  base::OnceCallback<void(bool)> should_retry_callback_;
  base::WeakPtrFactory<ManualRetryableTask> weak_factory_{this};
};

}  // namespace

class RetryStrategyTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(RetryStrategyTest, SuccessFirstTry) {
  MockRetryableTask task(0);  // Succeeds on first try (run_count > 0)
  RetryStrategy strategy(3, base::Seconds(1));

  bool result = false;
  bool called = false;
  strategy.Start(&task, base::BindLambdaForTesting([&](bool success) {
    result = success;
    called = true;
  }));

  EXPECT_TRUE(called);
  EXPECT_TRUE(result);
  EXPECT_EQ(1, task.run_count());
}

TEST_F(RetryStrategyTest, SuccessAfterRetries) {
  MockRetryableTask task(2);  // Succeeds on 3rd try (run_count > 2)
  RetryStrategy strategy(3, base::Seconds(1));

  bool result = false;
  bool called = false;
  strategy.Start(&task, base::BindLambdaForTesting([&](bool success) {
    result = success;
    called = true;
  }));

  // First try fails.
  EXPECT_FALSE(called);
  EXPECT_EQ(1, task.run_count());

  // Wait for 1st retry.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(called);
  EXPECT_EQ(2, task.run_count());

  // Wait for 2nd retry.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(called);
  EXPECT_TRUE(result);
  EXPECT_EQ(3, task.run_count());
}

TEST_F(RetryStrategyTest, AllRetriesFail) {
  MockRetryableTask task(10);  // Never succeeds within 3 retries.
  RetryStrategy strategy(3, base::Seconds(1));

  bool result = true;
  bool called = false;
  strategy.Start(&task, base::BindLambdaForTesting([&](bool success) {
    result = success;
    called = true;
  }));

  // Initial + 3 retries = 4 total runs.
  for (int i = 0; i < 3; ++i) {
    EXPECT_FALSE(called);
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  EXPECT_TRUE(called);
  EXPECT_FALSE(result);
  EXPECT_EQ(4, task.run_count());
}

TEST_F(RetryStrategyTest, ZeroRetries) {
  MockRetryableTask task(1);  // Fails first try.
  RetryStrategy strategy(0, base::Seconds(1));

  bool result = true;
  bool called = false;
  strategy.Start(&task, base::BindLambdaForTesting([&](bool success) {
    result = success;
    called = true;
  }));

  EXPECT_TRUE(called);
  EXPECT_FALSE(result);
  EXPECT_EQ(1, task.run_count());
}

TEST_F(RetryStrategyTest, TaskDestroyedBeforeRetry) {
  auto task = std::make_unique<MockRetryableTask>(10);
  RetryStrategy strategy(3, base::Seconds(1));

  bool result = true;
  bool called = false;
  strategy.Start(task.get(), base::BindLambdaForTesting([&](bool success) {
                   result = success;
                   called = true;
                 }));

  // First try fails.
  EXPECT_FALSE(called);
  EXPECT_EQ(1, task->run_count());

  // Destroy task before first retry.
  task.reset();

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(called);
  EXPECT_FALSE(result);  // Should report failure if task is gone.
}

TEST_F(RetryStrategyTest, StrategyDestroyedBeforeRetry) {
  MockRetryableTask task(10);
  auto strategy = std::make_unique<RetryStrategy>(3, base::Seconds(1));

  bool called = false;
  strategy->Start(
      &task, base::BindLambdaForTesting([&](bool success) { called = true; }));

  // First try fails.
  EXPECT_FALSE(called);
  EXPECT_EQ(1, task.run_count());

  // Destroy strategy before first retry.
  strategy.reset();

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(called);            // Callback should not be called.
  EXPECT_EQ(1, task.run_count());  // No more runs.
}

TEST_F(RetryStrategyTest, StopWithoutRetrying) {
  ManualRetryableTask task;
  RetryStrategy strategy(3, base::Seconds(1));

  bool result = false;
  bool called = false;
  strategy.Start(&task, base::BindLambdaForTesting([&](bool success) {
    result = success;
    called = true;
  }));

  EXPECT_FALSE(called);
  EXPECT_EQ(1, task.run_count());

  // Task finishes and says it shouldn't retry.
  task.Finish(false);

  EXPECT_TRUE(called);
  EXPECT_TRUE(result);
  EXPECT_EQ(1, task.run_count());
}

}  // namespace android
