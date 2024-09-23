// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_active_audio_throttle_observer.h"

#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

void TestCallback(int* counter,
                  int* active_counter,
                  const ash::ThrottleObserver* self) {
  (*counter)++;
  if (self->active()) {
    (*active_counter)++;
  }
}
}  // namespace

class ArcActiveAudioThrottleObserverTest : public testing::Test {
 public:
  using testing::Test::Test;

  ArcActiveAudioThrottleObserverTest(
      const ArcActiveAudioThrottleObserverTest&) = delete;
  ArcActiveAudioThrottleObserverTest& operator=(
      const ArcActiveAudioThrottleObserverTest&) = delete;

 protected:
  ArcActiveAudioThrottleObserver* observer() {
    return &audio_throttle_observer_;
  }

  ash::CrasAudioHandler& cras_audio_handler() {
    return cras_audio_handler_.Get();
  }

 private:
  ash::ScopedCrasAudioHandlerForTesting cras_audio_handler_;
  ArcActiveAudioThrottleObserver audio_throttle_observer_;
};

TEST_F(ArcActiveAudioThrottleObserverTest, TestConstructDestruct) {}

TEST_F(ArcActiveAudioThrottleObserverTest, TestStartStopSequence) {
  int call_count = 0;
  int active_count = 0;

  // ARC audio stream = 0, observer inactive on start observing.
  observer()->StartObserving(
      nullptr /* context */,
      base::BindRepeating(&TestCallback, &call_count, &active_count));
  EXPECT_EQ(1, call_count);
  EXPECT_EQ(0, active_count);
  EXPECT_FALSE(observer()->active());

  // ARC audio stream = 1, observer change to active
  observer()->OnNumberOfArcStreamsChanged(1);
  EXPECT_EQ(2, call_count);
  EXPECT_EQ(1, active_count);
  EXPECT_TRUE(observer()->active());

  // ARC audio stream = 2, observer still active
  observer()->OnNumberOfArcStreamsChanged(2);
  EXPECT_EQ(3, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_TRUE(observer()->active());

  // ARC audio stream = 0, observer change to inactive
  observer()->OnNumberOfArcStreamsChanged(0);
  EXPECT_EQ(4, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_FALSE(observer()->active());

  // ARC audio stream = 0, observer still inactive
  observer()->OnNumberOfArcStreamsChanged(0);
  EXPECT_EQ(5, call_count);
  EXPECT_EQ(2, active_count);
  EXPECT_FALSE(observer()->active());

  // ARC audio stream = 2, observer change to active
  observer()->OnNumberOfArcStreamsChanged(2);
  EXPECT_EQ(6, call_count);
  EXPECT_EQ(3, active_count);
  EXPECT_TRUE(observer()->active());
}

TEST_F(ArcActiveAudioThrottleObserverTest, TestActiveOnStartObserving) {
  int call_count = 0;
  int active_count = 0;

  // Set stream count = 1 before start observing
  cras_audio_handler().SetNumberOfArcStreamsForTesting(1);
  observer()->StartObserving(
      nullptr /* context */,
      base::BindRepeating(&TestCallback, &call_count, &active_count));
  EXPECT_EQ(1, call_count);
  EXPECT_EQ(1, active_count);
  EXPECT_TRUE(observer()->active());
}

using ArcActiveAudioThrottleObserverNoAudioHandlerTest = testing::Test;

// Test that with no audio handler, there is no error when start/stop
// observing. This should only happen in unit tests.
TEST_F(ArcActiveAudioThrottleObserverNoAudioHandlerTest, TestNoError) {
  ArcActiveAudioThrottleObserver observer;
  observer.StartObserving(nullptr /* context */,
                          base::BindRepeating(&TestCallback, nullptr, nullptr));
  observer.StopObserving();
}

}  // namespace arc
