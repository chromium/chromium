// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace util {

TEST(MultiSourceMemoryPressureMonitorTest, RunDispatchCallback) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  MultiSourceMemoryPressureMonitor monitor;
  monitor.Start();
  auto* aggregator = monitor.aggregator_for_testing();

  bool callback_called = false;
  monitor.SetDispatchCallback(base::BindLambdaForTesting(
      [&](base::MemoryPressureListener::MemoryPressureLevel) {
        callback_called = true;
      }));
  aggregator->OnVoteForTesting(
      absl::nullopt, base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  aggregator->NotifyListenersForTesting();
  EXPECT_TRUE(callback_called);

  // Clear vote so aggregator's destructor doesn't think there are loose voters.
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, absl::nullopt);
}

}  // namespace util
