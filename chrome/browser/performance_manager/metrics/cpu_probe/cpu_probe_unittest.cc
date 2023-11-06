// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/cpu_probe/cpu_probe.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/metrics/cpu_probe/cpu_probe.h"
#include "chrome/browser/performance_manager/metrics/cpu_probe/pressure_sample.h"
#include "chrome/browser/performance_manager/metrics/cpu_probe/pressure_test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::metrics {

class CpuProbeTest : public testing::Test {
 public:
  CpuProbeTest() : cpu_probe_(std::make_unique<FakeCpuProbe>()) {}

  void WaitForUpdate() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::RunLoop run_loop;
    cpu_probe_->RequestSample(
        base::BindOnce(&CpuProbeTest::CollectorCallback, base::Unretained(this))
            .Then(run_loop.QuitClosure()));
    run_loop.Run();
  }

  void CollectorCallback(absl::optional<PressureSample> sample) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (sample.has_value()) {
      samples_.push_back(sample->cpu_utilization);
    }
  }

  void RequestRepeatedUpdates(base::TimeDelta interval) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    timer_.Start(
        FROM_HERE, interval, base::BindLambdaForTesting([&] {
          cpu_probe_->RequestSample(base::BindOnce(
              &CpuProbeTest::CollectorCallback, base::Unretained(this)));
        }));
  }

 protected:
  SEQUENCE_CHECKER(sequence_checker_);

  base::test::TaskEnvironment task_environment_;

  // This member is a std::unique_ptr instead of a plain CpuProbe
  // so it can be replaced inside tests.
  std::unique_ptr<CpuProbe> cpu_probe_;

  // The samples reported by the callback.
  std::vector<double> samples_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer used to request samples repeatedly.
  base::RepeatingTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);
};

using CpuProbeDeathTest = CpuProbeTest;

TEST_F(CpuProbeTest, RequestSample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static_cast<FakeCpuProbe*>(cpu_probe_.get())
      ->SetLastSample(PressureSample{0.9});
  cpu_probe_->StartSampling();
  WaitForUpdate();

  EXPECT_THAT(samples_, testing::ElementsAre(0.9));
}

TEST_F(CpuProbeTest, RepeatedSamples) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PressureSample> samples = {
      // Value right after construction.
      PressureSample{0.6},
      // Value after first Update(), should be discarded.
      PressureSample{0.9},
      // Value after second Update(), should be reported.
      PressureSample{0.4},
  };

  base::RunLoop run_loop;
  cpu_probe_ = std::make_unique<StreamingCpuProbe>(std::move(samples),
                                                   run_loop.QuitClosure());
  cpu_probe_->StartSampling();
  RequestRepeatedUpdates(base::Milliseconds(1));
  run_loop.Run();

  EXPECT_THAT(samples_, testing::ElementsAre(0.4));
}

TEST_F(CpuProbeTest, DestroyWhileSampling) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static_cast<FakeCpuProbe*>(cpu_probe_.get())
      ->SetLastSample(PressureSample{0.9});
  cpu_probe_->StartSampling();

  // Test passes as long as it doesn't crash.
  base::RunLoop run_loop;
  cpu_probe_->RequestSample(base::BindOnce(
      [](base::ScopedClosureRunner quit_closure_runner,
         absl::optional<PressureSample>) {
        // `quit_closure_runner` will run the bound `quit_closure` when the
        // callback is destroyed. The callback function shouldn't actually
        // execute because the CpuProbe is destroyed before the RunLoop
        // starts, so there's no object to receive it.
        FAIL();
      },
      base::ScopedClosureRunner(run_loop.QuitClosure())));
  cpu_probe_.reset();
  run_loop.Run();
}

TEST_F(CpuProbeDeathTest, CalculateStateWrongValue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PressureSample> samples = {
      // Value right after construction.
      PressureSample{0.6},
      // Value after first Update(), should be discarded.
      PressureSample{0.9},
      // Crash expected.
      PressureSample{1.1},
  };

  base::RunLoop run_loop;
  cpu_probe_ = std::make_unique<StreamingCpuProbe>(std::move(samples),
                                                   run_loop.QuitClosure());
  cpu_probe_->StartSampling();
  RequestRepeatedUpdates(base::Milliseconds(1));
  EXPECT_CHECK_DEATH_WITH(run_loop.Run(), "cpu_utilization <= 1.0");
}

TEST_F(CpuProbeDeathTest, RequestSampleWithoutStartSampling) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static_cast<FakeCpuProbe*>(cpu_probe_.get())
      ->SetLastSample(PressureSample{0.9});
  EXPECT_CHECK_DEATH(WaitForUpdate());
}

TEST_F(CpuProbeDeathTest, StartSamplingTwice) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static_cast<FakeCpuProbe*>(cpu_probe_.get())
      ->SetLastSample(PressureSample{0.9});
  base::RunLoop run_loop;
  cpu_probe_->StartSampling(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_CHECK_DEATH(cpu_probe_->StartSampling());
}

}  // namespace performance_manager::metrics
