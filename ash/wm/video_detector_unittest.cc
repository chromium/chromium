// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/video_detector.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// Implementation that just records video state changes.
class TestObserver : public VideoDetector::Observer {
 public:
  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  bool empty() const { return states_.empty(); }
  void reset() { states_.clear(); }

  // Pops and returns the earliest-received state.
  VideoDetector::State PopState() {
    CHECK(!states_.empty());
    VideoDetector::State first_state = states_.front();
    states_.pop_front();
    return first_state;
  }

  // VideoDetector::Observer implementation.
  void OnVideoStateChanged(VideoDetector::State state) override {
    states_.push_back(state);
  }

 private:
  // States in the order they were received.
  base::circular_deque<VideoDetector::State> states_;
};

class VideoDetectorTest : public AshTestBase {
 public:
  VideoDetectorTest() = default;

  VideoDetectorTest(const VideoDetectorTest&) = delete;
  VideoDetectorTest& operator=(const VideoDetectorTest&) = delete;

  ~VideoDetectorTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    observer_ = std::make_unique<TestObserver>();
    detector_ = Shell::Get()->video_detector();
    detector_->AddObserver(observer_.get());
  }

  void TearDown() override {
    detector_->RemoveObserver(observer_.get());
    AshTestBase::TearDown();
  }

 protected:
  // Creates and returns a new window with |bounds|.
  std::unique_ptr<aura::Window> CreateTestWindow(const gfx::Rect& bounds) {
    return TestWindowBuilder()
        .SetColorWindowDelegate(SK_ColorRED)
        .SetBounds(bounds)
        .AllowAllWindowStates()
        .Build();
  }

  raw_ptr<VideoDetector, DanglingUntriaged> detector_;  // not owned
  std::unique_ptr<TestObserver> observer_;
};

// Verify that the video detector can distinguish fullscreen and windowed video
// activity.
TEST_F(VideoDetectorTest, ReportFullscreen) {
  UpdateDisplay("1024x768,1024x768");

  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 1024, 768));
  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_TRUE(window_state->IsFullscreen());
  window->Focus();
  detector_->OnVideoActivityStarted();
  EXPECT_EQ(VideoDetector::State::PLAYING_FULLSCREEN, observer_->PopState());
  EXPECT_TRUE(observer_->empty());

  // Make the window non-fullscreen.
  observer_->reset();
  window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_FALSE(window_state->IsFullscreen());
  EXPECT_EQ(VideoDetector::State::PLAYING_WINDOWED, observer_->PopState());
  EXPECT_TRUE(observer_->empty());

  // Open a second, fullscreen window. Fullscreen video should still be reported
  // due to the second window being fullscreen. This avoids situations where
  // non-fullscreen video could be reported when multiple videos are playing in
  // fullscreen and non-fullscreen windows.
  observer_->reset();
  std::unique_ptr<aura::Window> other_window =
      CreateTestWindow(gfx::Rect(1024, 0, 1024, 768));
  WindowState* other_window_state = WindowState::Get(other_window.get());
  other_window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_TRUE(other_window_state->IsFullscreen());
  EXPECT_EQ(VideoDetector::State::PLAYING_FULLSCREEN, observer_->PopState());
  EXPECT_TRUE(observer_->empty());

  // Make the second window non-fullscreen and check that the observer is
  // immediately notified about windowed video.
  observer_->reset();
  other_window_state->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_FALSE(other_window_state->IsFullscreen());
  EXPECT_EQ(VideoDetector::State::PLAYING_WINDOWED, observer_->PopState());
  EXPECT_TRUE(observer_->empty());
}

}  // namespace ash
