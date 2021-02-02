// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/dropped_frame_counter.h"

#include <vector>

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "cc/animation/animation_host.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"

namespace cc {
namespace {

class DroppedFrameCounterTestBase : public LayerTreeTest {
 public:
  DroppedFrameCounterTestBase() = default;
  ~DroppedFrameCounterTestBase() override = default;

  virtual void SetUpTestConfigAndExpectations() = 0;

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->commit_to_active_tree = false;
  }

  void SetupTree() override {
    LayerTreeTest::SetupTree();

    Layer* root_layer = layer_tree_host()->root_layer();
    scroll_layer_ = FakePictureLayer::Create(&client_);
    // Set up the layer so it always has something to paint.
    scroll_layer_->set_always_update_resources(true);
    scroll_layer_->SetBounds({3, 3});
    client_.set_bounds({3, 3});
    root_layer->AddChild(scroll_layer_);
  }

  void RunTest(CompositorMode mode) override {
    SetUpTestConfigAndExpectations();
    LayerTreeTest::RunTest(mode);
  }

  void BeginTest() override {
    ASSERT_GT(config_.animation_frames, 0u);

    // Start with requesting main-frames.
    PostSetNeedsCommitToMainThread();
  }

  void AfterTest() override {
    EXPECT_GE(total_frames_, config_.animation_frames);
    // It is possible to drop even more frame than what the test expects (e.g.
    // in slower machines, slower builds such as asan/tsan builds, etc.), since
    // the test does not strictly control both threads and deadlines. Therefore,
    // it is not possible to check for strict equality here.
    EXPECT_LE(expect_.min_dropped_main, dropped_main_);
    EXPECT_LE(expect_.min_dropped_compositor, dropped_compositor_);
    EXPECT_LE(expect_.min_dropped_smoothness, dropped_smoothness_);
  }

  // Compositor thread function overrides:
  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const viz::BeginFrameArgs& args) override {
    if (TestEnded())
      return;

    // Request a re-draw, and set a non-empty damage region (otherwise the
    // draw is aborted with 'no damage').
    host_impl->SetNeedsRedraw();
    host_impl->SetViewportDamage(gfx::Rect(0, 0, 10, 20));

    if (skip_main_thread_next_frame_) {
      skip_main_thread_next_frame_ = false;
    } else {
      // Request update from the main-thread too.
      host_impl->SetNeedsCommit();
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    // If the main-thread is blocked, then unblock it once the compositor thread
    // has already drawn a frame.
    base::WaitableEvent* wait = nullptr;
    {
      base::AutoLock lock(wait_lock_);
      wait = wait_;
    }

    if (wait) {
      // When the main-thread blocks during a frame, skip the main-thread for
      // the next frame, so that the main-thread can be in sync with the
      // compositor thread again.
      skip_main_thread_next_frame_ = true;
      wait->Signal();
    }
  }

  void DidReceivePresentationTimeOnThread(
      LayerTreeHostImpl* host_impl,
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override {
    ++presented_frames_;
    if (presented_frames_ < config_.animation_frames)
      return;

    auto* dropped_frame_counter = host_impl->dropped_frame_counter();
    DCHECK(dropped_frame_counter);

    total_frames_ = dropped_frame_counter->total_frames();
    dropped_main_ = dropped_frame_counter->total_main_dropped();
    dropped_compositor_ = dropped_frame_counter->total_compositor_dropped();
    dropped_smoothness_ = dropped_frame_counter->total_smoothness_dropped();
    EndTest();
  }

  // Main-thread function overrides:
  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    if (TestEnded())
      return;

    bool should_wait = false;
    if (config_.should_drop_main_every > 0) {
      should_wait =
          args.frame_id.sequence_number % config_.should_drop_main_every == 0;
    }

    if (should_wait) {
      base::WaitableEvent wait{base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED};
      {
        base::AutoLock lock(wait_lock_);
        wait_ = &wait;
      }
      wait.Wait();
      {
        base::AutoLock lock(wait_lock_);
        wait_ = nullptr;
      }
    }

    // Make some changes so that the main-thread needs to push updates to the
    // compositor thread (i.e. force a commit).
    auto const bounds = scroll_layer_->bounds();
    scroll_layer_->SetBounds({bounds.width(), bounds.height() + 1});
    if (config_.should_register_main_thread_animation) {
      animation_host()->SetAnimationCounts(1, true, true);
    }
  }

 protected:
  // The test configuration options. This is set before the test starts, and
  // remains unchanged after that. So it is safe to read these fields from
  // either threads.
  struct TestConfig {
    uint32_t should_drop_main_every = 0;
    uint32_t animation_frames = 0;
    bool should_register_main_thread_animation = false;
  } config_;

  // The test expectations. This is set before the test starts, and
  // remains unchanged after that. So it is safe to read these fields from
  // either threads.
  struct TestExpectation {
    uint32_t min_dropped_main = 0;
    uint32_t min_dropped_compositor = 0;
    uint32_t min_dropped_smoothness = 0;
  } expect_;

 private:
  // Set up a dummy picture layer so that every begin-main frame requires a
  // commit (without the dummy layer, the main-thread never has to paint, which
  // causes an early 'no damage' abort of the main-frame.
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> scroll_layer_;

  // This field is used only on the compositor thread to track how many frames
  // have been processed.
  uint32_t presented_frames_ = 0;

  // The |wait_| event is used when the test wants to deliberately force the
  // main-thread to block while processing begin-main-frames.
  base::Lock wait_lock_;
  base::WaitableEvent* wait_ = nullptr;

  // These fields are populated in the compositor thread when the desired number
  // of frames have been processed. These fields are subsequently compared
  // against the expectation after the test ends.
  uint32_t total_frames_ = 0;
  uint32_t dropped_main_ = 0;
  uint32_t dropped_compositor_ = 0;
  uint32_t dropped_smoothness_ = 0;

  bool skip_main_thread_next_frame_ = false;
};

class DroppedFrameCounterNoDropTest : public DroppedFrameCounterTestBase {
 public:
  ~DroppedFrameCounterNoDropTest() override = default;

  void SetUpTestConfigAndExpectations() override {
    config_.animation_frames = 28;
    config_.should_register_main_thread_animation = false;

    expect_.min_dropped_main = 0;
    expect_.min_dropped_compositor = 0;
    expect_.min_dropped_smoothness = 0;
  }
};

MULTI_THREAD_TEST_F(DroppedFrameCounterNoDropTest);

class DroppedFrameCounterMainDropsNoSmoothness
    : public DroppedFrameCounterTestBase {
 public:
  ~DroppedFrameCounterMainDropsNoSmoothness() override = default;

  void SetUpTestConfigAndExpectations() override {
    config_.animation_frames = 28;
    config_.should_drop_main_every = 5;
    config_.should_register_main_thread_animation = false;

    expect_.min_dropped_main = 5;
    expect_.min_dropped_smoothness = 0;
  }
};

// TODO(crbug.com/1115376) Disabled for flakiness.
// MULTI_THREAD_TEST_F(DroppedFrameCounterMainDropsNoSmoothness);

class DroppedFrameCounterMainDropsSmoothnessTest
    : public DroppedFrameCounterTestBase {
 public:
  ~DroppedFrameCounterMainDropsSmoothnessTest() override = default;

  void SetUpTestConfigAndExpectations() override {
    config_.animation_frames = 28;
    config_.should_drop_main_every = 5;
    config_.should_register_main_thread_animation = true;

    expect_.min_dropped_main = 5;
    expect_.min_dropped_smoothness = 5;
  }
};

// TODO(crbug.com/1115376) Disabled for flakiness.
// MULTI_THREAD_TEST_F(DroppedFrameCounterMainDropsSmoothnessTest);

class DroppedFrameCounterTest : public testing::Test {
 public:
  DroppedFrameCounterTest() {
    dropped_frame_counter_.set_total_counter(&total_frame_counter_);
    dropped_frame_counter_.OnFcpReceived();
  }
  ~DroppedFrameCounterTest() override = default;

  // For each boolean in frame_states produces a frame
  void SimulateFrameSequence(std::vector<bool> frame_states, int repeat) {
    for (int i = 0; i < repeat; i++) {
      for (auto is_dropped : frame_states) {
        viz::BeginFrameArgs args_ = SimulateBeginFrameArgs();
        dropped_frame_counter_.OnBeginFrame(args_, /*is_scroll_active=*/false);
        dropped_frame_counter_.OnEndFrame(args_, is_dropped);
        sequence_number_++;
        frame_time_ += interval_;
      }
    }
  }

  void AdvancetimeByIntervals(int interval_count) {
    frame_time_ += interval_ * interval_count;
  }

  double MaxPercentDroppedFrame() {
    return dropped_frame_counter_.sliding_window_max_percent_dropped();
  }

  double PercentDroppedFrame95Percentile() {
    return dropped_frame_counter_.SlidingWindow95PercentilePercentDropped();
  }

  double GetTotalFramesInWindow() {
    return base::TimeDelta::FromSeconds(1) / interval_;
  }

  void SetInterval(base::TimeDelta interval) { interval_ = interval; }

  base::TimeTicks GetNextFrameTime() const { return frame_time_ + interval_; }

 public:
  DroppedFrameCounter dropped_frame_counter_;

 private:
  TotalFrameCounter total_frame_counter_;
  uint64_t sequence_number_ = 1;
  uint64_t source_id_ = 1;
  const base::TickClock* tick_clock_ = base::DefaultTickClock::GetInstance();
  base::TimeTicks frame_time_ = tick_clock_->NowTicks();
  base::TimeDelta interval_ =
      base::TimeDelta::FromMicroseconds(16667);  // 16.667 ms

  viz::BeginFrameArgs SimulateBeginFrameArgs() {
    viz::BeginFrameId current_id_(source_id_, sequence_number_);
    viz::BeginFrameArgs args = viz::BeginFrameArgs();
    args.frame_id = current_id_;
    args.frame_time = frame_time_;
    args.interval = interval_;
    return args;
  }
};

TEST_F(DroppedFrameCounterTest, SimplePattern1) {
  // 2 out of every 3 frames are dropped (In total 80 frames out of 120).
  SimulateFrameSequence({true, true, true, false, true, false}, 20);

  // The max is the following window:
  //    16 * <sequence> + {true, true, true, false
  // Which means a max of 67 dropped frames.
  EXPECT_EQ(std::round(MaxPercentDroppedFrame()), 67);
  EXPECT_EQ(PercentDroppedFrame95Percentile(), 67);  // all values are in the
  // 67th bucket, and as a result 95th percentile is also 67.
}

TEST_F(DroppedFrameCounterTest, SimplePattern2) {
  // 1 out of every 5 frames are dropped (In total 24 frames out of 120).
  SimulateFrameSequence({false, false, false, false, true}, 24);

  double expected_percent_dropped_frame = (12 / GetTotalFramesInWindow()) * 100;
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrame(), expected_percent_dropped_frame);
  EXPECT_EQ(PercentDroppedFrame95Percentile(), 20);  // all values are in the
  // 20th bucket, and as a result 95th percentile is also 20.
}

TEST_F(DroppedFrameCounterTest, IncompleteWindow) {
  // There are only 5 frames submitted and both Max and 95pct should report
  // zero.
  SimulateFrameSequence({false, false, false, false, true}, 1);
  EXPECT_EQ(MaxPercentDroppedFrame(), 0.0);
  EXPECT_EQ(PercentDroppedFrame95Percentile(), 0);
}

TEST_F(DroppedFrameCounterTest, MaxPercentDroppedChanges) {
  // First 60 frames have 20% dropped.
  SimulateFrameSequence({false, false, false, false, true}, 12);

  double expected_percent_dropped_frame1 =
      (12 / GetTotalFramesInWindow()) * 100;
  EXPECT_EQ(MaxPercentDroppedFrame(), expected_percent_dropped_frame1);
  EXPECT_FLOAT_EQ(PercentDroppedFrame95Percentile(), 20);  // There is only one
  // element in the histogram and that is 20.

  // 30 new frames are added that have 18 dropped frames.
  // and the 30 frame before that had 6 dropped frames.
  // So in total in the window has 24 frames dropped out of 60 frames.
  SimulateFrameSequence({false, false, true, true, true}, 6);
  double expected_percent_dropped_frame2 =
      (24 / GetTotalFramesInWindow()) * 100;
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrame(), expected_percent_dropped_frame2);

  // 30 new frames are added that have 24 dropped frames.
  // and the 30 frame before that had 18 dropped frames.
  // So in total in the window has 42 frames dropped out of 60 frames.
  SimulateFrameSequence({false, true, true, true, true}, 6);
  double expected_percent_dropped_frame3 =
      (42 / GetTotalFramesInWindow()) * 100;
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrame(), expected_percent_dropped_frame3);

  // Percent dropped frame of window increases gradually to 70%.
  // 1 value exist when we reach 60 frames and 1 value thereafter for each
  // frame added. So there 61 values in histogram. Last value is 70 (2 sampels)
  // and then 67 with 1 sample, which would be the 95th percentile.
  EXPECT_EQ(PercentDroppedFrame95Percentile(), 67);
}

TEST_F(DroppedFrameCounterTest, MaxPercentDroppedWithIdleFrames) {
  // First 20 frames have 4 frames dropped (20%).
  SimulateFrameSequence({false, false, false, false, true}, 4);

  // Then no frames are added for 20 intervals.
  AdvancetimeByIntervals(20);

  // Then 20 frames have 16 frames dropped (60%).
  SimulateFrameSequence({false, false, true, true, true}, 4);

  // So in total, there are 40 frames in the 1 second window with 16 dropped
  // frames (40% in total).
  double expected_percent_dropped_frame = (16 / GetTotalFramesInWindow()) * 100;
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrame(), expected_percent_dropped_frame);
}

TEST_F(DroppedFrameCounterTest, NoCrashForIntervalLargerThanWindow) {
  SetInterval(base::TimeDelta::FromMilliseconds(1000));
  SimulateFrameSequence({false, false}, 1);
}

TEST_F(DroppedFrameCounterTest, Percentile95WithIdleFrames) {
  // Test scenario:
  //  . 4s of 20% dropped frames.
  //  . 96s of idle time.
  // The 96%ile dropped-frame metric should be 0.

  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::TimeDelta::FromMilliseconds(10);
  constexpr size_t kFps = base::TimeDelta::FromSeconds(1) / kInterval;
  static_assert(
      kFps % 5 == 0,
      "kFps must be a multiple of 5 because this test depends on it.");
  SetInterval(kInterval);

  const auto* histogram = dropped_frame_counter_.GetSlidingWindowHistogram();

  // First 4 seconds with 20% dropped frames.
  SimulateFrameSequence({false, false, false, false, true}, (kFps / 5) * 4);
  EXPECT_EQ(histogram->GetPercentDroppedFramePercentile(0.95), 20u);

  // Then no frames are added for 97s. Note that this 1s more than 96 seconds,
  // because the last second remains in the sliding window.
  AdvancetimeByIntervals(kFps * 97);

  // A single frame to flush the pipeline.
  SimulateFrameSequence({false}, 1);

  EXPECT_EQ(histogram->total_count(), 100u * kFps);
  EXPECT_EQ(histogram->GetPercentDroppedFramePercentile(0.96), 0u);
  EXPECT_GT(histogram->GetPercentDroppedFramePercentile(0.97), 0u);
}

TEST_F(DroppedFrameCounterTest, Percentile95WithIdleFramesWhileHidden) {
  // The test scenario is the same as |Percentile95WithIdleFrames| test:
  //  . 4s of 20% dropped frames.
  //  . 96s of idle time.
  // However, the 96s of idle time happens *after* the page becomes invisible
  // (e.g. after a tab-switch). In this case, the idle time *should not*
  // contribute to the sliding window.

  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::TimeDelta::FromMilliseconds(10);
  constexpr size_t kFps = base::TimeDelta::FromSeconds(1) / kInterval;
  static_assert(
      kFps % 5 == 0,
      "kFps must be a multiple of 5 because this test depends on it.");
  SetInterval(kInterval);

  const auto* histogram = dropped_frame_counter_.GetSlidingWindowHistogram();

  // First 4 seconds with 20% dropped frames.
  SimulateFrameSequence({false, false, false, false, true}, (kFps / 5) * 4);
  EXPECT_EQ(histogram->GetPercentDroppedFramePercentile(0.95), 20u);

  // Hide the page (thus resetting the pending frames), then idle for 96s before
  // producing a single frame.
  dropped_frame_counter_.ResetPendingFrames(GetNextFrameTime());
  AdvancetimeByIntervals(kFps * 97);

  // A single frame to flush the pipeline.
  SimulateFrameSequence({false}, 1);

  EXPECT_EQ(histogram->GetPercentDroppedFramePercentile(0.95), 20u);
}

TEST_F(DroppedFrameCounterTest, Percentile95WithIdleFramesThenHide) {
  // The test scenario is the same as |Percentile95WithIdleFramesWhileHidden|:
  //  . 4s of 20% dropped frames.
  //  . 96s of idle time.
  // However, the 96s of idle time happens *before* the page becomes invisible
  // (e.g. after a tab-switch). In this case, the idle time *should*
  // contribute to the sliding window.

  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::TimeDelta::FromMilliseconds(10);
  constexpr size_t kFps = base::TimeDelta::FromSeconds(1) / kInterval;
  static_assert(
      kFps % 5 == 0,
      "kFps must be a multiple of 5 because this test depends on it.");
  SetInterval(kInterval);

  const auto* histogram = dropped_frame_counter_.GetSlidingWindowHistogram();

  // First 4 seconds with 20% dropped frames.
  SimulateFrameSequence({false, false, false, false, true}, (kFps / 5) * 4);
  EXPECT_EQ(histogram->GetPercentDroppedFramePercentile(0.95), 20u);

  // Idle for 96s before hiding the page.
  AdvancetimeByIntervals(kFps * 97);
  dropped_frame_counter_.ResetPendingFrames(GetNextFrameTime());
  AdvancetimeByIntervals(kFps * 97);

  // A single frame to flush the pipeline.
  SimulateFrameSequence({false}, 1);

  EXPECT_EQ(histogram->GetPercentDroppedFramePercentile(0.96), 0u);
  EXPECT_GT(histogram->GetPercentDroppedFramePercentile(0.97), 0u);
}

}  // namespace
}  // namespace cc
