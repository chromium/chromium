// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/dropped_frame_counter.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "cc/animation/animation_host.h"
#include "cc/base/features.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/custom_metrics_recorder.h"
#include "cc/metrics/frame_sorter.h"
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
    dropped_frame_counter_ = std::make_unique<DroppedFrameCounter>();
    dropped_frame_counter_->set_total_counter(&total_frame_counter_);
    dropped_frame_counter_->OnFirstContentfulPaintReceived();
    frame_sorter_.AddObserver(dropped_frame_counter_.get());
  }
  ~DroppedFrameCounterTest() override = default;

  // For each boolean in frame_states produces a frame
  void SimulateFrameSequence(std::vector<bool> frame_states, int repeat) {
    for (int i = 0; i < repeat; i++) {
      for (auto is_dropped : frame_states) {
        viz::BeginFrameArgs args_ = SimulateBeginFrameArgs();
        if (dropped_frame_counter_->first_contentful_paint_received()) {
          frame_sorter_.AddNewFrame(args_);
          frame_sorter_.AddFrameResult(args_, CreateStubFrameInfo(is_dropped));
        }
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
      if (dropped_frame_counter_->first_contentful_paint_received()) {
        frame_sorter_.AddNewFrame(args[i]);
      }
      sequence_number_++;
      frame_time_ += interval_;
    }
    return args;
  }

  // Simulate a main and impl thread update on the same frame.
  void SimulateForkedFrame(bool main_dropped, bool impl_dropped) {
    viz::BeginFrameArgs args_ = SimulateBeginFrameArgs();
    if (dropped_frame_counter_->first_contentful_paint_received()) {
      frame_sorter_.AddNewFrame(args_);
      frame_sorter_.AddNewFrame(args_);
    }
    // End the 'main thread' arm of the fork.
    auto main_info = CreateStubFrameInfo(main_dropped);
    main_info.main_thread_response = FrameInfo::MainThreadResponse::kIncluded;
    frame_sorter_.AddFrameResult(args_, main_info);

    // End the 'compositor thread' arm of the fork.
    auto impl_info = CreateStubFrameInfo(impl_dropped);
    impl_info.main_thread_response = FrameInfo::MainThreadResponse::kMissing;
    frame_sorter_.AddFrameResult(args_, impl_info);

    sequence_number_++;
    frame_time_ += interval_;
  }

  void AdvancetimeByIntervals(int interval_count) {
    frame_time_ += interval_ * interval_count;
  }

  double PercentDroppedFrameMedian() {
    return dropped_frame_counter_->SlidingWindowMedianPercentDropped(
        smoothness_strategy_);
  }

  double PercentDroppedFrameVariance() {
    return dropped_frame_counter_->SlidingWindowPercentDroppedVariance(
        smoothness_strategy_);
  }

  const DroppedFrameCounter::SlidingWindowHistogram*
  GetSlidingWindowHistogram() {
    return dropped_frame_counter_->GetSlidingWindowHistogram(
        smoothness_strategy_);
  }

  double GetTotalFramesInWindow() { return base::Seconds(1) / interval_; }

  void SetInterval(base::TimeDelta interval) { interval_ = interval; }

  base::TimeTicks GetNextFrameTime() const { return frame_time_ + interval_; }

 public:
  std::unique_ptr<DroppedFrameCounter> dropped_frame_counter_;
  FrameSorter frame_sorter_;

 private:
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
  TotalFrameCounter total_frame_counter_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test class that supports parameterized tests for each of the different
// SmoothnessStrategy.

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

std::vector<SmoothnessStrategy> GetSmoothnessStrategyParams() {
  return std::vector<SmoothnessStrategy>{
      SmoothnessStrategy::kDefaultStrategy,
      SmoothnessStrategy::kCompositorFocusedStrategy};
}

std::string SmoothnessStrategyToString(const SmoothnessStrategy& s) {
  if (s == SmoothnessStrategy::kDefaultStrategy) {
    return "DefaultStrategy";
  } else if (s == SmoothnessStrategy::kCompositorFocusedStrategy) {
    return "CompositorFocusedStrategy";
  }
  return "INVALID";
}

INSTANTIATE_TEST_SUITE_P(,
                         SmoothnessStrategyDroppedFrameCounterTest,
                         ::testing::ValuesIn(GetSmoothnessStrategyParams()),
                         [](auto& param) {
                           return SmoothnessStrategyToString(param.param);
                         });

TEST_P(SmoothnessStrategyDroppedFrameCounterTest, SimplePattern1) {
  // 2 out of every 3 frames are dropped (In total 80 frames out of 120).
  SimulateFrameSequence({true, true, true, false, true, false}, 20);

  // The max is the following window:
  //    16 * <sequence> + {true, true, true, false
  EXPECT_EQ(PercentDroppedFrameMedian(), 65);
  EXPECT_LE(PercentDroppedFrameVariance(), 1);
}

TEST_P(SmoothnessStrategyDroppedFrameCounterTest, SimplePattern2) {
  // 1 out of every 5 frames are dropped (In total 24 frames out of 120).
  SimulateFrameSequence({false, false, false, false, true}, 24);

  // 20th bucket, and as a result 95th percentile is also 20.
  EXPECT_EQ(PercentDroppedFrameMedian(), 20);
  EXPECT_LE(PercentDroppedFrameVariance(), 1);
}

TEST_P(SmoothnessStrategyDroppedFrameCounterTest, IncompleteWindow) {
  // There are only 5 frames submitted, so Max, 95pct, median and variance
  // should report zero.
  SimulateFrameSequence({false, false, false, false, true}, 1);
  EXPECT_EQ(PercentDroppedFrameMedian(), 0);
  EXPECT_LE(PercentDroppedFrameVariance(), 1);
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
  dropped_frame_counter_->ResetPendingFrames(GetNextFrameTime());
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
  dropped_frame_counter_->ResetPendingFrames(GetNextFrameTime());
  AdvancetimeByIntervals(kFps * 97);

  // A single frame to flush the pipeline.
  SimulateFrameSequence({false}, 1);

  EXPECT_EQ(histogram->GetPercentDroppedFramePercentile(0.96), 0u);
  EXPECT_GT(histogram->GetPercentDroppedFramePercentile(0.97), 0u);
}

TEST_F(DroppedFrameCounterTest, FramesInFlightWhenFcpReceived) {
  // Start five frames in flight.
  std::vector<viz::BeginFrameArgs> pending_frames = SimulatePendingFrame(5);

  // Set that FCP was received after the third frame starts, but before it ends.
  base::TimeTicks time_fcp_sent =
      pending_frames[2].frame_time + pending_frames[2].interval / 2;
  dropped_frame_counter_->SetTimeFirstContentfulPaintReceivedForTesting(
      time_fcp_sent);

  // End each of the frames as dropped. The first three should not count for
  // smoothness, only the last two.
  for (const auto& frame : pending_frames) {
    frame_sorter_.AddFrameResult(frame, CreateStubFrameInfo(true));
  }
  EXPECT_EQ(dropped_frame_counter_->total_smoothness_dropped(), 2u);
}

TEST_F(DroppedFrameCounterTest, ForkedCompositorFrameReporter) {
  // Run different combinations of main and impl threads dropping, make sure
  // only one frame is counted as dropped each time.
  SimulateForkedFrame(false, false);
  EXPECT_EQ(dropped_frame_counter_->total_smoothness_dropped(), 0u);

  SimulateForkedFrame(true, false);
  EXPECT_EQ(dropped_frame_counter_->total_smoothness_dropped(), 1u);

  SimulateForkedFrame(false, true);
  EXPECT_EQ(dropped_frame_counter_->total_smoothness_dropped(), 2u);

  SimulateForkedFrame(true, true);
  EXPECT_EQ(dropped_frame_counter_->total_smoothness_dropped(), 3u);
}

TEST_F(DroppedFrameCounterTest, ReportOnEveryFrameForUI) {
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  static_assert(
      kFps % 5 == 0,
      "kFps must be a multiple of 5 because this test depends on it.");
  SetInterval(kInterval);

  dropped_frame_counter_->EnableReportForUI();
  TestCustomMetricsRecorder recorder;

  // 4 seconds with 20% dropped frames.
  SimulateFrameSequence({false, false, false, false, true}, (kFps / 5) * 4);

  // Recorded (kFps * 3) samples of 20% dropped frame percentage. Only 3 seconds
  // of frames reported because there is no reports for the very 1st second.
  // Off-by-one introduced by FrameSorter refactor.
  // We have inverted the order in which we call DFC::AddSortedFrame and
  // DFC::OnEndFrame, meaning that DFC's sliding_window_current_percent_dropped_
  // is set after OnEndFrame has been called at the 1s second threshold.
  // Therefore, we expect one less call to the frame recorder.
  EXPECT_EQ(recorder.report_count(), (kFps * 3) - 1);
  EXPECT_FLOAT_EQ(recorder.last_percent_dropped_frames(), 20.0f);

  recorder.Reset();

  // 4 seconds with 0 dropped frames.
  SimulateFrameSequence({false, false, false, false, false}, (kFps / 5) * 4);

  // Recorded (kFps * 4) samples of 0% dropped frame percentage.
  EXPECT_EQ(recorder.report_count(), kFps * 4);
  EXPECT_FLOAT_EQ(recorder.last_percent_dropped_frames(), 0.0f);
}

class DroppedFrameCounterLegacyMetricsTest : public DroppedFrameCounterTest {
 public:
  DroppedFrameCounterLegacyMetricsTest();
  ~DroppedFrameCounterLegacyMetricsTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

DroppedFrameCounterLegacyMetricsTest::DroppedFrameCounterLegacyMetricsTest() {
  frame_sorter_.RemoveObserver(dropped_frame_counter_.get());
  dropped_frame_counter_ = std::make_unique<DroppedFrameCounter>();
  frame_sorter_.Reset();
  dropped_frame_counter_->OnFirstContentfulPaintReceived();
  frame_sorter_.AddObserver(dropped_frame_counter_.get());
}

TEST_F(DroppedFrameCounterLegacyMetricsTest, DoesNotReportLegacyMetrics) {
  constexpr auto kInterval = base::Milliseconds(10);
  constexpr int kFps = base::Seconds(1).IntDiv(kInterval);
  static_assert(
      kFps % 5 == 0,
      "kFps must be a multiple of 5 because this test depends on it.");
  SetInterval(kInterval);

  dropped_frame_counter_->EnableReportForUI();
  TestCustomMetricsRecorder recorder;

  // 5 seconds with 20% dropped frames.
  SimulateFrameSequence({false, false, false, false, true}, (kFps / 5) * 6);
  // We have inverted the order in which we call DFC::AddSortedFrame and
  // DFC::OnEndFrame, meaning that DFC's sliding_window_current_percent_dropped_
  // is set after OnEndFrame has been called at the 1s second threshold.
  // Therefore, we expect one less call to the frame recorder.
  EXPECT_EQ(recorder.report_count(), (5 * kFps) - 1);

  // The following metrics should report data.
  // Average calculation
  EXPECT_GT(dropped_frame_counter_->total_smoothness_dropped(), 0.0);
  // Median calculation
  EXPECT_GT(PercentDroppedFrameMedian(), 0.0);
  // Compositor-focused median calculation
  EXPECT_GT(dropped_frame_counter_->SlidingWindowMedianPercentDropped(
                SmoothnessStrategy::kCompositorFocusedStrategy),
            0.0);
}

}  // namespace
}  // namespace cc
