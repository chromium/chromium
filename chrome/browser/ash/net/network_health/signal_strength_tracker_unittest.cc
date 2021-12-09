// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_health/signal_strength_tracker.h"

#include <array>
#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr std::array<uint8_t, 3> samples = {71, 74, 80};
}  // namespace

namespace chromeos {
namespace network_health {

class SignalStrengthTrackerTest : public ::testing::Test {
 public:
  SignalStrengthTrackerTest() {}

  void SetUp() override {
    tracker_ = std::make_unique<SignalStrengthTracker>();
  }

 protected:
  void PopulateSamples() {
    for (auto s : samples) {
      tracker_->AddSample(s);
    }
  }

  std::unique_ptr<SignalStrengthTracker>& tracker() { return tracker_; }

 private:
  std::unique_ptr<SignalStrengthTracker> tracker_;
};

// Verify that the Average of samples is calculated correctly.
TEST_F(SignalStrengthTrackerTest, Average) {
  PopulateSamples();
  ASSERT_FLOAT_EQ(75.0, tracker()->Average());
}

// Verify that the Standard Deviation of samples is calculated correctly.
TEST_F(SignalStrengthTrackerTest, StdDev) {
  PopulateSamples();
  ASSERT_FLOAT_EQ(4.5825757, tracker()->StdDev());
}

// Verify that the samples lists match. The samples list inside of the tracker
// are stored with the most recent value first.
TEST_F(SignalStrengthTrackerTest, Samples) {
  PopulateSamples();
  auto stored_samples = tracker()->Samples();
  auto size = stored_samples.size();
  ASSERT_EQ(samples.size(), size);
  for (int i = 0; i < samples.size(); i++) {
    ASSERT_EQ(samples[i], stored_samples[size - i - 1]);
  }
}

// Verify that the size of the samples in the tracker do not exceed the max
// size.
TEST_F(SignalStrengthTrackerTest, SamplesSize) {
  auto num_samples = kSignalStrengthListSize + 10;
  for (int i = 0; i < num_samples; i++) {
    tracker()->AddSample(i);
  }

  ASSERT_EQ(tracker()->Samples().size(), kSignalStrengthListSize);
  ASSERT_EQ(tracker()->Samples()[0], num_samples - 1);
}

}  // namespace network_health
}  // namespace chromeos
