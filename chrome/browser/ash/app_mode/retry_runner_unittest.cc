// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/retry_runner.h"

#include <optional>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::TestFuture;

namespace ash {

class RetryRunnerTest : public testing::Test {
 protected:
  using ResultCallback = base::OnceCallback<void(std::optional<int> result)>;

  // Task environment is needed because we call `PostDelayedTask()`.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(RetryRunnerTest, JobSucceedsOnFirstAttempt) {
  auto job = [](ResultCallback on_result) { std::move(on_result).Run(42); };

  TestFuture<std::optional<int>> result;
  auto handle = RunUpToNTimes<std::optional<int>>(
      /*n=*/1, base::BindRepeating(job), RetryIfNullopt<int>(),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(42, result.Take());
}

TEST_F(RetryRunnerTest, JobSucceedsOnSecondAttempt) {
  bool did_run = false;
  auto job = [&](ResultCallback on_result) {
    did_run ? std::move(on_result).Run(42)
            : std::move(on_result).Run(std::nullopt);
    did_run = true;
  };

  TestFuture<std::optional<int>> result;
  auto handle = RunUpToNTimes<std::optional<int>>(
      /*n=*/2, base::BindLambdaForTesting(job), RetryIfNullopt<int>(),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(42, result.Take());
}

TEST_F(RetryRunnerTest, JobThatFailsReturnsNullopt) {
  auto job = [](ResultCallback on_result) {
    std::move(on_result).Run(std::nullopt);
  };

  TestFuture<std::optional<int>> result;
  auto handle = RunUpToNTimes<std::optional<int>>(
      /*n=*/5, base::BindRepeating(job), RetryIfNullopt<int>(),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(std::nullopt, result.Take());
}

TEST_F(RetryRunnerTest, ReturnsOnFirstSuccessfulAttempt) {
  int attempts = 0;
  auto job = [&](ResultCallback on_result) {
    attempts++;
    attempts == 3 ? std::move(on_result).Run(42)
                  : std::move(on_result).Run(std::nullopt);
  };

  TestFuture<std::optional<int>> result;
  auto handle = RunUpToNTimes<std::optional<int>>(
      /*n=*/5, base::BindLambdaForTesting(job), RetryIfNullopt<int>(),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(42, result.Take());
  EXPECT_EQ(3, attempts);
}

TEST_F(RetryRunnerTest, RetriesNTimes) {
  int attempts = 0;
  auto job = [&](ResultCallback on_result) {
    attempts += 1;
    std::move(on_result).Run(std::nullopt);
  };

  TestFuture<std::optional<int>> result;
  auto handle = RunUpToNTimes<std::optional<int>>(
      /*n=*/5, base::BindLambdaForTesting(job), RetryIfNullopt<int>(),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(std::nullopt, result.Take());
  EXPECT_EQ(5, attempts);
}

TEST_F(RetryRunnerTest, DestroyingTaskCancelsIt) {
  auto job = [](ResultCallback on_result) {
    std::move(on_result).Run(std::nullopt);
  };

  TestFuture<std::optional<int>> result;
  auto handle = RunUpToNTimes<std::optional<int>>(
      /*n=*/5, base::BindRepeating(job), RetryIfNullopt<int>(),
      /*on_done=*/result.GetCallback());

  handle.reset();

  base::RunLoop loop;
  loop.RunUntilIdle();
  EXPECT_FALSE(result.IsReady());
}

TEST_F(RetryRunnerTest, WorksWithCancellableJob) {
  auto job = [&](ResultCallback on_result) {
    std::move(on_result).Run(42);
    return std::unique_ptr<CancellableJob>{};
  };

  TestFuture<std::optional<int>> result;
  auto handle = RunUpToNTimes<std::optional<int>>(
      /*n=*/8, base::BindLambdaForTesting(job), RetryIfNullopt<int>(),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(42, result.Take());
}

// Helper class that calls the given callback when destroyed.
class DestructorNotifer : public CancellableJob {
 public:
  explicit DestructorNotifer(base::OnceClosure on_destroy)
      : on_destroy_(std::move(on_destroy)) {}
  ~DestructorNotifer() override { std::move(on_destroy_).Run(); }

 private:
  base::OnceClosure on_destroy_;
};

TEST_F(RetryRunnerTest, DestroysHandleAfterAttempts) {
  constexpr int successful_attempt = 3;
  TestFuture<void> all_destroyed;
  auto on_destroy = base::BarrierClosure(3, all_destroyed.GetCallback());
  int attempts = 0;
  auto job = [&](ResultCallback on_result) -> std::unique_ptr<CancellableJob> {
    attempts++;
    attempts == successful_attempt ? std::move(on_result).Run(42)
                                   : std::move(on_result).Run(std::nullopt);
    return std::make_unique<DestructorNotifer>(on_destroy);
  };

  TestFuture<std::optional<int>> result;
  auto handle = RunUpToNTimes<std::optional<int>>(
      /*n=*/4, base::BindLambdaForTesting(job), RetryIfNullopt<int>(),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(42, result.Take());
  EXPECT_EQ(successful_attempt, attempts);
  EXPECT_TRUE(all_destroyed.IsReady()) << "Handle not destroyed";
  EXPECT_NE(handle, nullptr);
}

TEST_F(RetryRunnerTest, KeepsHandleAliveForJobDuration) {
  ResultCallback job_result;
  TestFuture<void> destroyed;
  auto job = [&](ResultCallback on_result) -> std::unique_ptr<CancellableJob> {
    job_result = std::move(on_result);
    return std::make_unique<DestructorNotifer>(destroyed.GetCallback());
  };

  TestFuture<std::optional<int>> result;
  auto handle = RunUpToNTimes<std::optional<int>>(
      /*n=*/4, base::BindLambdaForTesting(job), RetryIfNullopt<int>(),
      /*on_done=*/result.GetCallback());

  // The job started and the handle has not been destroyed.
  ASSERT_FALSE(job_result.is_null()) << "Job not started";
  EXPECT_FALSE(destroyed.IsReady()) << "Handle already destroyed";

  // Then once the current job finishes, the handle is destroyed.
  std::move(job_result).Run(std::nullopt);
  EXPECT_TRUE(destroyed.Wait()) << "Handle not destroyed";

  EXPECT_NE(handle, nullptr);
}

}  // namespace ash
