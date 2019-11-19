// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/post_job.h"

#include <atomic>

#include "base/task/test_task_traits_extension.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(PostJobTest, PostJobSimple) {
  test::TaskEnvironment task_environment;
  std::atomic_size_t num_tasks_to_run(4);
  auto handle = experimental::PostJob(
      FROM_HERE, ThreadPool(),
      BindLambdaForTesting(
          [&](experimental::JobDelegate* delegate) { --num_tasks_to_run; }),
      BindLambdaForTesting([&]() -> size_t { return num_tasks_to_run; }));
  handle.Join();
  DCHECK_EQ(num_tasks_to_run, 0U);
}

TEST(PostJobTest, PostJobExtension) {
  testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DCHECK_DEATH({
    auto handle = experimental::PostJob(
        FROM_HERE, TestExtensionBoolTrait(),
        BindRepeating([](experimental::JobDelegate* delegate) {}),
        BindRepeating([]() -> size_t { return 0; }));
  });
}

}  // namespace base