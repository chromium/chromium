// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/enterprise_memory_limit_evaluator.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {

using memory_pressure::MultiSourceMemoryPressureMonitor;

TEST(EnterpriseMemoryLimitEvaluatorTest, OnProcessMemoryMetricsAvailable) {
  base::test::TaskEnvironment task_environment;

  MultiSourceMemoryPressureMonitor monitor;
  base::MockCallback<MultiSourceMemoryPressureMonitor::DispatchCallback>
      dispatch_callback;
  monitor.SetDispatchCallbackForTesting(dispatch_callback.Get());

  EXPECT_CALL(dispatch_callback, Run(base::MEMORY_PRESSURE_LEVEL_CRITICAL));

  EnterpriseMemoryLimitEvaluator evaluator(monitor.CreateVoter());
  evaluator.SetResidentSetLimitMb(1);
  auto observer = evaluator.StartForTesting();

  observer->OnProcessMemoryMetricsAvailableForTesting(1025);
  EXPECT_EQ(monitor.aggregator_for_testing()->EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_CRITICAL);
  testing::Mock::VerifyAndClearExpectations(&dispatch_callback);

  // MEMORY_PRESSURE_LEVEL_NONE should trigger dispatch callback.
  EXPECT_CALL(dispatch_callback, Run(base::MEMORY_PRESSURE_LEVEL_NONE));

  observer->OnProcessMemoryMetricsAvailableForTesting(1023);
  testing::Mock::VerifyAndClearExpectations(&dispatch_callback);
  EXPECT_EQ(monitor.aggregator_for_testing()->EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_NONE);

  // An additional MEMORY_PRESSURE_LEVEL_NONE notification should not trigger
  // another dispatch.
  EXPECT_CALL(dispatch_callback, Run(base::MEMORY_PRESSURE_LEVEL_NONE))
      .Times(0);

  observer->OnProcessMemoryMetricsAvailableForTesting(1023);
  testing::Mock::VerifyAndClearExpectations(&dispatch_callback);
  EXPECT_EQ(monitor.aggregator_for_testing()->EvaluateVotesForTesting(),
            base::MEMORY_PRESSURE_LEVEL_NONE);

  evaluator.StopForTesting();
}

}  // namespace memory
