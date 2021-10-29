// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/task_trace.h"

#include <ostream>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace debug {

TEST(TaskTraceTest, NoTask) {
  TaskTrace task_trace;
  EXPECT_TRUE(task_trace.empty());
  EXPECT_EQ(task_trace.AddressesForTesting().size(), 0ul);
}

class ThreeTasksTest {
 public:
  ThreeTasksTest() {}

  void Run() {
    task_runner.PostTask(FROM_HERE, base::BindOnce(&ThreeTasksTest::TaskA,
                                                   base::Unretained(this)));
    task_environment.RunUntilIdle();
  }

  void TaskA() {
    TaskTrace task_trace;
    EXPECT_FALSE(task_trace.empty());
    base::span<const void* const> addresses = task_trace.AddressesForTesting();
    EXPECT_EQ(addresses.size(), 1ul);
    task_a_address = addresses[0];
    task_runner.PostTask(FROM_HERE, base::BindOnce(&ThreeTasksTest::TaskB,
                                                   base::Unretained(this)));
  }

  void TaskB() {
    TaskTrace task_trace;
    EXPECT_FALSE(task_trace.empty());
    base::span<const void* const> addresses = task_trace.AddressesForTesting();
    EXPECT_EQ(addresses.size(), 2ul);
    task_b_address = addresses[0];
    EXPECT_EQ(addresses[1], task_a_address);
    task_runner.PostTask(FROM_HERE, base::BindOnce(&ThreeTasksTest::TaskC,
                                                   base::Unretained(this)));
  }

  void TaskC() {
    TaskTrace task_trace;
    EXPECT_FALSE(task_trace.empty());
    base::span<const void* const> addresses = task_trace.AddressesForTesting();
    EXPECT_EQ(addresses.size(), 3ul);
    EXPECT_EQ(addresses[1], task_b_address);
    EXPECT_EQ(addresses[2], task_a_address);
  }

 private:
  base::test::TaskEnvironment task_environment;
  base::SingleThreadTaskRunner& task_runner =
      *task_environment.GetMainThreadTaskRunner();

  const void* task_a_address = nullptr;
  const void* task_b_address = nullptr;
};

TEST(TaskTraceTest, ThreeTasks) {
  ThreeTasksTest().Run();
}

}  // namespace debug
}  // namespace base
