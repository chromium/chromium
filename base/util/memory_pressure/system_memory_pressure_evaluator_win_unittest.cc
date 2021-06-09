// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/system_memory_pressure_evaluator_win.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace util {
namespace win {

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

  static const DWORDLONG kMBBytes = 1024 * 1024;

  explicit TestSystemMemoryPressureEvaluator(
      bool large_memory,
      std::unique_ptr<MemoryPressureVoter> voter)
      : SystemMemoryPressureEvaluator(std::move(voter)), mem_status_() {
    // Generate a plausible amount of memory.
    mem_status_.ullTotalPhys =
        static_cast<DWORDLONG>(GenerateTotalMemoryMb(large_memory)) * kMBBytes;

    // Rerun InferThresholds using the test fixture's GetSystemMemoryStatus.
    InferThresholds();
    // Stop the timer.
    StopObserving();
  }

  TestSystemMemoryPressureEvaluator(int system_memory_mb,
                                    int moderate_threshold_mb,
                                    int critical_threshold_mb)
      : SystemMemoryPressureEvaluator(moderate_threshold_mb,
                                      critical_threshold_mb,
                                      nullptr),
        mem_status_() {
    // Set the amount of system memory.
    mem_status_.ullTotalPhys =
        static_cast<DWORDLONG>(system_memory_mb * kMBBytes);

    // Stop the timer.
    StopObserving();
  }

  MOCK_METHOD1(OnMemoryPressure,
               void(base::MemoryPressureListener::MemoryPressureLevel level));

  // Generates an amount of total memory that is consistent with the requested
  // memory model.
  int GenerateTotalMemoryMb(bool large_memory) {
    int total_mb = 64;
    while (total_mb < SystemMemoryPressureEvaluator::kLargeMemoryThresholdMb)
      total_mb *= 2;
    if (large_memory)
      return total_mb * 2;
    return total_mb / 2;
  }

  // Sets up the memory status to reflect the provided absolute memory left.
  void SetMemoryFree(int phys_left_mb) {
    // ullTotalPhys is set in the constructor and not modified.

    // Set the amount of available memory.
    mem_status_.ullAvailPhys = static_cast<DWORDLONG>(phys_left_mb) * kMBBytes;
    DCHECK_LT(mem_status_.ullAvailPhys, mem_status_.ullTotalPhys);

    // These fields are unused.
    mem_status_.dwMemoryLoad = 0;
    mem_status_.ullTotalPageFile = 0;
    mem_status_.ullAvailPageFile = 0;
    mem_status_.ullTotalVirtual = 0;
    mem_status_.ullAvailVirtual = 0;
  }

  void SetNone() { SetMemoryFree(moderate_threshold_mb() + 1); }

  void SetModerate() { SetMemoryFree(moderate_threshold_mb() - 1); }

  void SetCritical() { SetMemoryFree(critical_threshold_mb() - 1); }

 private:
  bool GetSystemMemoryStatus(MEMORYSTATUSEX* mem_status) override {
    // Simply copy the memory status set by the test fixture.
    *mem_status = mem_status_;
    return true;
  }

  MEMORYSTATUSEX mem_status_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemMemoryPressureEvaluator);
};

class WinSystemMemoryPressureEvaluatorTest : public testing::Test {
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

// Tests the fundamental direct calculation of memory pressure with automatic
// small-memory thresholds.
TEST_F(WinSystemMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelSmall) {
  static const int kModerateMb =
      SystemMemoryPressureEvaluator::kSmallMemoryDefaultModerateThresholdMb;
  static const int kCriticalMb =
      SystemMemoryPressureEvaluator::kSmallMemoryDefaultCriticalThresholdMb;

  // Small-memory model.
  TestSystemMemoryPressureEvaluator evaluator(false, nullptr);

  EXPECT_EQ(kModerateMb, evaluator.moderate_threshold_mb());
  EXPECT_EQ(kCriticalMb, evaluator.critical_threshold_mb());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

// Tests the fundamental direct calculation of memory pressure with automatic
// large-memory thresholds.
TEST_F(WinSystemMemoryPressureEvaluatorTest,
       CalculateCurrentMemoryPressureLevelLarge) {
  static const int kModerateMb =
      SystemMemoryPressureEvaluator::kLargeMemoryDefaultModerateThresholdMb;
  static const int kCriticalMb =
      SystemMemoryPressureEvaluator::kLargeMemoryDefaultCriticalThresholdMb;

  // Large-memory model.
  TestSystemMemoryPressureEvaluator evaluator(true, nullptr);

  EXPECT_EQ(kModerateMb, evaluator.moderate_threshold_mb());
  EXPECT_EQ(kCriticalMb, evaluator.critical_threshold_mb());

  ASSERT_NO_FATAL_FAILURE(CalculateCurrentMemoryPressureLevelTest(&evaluator));
}

// Tests the fundamental direct calculation of memory pressure with manually
// specified threshold levels.
TEST_F(WinSystemMemoryPressureEvaluatorTest,
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
TEST_F(WinSystemMemoryPressureEvaluatorTest, CheckMemoryPressure) {
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

TEST_F(WinSystemMemoryPressureEvaluatorTest, OSSignalsMemoryPressureEvaluator) {
  MultiSourceMemoryPressureMonitor monitor;
  monitor.ResetSystemEvaluatorForTesting();
  testing::StrictMock<TestSystemMemoryPressureEvaluator> evaluator(
      true, monitor.CreateVoter());
  evaluator.CreateOSSignalPressureEvaluator(monitor.CreateVoter());

  // Mock function used to ensure that the proper memory pressure signals are
  // emitted.
  testing::MockFunction<void(base::MemoryPressureListener::MemoryPressureLevel)>
      mock_listener_function;
  base::MemoryPressureListener listener(
      FROM_HERE,
      base::BindLambdaForTesting(
          [&](base::MemoryPressureListener::MemoryPressureLevel level) {
            mock_listener_function.Call(level);
          }));

  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(
        mock_listener_function,
        Call(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL))
        .WillOnce(::testing::Invoke([&]() { std::move(quit_closure).Run(); }));
    // A manual-reset event that is not yet signaled.
    base::win::ScopedHandle event_low_memory(
        CreateEvent(nullptr, TRUE, FALSE, nullptr));
    auto* handle = event_low_memory.Get();
    // Replace the handle watched by the evaluator to be able to simulate a low
    // pressure OS notification.
    evaluator.ReplaceWatchedHandleForTesting(std::move(event_low_memory));
    ::SetEvent(handle);
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_listener_function);

    // |event_low_memory| will be automatically closed by the pressure
    // evaluator, no need to call CloseEvent on it.
  }

  {
    base::RunLoop run_loop;
    // The evaluator will automatically start watching for a high memory
    // notification after receiving the previous low memory notification, wait
    // for it to arrive.
    evaluator.WaitForHighMemoryNotificationForTesting(run_loop.QuitClosure());
    run_loop.Run();
    // There should be no MEMORY_PRESSURE_LEVEL_NONE notification emitted.
    testing::Mock::VerifyAndClearExpectations(&mock_listener_function);
  }

  // Do another low memory notification test to make sure that the evaluator
  // can detect several critical pressure sessions.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(
        mock_listener_function,
        Call(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL))
        .WillOnce(::testing::Invoke([&]() { std::move(quit_closure).Run(); }));
    base::win::ScopedHandle event_low_memory(
        CreateEvent(nullptr, TRUE, FALSE, nullptr));
    auto* handle = event_low_memory.Get();
    evaluator.ReplaceWatchedHandleForTesting(std::move(event_low_memory));
    ::SetEvent(handle);
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_listener_function);
  }
}

}  // namespace win
}  // namespace util
