// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/near_oom_monitor.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int64_t kTestSwapTotalKB = 128 * 1024;
const int64_t kTestSwapFreeThreshold = kTestSwapTotalKB / 4;
}  // namespace

class MockNearOomMonitor : public NearOomMonitor {
 public:
  explicit MockNearOomMonitor(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : NearOomMonitor(task_runner, kTestSwapFreeThreshold) {
    // Start with 128MB swap total and 64MB swap free.
    memory_info_.swap_total = kTestSwapTotalKB;
    memory_info_.swap_free = kTestSwapTotalKB / 2;
  }

  MockNearOomMonitor(const MockNearOomMonitor&) = delete;
  MockNearOomMonitor& operator=(const MockNearOomMonitor&) = delete;

  void SetSwapFree(int swap_free) { memory_info_.swap_free = swap_free; }

  void SimulateNonNearOom() {
    memory_info_.swap_free = memory_info_.swap_total;
  }

  void SimulateNearOom() { memory_info_.swap_free = 0; }

  bool is_get_system_memory_info_called() const {
    return is_get_system_memory_info_called_;
  }

 private:
  bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* memory_info) override {
    *memory_info = memory_info_;
    is_get_system_memory_info_called_ = true;
    return true;
  }

  bool ComponentCallbackIsEnabled() override { return false; }

  base::SystemMemoryInfoKB memory_info_;
  bool is_get_system_memory_info_called_ = false;
};

class TestNearOomObserver {
 public:
  explicit TestNearOomObserver(NearOomMonitor* monitor) {
    DCHECK(monitor);
    subscription_ = monitor->RegisterCallback(base::BindRepeating(
        &TestNearOomObserver::OnNearOomDetected, base::Unretained(this)));
  }

  TestNearOomObserver(const TestNearOomObserver&) = delete;
  TestNearOomObserver& operator=(const TestNearOomObserver&) = delete;

  void Unsubscribe() { subscription_ = {}; }

  bool is_detected() const { return is_detected_; }

 private:
  void OnNearOomDetected() { is_detected_ = true; }

  bool is_detected_ = false;
  base::CallbackListSubscription subscription_;
};

class NearOomMonitorTest : public testing::Test {
 public:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    monitor_ = std::make_unique<MockNearOomMonitor>(task_runner_);
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<MockNearOomMonitor> monitor_;
};

TEST_F(NearOomMonitorTest, Observe) {
  base::TimeDelta interval =
      monitor_->GetMonitoringInterval() + base::Seconds(1);

  TestNearOomObserver observer1(monitor_.get());
  TestNearOomObserver observer2(monitor_.get());

  monitor_->SimulateNonNearOom();
  task_runner_->FastForwardBy(interval);
  EXPECT_TRUE(monitor_->is_get_system_memory_info_called());
  EXPECT_FALSE(observer1.is_detected());
  EXPECT_FALSE(observer2.is_detected());

  observer2.Unsubscribe();

  monitor_->SimulateNearOom();
  task_runner_->FastForwardBy(interval);
  EXPECT_TRUE(observer1.is_detected());
  EXPECT_FALSE(observer2.is_detected());

  observer1.Unsubscribe();
}

TEST_F(NearOomMonitorTest, Cooldown) {
  base::TimeDelta interval =
      monitor_->GetMonitoringInterval() + base::Seconds(1);
  base::TimeDelta cooldown_interval =
      monitor_->GetCooldownInterval() + base::Seconds(1);

  monitor_->SimulateNearOom();

  // The first detection should happen when |interval| is passed.
  TestNearOomObserver observer1(monitor_.get());
  task_runner_->FastForwardBy(interval);
  EXPECT_TRUE(observer1.is_detected());
  observer1.Unsubscribe();

  // After a detection, the next detection shouldn't happen until
  // |cooldown_interval| is passed.
  TestNearOomObserver observer2(monitor_.get());
  task_runner_->FastForwardBy(interval);
  EXPECT_FALSE(observer2.is_detected());
  task_runner_->FastForwardBy(cooldown_interval);
  EXPECT_TRUE(observer2.is_detected());
  observer2.Unsubscribe();
}
