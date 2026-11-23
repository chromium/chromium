// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/output_volume_monitor.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

// Returns an output node that isn't the primary active one, or 0.
uint64_t OtherOutputNode(ash::CrasAudioHandler& cras) {
  ash::AudioDeviceList devices;
  cras.GetAudioDevices(&devices);
  for (const ash::AudioDevice& device : devices) {
    if (!device.is_input && device.id != cras.GetPrimaryActiveOutputNode()) {
      return device.id;
    }
  }
  return 0;
}

class OutputVolumeMonitorChromeOSTest : public testing::Test {
 protected:
  ash::CrasAudioHandler& cras() { return cras_audio_handler_.Get(); }

  base::test::TaskEnvironment task_environment_;
  ash::ScopedCrasAudioHandlerForTesting cras_audio_handler_;
  std::vector<OutputVolumeState> reported_;
  std::unique_ptr<OutputVolumeMonitor> monitor_ =
      OutputVolumeMonitor::Create(base::BindRepeating(
          [](std::vector<OutputVolumeState>* reported,
             const OutputVolumeState& state) { reported->push_back(state); },
          &reported_));
};

TEST_F(OutputVolumeMonitorChromeOSTest, ReadsMuteAndVolume) {
  ASSERT_TRUE(monitor_);
  // Create() doesn't report the initial state.
  EXPECT_TRUE(reported_.empty());

  cras().SetOutputVolumePercent(40);
  cras().SetOutputMute(true);

  EXPECT_EQ(monitor_->GetState(),
            (OutputVolumeState{.muted = true, .level = 0.4f}));
}

TEST_F(OutputVolumeMonitorChromeOSTest, ReportsMuteChangesOnce) {
  ASSERT_TRUE(monitor_);
  cras().SetOutputVolumePercent(50);
  reported_.clear();

  cras().SetOutputMute(true);
  // Already muted: nothing new to report.
  cras().SetOutputMute(true);
  cras().SetOutputMute(false);

  EXPECT_THAT(reported_, testing::ElementsAre(
                             OutputVolumeState{.muted = true, .level = 0.5f},
                             OutputVolumeState{.muted = false, .level = 0.5f}));
}

TEST_F(OutputVolumeMonitorChromeOSTest, ReportsVolumeChangesOnce) {
  ASSERT_TRUE(monitor_);

  cras().SetOutputVolumePercent(5);
  // Same volume again: nothing new to report.
  cras().SetOutputVolumePercent(5);

  EXPECT_THAT(reported_, testing::ElementsAre(OutputVolumeState{
                             .muted = false, .level = 0.05f}));
}

// CRAS applies one volume to all active outputs, whichever reports it.
TEST_F(OutputVolumeMonitorChromeOSTest, ReportsVolumeOfSecondaryActiveOutput) {
  ASSERT_TRUE(monitor_);
  const uint64_t primary = cras().GetPrimaryActiveOutputNode();
  const uint64_t other = OtherOutputNode(cras());
  ASSERT_NE(other, 0u);
  ASSERT_TRUE(cras().SetActiveOutputNodes({primary, other}));
  reported_.clear();

  ash::FakeCrasAudioClient::Get()->NotifyOutputNodeVolumeChangedForTesting(
      other, 30);

  EXPECT_THAT(reported_, testing::ElementsAre(
                             OutputVolumeState{.muted = false, .level = 0.3f}));
}

TEST_F(OutputVolumeMonitorChromeOSTest, ReportsActiveOutputChange) {
  ASSERT_TRUE(monitor_);
  const uint64_t other = OtherOutputNode(cras());
  ASSERT_NE(other, 0u);
  // An inactive node's volume only goes to prefs until the node is active.
  cras().SetVolumeGainPercentForDevice(other, 20);
  // Real CRAS echoes volume changes asynchronously, after the switch.
  ash::FakeCrasAudioClient::Get()->send_volume_change_events_asynchronous();
  reported_.clear();

  ASSERT_TRUE(cras().SetActiveOutputNodes({other}));

  EXPECT_THAT(reported_, testing::ElementsAre(
                             OutputVolumeState{.muted = false, .level = 0.2f}));
  // The echo of the same volume changes nothing.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(reported_.size(), 1u);
}

TEST_F(OutputVolumeMonitorChromeOSTest, UnknownWithoutActiveOutput) {
  ASSERT_TRUE(monitor_);
  const uint64_t primary = cras().GetPrimaryActiveOutputNode();

  ASSERT_TRUE(cras().SetActiveOutputNodes({}));

  EXPECT_EQ(monitor_->GetState(), std::nullopt);
  EXPECT_TRUE(reported_.empty());

  // A known state is reported again once an output is active.
  ASSERT_TRUE(cras().SetActiveOutputNodes({primary}));
  EXPECT_EQ(reported_.size(), 1u);
}

}  // namespace

}  // namespace ttc
