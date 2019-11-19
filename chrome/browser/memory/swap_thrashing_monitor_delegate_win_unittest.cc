// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/swap_thrashing_monitor_delegate_win.h"

#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {

namespace {

class MockHardFaultDeltasWindow;

class TestSwapThrashingMonitorDelegateWin
    : public SwapThrashingMonitorDelegateWin {
 public:
  using HardFaultDeltasWindow =
      SwapThrashingMonitorDelegateWin::HardFaultDeltasWindow;

  TestSwapThrashingMonitorDelegateWin()
      : SwapThrashingMonitorDelegateWin::SwapThrashingMonitorDelegateWin(),
        observation_override_(0U) {
    SetMockWindow();
  }
  ~TestSwapThrashingMonitorDelegateWin() override {}

  void SetMockWindow();

  bool RecordHardFaultCountForChromeProcesses() override {
    hard_fault_deltas_window_->OnObservation(observation_override_);
    return true;
  }

  // Override the next observations that the window will receive.
  void OverrideNextObservationDelta(uint64_t observation_delta) {
    observation_override_ += observation_delta;
  }

  MockHardFaultDeltasWindow* sample_window() { return mock_window_; }

 private:
  uint64_t observation_override_;

  MockHardFaultDeltasWindow* mock_window_;
};

// A mock for the HardFaultDeltasWindow class.
//
// Used to access some protected fields of the HardFaultDeltasWindow class.
class MockHardFaultDeltasWindow
    : public TestSwapThrashingMonitorDelegateWin::HardFaultDeltasWindow {
 public:
  MockHardFaultDeltasWindow()
      : TestSwapThrashingMonitorDelegateWin::HardFaultDeltasWindow() {}
  ~MockHardFaultDeltasWindow() override {}

  size_t SampleCount() { return observation_deltas_.size(); }

  size_t WindowLength() { return kHardFaultDeltasWindowSize; }
};

void TestSwapThrashingMonitorDelegateWin::SetMockWindow() {
  std::unique_ptr<MockHardFaultDeltasWindow> mock_window =
      std::make_unique<MockHardFaultDeltasWindow>();
  mock_window_ = mock_window.get();
  hard_fault_deltas_window_.reset(mock_window.release());
}

}  // namespace

TEST(SwapThrashingMonitorDelegateWinTest, StateTransition) {
  TestSwapThrashingMonitorDelegateWin monitor;

  // Arbitrarily chosen high thrashing rate.
  const uint64_t kHighSwappingRate = 5000;

  // This state test the transitions from the SWAP_THRASHING_LEVEL_NONE state
  // to the SWAP_THRASHING_LEVEL_CONFIRMED one and then ensure that the cooldown
  // mechanism works.

  // We expect the system to initially be in the SWAP_THRASHING_LEVEL_NONE
  // state.
  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE,
            monitor.SampleAndCalculateSwapThrashingLevel());

  // Fill the sample window with 0s.
  while (monitor.sample_window()->SampleCount() <
         monitor.sample_window()->WindowLength()) {
    monitor.OverrideNextObservationDelta(0);
    EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE,
              monitor.SampleAndCalculateSwapThrashingLevel());
  }

  EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE,
            monitor.SampleAndCalculateSwapThrashingLevel());
  EXPECT_EQ(monitor.sample_window()->SampleCount(),
            monitor.sample_window()->WindowLength());

  // Adds high samples until we reach the suspected status.
  size_t high_sample_count = 0;
  while (high_sample_count < monitor.sample_window()->WindowLength()) {
    high_sample_count++;
    monitor.OverrideNextObservationDelta(kHighSwappingRate);
    if (monitor.SampleAndCalculateSwapThrashingLevel() ==
        SwapThrashingLevel::SWAP_THRASHING_LEVEL_SUSPECTED) {
      break;
    }
  }
  size_t suspected_state_sample_count = high_sample_count;
  EXPECT_LT(suspected_state_sample_count,
            monitor.sample_window()->WindowLength());

  // And now add samples until we reach the confirmed value.
  while (high_sample_count < monitor.sample_window()->WindowLength()) {
    high_sample_count++;
    monitor.OverrideNextObservationDelta(kHighSwappingRate);
    if (monitor.SampleAndCalculateSwapThrashingLevel() ==
        SwapThrashingLevel::SWAP_THRASHING_LEVEL_CONFIRMED) {
      break;
    }
  }
  size_t confirmed_state_sample_count = high_sample_count;

  EXPECT_LE(confirmed_state_sample_count,
            monitor.sample_window()->WindowLength());
  EXPECT_GT(confirmed_state_sample_count, suspected_state_sample_count);

  // Finish filling the window with high values.
  while (high_sample_count < monitor.sample_window()->WindowLength()) {
    high_sample_count++;
    monitor.OverrideNextObservationDelta(kHighSwappingRate);
    EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_CONFIRMED,
              monitor.SampleAndCalculateSwapThrashingLevel());
  }

  // Add 0s to the windows, the thrashing level should slowly decrease.
  while (high_sample_count > 0) {
    monitor.OverrideNextObservationDelta(0);
    if (high_sample_count > confirmed_state_sample_count) {
      EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_CONFIRMED,
                monitor.SampleAndCalculateSwapThrashingLevel());
    } else if (high_sample_count > suspected_state_sample_count) {
      EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_SUSPECTED,
                monitor.SampleAndCalculateSwapThrashingLevel());
    } else {
      EXPECT_EQ(SwapThrashingLevel::SWAP_THRASHING_LEVEL_NONE,
                monitor.SampleAndCalculateSwapThrashingLevel());
    }
    high_sample_count--;
  }
}

}  // namespace memory
