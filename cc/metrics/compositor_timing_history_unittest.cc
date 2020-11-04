// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_timing_history.h"

#include "base/logging.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/metrics/dropped_frame_counter.h"
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

 protected:
  std::unique_ptr<RenderingStatsInstrumentation> rendering_stats_;
  std::unique_ptr<CompositorFrameReportingController> reporting_controller_;
  TestCompositorTimingHistory timing_history_;
  base::TimeTicks now_;
  uint64_t sequence_number = 0;

  viz::BeginFrameArgs GetFakeBeginFrameArg(bool on_critical_path = true) {
    viz::BeginFrameArgs args = viz::BeginFrameArgs();
    const uint64_t kSourceId = 1;
    args.frame_id = {kSourceId, ++sequence_number};
    args.frame_time = Now();
    args.on_critical_path = on_critical_path;
    return args;
  }
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

  timing_history_.WillBeginMainFrame(GetFakeBeginFrameArg());
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
  timing_history_.DidDraw(true, false);

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

  viz::BeginFrameArgs args_ = GetFakeBeginFrameArg(false);
  timing_history_.WillBeginMainFrame(args_);
  AdvanceNowBy(begin_main_frame_queue_duration);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  // BeginMainFrameAborted counts as a commit complete.
  timing_history_.BeginMainFrameAborted(
      args_.frame_id, CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);
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
  timing_history_.DidDraw(false, false);

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

  viz::BeginFrameArgs args_ = GetFakeBeginFrameArg();
  timing_history_.WillBeginMainFrame(args_);
  AdvanceNowBy(begin_main_frame_queue_duration_critical);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  timing_history_.BeginMainFrameAborted(
      args_.frame_id, CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);

  args_ = GetFakeBeginFrameArg(false);
  timing_history_.WillBeginMainFrame(args_);
  AdvanceNowBy(begin_main_frame_queue_duration_not_critical);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  timing_history_.BeginMainFrameAborted(
      args_.frame_id, CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);

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
  viz::BeginFrameArgs args_ = GetFakeBeginFrameArg();
  timing_history_.WillBeginMainFrame(args_);
  AdvanceNowBy(begin_main_frame_queue_duration_critical);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  // BeginMainFrameAborted counts as a commit complete.
  timing_history_.BeginMainFrameAborted(
      args_.frame_id, CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);

  // A bunch of faster non critical frames that are newer.
  for (int i = 0; i < 100; i++) {
    args_ = GetFakeBeginFrameArg(false);
    timing_history_.WillBeginMainFrame(args_);
    AdvanceNowBy(begin_main_frame_queue_duration_not_critical);
    timing_history_.BeginMainFrameStarted(Now());
    AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
    // BeginMainFrameAborted counts as a commit complete.
    timing_history_.BeginMainFrameAborted(
        args_.frame_id, CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);
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
  viz::BeginFrameArgs args_ = GetFakeBeginFrameArg(false);
  timing_history_.WillBeginMainFrame(args_);
  AdvanceNowBy(begin_main_frame_queue_duration_not_critical);
  timing_history_.BeginMainFrameStarted(Now());
  AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
  timing_history_.BeginMainFrameAborted(
      args_.frame_id, CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);

  // A bunch of slower critical frames that are newer.
  for (int i = 0; i < 100; i++) {
    args_ = GetFakeBeginFrameArg();
    timing_history_.WillBeginMainFrame(args_);
    AdvanceNowBy(begin_main_frame_queue_duration_critical);
    timing_history_.BeginMainFrameStarted(Now());
    AdvanceNowBy(begin_main_frame_start_to_ready_to_commit_duration);
    timing_history_.BeginMainFrameAborted(
        args_.frame_id, CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);
  }

  // Recent slow critical BeginMainFrames should result in the
  // not critical estimate also being slow.
  EXPECT_EQ(begin_main_frame_queue_duration_critical,
            timing_history_.BeginMainFrameQueueDurationCriticalEstimate());
  EXPECT_EQ(begin_main_frame_queue_duration_critical,
            timing_history_.BeginMainFrameQueueDurationNotCriticalEstimate());
}

TEST_F(CompositorTimingHistoryTest, DroppedFrameCountOnMainFrameAbort) {
  DroppedFrameCounter c;
  reporting_controller_->SetDroppedFrameCounter(&c);

  // Start a few begin-main-frames, but abort the main-frames due to no damage.
  for (int i = 0; i < 5; ++i) {
    auto args = GetFakeBeginFrameArg(false);
    timing_history_.WillBeginMainFrame(args);
    timing_history_.BeginMainFrameStarted(Now());
    timing_history_.BeginMainFrameAborted(
        args.frame_id, CommitEarlyOutReason::FINISHED_NO_UPDATES);
  }
  EXPECT_EQ(0u, c.total_compositor_dropped());

  // Start a few begin-main-frames, but abort the main-frames due to no damage.
  for (int i = 0; i < 5; ++i) {
    auto args = GetFakeBeginFrameArg(false);
    timing_history_.WillBeginMainFrame(args);
    timing_history_.BeginMainFrameStarted(Now());
    timing_history_.BeginMainFrameAborted(
        args.frame_id, CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT);
  }
  timing_history_.WillBeginMainFrame(GetFakeBeginFrameArg());
  EXPECT_EQ(5u, c.total_compositor_dropped());

  reporting_controller_->SetDroppedFrameCounter(nullptr);
  reporting_controller_ = nullptr;
}

}  // namespace
}  // namespace cc
