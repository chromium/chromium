// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/task_trace.h"

#include <ostream>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace debug {

TEST(TaskTraceTest, NoTask) {
  TaskTrace task_trace;
  EXPECT_TRUE(task_trace.empty());
  std::array<const void*, 4> addresses = {0};
  EXPECT_EQ(task_trace.GetAddresses(addresses), 0ul);
}

class ThreeTasksTest {
 public:
  ThreeTasksTest() = default;

  void Run() {
    task_runner->PostTask(FROM_HERE, base::BindOnce(&ThreeTasksTest::TaskA,
                                                    base::Unretained(this)));
    task_environment.RunUntilIdle();
  }

  void TaskA() {
    TaskTrace task_trace;
    EXPECT_FALSE(task_trace.empty());
    std::array<const void*, 4> addresses = {0};
    EXPECT_EQ(task_trace.GetAddresses(addresses), 1ul);
    task_a_address = addresses[0];
    task_runner->PostTask(FROM_HERE, base::BindOnce(&ThreeTasksTest::TaskB,
                                                    base::Unretained(this)));
  }

  void TaskB() {
    TaskTrace task_trace;
    EXPECT_FALSE(task_trace.empty());
    std::array<const void*, 4> addresses = {0};
    EXPECT_EQ(task_trace.GetAddresses(addresses), 2ul);
    task_b_address = addresses[0];
    EXPECT_EQ(addresses[1], task_a_address);
    task_runner->PostTask(FROM_HERE, base::BindOnce(&ThreeTasksTest::TaskC,
                                                    base::Unretained(this)));
  }

  void TaskC() {
    TaskTrace task_trace;
    EXPECT_FALSE(task_trace.empty());
    std::array<const void*, 4> addresses;
    EXPECT_EQ(task_trace.GetAddresses(addresses), 3ul);
    EXPECT_EQ(addresses[1], task_b_address);
    EXPECT_EQ(addresses[2], task_a_address);
  }

 private:
  base::test::TaskEnvironment task_environment;
  const raw_ref<base::SingleThreadTaskRunner> task_runner{
      *task_environment.GetMainThreadTaskRunner()};

  raw_ptr<const void> task_a_address = nullptr;
  raw_ptr<const void> task_b_address = nullptr;
};

TEST(TaskTraceTest, ThreeTasks) {
  ThreeTasksTest().Run();
}

}  // namespace debug
}  // namespace base
