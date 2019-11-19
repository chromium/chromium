// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_tracker.h"

#include <vector>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc {

class FrameSequenceTrackerTest : public testing::Test {
 public:
  const uint32_t kImplDamage = 0x1;
  const uint32_t kMainDamage = 0x2;

  FrameSequenceTrackerTest()
      : compositor_frame_reporting_controller_(
            std::make_unique<CompositorFrameReportingController>()),
        collection_(/* is_single_threaded=*/false,
                    compositor_frame_reporting_controller_.get()) {
    collection_.StartSequence(FrameSequenceTrackerType::kTouchScroll);
    tracker_ = collection_.GetTrackerForTesting(
        FrameSequenceTrackerType::kTouchScroll);
  }
  ~FrameSequenceTrackerTest() override = default;

  void CreateNewTracker() {
    collection_.StartSequence(FrameSequenceTrackerType::kTouchScroll);
    tracker_ = collection_.GetTrackerForTesting(
        FrameSequenceTrackerType::kTouchScroll);
  }

  viz::BeginFrameArgs CreateBeginFrameArgs(
      uint64_t source_id,
      uint64_t sequence_number,
      base::TimeTicks now = base::TimeTicks::Now()) {
    auto interval = base::TimeDelta::FromMilliseconds(16);
    auto deadline = now + interval;
    return viz::BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, source_id,
                                       sequence_number, now, deadline, interval,
                                       viz::BeginFrameArgs::NORMAL);
  }

  void StartImplAndMainFrames(const viz::BeginFrameArgs& args) {
    collection_.NotifyBeginImplFrame(args);
    collection_.NotifyBeginMainFrame(args);
  }

  uint32_t DispatchCompleteFrame(const viz::BeginFrameArgs& args,
                                 uint32_t damage_type,
                                 bool has_missing_content = false) {
    StartImplAndMainFrames(args);

    if (damage_type & kImplDamage) {
      if (!(damage_type & kMainDamage)) {
        collection_.NotifyMainFrameCausedNoDamage(args);
      }
      uint32_t frame_token = NextFrameToken();
      collection_.NotifySubmitFrame(frame_token, has_missing_content,
                                    viz::BeginFrameAck(args, true), args);
      return frame_token;
    } else {
      collection_.NotifyImplFrameCausedNoDamage(
          viz::BeginFrameAck(args, false));
      collection_.NotifyMainFrameCausedNoDamage(args);
    }
    return 0;
  }

  uint32_t NextFrameToken() {
    static uint32_t frame_token = 0;
    return ++frame_token;
  }

  // Check whether a type of tracker exists in |frame_trackers_| or not.
  bool TrackerExists(FrameSequenceTrackerType type) const {
    return collection_.frame_trackers_.contains(type);
  }

  void TestNotifyFramePresented() {
    collection_.StartSequence(FrameSequenceTrackerType::kCompositorAnimation);
    collection_.StartSequence(FrameSequenceTrackerType::kMainThreadAnimation);
    // The kTouchScroll tracker is created in the test constructor, and the
    // kUniversal tracker is created in the FrameSequenceTrackerCollection
    // constructor.
    EXPECT_EQ(collection_.frame_trackers_.size(), 3u);
    collection_.StartSequence(FrameSequenceTrackerType::kUniversal);
    EXPECT_EQ(collection_.frame_trackers_.size(), 4u);

    collection_.StopSequence(kCompositorAnimation);
    EXPECT_EQ(collection_.frame_trackers_.size(), 3u);
    EXPECT_TRUE(collection_.frame_trackers_.contains(
        FrameSequenceTrackerType::kMainThreadAnimation));
    EXPECT_TRUE(collection_.frame_trackers_.contains(
        FrameSequenceTrackerType::kTouchScroll));
    ASSERT_EQ(collection_.removal_trackers_.size(), 1u);
    EXPECT_EQ(collection_.removal_trackers_[0]->type_,
              FrameSequenceTrackerType::kCompositorAnimation);

    gfx::PresentationFeedback feedback;
    collection_.NotifyFramePresented(1u, feedback);
    // NotifyFramePresented should call ReportFramePresented on all the
    // |removal_trackers_|, which changes their termination_status_ to
    // kReadyForTermination. So at this point, the |removal_trackers_| should be
    // empty.
    EXPECT_TRUE(collection_.removal_trackers_.empty());
  }

  void ReportMetrics() {
    base::HistogramTester histogram_tester;

    // Test that there is no main thread frames expected.
    tracker_->impl_throughput_.frames_expected = 100u;
    tracker_->impl_throughput_.frames_produced = 85u;
    tracker_->ReportMetrics();
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.CompositorThread.TouchScroll", 1u);
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.MainThread.TouchScroll", 0u);
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.SlowerThread.TouchScroll", 1u);

    // Test that both are reported.
    tracker_->main_throughput_.frames_expected = 50u;
    tracker_->main_throughput_.frames_produced = 25u;
    tracker_->ReportMetrics();
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.CompositorThread.TouchScroll", 2u);
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.MainThread.TouchScroll", 1u);
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.SlowerThread.TouchScroll", 2u);

    // Test that none is reported.
    tracker_->main_throughput_.frames_expected = 2u;
    tracker_->main_throughput_.frames_produced = 1u;
    tracker_->impl_throughput_.frames_expected = 2u;
    tracker_->impl_throughput_.frames_produced = 1u;
    tracker_->ReportMetrics();
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.CompositorThread.TouchScroll", 2u);
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.MainThread.TouchScroll", 1u);
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.SlowerThread.TouchScroll", 2u);

    // Test the case where compositor and main thread have the same throughput.
    tracker_->impl_throughput_.frames_expected = 20u;
    tracker_->impl_throughput_.frames_produced = 18u;
    tracker_->main_throughput_.frames_expected = 20u;
    tracker_->main_throughput_.frames_produced = 18u;
    tracker_->ReportMetrics();
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.CompositorThread.TouchScroll", 3u);
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.MainThread.TouchScroll", 2u);
    histogram_tester.ExpectTotalCount(
        "Graphics.Smoothness.Throughput.SlowerThread.TouchScroll", 3u);
  }

  base::TimeDelta TimeDeltaToReort() const {
    return tracker_->time_delta_to_report_;
  }

  unsigned NumberOfTrackers() const {
    return collection_.frame_trackers_.size();
  }
  unsigned NumberOfRemovalTrackers() const {
    return collection_.removal_trackers_.size();
  }

 protected:
  uint32_t number_of_frames_checkerboarded() const {
    return tracker_->checkerboarding_.frames_checkerboarded;
  }

  std::unique_ptr<CompositorFrameReportingController>
      compositor_frame_reporting_controller_;
  FrameSequenceTrackerCollection collection_;
  FrameSequenceTracker* tracker_;
};

// Tests that the tracker works correctly when the source-id for the
// begin-frames change.
TEST_F(FrameSequenceTrackerTest, SourceIdChangeDuringSequence) {
  const uint64_t source_1 = 1;
  uint64_t sequence_1 = 0;

  // Dispatch some frames, both causing damage to impl/main, and both impl and
  // main providing damage to the frame.
  auto args_1 = CreateBeginFrameArgs(source_1, ++sequence_1);
  DispatchCompleteFrame(args_1, kImplDamage | kMainDamage);
  args_1 = CreateBeginFrameArgs(source_1, ++sequence_1);
  DispatchCompleteFrame(args_1, kImplDamage | kMainDamage);

  // Start a new tracker.
  CreateNewTracker();

  // Change the source-id, and start an impl frame. This time, the main-frame
  // does not provide any damage.
  const uint64_t source_2 = 2;
  uint64_t sequence_2 = 0;
  auto args_2 = CreateBeginFrameArgs(source_2, ++sequence_2);
  collection_.NotifyBeginImplFrame(args_2);
  collection_.NotifyBeginMainFrame(args_2);
  collection_.NotifyMainFrameCausedNoDamage(args_2);
  // Since the main-frame did not have any new damage from the latest
  // BeginFrameArgs, the submit-frame will carry the previous BeginFrameArgs
  // (from source_1);
  collection_.NotifySubmitFrame(NextFrameToken(), /*has_missing_content=*/false,
                                viz::BeginFrameAck(args_2, true), args_1);
}

TEST_F(FrameSequenceTrackerTest, UniversalTrackerCreation) {
  // The universal tracker should be explicitly created by the object that
  // manages the |collection_|
  EXPECT_FALSE(TrackerExists(FrameSequenceTrackerType::kUniversal));
}

TEST_F(FrameSequenceTrackerTest, UniversalTrackerRestartableAfterClearAll) {
  collection_.StartSequence(FrameSequenceTrackerType::kUniversal);
  EXPECT_TRUE(TrackerExists(FrameSequenceTrackerType::kUniversal));

  collection_.ClearAll();
  EXPECT_FALSE(TrackerExists(FrameSequenceTrackerType::kUniversal));

  collection_.StartSequence(FrameSequenceTrackerType::kUniversal);
  EXPECT_TRUE(TrackerExists(FrameSequenceTrackerType::kUniversal));
}

TEST_F(FrameSequenceTrackerTest, TestNotifyFramePresented) {
  TestNotifyFramePresented();
}

// Base case for checkerboarding: present a single frame with checkerboarding,
// followed by a non-checkerboard frame.
TEST_F(FrameSequenceTrackerTest, CheckerboardingSimple) {
  CreateNewTracker();

  const uint64_t source_1 = 1;
  uint64_t sequence_1 = 0;

  // Dispatch some frames, both causing damage to impl/main, and both impl and
  // main providing damage to the frame.
  auto args_1 = CreateBeginFrameArgs(source_1, ++sequence_1);
  bool has_missing_content = true;
  auto frame_token = DispatchCompleteFrame(args_1, kImplDamage | kMainDamage,
                                           has_missing_content);

  const auto interval = viz::BeginFrameArgs::DefaultInterval();
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), interval, 0);
  collection_.NotifyFramePresented(frame_token, feedback);

  // Submit another frame with no checkerboarding.
  has_missing_content = false;
  frame_token =
      DispatchCompleteFrame(CreateBeginFrameArgs(source_1, ++sequence_1),
                            kImplDamage | kMainDamage, has_missing_content);
  feedback =
      gfx::PresentationFeedback(base::TimeTicks::Now() + interval, interval, 0);
  collection_.NotifyFramePresented(frame_token, feedback);

  EXPECT_EQ(1u, number_of_frames_checkerboarded());
}

// Present a single frame with checkerboarding, followed by a non-checkerboard
// frame after a few vsyncs.
TEST_F(FrameSequenceTrackerTest, CheckerboardingMultipleFrames) {
  CreateNewTracker();

  const uint64_t source_1 = 1;
  uint64_t sequence_1 = 0;

  // Dispatch some frames, both causing damage to impl/main, and both impl and
  // main providing damage to the frame.
  auto args_1 = CreateBeginFrameArgs(source_1, ++sequence_1);
  bool has_missing_content = true;
  auto frame_token = DispatchCompleteFrame(args_1, kImplDamage | kMainDamage,
                                           has_missing_content);

  const auto interval = viz::BeginFrameArgs::DefaultInterval();
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), interval, 0);
  collection_.NotifyFramePresented(frame_token, feedback);

  // Submit another frame with no checkerboarding.
  has_missing_content = false;
  frame_token =
      DispatchCompleteFrame(CreateBeginFrameArgs(source_1, ++sequence_1),
                            kImplDamage | kMainDamage, has_missing_content);
  feedback = gfx::PresentationFeedback(base::TimeTicks::Now() + interval * 3,
                                       interval, 0);
  collection_.NotifyFramePresented(frame_token, feedback);

  EXPECT_EQ(3u, number_of_frames_checkerboarded());
}

// Present multiple checkerboarded frames, followed by a non-checkerboard
// frame.
TEST_F(FrameSequenceTrackerTest, MultipleCheckerboardingFrames) {
  CreateNewTracker();

  const uint32_t kFrames = 3;
  const uint64_t source_1 = 1;
  uint64_t sequence_1 = 0;

  // Submit |kFrames| number of frames with checkerboarding.
  std::vector<uint32_t> frames;
  for (uint32_t i = 0; i < kFrames; ++i) {
    auto args_1 = CreateBeginFrameArgs(source_1, ++sequence_1);
    bool has_missing_content = true;
    auto frame_token = DispatchCompleteFrame(args_1, kImplDamage | kMainDamage,
                                             has_missing_content);
    frames.push_back(frame_token);
  }

  base::TimeTicks present_now = base::TimeTicks::Now();
  const auto interval = viz::BeginFrameArgs::DefaultInterval();
  for (auto frame_token : frames) {
    gfx::PresentationFeedback feedback(present_now, interval, 0);
    collection_.NotifyFramePresented(frame_token, feedback);
    present_now += interval;
  }

  // Submit another frame with no checkerboarding.
  bool has_missing_content = false;
  auto frame_token =
      DispatchCompleteFrame(CreateBeginFrameArgs(source_1, ++sequence_1),
                            kImplDamage | kMainDamage, has_missing_content);
  gfx::PresentationFeedback feedback(present_now, interval, 0);
  collection_.NotifyFramePresented(frame_token, feedback);

  EXPECT_EQ(kFrames, number_of_frames_checkerboarded());
}

TEST_F(FrameSequenceTrackerTest, ReportMetrics) {
  ReportMetrics();
}

TEST_F(FrameSequenceTrackerTest, ReportMetricsAtFixedInterval) {
  const uint64_t source = 1;
  uint64_t sequence = 0;
  base::TimeDelta first_time_delta = base::TimeDelta::FromSeconds(1);
  auto args = CreateBeginFrameArgs(source, ++sequence,
                                   base::TimeTicks::Now() + first_time_delta);

  // args.frame_time is less than 5s of the tracker creation time, so won't
  // schedule this tracker to report its throughput.
  collection_.NotifyBeginImplFrame(args);
  EXPECT_EQ(NumberOfTrackers(), 1u);
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);

  // Now args.frame_time is 5s since the tracker creation time, so this tracker
  // should be scheduled to report its throughput.
  args = CreateBeginFrameArgs(source, ++sequence,
                              args.frame_time + TimeDeltaToReort());
  collection_.NotifyBeginImplFrame(args);
  EXPECT_EQ(NumberOfTrackers(), 1u);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
}

}  // namespace cc
