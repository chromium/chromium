// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/retry_runner.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using base::test::TestFuture;

namespace ash {

class RetryRunnerTest : public testing::Test {
 protected:
  using ResultCallback = base::OnceCallback<void(absl::optional<int> result)>;

  // Task environment is needed because we call `PostDelayedTask()`.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(RetryRunnerTest, JobSucceedsOnFirstAttempt) {
  auto job = [](ResultCallback on_result) { std::move(on_result).Run(42); };

  TestFuture<absl::optional<int>> result;
  auto handle = RunUpToNTimes<int>(
      /*n=*/1, base::BindRepeating(job),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(42, result.Take());
}

TEST_F(RetryRunnerTest, JobSucceedsOnSecondAttempt) {
  bool did_run = false;
  auto job = [&](ResultCallback on_result) {
    did_run ? std::move(on_result).Run(42)
            : std::move(on_result).Run(absl::nullopt);
    did_run = true;
  };

  TestFuture<absl::optional<int>> result;
  auto handle = RunUpToNTimes<int>(
      /*n=*/2, base::BindLambdaForTesting(job),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(42, result.Take());
}

TEST_F(RetryRunnerTest, JobThatFailsReturnsNullopt) {
  auto job = [](ResultCallback on_result) {
    std::move(on_result).Run(absl::nullopt);
  };

  TestFuture<absl::optional<int>> result;
  auto handle = RunUpToNTimes<int>(
      /*n=*/5, base::BindRepeating(job),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(absl::nullopt, result.Take());
}

TEST_F(RetryRunnerTest, ReturnsOnFirstSuccessfulAttempt) {
  int attempts = 0;
  auto job = [&](ResultCallback on_result) {
    attempts++;
    attempts == 3 ? std::move(on_result).Run(42)
                  : std::move(on_result).Run(absl::nullopt);
  };

  TestFuture<absl::optional<int>> result;
  auto handle = RunUpToNTimes<int>(
      /*n=*/5, base::BindLambdaForTesting(job),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(42, result.Take());
  EXPECT_EQ(3, attempts);
}

TEST_F(RetryRunnerTest, RetriesNTimes) {
  int attempts = 0;
  auto job = [&](ResultCallback on_result) {
    attempts += 1;
    std::move(on_result).Run(absl::nullopt);
  };

  TestFuture<absl::optional<int>> result;
  auto handle = RunUpToNTimes<int>(
      /*n=*/5, base::BindLambdaForTesting(job),
      /*on_done=*/result.GetCallback());

  EXPECT_EQ(absl::nullopt, result.Take());
  EXPECT_EQ(5, attempts);
}

TEST_F(RetryRunnerTest, DestroyingTaskCancelsIt) {
  auto job = [](ResultCallback on_result) {
    std::move(on_result).Run(absl::nullopt);
  };

  TestFuture<absl::optional<int>> result;
  auto handle = RunUpToNTimes<int>(
      /*n=*/5, base::BindRepeating(job),
      /*on_done=*/result.GetCallback());

  handle.reset();

  base::RunLoop loop;
  loop.RunUntilIdle();
  EXPECT_FALSE(result.IsReady());
}

}  // namespace ash
