// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_tracker.h"

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/frame_sequence_tracker_collection.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc {

namespace {

const char* ParseNumber(const char* str, uint64_t* retvalue) {
  uint64_t number = 0;
  for (; *str >= '0' && *str <= '9'; ++str) {
    number *= 10;
    number += *str - '0';
  }
  *retvalue = number;
  return str;
}

}  // namespace

class FrameSequenceTrackerTest : public testing::Test {
 public:
  const uint32_t kImplDamage = 0x1;
  const uint32_t kMainDamage = 0x2;

  FrameSequenceTrackerTest()
      : compositor_frame_reporting_controller_(
            std::make_unique<CompositorFrameReportingController>(
                /*should_report_histograms=*/true,
                /*should_report_ukm=*/false,
                /*layer_tree_host_id=*/1)),
        collection_(/*is_single_threaded=*/false,
                    compositor_frame_reporting_controller_.get()) {
    tracker_ = collection_.StartScrollSequence(
        FrameSequenceTrackerType::kTouchScroll,
        FrameInfo::SmoothEffectDrivingThread::kCompositor);
  }
  ~FrameSequenceTrackerTest() override = default;

  void CreateNewTracker(FrameInfo::SmoothEffectDrivingThread thread_type =
                            FrameInfo::SmoothEffectDrivingThread::kCompositor) {
    tracker_ = collection_.StartScrollSequence(
        FrameSequenceTrackerType::kTouchScroll, thread_type);
  }

  viz::BeginFrameArgs CreateBeginFrameArgs(
      uint64_t source_id,
      uint64_t sequence_number,
      base::TimeTicks now = base::TimeTicks::Now()) {
    auto interval = base::Milliseconds(16);
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
        collection_.NotifyMainFrameCausedNoDamage(args, false);
      } else {
        collection_.NotifyMainFrameProcessed(args);
      }
      uint32_t frame_token = NextFrameToken();
      collection_.NotifySubmitFrame(frame_token, has_missing_content,
                                    viz::BeginFrameAck(args, true), args);
      collection_.NotifyFrameEnd(args, args);
      return frame_token;
    } else {
      collection_.NotifyImplFrameCausedNoDamage(
          viz::BeginFrameAck(args, false));
      collection_.NotifyMainFrameCausedNoDamage(args, true);
      collection_.NotifyFrameEnd(args, args);
    }
    return 0;
  }

  uint32_t NextFrameToken() {
    static uint32_t frame_token = 0;
    return ++frame_token;
  }

  // Check whether a type of tracker exists in |frame_trackers_| or not.
  bool TrackerExists(FrameSequenceTrackerType type) const {
    auto key =
        std::make_pair(type, FrameInfo::SmoothEffectDrivingThread::kUnknown);
    if (type == FrameSequenceTrackerType::kTouchScroll ||
        type == FrameSequenceTrackerType::kWheelScroll ||
        type == FrameSequenceTrackerType::kScrollbarScroll) {
      key = std::make_pair(type,
                           FrameInfo::SmoothEffectDrivingThread::kCompositor);
      if (!collection_.frame_trackers_.contains(key))
        key = std::make_pair(type, FrameInfo::SmoothEffectDrivingThread::kMain);
    }
    return collection_.frame_trackers_.contains(key);
  }

  bool RemovalTrackerExists(unsigned index,
                            FrameSequenceTrackerType type) const {
    DCHECK_GT(collection_.removal_trackers_.size(), index);
    return collection_.removal_trackers_[index]->type() == type;
  }

  void GenerateSequence(const char* str) {
    const uint64_t source_id = 1;
    uint64_t current_frame = 0;
    viz::BeginFrameArgs last_activated_main_args;
    while (*str) {
      const char command = *str++;
      uint64_t sequence = 0, dummy = 0, last_activated_main = 0;
      switch (command) {
        case 'b':
        case 'P':
        case 'n':
        case 's':
        case 'E':
          ASSERT_EQ(*str, '(') << command;
          str = ParseNumber(++str, &sequence);
          ASSERT_EQ(*str, ')');
          ++str;
          break;

        case 'B':
        case 'N':
          ASSERT_EQ(*str, '(');
          str = ParseNumber(++str, &dummy);
          ASSERT_EQ(*str, ',');
          str = ParseNumber(++str, &sequence);
          ASSERT_EQ(*str, ')');
          ++str;
          break;

        case 'e':
          ASSERT_EQ(*str, '(');
          str = ParseNumber(++str, &sequence);
          ASSERT_EQ(*str, ',');
          str = ParseNumber(++str, &last_activated_main);
          ASSERT_EQ(*str, ')');
          ++str;
          break;

        case 'R':
          break;

        default:
          NOTREACHED() << command << str;
      }

      switch (command) {
        case 'b':
          current_frame = sequence;
          collection_.NotifyBeginImplFrame(
              CreateBeginFrameArgs(source_id, sequence));
          break;

        case 'P':
          collection_.NotifyFramePresented(
              sequence, {base::TimeTicks::Now(),
                         viz::BeginFrameArgs::DefaultInterval(), 0});
          break;

        case 'R':
          collection_.NotifyPauseFrameProduction();
          break;

        case 'n':
          collection_.NotifyImplFrameCausedNoDamage(
              viz::BeginFrameAck(source_id, sequence, false, 0));
          break;

        case 's': {
          auto frame_token = sequence;
          if (current_frame == 0)
            current_frame = 1;
          auto args = CreateBeginFrameArgs(source_id, current_frame);
          auto main_args = args;
          if (*str == 'S') {
            ++str;
            ASSERT_EQ(*str, '(');
            str = ParseNumber(++str, &sequence);
            ASSERT_EQ(*str, ')');
            ++str;
            main_args = CreateBeginFrameArgs(source_id, sequence);
          }
          collection_.NotifySubmitFrame(
              frame_token, /*has_missing_content=*/false,
              viz::BeginFrameAck(args, true), main_args);
          break;
        }

        case 'e': {
          auto args = CreateBeginFrameArgs(source_id, sequence);
          if (last_activated_main != 0)
            DCHECK_EQ(last_activated_main_args.frame_id.sequence_number,
                      last_activated_main);
          collection_.NotifyFrameEnd(args, last_activated_main_args);
          break;
        }

        case 'E':
          last_activated_main_args = CreateBeginFrameArgs(source_id, sequence);
          collection_.NotifyMainFrameProcessed(last_activated_main_args);
          break;

        case 'B':
          collection_.NotifyBeginMainFrame(
              CreateBeginFrameArgs(source_id, sequence));
          break;

        case 'N':
          collection_.NotifyMainFrameCausedNoDamage(
              CreateBeginFrameArgs(source_id, sequence), true);
          break;

        default:
          NOTREACHED();
      }
    }
  }

  void ReportMetrics() { tracker_->metrics_->ReportMetrics(); }

  base::TimeDelta TimeDeltaToReport() const {
    return tracker_->time_delta_to_report_;
  }

  unsigned NumberOfTrackers() const {
    return collection_.frame_trackers_.size();
  }
  unsigned NumberOfCustomTrackers() const {
    return collection_.custom_frame_trackers_.size();
  }
  unsigned NumberOfRemovalTrackers() const {
    return collection_.removal_trackers_.size();
  }

  uint64_t BeginImplFrameDataPreviousSequence() const {
    return tracker_->begin_impl_frame_data_.previous_sequence;
  }

  uint64_t BeginMainFrameDataPreviousSequence() const {
    return tracker_->begin_main_frame_data_.previous_sequence;
  }

  base::flat_set<uint32_t> IgnoredFrameTokens() const {
    return tracker_->ignored_frame_tokens_;
  }

  FrameSequenceMetrics::ThroughputData& ImplThroughput(
      FrameSequenceTracker* tracker) const {
    return tracker->impl_throughput();
  }

  FrameSequenceMetrics::ThroughputData& ImplThroughput() const {
    return tracker_->impl_throughput();
  }

  FrameSequenceMetrics::ThroughputData& MainThroughput(
      FrameSequenceTracker* tracker) const {
    return tracker->main_throughput();
  }

  FrameSequenceMetrics::ThroughputData& MainThroughput() const {
    return tracker_->main_throughput();
  }

  FrameSequenceTracker::TerminationStatus GetTerminationStatus(
      FrameSequenceTracker* tracker) {
    return tracker->termination_status_;
  }
  FrameSequenceTracker::TerminationStatus GetTerminationStatus() {
    return tracker_->termination_status_;
  }

 protected:
  std::unique_ptr<CompositorFrameReportingController>
      compositor_frame_reporting_controller_;
  FrameSequenceTrackerCollection collection_;
  raw_ptr<FrameSequenceTracker, DanglingUntriaged> tracker_;
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
  collection_.NotifyMainFrameCausedNoDamage(args_2, true);
  // Since the main-frame did not have any new damage from the latest
  // BeginFrameArgs, the submit-frame will carry the previous BeginFrameArgs
  // (from source_1);
  collection_.NotifySubmitFrame(NextFrameToken(), /*has_missing_content=*/false,
                                viz::BeginFrameAck(args_2, true), args_1);
}

TEST_F(FrameSequenceTrackerTest, TestNotifyFramePresented) {
  collection_.StartSequence(FrameSequenceTrackerType::kCompositorAnimation);
  collection_.StartSequence(FrameSequenceTrackerType::kMainThreadAnimation);
  EXPECT_EQ(NumberOfTrackers(), 3u);

  collection_.StopSequence(FrameSequenceTrackerType::kCompositorAnimation);
  EXPECT_EQ(NumberOfTrackers(), 2u);
  EXPECT_TRUE(TrackerExists(FrameSequenceTrackerType::kMainThreadAnimation));
  EXPECT_TRUE(TrackerExists(FrameSequenceTrackerType::kTouchScroll));
  // StopSequence should have destroyed all trackers because there is no frame
  // awaiting presentation.
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);
}

TEST_F(FrameSequenceTrackerTest, TestJankWithZeroIntervalInFeedback) {
  // Test if jank can be correctly counted if presentation feedback reports
  // zero frame interval.
  const uint64_t source = 1;
  uint64_t sequence = 1;
  uint64_t frame_token = sequence;
  const char* histogram_name =
      "Graphics.Smoothness.Jank.Compositor.TouchScroll";
  const base::TimeDelta zero_interval = base::Milliseconds(0);
  base::HistogramTester histogram_tester;

  CreateNewTracker();
  base::TimeTicks args_timestamp = base::TimeTicks::Now();
  auto args = CreateBeginFrameArgs(source, sequence, args_timestamp);

  // Frame 1
  collection_.NotifyBeginImplFrame(args);
  collection_.NotifySubmitFrame(sequence, false, viz::BeginFrameAck(args, true),
                                args);
  collection_.NotifyFrameEnd(args, args);

  collection_.NotifyFramePresented(
      frame_token,
      /*feedback=*/{args_timestamp, zero_interval, 0});

  // Frame 2
  ++sequence;
  ++frame_token;
  args_timestamp += base::Milliseconds(16.67);
  args = CreateBeginFrameArgs(source, sequence, args_timestamp);
  collection_.NotifyBeginImplFrame(args);
  collection_.NotifySubmitFrame(sequence, false, viz::BeginFrameAck(args, true),
                                args);
  collection_.NotifyFrameEnd(args, args);
  collection_.NotifyFramePresented(
      frame_token,
      /*feedback=*/{args_timestamp, zero_interval, 0});

  // Frame 3: There is one jank (frame interval incremented from 16.67ms
  // to 30.0ms)
  ++sequence;
  ++frame_token;
  args_timestamp += base::Milliseconds(30.0);
  args = CreateBeginFrameArgs(source, sequence, args_timestamp);
  collection_.NotifyBeginImplFrame(args);
  collection_.NotifySubmitFrame(sequence, false, viz::BeginFrameAck(args, true),
                                args);
  collection_.NotifyFrameEnd(args, args);
  collection_.NotifyFramePresented(
      frame_token,
      /*feedback=*/{args_timestamp, zero_interval, 0});

  // Frame 4: There is no jank since the increment from 30ms to  31ms is too
  // small. This tests if |NotifyFramePresented| can correctly handle the
  // situation when the frame interval reported in presentation feedback is 0.
  ++sequence;
  ++frame_token;
  args_timestamp += base::Milliseconds(31.0);
  args = CreateBeginFrameArgs(source, sequence, args_timestamp);
  collection_.NotifyBeginImplFrame(args);
  collection_.NotifySubmitFrame(sequence, false, viz::BeginFrameAck(args, true),
                                args);
  collection_.NotifyFrameEnd(args, args);
  collection_.NotifyFramePresented(
      frame_token,
      /*feedback=*/{args_timestamp, zero_interval, 0});
  ImplThroughput().frames_expected = 100u;
  ReportMetrics();
  histogram_tester.ExpectTotalCount(
      "Graphics.Smoothness.Jank.Compositor.TouchScroll", 1u);

  // There should be only one jank for frame 3.
  EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name),
              testing::ElementsAre(base::Bucket(1, 1)));
}

TEST_F(FrameSequenceTrackerTest, ReportMetricsAtFixedInterval) {
  const uint64_t source = 1;
  uint64_t sequence = 0;
  base::TimeDelta first_time_delta = base::Seconds(1);
  auto args = CreateBeginFrameArgs(source, ++sequence,
                                   base::TimeTicks::Now() + first_time_delta);

  // args.frame_time is less than 5s of the tracker creation time, so won't
  // schedule this tracker to report its throughput.
  collection_.NotifyBeginImplFrame(args);
  collection_.NotifyImplFrameCausedNoDamage(viz::BeginFrameAck(args, false));
  collection_.NotifyFrameEnd(args, args);

  EXPECT_EQ(NumberOfTrackers(), 1u);
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);

  ImplThroughput().frames_expected += 101;
  // Now args.frame_time is 5s since the tracker creation time, so this tracker
  // should be scheduled to report its throughput.
  args = CreateBeginFrameArgs(source, ++sequence,
                              args.frame_time + TimeDeltaToReport());
  collection_.NotifyBeginImplFrame(args);
  collection_.NotifyImplFrameCausedNoDamage(viz::BeginFrameAck(args, false));
  collection_.NotifyFrameEnd(args, args);
  EXPECT_EQ(NumberOfTrackers(), 1u);
  // At NotifyFrameEnd, the tracker is removed from removal_tracker_ list.
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);
}

TEST_F(FrameSequenceTrackerTest, ReportWithoutBeginImplFrame) {
  const uint64_t source = 1;
  uint64_t sequence = 0;

  auto args = CreateBeginFrameArgs(source, ++sequence);
  collection_.NotifyBeginMainFrame(args);

  EXPECT_EQ(BeginImplFrameDataPreviousSequence(), 0u);
  // Call to ReportBeginMainFrame should early exit.
  EXPECT_EQ(BeginMainFrameDataPreviousSequence(), 0u);

  uint32_t frame_token = NextFrameToken();
  collection_.NotifySubmitFrame(frame_token, false,
                                viz::BeginFrameAck(args, true), args);

  // Call to ReportSubmitFrame should early exit.
  EXPECT_TRUE(IgnoredFrameTokens().contains(frame_token));

  gfx::PresentationFeedback feedback;
  collection_.NotifyFramePresented(frame_token, feedback);
  EXPECT_EQ(ImplThroughput().frames_produced, 0u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);
}

TEST_F(FrameSequenceTrackerTest, MainFrameTracking) {
  const uint64_t source = 1;
  uint64_t sequence = 0;

  auto args = CreateBeginFrameArgs(source, ++sequence);
  auto frame_1 = DispatchCompleteFrame(args, kImplDamage | kMainDamage);

  args = CreateBeginFrameArgs(source, ++sequence);
  auto frame_2 = DispatchCompleteFrame(args, kImplDamage);

  gfx::PresentationFeedback feedback;
  collection_.NotifyFramePresented(frame_1, feedback);
  collection_.NotifyFramePresented(frame_2, feedback);
}

TEST_F(FrameSequenceTrackerTest, MainFrameNoDamageTracking) {
  const uint64_t source = 1;
  uint64_t sequence = 0;

  const auto first_args = CreateBeginFrameArgs(source, ++sequence);
  DispatchCompleteFrame(first_args, kImplDamage | kMainDamage);

  // Now, start the next frame, but for main, respond with the previous args.
  const auto second_args = CreateBeginFrameArgs(source, ++sequence);
  StartImplAndMainFrames(second_args);

  uint32_t frame_token = NextFrameToken();
  collection_.NotifySubmitFrame(frame_token, /*has_missing_content=*/false,
                                viz::BeginFrameAck(second_args, true),
                                first_args);
  collection_.NotifyFrameEnd(second_args, second_args);

  // Start and submit the next frame, with no damage from main.
  auto args = CreateBeginFrameArgs(source, ++sequence);
  collection_.NotifyBeginImplFrame(args);
  frame_token = NextFrameToken();
  collection_.NotifySubmitFrame(frame_token, /*has_missing_content=*/false,
                                viz::BeginFrameAck(args, true), first_args);
  collection_.NotifyFrameEnd(args, args);

  // Now, submit a frame with damage from main from |second_args|.
  collection_.NotifyMainFrameProcessed(second_args);
  args = CreateBeginFrameArgs(source, ++sequence);
  StartImplAndMainFrames(args);
  frame_token = NextFrameToken();
  collection_.NotifySubmitFrame(frame_token, /*has_missing_content=*/false,
                                viz::BeginFrameAck(args, true), second_args);
  collection_.NotifyFrameEnd(args, args);
}

TEST_F(FrameSequenceTrackerTest, SimpleSequenceOneFrame) {
  const char sequence[] = "b(1)B(0,1)s(1)S(1)e(1,0)P(1)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 1u);
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
  EXPECT_EQ(ImplThroughput().frames_produced, 1u);
  EXPECT_EQ(MainThroughput().frames_produced, 1u);
}

TEST_F(FrameSequenceTrackerTest, SimpleSequenceOneFrameNoDamage) {
  const char sequence[] = "b(1)B(0,1)N(1,1)n(1)e(1,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
  EXPECT_EQ(ImplThroughput().frames_produced, 0u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);

  const char second_sequence[] = "b(2)B(1,2)n(2)N(2,2)e(2,0)";
  GenerateSequence(second_sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
  EXPECT_EQ(ImplThroughput().frames_produced, 0u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);
}

TEST_F(FrameSequenceTrackerTest, MultipleNoDamageNotifications) {
  const char sequence[] = "b(1)n(1)n(1)e(1,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
  EXPECT_EQ(ImplThroughput().frames_produced, 0u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);
}

TEST_F(FrameSequenceTrackerTest, MultipleNoDamageNotificationsFromMain) {
  const char sequence[] = "b(1)B(0,1)N(1,1)n(1)N(0,1)e(1,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
  EXPECT_EQ(ImplThroughput().frames_produced, 0u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);
}

TEST_F(FrameSequenceTrackerTest, DelayedMainFrameNoDamage) {
  const char sequence[] =
      "b(1)B(0,1)n(1)e(1,0)b(2)n(2)e(2,0)b(3)N(0,1)n(3)e(3,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
  EXPECT_EQ(ImplThroughput().frames_produced, 0u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);
}

TEST_F(FrameSequenceTrackerTest, DelayedMainFrameNoDamageFromOlderFrame) {
  // Start a sequence, and receive a 'no damage' from an earlier frame.
  const char second_sequence[] = "b(2)B(0,2)N(2,1)n(2)N(2,2)e(2,0)";
  GenerateSequence(second_sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
  EXPECT_EQ(ImplThroughput().frames_produced, 0u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);
}

// This tests when a BeginMainFrame leads to No Damage, after the next Main
// Frame has started. This should not crash.
TEST_F(FrameSequenceTrackerTest, DelayedMainFrameNoDamageAfterNextMainFrame) {
  const char sequence[] =
      "b(1)B(0,1)n(1)e(1,0)E(1)b(2)B(0,2)N(0,1)n(2)N(0,2)e(2,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
  EXPECT_EQ(ImplThroughput().frames_produced, 0u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);
}

TEST_F(FrameSequenceTrackerTest, StateResetDuringSequence) {
  const char sequence[] = "b(1)B(0,1)n(1)N(1,1)Re(1,0)b(2)n(2)e(2,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
  EXPECT_EQ(ImplThroughput().frames_produced, 0u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);
}

TEST_F(FrameSequenceTrackerTest, NoCompositorDamageSubmitFrame) {
  const char sequence[] = "b(1)n(1)B(0,1)E(1)s(1)S(1)e(1,1)P(1)b(2)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 2u);
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
  EXPECT_EQ(ImplThroughput().frames_produced, 1u);
  EXPECT_EQ(MainThroughput().frames_produced, 1u);
}

TEST_F(FrameSequenceTrackerTest, SequenceStateResetsDuringFrame) {
  const char sequence[] = "b(1)Rn(1)e(1,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
  EXPECT_EQ(ImplThroughput().frames_produced, 0u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);

  GenerateSequence("b(2)s(1)e(2,0)P(1)b(4)");
  EXPECT_EQ(ImplThroughput().frames_expected, 3u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
  EXPECT_EQ(ImplThroughput().frames_produced, 1u);
  EXPECT_EQ(MainThroughput().frames_produced, 0u);
}

TEST_F(FrameSequenceTrackerTest, BeginImplFrameBeforeTerminate) {
  const char sequence[] = "b(1)s(1)e(1,0)b(4)P(1)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 4u);
  EXPECT_EQ(ImplThroughput().frames_produced, 1u);
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(ImplThroughput().frames_expected, 4u);
  EXPECT_EQ(ImplThroughput().frames_produced, 1u);
}

// b(2417)B(0,2417)E(2417)n(2417)N(2417,2417)
TEST_F(FrameSequenceTrackerTest, SequenceNumberReset) {
  const char sequence[] =
      "b(6)B(0,6)n(6)e(6,0)Rb(1)B(0,1)N(1,1)n(1)e(1,0)b(2)B(1,2)n(2)e(2,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
}

TEST_F(FrameSequenceTrackerTest, MainThroughputWithHighLatency) {
  const char sequence[] = "b(1)B(0,1)n(1)e(1,0)b(2)E(1)s(1)S(1)e(2,1)P(1)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 1u);
  EXPECT_EQ(ImplThroughput().frames_produced, 1u);
  EXPECT_EQ(MainThroughput().frames_expected, 2u);
  EXPECT_EQ(MainThroughput().frames_produced, 1u);
}

TEST_F(FrameSequenceTrackerTest, TrackLastImplFrame1) {
  GenerateSequence("b(1)s(1)e(1,0)b(4)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  FrameSequenceTracker* removal_tracker =
      collection_.GetRemovalTrackerForTesting(
          FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
  GenerateSequence("P(1)");
  // There is still one impl-frame not processed not, so the tracker is not yet
  // ready for termination.
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
}

// Following 2 cases are for: b(1)s(1)P(1), and StopSequence can happen
// anywhere after b and before P. Because there is no e when P happens, the
// tracker is not ready for termination.
TEST_F(FrameSequenceTrackerTest, TrackLastImplFrame5) {
  GenerateSequence("b(1)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  FrameSequenceTracker* removal_tracker =
      collection_.GetRemovalTrackerForTesting(
          FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
  GenerateSequence("s(1)P(1)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
}

TEST_F(FrameSequenceTrackerTest, TrackLastImplFrame6) {
  GenerateSequence("b(1)s(1)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  FrameSequenceTracker* removal_tracker =
      collection_.GetRemovalTrackerForTesting(
          FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
  GenerateSequence("P(1)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
}

// All the following cases are for one complete impl + one incomplete:
// b(1)s(1)e(1,0)xxxxxxxxP(1)
// The 'xxxxx' is an incomplete impl frame that has no damage, it could be
// 1. b(2)n(2)e(2,0)P(1), and StopSequence can happen anywhere after b and
//    before P.
// 2. b(2)n(2)P(1), and StopSequence can happen anywhere after b and before P.
//    In this case, the tracker is not ready for termination yet because e never
//    happens.
TEST_F(FrameSequenceTrackerTest, TrackLastImplFrame10) {
  GenerateSequence("b(1)s(1)e(1,0)b(2)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  FrameSequenceTracker* removal_tracker =
      collection_.GetRemovalTrackerForTesting(
          FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
  GenerateSequence("n(2)P(1)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
}

TEST_F(FrameSequenceTrackerTest, TrackLastImplFrame11) {
  GenerateSequence("b(1)s(1)e(1,0)b(2)n(2)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  FrameSequenceTracker* removal_tracker =
      collection_.GetRemovalTrackerForTesting(
          FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
  GenerateSequence("P(1)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
}

// This test ensure that the tracker would terminate at e.
TEST_F(FrameSequenceTrackerTest, TrackLastImplFrame24) {
  GenerateSequence("b(1)s(1)P(1)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  GenerateSequence("e(1,0)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);
}

TEST_F(FrameSequenceTrackerTest, IgnoredFrameTokensRemovedAtPresentation1) {
  GenerateSequence("b(5)s(1)e(5,0)P(1)");
  auto args = CreateBeginFrameArgs(/*source_id=*/1u, 1u);
  // Ack to an impl frame that doesn't exist in this tracker.
  collection_.NotifySubmitFrame(2, /*has_missing_content=*/false,
                                viz::BeginFrameAck(args, true), args);
  EXPECT_EQ(IgnoredFrameTokens().size(), 1u);
  GenerateSequence("P(3)");
  // Any token that is < 3 should have been removed.
  EXPECT_EQ(IgnoredFrameTokens().size(), 0u);
}

// Test the case where the frame tokens wraps around the 32-bit max value.
TEST_F(FrameSequenceTrackerTest, IgnoredFrameTokensRemovedAtPresentation2) {
  GenerateSequence("b(5)");
  auto args = CreateBeginFrameArgs(1u, 1u);
  // Ack to an impl frame that doesn't exist in this tracker.
  collection_.NotifySubmitFrame(UINT32_MAX, /*has_missing_content=*/false,
                                viz::BeginFrameAck(args, true), args);
  EXPECT_EQ(IgnoredFrameTokens().size(), 1u);

  args = CreateBeginFrameArgs(1u, 5u);
  collection_.NotifySubmitFrame(1, false, viz::BeginFrameAck(args, true), args);
  GenerateSequence("e(5,0)P(1)");
  EXPECT_TRUE(IgnoredFrameTokens().empty());
}

TEST_F(FrameSequenceTrackerTest, TerminationWithNullPresentationTimeStamp) {
  GenerateSequence("b(1)s(1)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  // Even if the presentation timestamp is null, as long as this presentation
  // is acking the last impl frame, we consider that impl frame completed and
  // so the tracker is ready for termination.
  collection_.NotifyFramePresented(
      1, {base::TimeTicks(), viz::BeginFrameArgs::DefaultInterval(), 0});
  GenerateSequence("e(1,0)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);
}

// Test that a tracker is terminated after 3 submitted frames, remove this
// once crbug.com/1072482 is fixed.
TEST_F(FrameSequenceTrackerTest, TerminationAfterThreeSubmissions1) {
  GenerateSequence("b(1)s(1)e(1,0)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  GenerateSequence("b(2)s(2)e(2,0)b(3)s(3)e(3,0)b(4)s(4)e(4,0)b(5)s(5)e(5,0)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);
}

TEST_F(FrameSequenceTrackerTest, TerminationAfterThreeSubmissions2) {
  GenerateSequence("b(1)");
  auto args = CreateBeginFrameArgs(1u, 1u);
  // Ack to an impl frame that doesn't exist in this tracker.
  collection_.NotifySubmitFrame(UINT32_MAX, /*has_missing_content=*/false,
                                viz::BeginFrameAck(args, true), args);
  GenerateSequence("e(1,0)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  GenerateSequence("b(2)s(1)e(2,0)b(3)s(2)e(3,0)b(4)s(3)e(4,0)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);
}

TEST_F(FrameSequenceTrackerTest, TerminationAfterThreeSubmissions3) {
  GenerateSequence(
      "b(1)s(1)e(1,0)P(1)b(2)s(2)e(2,0)P(2)b(3)s(3)e(3,0)P(3)b(4)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  GenerateSequence("s(4)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage1) {
  const char sequence[] =
      "b(1)B(0,1)n(1)e(1,0)b(2)E(1)B(1,2)n(2)e(2,1)b(3)E(2)B(2,3)n(3)e(3,2)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  // At E(2), B(0,1) is treated no damage.
  EXPECT_EQ(MainThroughput().frames_expected, 2u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage2) {
  const char sequence[] =
      "b(1)B(0,1)n(1)e(1,0)b(2)E(1)B(1,2)n(2)e(2,1)b(3)n(3)e(3,1)b(4)n(4)e(4,1)"
      "b(8)E(2)"
      "B(8,8)n(8)e(8,2)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  // At E(2), B(0,1) is treated as no damage.
  EXPECT_EQ(MainThroughput().frames_expected, 7u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage3) {
  const char sequence[] =
      "b(34)B(0,34)n(34)e(34,0)b(35)n(35)e(35,0)b(36)E(34)n(36)e(36,34)b(39)s("
      "1)e(39,34)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 1u);
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage4) {
  const char sequence[] =
      "b(9)B(0,9)n(9)Re(9,0)E(9)b(11)B(0,11)n(11)e(11,9)b(12)E(11)B(11,12)s(1)"
      "S(11)e(12,11)b(13)E(12)s(2)S(12)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 2u);
  EXPECT_EQ(MainThroughput().frames_expected, 2u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage5) {
  const char sequence[] =
      "b(1)B(0,1)E(1)s(1)S(1)e(1,0)b(2)n(2)e(2,0)b(3)B(1,3)n(3)e(3,0)E(3)b(4)B("
      "3,4)n("
      "4)e(4,3)E(4)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 1u);
  // At E(4), we treat B(1,3) as if it had no damage.
  EXPECT_EQ(MainThroughput().frames_expected, 3u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage6) {
  const char sequence[] =
      "b(1)B(0,1)E(1)s(1)S(1)e(1,1)b(2)B(1,2)E(2)n(2)N(2,2)e(2,2)b(3)B(0,3)E(3)"
      "n(3)"
      "N(3,3)e(3,3)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 1u);
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage7) {
  const char sequence[] =
      "b(8)B(0,8)n(8)e(8,0)b(9)E(8)B(8,9)E(9)s(1)S(8)e(9,9)b(10)s(2)S(9)e(10,"
      "9)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 2u);
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage8) {
  const char sequence[] =
      "b(18)B(0,18)E(18)n(18)N(18,18)Re(18,18)b(20)B(0,20)N(20,20)n(20)N(0,20)"
      "e("
      "20,18)b(21)B(0,21)E(21)s(1)S(21)e(21,21)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 1u);
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage9) {
  const char sequence[] =
      "b(78)n(78)Re(78,0)Rb(82)B(0,82)E(82)n(82)N(82,82)Re(82,82)b(86)B(0,86)E("
      "86)n("
      "86)e(86,86)b(87)s(1)S(86)e(87,86)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 1u);
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage10) {
  const char sequence[] =
      "b(2)B(0,2)E(2)n(2)N(2,2)e(2,2)b(3)B(0,3)E(3)n(3)N(3,3)e(3,3)b(4)B(0,4)E("
      "4)n("
      "4)N(4,4)e(4,4)b(5)B(0,5)E(5)n(5)N(5,5)e(5,5)b(6)B(0,6)n(6)e(6,5)E(6)Rb("
      "8)B(0,"
      "8)E(8)n(8)N(8,8)e(8,8)";
  GenerateSequence(sequence);
  EXPECT_EQ(ImplThroughput().frames_expected, 0u);
  EXPECT_EQ(MainThroughput().frames_expected, 0u);
}

// A presentation with a frame token that is > the main frame token submitted.
TEST_F(FrameSequenceTrackerTest, MainThreadPresentWithNonMatchedToken) {
  const char sequence[] = "b(1)B(0,1)E(1)s(1)S(1)e(1,0)b(2)s(2)S(1)e(2,1)P(2)";
  GenerateSequence(sequence);
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
  EXPECT_EQ(MainThroughput().frames_produced, 1u);
}

TEST_F(FrameSequenceTrackerTest, CoalescedMainThreadPresent) {
  const char sequence[] =
      "b(1)B(0,1)E(1)s(1)S(1)e(1,1)b(2)B(1,2)E(2)s(2)S(2)e(2,2)P(2)";
  GenerateSequence(sequence);
  EXPECT_EQ(MainThroughput().frames_expected, 2u);
  EXPECT_EQ(MainThroughput().frames_produced, 1u);
}

TEST_F(FrameSequenceTrackerTest, MainThreadPresentWithNullTimeStamp) {
  const char sequence[] = "b(1)B(0,1)E(1)s(1)S(1)e(1,1)";
  GenerateSequence(sequence);
  collection_.NotifyFramePresented(
      1, {base::TimeTicks(), viz::BeginFrameArgs::DefaultInterval(),
          gfx::PresentationFeedback::kFailure});
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
  // No presentation, no main frame produced.
  EXPECT_EQ(MainThroughput().frames_produced, 0u);
  GenerateSequence("b(2)s(2)S(1)e(2,0)P(2)");
  EXPECT_EQ(MainThroughput().frames_expected, 1u);
  // The main frame update is caught up here.
  EXPECT_EQ(MainThroughput().frames_produced, 1u);
}

TEST_F(FrameSequenceTrackerTest, TrackerTypeEncoding) {
  // The test begins with a kTouchScroll tracker
  EXPECT_EQ(NumberOfTrackers(), 1u);
  ActiveFrameSequenceTrackers active_encoded =
      collection_.FrameSequenceTrackerActiveTypes();
  EXPECT_EQ(active_encoded, 16);  // 1 << 4
}

TEST_F(FrameSequenceTrackerTest, CustomTrackers) {
  CustomTrackerResults results;
  collection_.set_custom_tracker_results_added_callback(
      base::BindLambdaForTesting([&](const CustomTrackerResults& reported) {
        for (const auto& pair : reported)
          results[pair.first] = pair.second;
      }));

  // Start custom tracker 1.
  collection_.StartCustomSequence(1);
  EXPECT_EQ(1u, NumberOfCustomTrackers());

  // No reports.
  uint32_t frame_token = 1u;
  collection_.NotifyFramePresented(frame_token, {});
  EXPECT_EQ(0u, results.size());

  // Start custom tracker 2 and 3 in addition to 1.
  collection_.StartCustomSequence(2);
  collection_.StartCustomSequence(3);
  EXPECT_EQ(3u, NumberOfCustomTrackers());

  // All custom trackers are running. No reports.
  collection_.NotifyFramePresented(frame_token, {});
  EXPECT_EQ(0u, results.size());

  // Tracker 2 is stopped and scheduled to terminate.
  collection_.StopCustomSequence(2);
  EXPECT_EQ(2u, NumberOfCustomTrackers());

  // Tracker 2 has zero expected frames.
  collection_.NotifyFramePresented(frame_token, {});
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(0u, results[2].frames_expected);

  // Simple sequence of one frame.
  const char sequence[] = "b(1)B(0,1)s(1)S(1)e(1,0)P(1)";
  GenerateSequence(sequence);

  // Stop all custom trackers.
  collection_.StopCustomSequence(1);
  collection_.StopCustomSequence(3);
  EXPECT_EQ(0u, NumberOfCustomTrackers());

  // Tracker 1 and 3 and should report.
  collection_.NotifyFramePresented(frame_token, {});
  EXPECT_EQ(3u, results.size());
  EXPECT_EQ(1u, results[1].frames_produced);
  EXPECT_EQ(1u, results[1].frames_expected);
  EXPECT_EQ(0u, results[2].frames_produced);
  EXPECT_EQ(0u, results[2].frames_expected);
  EXPECT_EQ(1u, results[3].frames_produced);
  EXPECT_EQ(1u, results[3].frames_expected);
}

}  // namespace cc
