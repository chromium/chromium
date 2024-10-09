// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/task/post_job.h"

#include <atomic>
#include <iterator>
#include <numeric>

#include "base/barrier_closure.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/platform_thread.h"
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
  EXPECT_EQ(num_tasks_to_run, 0U);
}

TEST(PostJobTest, CreateJobSimple) {
  test::TaskEnvironment task_environment;
  std::atomic_size_t num_tasks_to_run(4);
  TestWaitableEvent threads_continue;
  RepeatingClosure barrier = BarrierClosure(
      num_tasks_to_run,
      BindLambdaForTesting([&threads_continue] { threads_continue.Signal(); }));
  bool job_started = false;
  auto handle =
      CreateJob(FROM_HERE, {}, BindLambdaForTesting([&](JobDelegate* delegate) {
                  EXPECT_TRUE(job_started);
                  barrier.Run();
                  threads_continue.Wait();
                  --num_tasks_to_run;
                }),
                BindLambdaForTesting([&](size_t /*worker_count*/) -> size_t {
                  EXPECT_TRUE(job_started);
                  return num_tasks_to_run;
                }));

  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_EQ(num_tasks_to_run, 4U);
  job_started = true;
  handle.Join();
  EXPECT_EQ(num_tasks_to_run, 0U);
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
