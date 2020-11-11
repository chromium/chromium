// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/system_memory_pressure_evaluator_mac.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace util {
namespace mac {

class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  using SystemMemoryPressureEvaluator::
      MemoryPressureLevelForMacMemoryPressureLevel;

  TestSystemMemoryPressureEvaluator(std::unique_ptr<MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(std::move(voter)) {}

  // A HistogramTester for verifying correct UMA stat generation.
  base::HistogramTester tester;

  // Sets the raw macOS memory pressure level read by the memory pressure
  // evaluator.
  int macos_pressure_level_for_testing_;

  // Exposes the UpdatePressureLevel() method for testing.
  void UpdatePressureLevel() {
    SystemMemoryPressureEvaluator::UpdatePressureLevel();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSystemMemoryPressureEvaluator);

  int GetMacMemoryPressureLevel() override {
    return macos_pressure_level_for_testing_;
  }
};

TEST(MacSystemMemoryPressureEvaluatorTest,
     MemoryPressureFromMacMemoryPressure) {
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(
                    DISPATCH_MEMORYPRESSURE_NORMAL));
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(
                    DISPATCH_MEMORYPRESSURE_WARN));
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(
                    DISPATCH_MEMORYPRESSURE_CRITICAL));
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(0));
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(3));
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(5));
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            TestSystemMemoryPressureEvaluator::
                MemoryPressureLevelForMacMemoryPressureLevel(-1));
}

TEST(MacSystemMemoryPressureEvaluatorTest, CurrentMemoryPressure) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  base::MemoryPressureListener::MemoryPressureLevel memory_pressure =
      evaluator.current_vote();
  EXPECT_TRUE(
      memory_pressure ==
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE ||
      memory_pressure ==
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE ||
      memory_pressure ==
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

TEST(MacSystemMemoryPressureEvaluatorTest, MemoryPressureConversion) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
  TestSystemMemoryPressureEvaluator evaluator(nullptr);

  evaluator.macos_pressure_level_for_testing_ = DISPATCH_MEMORYPRESSURE_NORMAL;
  evaluator.UpdatePressureLevel();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator.current_vote());

  evaluator.macos_pressure_level_for_testing_ = DISPATCH_MEMORYPRESSURE_WARN;
  evaluator.UpdatePressureLevel();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator.current_vote());

  evaluator.macos_pressure_level_for_testing_ =
      DISPATCH_MEMORYPRESSURE_CRITICAL;
  evaluator.UpdatePressureLevel();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator.current_vote());
}

}  // namespace mac
}  // namespace util
