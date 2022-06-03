// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/post_job.h"

#include <atomic>
#include <iterator>
#include <numeric>

#include "base/task/test_task_traits_extension.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(PostJobTest, PostJobSimple) {
  test::TaskEnvironment task_environment;
  std::atomic_size_t num_tasks_to_run(4);
  auto handle = PostJob(
      FROM_HERE, {},
      BindLambdaForTesting([&](JobDelegate* delegate) { --num_tasks_to_run; }),
      BindLambdaForTesting(
          [&](size_t /*worker_count*/) -> size_t { return num_tasks_to_run; }));
  handle.Join();
  DCHECK_EQ(num_tasks_to_run, 0U);
}

TEST(PostJobTest, PostJobExtension) {
  testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DCHECK_DEATH({
    auto handle = PostJob(
        FROM_HERE, TestExtensionBoolTrait(),
        BindRepeating([](JobDelegate* delegate) {}),
        BindRepeating([](size_t /*worker_count*/) -> size_t { return 0; }));
  });
}

// Verify that concurrent accesses with task_id as the only form of
// synchronisation doesn't trigger a race.
TEST(PostJobTest, TaskIds) {
  static constexpr size_t kNumConcurrentThreads = 2;
  static constexpr size_t kNumTasksToRun = 1000;
  base::test::TaskEnvironment task_environment;

  size_t concurrent_array[kNumConcurrentThreads] = {0};
  std::atomic_size_t remaining_tasks{kNumTasksToRun};
  base::JobHandle handle = base::PostJob(
      FROM_HERE, {}, BindLambdaForTesting([&](base::JobDelegate* job) {
        uint8_t id = job->GetTaskId();
        size_t& slot = concurrent_array[id];
        slot++;
        --remaining_tasks;
      }),
      BindLambdaForTesting([&remaining_tasks](size_t) {
        return std::min(remaining_tasks.load(), kNumConcurrentThreads);
      }));
  handle.Join();
  EXPECT_EQ(kNumTasksToRun, std::accumulate(std::begin(concurrent_array),
                                            std::end(concurrent_array), 0U));
}

}  // namespace base
