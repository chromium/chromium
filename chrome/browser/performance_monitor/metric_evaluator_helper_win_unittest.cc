// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_evaluator_helper_win.h"

#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {

class MetricEvaluatorsHelperWinTest : public testing::Test {
 public:
  MetricEvaluatorsHelperWinTest() = default;
  ~MetricEvaluatorsHelperWinTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  MetricEvaluatorsHelperWin metric_evaluator_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricEvaluatorsHelperWinTest);
};

TEST_F(MetricEvaluatorsHelperWinTest, GetFreeMemory) {
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        auto value = metric_evaluator_helper_.GetFreePhysicalMemoryMb();
        EXPECT_TRUE(value);
        EXPECT_GT(value.value(), 0);
      }));
  task_environment_.RunUntilIdle();
}

}  // namespace performance_monitor
