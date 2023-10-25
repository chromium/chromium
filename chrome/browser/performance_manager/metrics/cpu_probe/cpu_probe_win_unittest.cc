// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/cpu_probe/cpu_probe_win.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/performance_manager/metrics/cpu_probe/cpu_probe.h"
#include "chrome/browser/performance_manager/metrics/cpu_probe/pressure_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::metrics {

class CpuProbeWinTest : public testing::Test {
 public:
  CpuProbeWinTest() = default;

  ~CpuProbeWinTest() override = default;

  void SetUp() override {
    probe_ = std::make_unique<FakePlatformCpuProbe<CpuProbeWin>>();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakePlatformCpuProbe<CpuProbeWin>> probe_;
};

TEST_F(CpuProbeWinTest, ProductionDataNoCrash) {
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), absl::nullopt)
      << "No baseline on first Update()";

  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  absl::optional<PressureSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_GE(sample->cpu_utilization, 0.0);
  EXPECT_LE(sample->cpu_utilization, 1.0);
}

}  // namespace performance_manager::metrics
