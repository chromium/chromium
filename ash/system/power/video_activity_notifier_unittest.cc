// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/video_activity_notifier.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/wm/video_detector.h"
#include "base/macros.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"

namespace ash {

class VideoActivityNotifierTest : public AshTestBase {
 public:
  VideoActivityNotifierTest() = default;
  ~VideoActivityNotifierTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    power_client_ = static_cast<chromeos::FakePowerManagerClient*>(
        chromeos::PowerManagerClient::Get());
    detector_ = std::make_unique<VideoDetector>();
    notifier_.reset(new VideoActivityNotifier(detector_.get()));
  }

  void TearDown() override {
    notifier_.reset();
    detector_.reset();
    AshTestBase::TearDown();
  }

 protected:
  chromeos::FakePowerManagerClient* power_client_;  // Not owned.

  std::unique_ptr<VideoDetector> detector_;
  std::unique_ptr<VideoActivityNotifier> notifier_;

  DISALLOW_COPY_AND_ASSIGN(VideoActivityNotifierTest);
};

// Test that powerd is notified immediately when video changes to a new playing
// state or the screen is unlocked.
TEST_F(VideoActivityNotifierTest, NotifyImmediatelyOnStateChange) {
  EXPECT_FALSE(power_client_->have_video_activity_report());

  notifier_->OnVideoStateChanged(VideoDetector::State::PLAYING_WINDOWED);
  EXPECT_FALSE(power_client_->PopVideoActivityReport());

  notifier_->OnVideoStateChanged(VideoDetector::State::PLAYING_FULLSCREEN);
  EXPECT_TRUE(power_client_->PopVideoActivityReport());

  notifier_->OnLockStateChanged(true);
  EXPECT_FALSE(power_client_->have_video_activity_report());

  notifier_->OnLockStateChanged(false);
  EXPECT_TRUE(power_client_->PopVideoActivityReport());

  notifier_->OnVideoStateChanged(VideoDetector::State::PLAYING_WINDOWED);
  EXPECT_FALSE(power_client_->PopVideoActivityReport());

  notifier_->OnVideoStateChanged(VideoDetector::State::NOT_PLAYING);
  EXPECT_FALSE(power_client_->have_video_activity_report());

  notifier_->OnVideoStateChanged(VideoDetector::State::PLAYING_WINDOWED);
  EXPECT_FALSE(power_client_->PopVideoActivityReport());
}

// Test that powerd is notified periodically while video is ongoing.
TEST_F(VideoActivityNotifierTest, NotifyPeriodically) {
  // The timer shouldn't be running initially.
  EXPECT_FALSE(notifier_->TriggerTimeoutForTest());

  // The timer should start in response to windowed video.
  notifier_->OnVideoStateChanged(VideoDetector::State::PLAYING_WINDOWED);
  EXPECT_FALSE(power_client_->PopVideoActivityReport());
  EXPECT_FALSE(power_client_->have_video_activity_report());
  EXPECT_TRUE(notifier_->TriggerTimeoutForTest());
  EXPECT_FALSE(power_client_->PopVideoActivityReport());
  EXPECT_FALSE(power_client_->have_video_activity_report());

  // After fullscreen video starts, the timer should start reporting that
  // instead.
  notifier_->OnVideoStateChanged(VideoDetector::State::PLAYING_FULLSCREEN);
  EXPECT_TRUE(power_client_->PopVideoActivityReport());
  EXPECT_FALSE(power_client_->have_video_activity_report());
  EXPECT_TRUE(notifier_->TriggerTimeoutForTest());
  EXPECT_TRUE(power_client_->PopVideoActivityReport());
  EXPECT_FALSE(power_client_->have_video_activity_report());

  // Locking the screen should stop the timer.
  notifier_->OnLockStateChanged(true);
  EXPECT_FALSE(notifier_->TriggerTimeoutForTest());
  EXPECT_FALSE(power_client_->have_video_activity_report());

  // Unlocking it should restart the timer.
  notifier_->OnLockStateChanged(false);
  EXPECT_TRUE(power_client_->PopVideoActivityReport());
  EXPECT_FALSE(power_client_->have_video_activity_report());
  EXPECT_TRUE(notifier_->TriggerTimeoutForTest());
  EXPECT_TRUE(power_client_->PopVideoActivityReport());
  EXPECT_FALSE(power_client_->have_video_activity_report());

  // The timer should stop when video video.
  notifier_->OnVideoStateChanged(VideoDetector::State::NOT_PLAYING);
  EXPECT_FALSE(notifier_->TriggerTimeoutForTest());
  EXPECT_FALSE(power_client_->have_video_activity_report());
}

}  // namespace ash
