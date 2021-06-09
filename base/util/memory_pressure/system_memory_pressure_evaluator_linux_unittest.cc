// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/system_memory_pressure_evaluator_linux.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace util {
namespace os_linux {

namespace {

struct PressureSettings {
  int phys_left_mb;
  base::MemoryPressureListener::MemoryPressureLevel level;
};

}  // namespace

// This is outside of the anonymous namespace so that it can be seen as a friend
// to the evaluator class.
class TestSystemMemoryPressureEvaluator : public SystemMemoryPressureEvaluator {
 public:
  using SystemMemoryPressureEvaluator::CalculateCurrentPressureLevel;
  using SystemMemoryPressureEvaluator::CheckMemoryPressure;

  static const unsigned long kKiBperMiB = 1024;
  static const unsigned long kMemoryTotalMb = 4096;

  TestSystemMemoryPressureEvaluator(bool large_memory,
                                    std::unique_ptr<MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(std::move(voter)) {
    // Generate a plausible amount of memory.
    mem_status_.total = kMemoryTotalMb * kKiBperMiB;

    // Rerun InferThresholds.
    InferThresholds();
    // Stop the timer.
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(int system_memory_mb,
                                    int moderate_threshold_mb,
                                    int critical_threshold_mb)
      : SystemMemoryPressureEvaluator(moderate_threshold_mb,
                                      critical_threshold_mb,
                                      nullptr) {
    // Set the amount of system memory.
    mem_status_.total = system_memory_mb * kKiBperMiB;

    // Stop the timer.
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(const TestSystemMemoryPressureEvaluator&) =
      delete;
  TestSystemMemoryPressureEvaluator& operator=(
      const TestSystemMemoryPressureEvaluator&) = delete;

  MOCK_METHOD1(OnMemoryPressure,
               void(base::MemoryPressureListener::MemoryPressureLevel level));

  // Sets up the memory status to reflect the provided absolute memory left.
  void SetMemoryFree(int phys_left_mb) {
    // ullTotalPhys is set in the constructor and not modified.

    // Set the amount of available memory.
    mem_status_.available = phys_left_mb * kKiBperMiB;
    DCHECK_LT(mem_status_.available, mem_status_.total);
  }

  void SetNone() { SetMemoryFree(moderate_threshold_mb() + 1); }

  void SetModerate() { SetMemoryFree(moderate_threshold_mb() - 1); }

  void SetCritical() { SetMemoryFree(critical_threshold_mb() - 1); }

  bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* mem_info) override {
    *mem_info = mem_status_;
    return true;
  }

 private:
  base::SystemMemoryInfoKB mem_status_;
};

class LinuxSystemMemoryPressureEvaluatorTest : public testing::Test {
 protected:
  void CalculateCurrentMemoryPressureLevelTest(
      TestSystemMemoryPressureEvaluator* evaluator) {
    int mod = evaluator->moderate_threshold_mb();
    evaluator->SetMemoryFree(mod + 1);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(mod);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(mod - 1);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    int crit = evaluator->critical_threshold_mb();
    evaluator->SetMemoryFree(crit + 1);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(crit);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel());

    evaluator->SetMemoryFree(crit - 1);
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
              evaluator->CalculateCurrentPressureLevel());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

// Tests the fundamental direct calculation of memory pressure with manually
// specified threshold levels.
TEST_F(LinuxSystemMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelCustom) {
  static const int kSystemMb = 512;
  static const int kModerateMb = 256;
  static const int kCriticalMb = 128;

  TestSystemMemoryPressureEvaluator evaluator(kSystemMb, kModerateMb,
                                              kCriticalMb);

  EXPECT_EQ(kModerateMb, evaluator.moderate_threshold_mb());
  EXPECT_EQ(kCriticalMb, evaluator.critical_threshold_mb());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

// This test tests the various transition states from memory pressure, looking
// for the correct behavior on event reposting as well as state updates.
TEST_F(LinuxSystemMemoryPressureEvaluatorTest, CheckMemoryPressure) {
  MultiSourceMemoryPressureMonitor monitor;
  monitor.ResetSystemEvaluatorForTesting();

  // Large-memory.
  testing::StrictMock<TestSystemMemoryPressureEvaluator> evaluator(
      true, monitor.CreateVoter());

  base::MemoryPressureListener listener(
      FROM_HERE,
      base::BindRepeating(&TestSystemMemoryPressureEvaluator::OnMemoryPressure,
                          base::Unretained(&evaluator)));

  // Checking the memory pressure at 0% load should not produce any
  // events.
  evaluator.SetNone();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator.current_vote());

  // Setting the memory level to 80% should produce a moderate pressure level.
  EXPECT_CALL(
      evaluator,
      OnMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.SetModerate();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&evaluator);

  // Check that the event gets reposted after a while.
  const int kModeratePressureCooldownCycles =
      evaluator.kModeratePressureCooldown / evaluator.kMemorySamplingPeriod;

  for (int i = 0; i < kModeratePressureCooldownCycles; ++i) {
    if (i + 1 == kModeratePressureCooldownCycles) {
      EXPECT_CALL(
          evaluator,
          OnMemoryPressure(
              base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE));
    }
    evaluator.CheckMemoryPressure();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&evaluator);
  }

  // Setting the memory usage to 99% should produce critical levels.
  EXPECT_CALL(
      evaluator,
      OnMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator.SetCritical();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&evaluator);

  // Calling it again should immediately produce a second call.
  EXPECT_CALL(
      evaluator,
      OnMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL));
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&evaluator);

  // When lowering the pressure again there should be a notification and the
  // pressure should go back to moderate.
  EXPECT_CALL(
      evaluator,
      OnMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE));
  evaluator.SetModerate();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&evaluator);

  // Check that the event gets reposted after a while.
  for (int i = 0; i < kModeratePressureCooldownCycles; ++i) {
    if (i + 1 == kModeratePressureCooldownCycles) {
      EXPECT_CALL(
          evaluator,
          OnMemoryPressure(
              base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE));
    }
    evaluator.CheckMemoryPressure();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
              evaluator.current_vote());
    testing::Mock::VerifyAndClearExpectations(&evaluator);
  }

  // Going down to no pressure should not produce an notification.
  evaluator.SetNone();
  evaluator.CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator.current_vote());
  testing::Mock::VerifyAndClearExpectations(&evaluator);
}

}  // namespace os_linux
}  // namespace util
