// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_timing_history.h"

#include "base/test/metrics/histogram_tester.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/test/fake_compositor_frame_reporting_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class CompositorTimingHistoryTest;

class TestCompositorTimingHistory : public CompositorTimingHistory {
 public:
  TestCompositorTimingHistory(
      CompositorTimingHistoryTest* test,
      RenderingStatsInstrumentation* rendering_stats,
      CompositorFrameReportingController* reporting_controller)
      : CompositorTimingHistory(false,
                                RENDERER_UMA,
                                rendering_stats,
                                reporting_controller),
        test_(test) {}

  TestCompositorTimingHistory(const TestCompositorTimingHistory&) = delete;
  TestCompositorTimingHistory& operator=(const TestCompositorTimingHistory&) =
      delete;

 protected:
  base::TimeTicks Now() const override;

  CompositorTimingHistoryTest* test_;
};

class CompositorTimingHistoryTest : public testing::Test {
 public:
  CompositorTimingHistoryTest()
      : rendering_stats_(RenderingStatsInstrumentation::Create()),
        reporting_controller_(
            std::make_unique<FakeCompositorFrameReportingController>()),
        timing_history_(this,
                        rendering_stats_.get(),
                        reporting_controller_.get()) {
    AdvanceNowBy(base::TimeDelta::FromMilliseconds(1));
    timing_history_.SetRecordingEnabled(true);
  }

  void AdvanceNowBy(base::TimeDelta delta) { now_ += delta; }

  base::TimeTicks Now() { return now_; }

  // TODO(xidachen): the composited_animations_count should just be 0.
  void DrawMainFrame(int advance_ms,
                     int composited_animations_count,
                     int main_thread_animations_count,
                     bool current_frame_had_raf = false,
                     bool next_frame_has_pending_raf = false) {
    timing_history_.WillBeginMainFrame(true, Now());
    timing_history_.BeginMainFrameStarted(Now());
    timing_history_.WillCommit();
    timing_history_.DidCommit();
    timing_history_.ReadyToActivate();
    timing_history_.WillActivate();
    timing_history_.DidActivate();
    timing_history_.WillDraw();
    AdvanceNowBy(base::TimeDelta::FromMicroseconds(advance_ms));
    timing_history_.DidDraw(true, Now(), composited_animations_count,
                            main_thread_animations_count, current_frame_had_raf,
                            next_frame_has_pending_raf, false);
  }

  void DrawImplFrame(int advance_ms,
                     int composited_animations_count,
                     int main_thread_animations_count,
                     bool has_custom_property_animation) {
    timing_history_.WillBeginMainFrame(true, Now());
    timing_history_.BeginMainFrameStarted(Now());
    timing_history_.BeginMainFrameAborted();
    timing_history_.WillActivate();
    timing_history_.DidActivate();
    timing_history_.WillDraw();
    AdvanceNowBy(base::TimeDelta::FromMicroseconds(advance_ms));
    timing_history_.DidDraw(false, Now(), composited_animations_count,
                            main_thread_animations_count, false, false,
                            has_custom_property_animation);
  }

 protected:
  std::unique_ptr<RenderingStatsInstrumentation> rendering_stats_;
  std::unique_ptr<CompositorFrameReportingController> reporting_controller_;
  TestCompositorTimingHistory timing_history_;
  base::TimeTicks now_;
};

base::TimeTicks TestCompositorTimingHistory::Now() const {
  return test_->Now();
}

TEST_F(CompositorTimingHistoryTest, AllSequential_Commit) {
  base::TimeDelta one_second = base::TimeDelta::FromSeconds(1);

  // Critical BeginMainFrames are faster than non critical ones,
  // as expected.
  base::TimeDelta begin_main_frame_queue_duration =
      base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta begin_main_frame_start_to_ready_to_commit_duration =
      base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta prepare_tiles_duration = base::TimeDelta::FromMilliseconds(2);
  base::TimeDelta prepare_tiles_end_to_ready_to_activate_duration =
      base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta commit_to_ready_to_activate_duration =
      base::TimeDelta::FromMilliseconds(3);
  base::TimeDelta commit_duration = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta activate_duration = base::TimeDelta::FromMilliseconds(4);
  base::TimeDelta draw_duration = base::TimeDelta::FromMilliseconds(5);

  timing_history_.WillBeginMainFrame(true, Now());
  AdvanceNowBy(begin_main_frame_queue_duration);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  timing_history_.NotifyReadyToCommit(nullptr);
  timing_history_.WillCommit();
  AdvanceNowBy(commit_duration);
  timing_history_.DidCommit();
  timing_history_.WillPrepareTiles();
  AdvanceNowBy(prepare_tiles_duration);
  timing_history_.DidPrepareTiles();
  AdvanceNowBy(prepare_tiles_end_to_ready_to_activate_duration);
  timing_history_.ReadyToActivate();
  // Do not count idle time between notification and actual activation.
  AdvanceNowBy(one_second);
  timing_history_.WillActivate();
  AdvanceNowBy(activate_duration);
  timing_history_.DidActivate();
  // Do not count idle time between activate and draw.
  AdvanceNowBy(one_second);
  timing_history_.WillDraw();
  AdvanceNowBy(draw_duration);
  timing_history_.DidDraw(true, Now(), 0, 0, false, false, false);

  EXPECT_EQ(begin_main_frame_queue_duration,
            timing_history_.BeginMainFrameQueueDurationCriticalEstimate());
  EXPECT_EQ(begin_main_frame_queue_duration,
            timing_history_.BeginMainFrameQueueDurationNotCriticalEstimate());

  EXPECT_EQ(
      begin_main_frame_start_to_ready_to_commit_duration,
      timing_history_.BeginMainFrameStartToReadyToCommitDurationEstimate());
  EXPECT_EQ(commit_duration, timing_history_.CommitDurationEstimate());
  EXPECT_EQ(commit_to_ready_to_activate_duration,
            timing_history_.CommitToReadyToActivateDurationEstimate());
  EXPECT_EQ(prepare_tiles_duration,
            timing_history_.PrepareTilesDurationEstimate());
  EXPECT_EQ(activate_duration, timing_history_.ActivateDurationEstimate());
  EXPECT_EQ(draw_duration, timing_history_.DrawDurationEstimate());
}

TEST_F(CompositorTimingHistoryTest, AllSequential_BeginMainFrameAborted) {
  base::TimeDelta one_second = base::TimeDelta::FromSeconds(1);

  base::TimeDelta begin_main_frame_queue_duration =
      base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta begin_main_frame_start_to_ready_to_commit_duration =
      base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta prepare_tiles_duration = base::TimeDelta::FromMilliseconds(2);
  base::TimeDelta prepare_tiles_end_to_ready_to_activate_duration =
      base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta activate_duration = base::TimeDelta::FromMilliseconds(4);
  base::TimeDelta draw_duration = base::TimeDelta::FromMilliseconds(5);

  timing_history_.WillBeginMainFrame(false, Now());
  AdvanceNowBy(begin_main_frame_queue_duration);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  // BeginMainFrameAborted counts as a commit complete.
  timing_history_.BeginMainFrameAborted();
  timing_history_.WillPrepareTiles();
  AdvanceNowBy(prepare_tiles_duration);
  timing_history_.DidPrepareTiles();
  AdvanceNowBy(prepare_tiles_end_to_ready_to_activate_duration);
  // Do not count idle time between notification and actual activation.
  AdvanceNowBy(one_second);
  timing_history_.WillActivate();
  AdvanceNowBy(activate_duration);
  timing_history_.DidActivate();
  // Do not count idle time between activate and draw.
  AdvanceNowBy(one_second);
  timing_history_.WillDraw();
  AdvanceNowBy(draw_duration);
  timing_history_.DidDraw(false, Now(), 0, 0, false, false, false);

  EXPECT_EQ(base::TimeDelta(),
            timing_history_.BeginMainFrameQueueDurationCriticalEstimate());
  EXPECT_EQ(begin_main_frame_queue_duration,
            timing_history_.BeginMainFrameQueueDurationNotCriticalEstimate());

  EXPECT_EQ(prepare_tiles_duration,
            timing_history_.PrepareTilesDurationEstimate());
  EXPECT_EQ(activate_duration, timing_history_.ActivateDurationEstimate());
  EXPECT_EQ(draw_duration, timing_history_.DrawDurationEstimate());
}

TEST_F(CompositorTimingHistoryTest, BeginMainFrame_CriticalFaster) {
  // Critical BeginMainFrames are faster than non critical ones.
  base::TimeDelta begin_main_frame_queue_duration_critical =
      base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta begin_main_frame_queue_duration_not_critical =
      base::TimeDelta::FromMilliseconds(2);
  base::TimeDelta begin_main_frame_start_to_ready_to_commit_duration =
      base::TimeDelta::FromMilliseconds(1);

  timing_history_.WillBeginMainFrame(true, Now());
  AdvanceNowBy(begin_main_frame_queue_duration_critical);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  timing_history_.BeginMainFrameAborted();

  timing_history_.WillBeginMainFrame(false, Now());
  AdvanceNowBy(begin_main_frame_queue_duration_not_critical);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  timing_history_.BeginMainFrameAborted();

  // Since the critical BeginMainFrames are faster than non critical ones,
  // the expectations are straightforward.
  EXPECT_EQ(begin_main_frame_queue_duration_critical,
            timing_history_.BeginMainFrameQueueDurationCriticalEstimate());
  EXPECT_EQ(begin_main_frame_queue_duration_not_critical,
            timing_history_.BeginMainFrameQueueDurationNotCriticalEstimate());
}

TEST_F(CompositorTimingHistoryTest, BeginMainFrames_OldCriticalSlower) {
  // Critical BeginMainFrames are slower than non critical ones,
  // which is unexpected, but could occur if one type of frame
  // hasn't been sent for a significant amount of time.
  base::TimeDelta begin_main_frame_queue_duration_critical =
      base::TimeDelta::FromMilliseconds(2);
  base::TimeDelta begin_main_frame_queue_duration_not_critical =
      base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta begin_main_frame_start_to_ready_to_commit_duration =
      base::TimeDelta::FromMilliseconds(1);

  // A single critical frame that is slow.
  timing_history_.WillBeginMainFrame(true, Now());
  AdvanceNowBy(begin_main_frame_queue_duration_critical);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  // BeginMainFrameAborted counts as a commit complete.
  timing_history_.BeginMainFrameAborted();

  // A bunch of faster non critical frames that are newer.
  for (int i = 0; i < 100; i++) {
    timing_history_.WillBeginMainFrame(false, Now());
    AdvanceNowBy(begin_main_frame_queue_duration_not_critical);
    timing_history_.BeginMainFrameStarted(Now());
    AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
    // BeginMainFrameAborted counts as a commit complete.
    timing_history_.BeginMainFrameAborted();
  }

  // Recent fast non critical BeginMainFrames should result in the
  // critical estimate also being fast.
  EXPECT_EQ(begin_main_frame_queue_duration_not_critical,
            timing_history_.BeginMainFrameQueueDurationCriticalEstimate());
  EXPECT_EQ(begin_main_frame_queue_duration_not_critical,
            timing_history_.BeginMainFrameQueueDurationNotCriticalEstimate());
}

TEST_F(CompositorTimingHistoryTest, BeginMainFrames_NewCriticalSlower) {
  // Critical BeginMainFrames are slower than non critical ones,
  // which is unexpected, but could occur if one type of frame
  // hasn't been sent for a significant amount of time.
  base::TimeDelta begin_main_frame_queue_duration_critical =
      base::TimeDelta::FromMilliseconds(2);
  base::TimeDelta begin_main_frame_queue_duration_not_critical =
      base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta begin_main_frame_start_to_ready_to_commit_duration =
      base::TimeDelta::FromMilliseconds(1);

  // A single non critical frame that is fast.
  timing_history_.WillBeginMainFrame(false, Now());
  AdvanceNowBy(begin_main_frame_queue_duration_not_critical);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  timing_history_.BeginMainFrameAborted();

  // A bunch of slower critical frames that are newer.
  for (int i = 0; i < 100; i++) {
    timing_history_.WillBeginMainFrame(true, Now());
    AdvanceNowBy(begin_main_frame_queue_duration_critical);
    timing_history_.BeginMainFrameStarted(Now());
    AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
    timing_history_.BeginMainFrameAborted();
  }

  // Recent slow critical BeginMainFrames should result in the
  // not critical estimate also being slow.
  EXPECT_EQ(begin_main_frame_queue_duration_critical,
            timing_history_.BeginMainFrameQueueDurationCriticalEstimate());
  EXPECT_EQ(begin_main_frame_queue_duration_critical,
            timing_history_.BeginMainFrameQueueDurationNotCriticalEstimate());
}

void TestAnimationUMA(const base::HistogramTester& histogram_tester,
                      base::HistogramBase::Count composited_animation_frames,
                      base::HistogramBase::Count main_thread_animation_frames) {
  histogram_tester.ExpectTotalCount(
      "Scheduling.Renderer.DrawIntervalWithCompositedAnimations2",
      composited_animation_frames);
  histogram_tester.ExpectTotalCount(
      "Scheduling.Renderer.DrawIntervalWithMainThreadAnimations2",
      main_thread_animation_frames);
}

TEST_F(CompositorTimingHistoryTest, AnimationNotReported) {
  base::HistogramTester histogram_tester;

  // Initial frame has no main-thread animations or rAF.
  DrawMainFrame(123, 0, 0);
  TestAnimationUMA(histogram_tester, 0, 0);

  // The next frame has one composited and one main thread animation running,
  // but as the previous frame had no animation we shouldn't report anything.
  DrawMainFrame(456, 1, 1);
  TestAnimationUMA(histogram_tester, 0, 0);

  DrawMainFrame(123, 0, 0);
  TestAnimationUMA(histogram_tester, 0, 0);

  // The next frame has just one main thread animation running, but again as the
  // previous frame had no animation we shouldn't report anything.
  DrawMainFrame(456, 0, 1);
  TestAnimationUMA(histogram_tester, 0, 0);

  DrawMainFrame(123, 0, 0);
  TestAnimationUMA(histogram_tester, 0, 0);

  // The next frame has no main thread animations but did have a rAF callback.
  // Again as the previous frame had no visual change we shouldn't report.
  DrawMainFrame(123, 0, 0, true);
  TestAnimationUMA(histogram_tester, 0, 0);

  DrawMainFrame(123, 0, 0);
  TestAnimationUMA(histogram_tester, 0, 0);

  // Finally, test the combination of both main thread animations and rAF
  // callbacks being called.
  DrawMainFrame(123, 1, 2, true);
  TestAnimationUMA(histogram_tester, 0, 0);
}

TEST_F(CompositorTimingHistoryTest, ConsecutiveFramesAnimationsReported) {
  base::HistogramTester histogram_tester;

  DrawMainFrame(123, 1, 0);
  TestAnimationUMA(histogram_tester, 0, 0);

  DrawMainFrame(456, 1, 0);
  TestAnimationUMA(histogram_tester, 1, 0);
  histogram_tester.ExpectBucketCount(
      "Scheduling.Renderer.DrawIntervalWithCompositedAnimations2", 456, 1);

  DrawMainFrame(321, 0, 1);
  TestAnimationUMA(histogram_tester, 1, 0);

  DrawMainFrame(654, 0, 1);
  TestAnimationUMA(histogram_tester, 1, 1);
  histogram_tester.ExpectBucketCount(
      "Scheduling.Renderer.DrawIntervalWithMainThreadAnimations2", 654, 1);

  DrawMainFrame(123, 0, 1);
  TestAnimationUMA(histogram_tester, 1, 2);

  DrawMainFrame(456, 0, 1);
  TestAnimationUMA(histogram_tester, 1, 3);

  // Main thread and rAF animations are both considered to be part of the same
  // animation type.
  DrawMainFrame(789, 0, 0, true, true);
  TestAnimationUMA(histogram_tester, 1, 4);

  DrawMainFrame(987, 0, 1, false);
  TestAnimationUMA(histogram_tester, 1, 5);

  // However if there is no pending rAF for a frame, we don't count the one
  // after it as being linked.
  DrawMainFrame(789, 0, 0, true, false);
  TestAnimationUMA(histogram_tester, 1, 6);

  DrawMainFrame(987, 0, 0, true, true);
  TestAnimationUMA(histogram_tester, 1, 6);
}

TEST_F(CompositorTimingHistoryTest, InterFrameAnimationsNotReported) {
  base::HistogramTester histogram_tester;

  DrawMainFrame(123, 0, 1);
  TestAnimationUMA(histogram_tester, 0, 0);

  // The previous frame had a main thread animation, where the current one is
  // main thread compositable animation, we don't measure the timing from a
  // different animation type.
  DrawMainFrame(456, 0, 1);
  TestAnimationUMA(histogram_tester, 0, 1);

  DrawMainFrame(321, 1, 0);
  TestAnimationUMA(histogram_tester, 0, 1);

  DrawMainFrame(654, 0, 1);
  TestAnimationUMA(histogram_tester, 0, 1);

  DrawMainFrame(123, 1, 0);
  TestAnimationUMA(histogram_tester, 0, 1);
}

TEST_F(CompositorTimingHistoryTest, AnimationsWithNewActiveTreeNotUsed) {
  base::HistogramTester histogram_tester;

  DrawImplFrame(123, 1, 1, false);
  TestAnimationUMA(histogram_tester, 0, 0);

  DrawImplFrame(456, 1, 0, false);
  TestAnimationUMA(histogram_tester, 1, 0);
  histogram_tester.ExpectBucketCount(
      "Scheduling.Renderer.DrawIntervalWithCompositedAnimations2", 456, 1);

  DrawMainFrame(321, 0, 1);
  TestAnimationUMA(histogram_tester, 1, 0);

  // This frame verifies that we record that there is a composited animation,
  // so in the next frame when there is a composited animation, we report it.
  DrawImplFrame(234, 1, 1, false);
  TestAnimationUMA(histogram_tester, 1, 0);

  // Even though the previous frame had no main thread animation, we report it
  // in this frame because the previous main frame had a main thread animation
  // with the time between main frame draws.
  DrawMainFrame(654, 1, 1);
  TestAnimationUMA(histogram_tester, 2, 1);
  histogram_tester.ExpectBucketCount(
      "Scheduling.Renderer.DrawIntervalWithCompositedAnimations2", 654, 1);
  // The recorded time for this main thread animation should be the total time
  // between the two new tree activations, which is 234 + 654 = 888.
  histogram_tester.ExpectBucketCount(
      "Scheduling.Renderer.DrawIntervalWithMainThreadAnimations2", 888, 1);

  DrawImplFrame(123, 1, 0, false);
  TestAnimationUMA(histogram_tester, 3, 1);
  histogram_tester.ExpectBucketCount(
      "Scheduling.Renderer.DrawIntervalWithCompositedAnimations2", 123, 1);
}

TEST_F(CompositorTimingHistoryTest, CustomPropertyAnimations) {
  base::HistogramTester histogram_tester;

  DrawImplFrame(123, 1, 0, true);
  TestAnimationUMA(histogram_tester, 0, 0);

  DrawImplFrame(456, 1, 0, true);
  TestAnimationUMA(histogram_tester, 1, 0);

  histogram_tester.ExpectBucketCount(
      "Scheduling.Renderer.DrawIntervalWithCompositedAnimations2", 456, 1);
  histogram_tester.ExpectBucketCount(
      "Scheduling.Renderer.DrawIntervalWithCustomPropertyAnimations2", 456, 1);

  DrawImplFrame(1234, 1, 0, false);
  DrawImplFrame(2345, 1, 0, true);
  TestAnimationUMA(histogram_tester, 3, 0);
  histogram_tester.ExpectBucketCount(
      "Scheduling.Renderer.DrawIntervalWithCompositedAnimations2", 2345, 1);
  // This impl frame does have custom property animation, but the previous impl
  // frame doesn't, so we won't report it.
  histogram_tester.ExpectBucketCount(
      "Scheduling.Renderer.DrawIntervalWithCustomPropertyAnimations2", 2345, 0);
}

}  // namespace
}  // namespace cc
