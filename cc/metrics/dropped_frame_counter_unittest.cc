// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/dropped_frame_counter.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "cc/animation/animation_host.h"
#include "cc/metrics/custom_metrics_recorder.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_frame_info.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_test.h"

namespace cc {
namespace {

using SmoothnessStrategy = DroppedFrameCounter::SmoothnessStrategy;

FrameInfo CreateStubFrameInfo(bool is_dropped) {
  return CreateFakeFrameInfo(is_dropped
                                 ? FrameInfo::FrameFinalState::kDropped
                                 : FrameInfo::FrameFinalState::kPresentedAll);
}

class TestCustomMetricsRecorder : public CustomMetricRecorder {
 public:
  TestCustomMetricsRecorder() = default;
  ~TestCustomMetricsRecorder() override = default;

  // CustomMetricRecorder:
  void ReportPercentDroppedFramesInOneSecondWindow2(double percent) override {
    ++report_count_;
    last_percent_dropped_frames_ = percent;
  }
  void ReportEventLatency(
      std::vector<EventLatencyTracker::LatencyData> latencies) override {}

  void Reset() {
    report_count_ = 0u;
    last_percent_dropped_frames_ = 0;
  }

  int report_count() const { return report_count_; }

  double last_percent_dropped_frames() const {
    return last_percent_dropped_frames_;
  }

 private:
  int report_count_ = 0u;
  double last_percent_dropped_frames_ = 0;
};

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
    EXPECT_LE(expect_.min_partial, partial_);
    EXPECT_LE(expect_.min_dropped, dropped_);
    EXPECT_LE(expect_.min_dropped_smoothness, dropped_smoothness_);
  }

  // Compositor thread function overrides:
  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const viz::BeginFrameArgs& args,
                                  bool has_damage) override {
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
    partial_ = dropped_frame_counter->total_partial();
    dropped_ = dropped_frame_counter->total_dropped();
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
      animation_host()->SetAnimationCounts(1);
      animation_host()->SetCurrentFrameHadRaf(true);
      animation_host()->SetNextFrameHasPendingRaf(true);
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
    uint32_t min_partial = 0;
    uint32_t min_dropped = 0;
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
  raw_ptr<base::WaitableEvent> wait_ = nullptr;

  // These fields are populated in the compositor thread when the desired number
  // of frames have been processed. These fields are subsequently compared
  // against the expectation after the test ends.
  uint32_t total_frames_ = 0;
  uint32_t partial_ = 0;
  uint32_t dropped_ = 0;
  uint32_t dropped_smoothness_ = 0;

  bool skip_main_thread_next_frame_ = false;
};

class DroppedFrameCounterNoDropTest : public DroppedFrameCounterTestBase {
 public:
  ~DroppedFrameCounterNoDropTest() override = default;

  void SetUpTestConfigAndExpectations() override {
    config_.animation_frames = 28;
    config_.should_register_main_thread_animation = false;

    expect_.min_partial = 0;
    expect_.min_dropped = 0;
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

    expect_.min_partial = 5;
    expect_.min_dropped_smoothness = 0;
  }
};

// TODO(crbug.com/40144326) Disabled for flakiness.
// MULTI_THREAD_TEST_F(DroppedFrameCounterMainDropsNoSmoothness);

class DroppedFrameCounterMainDropsSmoothnessTest
    : public DroppedFrameCounterTestBase {
 public:
  ~DroppedFrameCounterMainDropsSmoothnessTest() override = default;

  void SetUpTestConfigAndExpectations() override {
    config_.animation_frames = 28;
    config_.should_drop_main_every = 5;
    config_.should_register_main_thread_animation = true;

    expect_.min_partial = 5;
    expect_.min_dropped_smoothness = 5;
  }
};

// TODO(crbug.com/40144326) Disabled for flakiness.
// MULTI_THREAD_TEST_F(DroppedFrameCounterMainDropsSmoothnessTest);

class DroppedFrameCounterTest : public testing::Test {
 public:
  explicit DroppedFrameCounterTest(SmoothnessStrategy smoothness_strategy =
                                       SmoothnessStrategy::kDefaultStrategy)
      : smoothness_strategy_(smoothness_strategy) {
    dropped_frame_counter_.set_total_counter(&total_frame_counter_);
    dropped_frame_counter_.OnFcpReceived();
  }
  ~DroppedFrameCounterTest() override = default;

  // For each boolean in frame_states produces a frame
  void SimulateFrameSequence(std::vector<bool> frame_states, int repeat) {
    for (int i = 0; i < repeat; i++) {
      for (auto is_dropped : frame_states) {
        viz::BeginFrameArgs args_ = SimulateBeginFrameArgs();
        dropped_frame_counter_.OnBeginFrame(args_);
        dropped_frame_counter_.OnEndFrame(args_,
                                          CreateStubFrameInfo(is_dropped));
        sequence_number_++;
        frame_time_ += interval_;
      }
    }
  }

  // Make a sequence of frame states where the first |dropped_frames| out of
  // |total_frames| are dropped.
  std::vector<bool> MakeFrameSequence(int dropped_frames, int total_frames) {
    std::vector<bool> frame_states(total_frames, false);
    for (int i = 0; i < dropped_frames; i++) {
      frame_states[i] = true;
    }
    return frame_states;
  }

  std::vector<viz::BeginFrameArgs> SimulatePendingFrame(int repeat) {
    std::vector<viz::BeginFrameArgs> args(repeat);
    for (int i = 0; i < repeat; i++) {
      args[i] = SimulateBeginFrameArgs();
      dropped_frame_counter_.OnBeginFrame(args[i]);
      sequence_number_++;
      frame_time_ += interval_;
    }
    return args;
  }

  // Simulate a main and impl thread update on the same frame.
  void SimulateForkedFrame(bool main_dropped, bool impl_dropped) {
    viz::BeginFrameArgs args_ = SimulateBeginFrameArgs();
    dropped_frame_counter_.OnBeginFrame(args_);
    dropped_frame_counter_.OnBeginFrame(args_);

    // End the 'main thread' arm of the fork.
    auto main_info = CreateStubFrameInfo(main_dropped);
    main_info.main_thread_response = FrameInfo::MainThreadResponse::kIncluded;
    dropped_frame_counter_.OnEndFrame(args_, main_info);

    // End the 'compositor thread' arm of the fork.
    auto impl_info = CreateStubFrameInfo(impl_dropped);
    impl_info.main_thread_response = FrameInfo::MainThreadResponse::kMissing;
    dropped_frame_counter_.OnEndFrame(args_, impl_info);

    sequence_number_++;
    frame_time_ += interval_;
  }

  void AdvancetimeByIntervals(int interval_count) {
    frame_time_ += interval_ * interval_count;
  }

  double MaxPercentDroppedFrame() {
    return dropped_frame_counter_.sliding_window_max_percent_dropped();
  }

  double MaxPercentDroppedFrameAfter1Sec() {
    auto percent_dropped =
        dropped_frame_counter_.max_percent_dropped_After_1_sec();
    EXPECT_TRUE(percent_dropped.has_value());
    return percent_dropped.value();
  }

  double MaxPercentDroppedFrameAfter2Sec() {
    auto percent_dropped =
        dropped_frame_counter_.max_percent_dropped_After_2_sec();
    EXPECT_TRUE(percent_dropped.has_value());
    return percent_dropped.value();
  }

  double MaxPercentDroppedFrameAfter5Sec() {
    auto percent_dropped =
        dropped_frame_counter_.max_percent_dropped_After_5_sec();
    EXPECT_TRUE(percent_dropped.has_value());
    return percent_dropped.value();
  }

  double PercentDroppedFrame95Percentile() {
    return dropped_frame_counter_.SlidingWindow95PercentilePercentDropped(
        smoothness_strategy_);
  }

  double PercentDroppedFrameMedian() {
    return dropped_frame_counter_.SlidingWindowMedianPercentDropped(
        smoothness_strategy_);
  }

  double PercentDroppedFrameVariance() {
    return dropped_frame_counter_.SlidingWindowPercentDroppedVariance(
        smoothness_strategy_);
  }

  const DroppedFrameCounter::SlidingWindowHistogram*
  GetSlidingWindowHistogram() {
    return dropped_frame_counter_.GetSlidingWindowHistogram(
        smoothness_strategy_);
  }

  double GetTotalFramesInWindow() { return base::Seconds(1) / interval_; }

  void SetInterval(base::TimeDelta interval) { interval_ = interval; }

  base::TimeTicks GetNextFrameTime() const { return frame_time_ + interval_; }

  // Wrap calls with EXPECT_TRUE. Logs the buckets and returns false if they
  // don't match (within a given epsilon).
  bool CheckSmoothnessBuckets(std::vector<double> expected_buckets) {
    constexpr double epsilon = 0.001;
    bool buckets_match = true;
    std::vector<double> buckets =
        GetSlidingWindowHistogram()->GetPercentDroppedFrameBuckets();
    if (buckets.size() != expected_buckets.size()) {
      buckets_match = false;
    } else {
      for (size_t i = 0; i < buckets.size(); i++) {
        if (std::abs(buckets[i] - expected_buckets[i]) > epsilon) {
          buckets_match = false;
          break;
        }
      }
    }
    if (!buckets_match) {
      LOG(ERROR) << "Smoothness buckets do not match!";
      LOG(ERROR) << "Expected: " << testing::PrintToString(expected_buckets);
      LOG(ERROR) << "  Actual: " << testing::PrintToString(buckets);
    }
    return buckets_match;
  }

 public:
  DroppedFrameCounter dropped_frame_counter_;

 private:
  TotalFrameCounter total_frame_counter_;
  uint64_t sequence_number_ = 1;
  uint64_t source_id_ = 1;
  raw_ptr<const base::TickClock> tick_clock_ =
      base::DefaultTickClock::GetInstance();
  base::TimeTicks frame_time_ = tick_clock_->NowTicks();
  base::TimeDelta interval_ = base::Microseconds(16667);  // 16.667 ms

  SmoothnessStrategy smoothness_strategy_;

  viz::BeginFrameArgs SimulateBeginFrameArgs() {
    viz::BeginFrameId current_id_(source_id_, sequence_number_);
    viz::BeginFrameArgs args = viz::BeginFrameArgs();
    args.frame_id = current_id_;
    args.frame_time = frame_time_;
    args.interval = interval_;
    return args;
  }
};

// Test class that supports parameterized tests for each of the different
// SmoothnessStrategy.
//
// TODO(jonross): when we build the other strategies parameterize the
// expectations.
class SmoothnessStrategyDroppedFrameCounterTest
    : public DroppedFrameCounterTest,
      public testing::WithParamInterface<SmoothnessStrategy> {
 public:
  SmoothnessStrategyDroppedFrameCounterTest()
      : DroppedFrameCounterTest(GetParam()) {}
  ~SmoothnessStrategyDroppedFrameCounterTest() override = default;
  SmoothnessStrategyDroppedFrameCounterTest(
      const SmoothnessStrategyDroppedFrameCounterTest&) = delete;
  SmoothnessStrategyDroppedFrameCounterTest& operator=(
      const SmoothnessStrategyDroppedFrameCounterTest&) = delete;
};

INSTANTIATE_TEST_SUITE_P(
    DefaultStrategy,
    SmoothnessStrategyDroppedFrameCounterTest,
    ::testing::Values(SmoothnessStrategy::kDefaultStrategy));

TEST_P(SmoothnessStrategyDroppedFrameCounterTest, SimplePattern1) {
  // 2 out of every 3 frames are dropped (In total 80 frames out of 120).
  SimulateFrameSequence({true, true, true, false, true, false}, 20);

  // The max is the following window:
  //    16 * <sequence> + {true, true, true, false
  // Which means a max of 67 dropped frames.
  EXPECT_EQ(std::round(MaxPercentDroppedFrame()), 67);
  EXPECT_EQ(PercentDroppedFrame95Percentile(), 67);  // all values are in the
  // 65th-67th bucket, and as a result 95th percentile is also 67.
  EXPECT_EQ(PercentDroppedFrameMedian(), 65);
  EXPECT_LE(PercentDroppedFrameVariance(), 1);
}

TEST_P(SmoothnessStrategyDroppedFrameCounterTest, SimplePattern2) {
  // 1 out of every 5 frames are dropped (In total 24 frames out of 120).
  SimulateFrameSequence({false, false, false, false, true}, 24);

  double expected_percent_dropped_frame = (12 / GetTotalFramesInWindow()) * 100;
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrame(), expected_percent_dropped_frame);
  EXPECT_EQ(PercentDroppedFrame95Percentile(),
            20);  // all values are in the
  // 20th bucket, and as a result 95th percentile is also 20.
  EXPECT_EQ(PercentDroppedFrameMedian(), 20);
  EXPECT_LE(PercentDroppedFrameVariance(), 1);
}

TEST_P(SmoothnessStrategyDroppedFrameCounterTest, IncompleteWindow) {
  // There are only 5 frames submitted, so Max, 95pct, median and variance
  // should report zero.
  SimulateFrameSequence({false, false, false, false, true}, 1);
  EXPECT_EQ(MaxPercentDroppedFrame(), 0.0);
  EXPECT_EQ(PercentDroppedFrame95Percentile(), 0);
  EXPECT_EQ(PercentDroppedFrameMedian(), 0);
  EXPECT_LE(PercentDroppedFrameVariance(), 1);
}

TEST_P(SmoothnessStrategyDroppedFrameCounterTest, MaxPercentDroppedChanges) {
  // First 60 frames have 20% dropped.
  SimulateFrameSequence({false, false, false, false, true}, 12);

  double expected_percent_dropped_frame1 =
      (12 / GetTotalFramesInWindow()) * 100;
  EXPECT_EQ(MaxPercentDroppedFrame(), expected_percent_dropped_frame1);
  EXPECT_FLOAT_EQ(PercentDroppedFrame95Percentile(),
                  20);  // There is only one
  // element in the histogram and that is 20.
  EXPECT_EQ(PercentDroppedFrameMedian(), 20);
  EXPECT_LE(PercentDroppedFrameVariance(), 1);

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
  SetInterval(base::Milliseconds(1000));
  SimulateFrameSequence({false, false}, 1);
}

TEST_P(SmoothnessStrategyDroppedFrameCounterTest, Percentile95WithIdleFrames) {
  // Test scenario:
  //  . 4s of 20% dropped frames.
  //  . 96s of idle time.
  // The 96%ile dropped-frame metric should be 0.

  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  static_assert(
      kFps % 5 == 0,
      "kFps must be a multiple of 5 because this test depends on it.");
  SetInterval(kInterval);

  const auto* histogram = GetSlidingWindowHistogram();

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

TEST_P(SmoothnessStrategyDroppedFrameCounterTest,
       Percentile95WithIdleFramesWhileHidden) {
  // The test scenario is the same as |Percentile95WithIdleFrames| test:
  //  . 4s of 20% dropped frames.
  //  . 96s of idle time.
  // However, the 96s of idle time happens *after* the page becomes invisible
  // (e.g. after a tab-switch). In this case, the idle time *should not*
  // contribute to the sliding window.

  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  static_assert(
      kFps % 5 == 0,
      "kFps must be a multiple of 5 because this test depends on it.");
  SetInterval(kInterval);

  const auto* histogram = GetSlidingWindowHistogram();

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

TEST_P(SmoothnessStrategyDroppedFrameCounterTest,
       Percentile95WithIdleFramesThenHide) {
  // The test scenario is the same as |Percentile95WithIdleFramesWhileHidden|:
  //  . 4s of 20% dropped frames.
  //  . 96s of idle time.
  // However, the 96s of idle time happens *before* the page becomes invisible
  // (e.g. after a tab-switch). In this case, the idle time *should*
  // contribute to the sliding window.

  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  static_assert(
      kFps % 5 == 0,
      "kFps must be a multiple of 5 because this test depends on it.");
  SetInterval(kInterval);

  const auto* histogram = GetSlidingWindowHistogram();

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

// Tests that when ResetPendingFrames updates the sliding window, that the max
// PercentDroppedFrames is also updated accordingly. (https://crbug.com/1225307)
TEST_P(SmoothnessStrategyDroppedFrameCounterTest,
       ResetPendingFramesUpdatesMaxPercentDroppedFrames) {
  // This tests a scenario where gaps in frame production lead to having
  // leftover frames in the sliding window for calculations of
  // ResetPendingFrames.
  //
  // Testing for when those frames are sufficient to change the current maximum
  // PercentDroppedFrames.
  //
  // This has been first seen in GpuCrash_InfoForDualHardwareGpus which forces
  // a GPU crash. Introducing long periods of idle while the Renderer waits for
  // a new GPU Process. (https://crbug.com/1164647)

  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  SetInterval(kInterval);

  // One good frame
  SimulateFrameSequence({false}, 1);
  // Advance 1s so that when we process the first window, we go from having
  // enough frames in the interval, to no longer having enough.
  AdvancetimeByIntervals(kFps);

  // The first frame should fill up the sliding window. It isn't dropped, so
  // there should be 0 dropped frames. This will pop the first reported frame.
  // The second frame is dropped, however we are now tracking less frames than
  // the 1s window. So we won't use it in calculations yet.
  SimulateFrameSequence({false, true}, 1);
  EXPECT_EQ(dropped_frame_counter_.sliding_window_max_percent_dropped(), 0u);

  // Advance 1s so that we will attempt to update the window when resetting the
  // pending frames. The pending dropped frame above should be calculated here,
  // and the max percentile should be updated.
  AdvancetimeByIntervals(kFps);
  dropped_frame_counter_.ResetPendingFrames(GetNextFrameTime());
  EXPECT_GT(dropped_frame_counter_.sliding_window_max_percent_dropped(), 0u);

  // There should be enough sliding windows reported with 0 dropped frames that
  // the 95th percentile stays at 0.
  EXPECT_EQ(PercentDroppedFrame95Percentile(), 0u);
}

TEST_F(DroppedFrameCounterTest, ResetPendingFramesAccountingForPendingFrames) {
  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  SetInterval(kInterval);

  // First 2 seconds with 20% dropped frames.
  SimulateFrameSequence({false, false, false, false, true}, (kFps / 5) * 2);

  // Have a pending frame which would hold the frames in queue.
  SimulatePendingFrame(1);

  // One second with 40% dropped frames.
  SimulateFrameSequence({false, false, false, true, true}, (kFps / 5));

  // On the first 2 seconds are accounted for and pdf is 20%.
  EXPECT_EQ(MaxPercentDroppedFrame(), 20);

  dropped_frame_counter_.ResetPendingFrames(GetNextFrameTime());

  // After resetting the pending frames, the pdf would be 40%.
  EXPECT_EQ(MaxPercentDroppedFrame(), 40);
}

TEST_F(DroppedFrameCounterTest, Reset) {
  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  SetInterval(kInterval);

  // First 2 seconds with 20% dropped frames.
  SimulateFrameSequence({false, false, false, false, true}, (kFps / 5) * 2);

  // Have a pending frame which would hold the frames in queue.
  SimulatePendingFrame(1);

  // Another 2 seconds with 40% dropped frames.
  SimulateFrameSequence({false, false, false, true, true}, (kFps / 5) * 2);

  EXPECT_EQ(MaxPercentDroppedFrame(), 20u);

  dropped_frame_counter_.Reset();  // Simulating gpu thread crash

  // After reset the max percent dropped frame would be 0 and frames in queue
  // behind the pending frame would not affect it.
  EXPECT_EQ(MaxPercentDroppedFrame(), 0u);
}

TEST_F(DroppedFrameCounterTest, ConsistentSmoothnessRatings) {
  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  static_assert(kFps == 100,
                "kFps must be 100 because this test depends on it.");
  SetInterval(kInterval);

  // Add 5 seconds with 2% dropped frames. This should be in the first bucket.
  SimulateFrameSequence(MakeFrameSequence(1, 50), (kFps / 50) * 5);
  EXPECT_TRUE(CheckSmoothnessBuckets({100, 0, 0, 0, 0, 0, 0}));

  // Add 5 seconds with 5% dropped frames. This should be in the second bucket.
  dropped_frame_counter_.Reset();
  dropped_frame_counter_.OnFcpReceived();
  SimulateFrameSequence(MakeFrameSequence(1, 20), (kFps / 20) * 5);
  EXPECT_TRUE(CheckSmoothnessBuckets({0, 100, 0, 0, 0, 0, 0}));

  // Add 5 seconds with 10% dropped frames. This should be in the third bucket.
  dropped_frame_counter_.Reset();
  dropped_frame_counter_.OnFcpReceived();
  SimulateFrameSequence(MakeFrameSequence(1, 10), (kFps / 10) * 5);
  EXPECT_TRUE(CheckSmoothnessBuckets({0, 0, 100, 0, 0, 0, 0}));

  // Add 5 seconds with 20% dropped frames. This should be in the fourth bucket.
  dropped_frame_counter_.Reset();
  dropped_frame_counter_.OnFcpReceived();
  SimulateFrameSequence({false, false, false, false, true}, (kFps / 5) * 5);
  EXPECT_TRUE(CheckSmoothnessBuckets({0, 0, 0, 100, 0, 0, 0}));

  // Add 5 seconds with 40% dropped frames. This should be in the fifth bucket.
  dropped_frame_counter_.Reset();
  dropped_frame_counter_.OnFcpReceived();
  SimulateFrameSequence({false, false, false, true, true}, (kFps / 5) * 5);
  EXPECT_TRUE(CheckSmoothnessBuckets({0, 0, 0, 0, 100, 0, 0}));

  // Add 5 seconds with 60% dropped frames. This should be in the sixth bucket.
  dropped_frame_counter_.Reset();
  dropped_frame_counter_.OnFcpReceived();
  SimulateFrameSequence({false, false, true, true, true}, (kFps / 5) * 5);
  EXPECT_TRUE(CheckSmoothnessBuckets({0, 0, 0, 0, 0, 100, 0}));

  // Add 5 seconds with 80% dropped frames. This should be in the last bucket.
  dropped_frame_counter_.Reset();
  dropped_frame_counter_.OnFcpReceived();
  SimulateFrameSequence({false, true, true, true, true}, (kFps / 5) * 5);
  EXPECT_TRUE(CheckSmoothnessBuckets({0, 0, 0, 0, 0, 0, 100}));
}

TEST_F(DroppedFrameCounterTest, MovingSmoothnessRatings) {
  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  static_assert(kFps == 100,
                "kFps must be 100 because this test depends on it.");
  SetInterval(kInterval);

  // Add a second with 40% dropped frames. Nothing should be added to the
  // histogram yet.
  SimulateFrameSequence({false, false, false, true, true}, kFps / 5);
  EXPECT_TRUE(CheckSmoothnessBuckets({0, 0, 0, 0, 0, 0, 0}));

  // Add a second with 80% dropped frames. All very bad buckets should have some
  // entries.
  SimulateFrameSequence({false, true, true, true, true}, kFps / 5);
  EXPECT_TRUE(CheckSmoothnessBuckets({0, 0, 0, 0, 22, 64, 14}));

  // Add a second with 10% dropped frames. Should be mostly very bad, with a few
  // bad and okay windows.
  SimulateFrameSequence(MakeFrameSequence(1, 10), kFps / 10);
  EXPECT_TRUE(CheckSmoothnessBuckets({0, 0, 1, 9, 29, 50, 11}));

  // Add a second with 5% dropped frames, and a second with no dropped frames.
  // The sliding window should shift from ok to very good over time.
  SimulateFrameSequence(MakeFrameSequence(1, 20), kFps / 20);
  SimulateFrameSequence({false}, kFps);
  EXPECT_TRUE(CheckSmoothnessBuckets({15, 12.5, 23, 4.5, 14.5, 25, 5.5}));

  // Clear the counter, then add a second with 100% dropped frames and a second
  // with 0% dropped frames. As the sliding window shifts each integer percent
  // (other than 100%) should be reported once, exactly matching the size of
  // each bucket.
  dropped_frame_counter_.Reset();
  dropped_frame_counter_.OnFcpReceived();
  SimulateFrameSequence({true}, kFps);
  SimulateFrameSequence({false}, kFps);
  EXPECT_TRUE(CheckSmoothnessBuckets({3, 3, 6, 13, 25, 25, 25}));
}

TEST_F(DroppedFrameCounterTest, FramesInFlightWhenFcpReceived) {
  // Start five frames in flight.
  std::vector<viz::BeginFrameArgs> pending_frames = SimulatePendingFrame(5);

  // Set that FCP was received after the third frame starts, but before it ends.
  base::TimeTicks time_fcp_sent =
      pending_frames[2].frame_time + pending_frames[2].interval / 2;
  dropped_frame_counter_.SetTimeFcpReceivedForTesting(time_fcp_sent);

  // End each of the frames as dropped. The first three should not count for
  // smoothness, only the last two.
  for (const auto& frame : pending_frames) {
    dropped_frame_counter_.OnEndFrame(frame, CreateStubFrameInfo(true));
  }
  EXPECT_EQ(dropped_frame_counter_.total_smoothness_dropped(), 2u);
}

TEST_F(DroppedFrameCounterTest, ForkedCompositorFrameReporter) {
  // Run different combinations of main and impl threads dropping, make sure
  // only one frame is counted as dropped each time.
  SimulateForkedFrame(false, false);
  EXPECT_EQ(dropped_frame_counter_.total_smoothness_dropped(), 0u);

  SimulateForkedFrame(true, false);
  EXPECT_EQ(dropped_frame_counter_.total_smoothness_dropped(), 1u);

  SimulateForkedFrame(false, true);
  EXPECT_EQ(dropped_frame_counter_.total_smoothness_dropped(), 2u);

  SimulateForkedFrame(true, true);
  EXPECT_EQ(dropped_frame_counter_.total_smoothness_dropped(), 3u);
}

TEST_F(DroppedFrameCounterTest, WorstSmoothnessTiming) {
  // Set an interval that rounds up nicely with 1 second.
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  static_assert(
      kFps % 5 == 0,
      "kFps must be a multiple of 5 because this test depends on it.");
  SetInterval(kInterval);

  // Prepare a second of pending frames, and send FCP after the last of these
  // frames.
  dropped_frame_counter_.Reset();
  std::vector<viz::BeginFrameArgs> pending_frames = SimulatePendingFrame(kFps);
  const auto& last_frame = pending_frames.back();
  base::TimeTicks time_fcp_sent =
      last_frame.frame_time + last_frame.interval / 2;
  dropped_frame_counter_.OnFcpReceived();
  dropped_frame_counter_.SetTimeFcpReceivedForTesting(time_fcp_sent);

  // End each of the pending frames as dropped. These shouldn't affect any of
  // the metrics.
  for (const auto& frame : pending_frames) {
    dropped_frame_counter_.OnEndFrame(frame, CreateStubFrameInfo(true));
  }

  // After FCP time, add a second each of 80% and 60%, and three seconds of 40%
  // dropped frames. This should be five seconds total.
  SimulateFrameSequence({false, true, true, true, true}, kFps / 5);
  SimulateFrameSequence({false, false, true, true, true}, kFps / 5);
  SimulateFrameSequence({false, false, false, true, true}, (kFps / 5) * 3);

  // Next two seconds are 20% dropped frames.
  SimulateFrameSequence({false, false, false, false, true}, (kFps / 5) * 2);

  // The first 1, 2, and 5 seconds shouldn't be recorded in the corresponding
  // max dropped after N seconds metrics.
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrame(), 80);
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrameAfter1Sec(), 60);
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrameAfter2Sec(), 40);
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrameAfter5Sec(), 20);

  // Next second is 100% dropped frames, all metrics should include this.
  SimulateFrameSequence({true}, kFps);
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrame(), 100);
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrameAfter1Sec(), 100);
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrameAfter2Sec(), 100);
  EXPECT_FLOAT_EQ(MaxPercentDroppedFrameAfter5Sec(), 100);
}

TEST_F(DroppedFrameCounterTest, ReportOnEveryFrameForUI) {
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  static_assert(
      kFps % 5 == 0,
      "kFps must be a multiple of 5 because this test depends on it.");
  SetInterval(kInterval);

  dropped_frame_counter_.EnableReporForUI();
  TestCustomMetricsRecorder recorder;

  // 4 seconds with 20% dropped frames.
  SimulateFrameSequence({false, false, false, false, true}, (kFps / 5) * 4);

  // Recorded (kFps * 3) samples of 20% dropped frame percentage. Only 3 seconds
  // of frames reported because there is no reports for the very 1st second.
  EXPECT_EQ(recorder.report_count(), kFps * 3);
  EXPECT_FLOAT_EQ(recorder.last_percent_dropped_frames(), 20.0f);

  recorder.Reset();

  // 4 seconds with 0 dropped frames.
  SimulateFrameSequence({false, false, false, false, false}, (kFps / 5) * 4);

  // Recorded (kFps * 4) samples of 0% dropped frame percentage.
  EXPECT_EQ(recorder.report_count(), kFps * 4);
  EXPECT_FLOAT_EQ(recorder.last_percent_dropped_frames(), 0.0f);
}

}  // namespace
}  // namespace cc
