// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/enterprise_memory_limit_evaluator.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {

TEST(EnterpriseMemoryLimitEvaluatorTest, OnProcessMemoryMetricsAvailable) {
  base::test::TaskEnvironment task_environment;

  memory_pressure::MultiSourceMemoryPressureMonitor monitor;
  bool cb_called = false;
  monitor.SetDispatchCallbackForTesting(base::BindLambdaForTesting(
      [&](base::MemoryPressureListener::MemoryPressureLevel level) {
        EXPECT_EQ(level,
                  base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
        cb_called = true;
      }));

  EnterpriseMemoryLimitEvaluator evaluator(monitor.CreateVoter());
  evaluator.SetResidentSetLimitMb(1);
  auto observer = evaluator.StartForTesting();

  observer->OnProcessMemoryMetricsAvailableForTesting(1025);
  EXPECT_TRUE(cb_called);

  cb_called = false;

  // MEMORY_PRESSURE_LEVEL_NONE should not trigger dispatch callback.
  observer->OnProcessMemoryMetricsAvailableForTesting(1023);
  EXPECT_FALSE(cb_called);
  EXPECT_EQ(monitor.aggregator_for_testing()->EvaluateVotesForTesting(),
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);

  evaluator.StopForTesting();
}

}  // namespace memory
