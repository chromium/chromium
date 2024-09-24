// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/metrics/frame_sequence_tracker.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/frame_sequence_tracker_collection.h"
#include "cc/metrics/frame_sorter.h"
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
                    compositor_frame_reporting_controller_.get()),
        sorter_(base::BindRepeating(&FrameSequenceTrackerTest::OnFrameResult,
                                    base::Unretained(this))) {
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

  void StartFrames(const viz::BeginFrameArgs& args) {
    collection_.NotifyBeginImplFrame(args);
  }

  uint32_t DispatchCompleteFrame(const viz::BeginFrameArgs& args,
                                 uint32_t damage_type,
                                 bool has_missing_content = false) {
    StartFrames(args);

    if (damage_type & kImplDamage) {
      uint32_t frame_token = NextFrameToken();
      collection_.NotifyFrameEnd(args, args);
      return frame_token;
    } else {
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
    viz::BeginFrameArgs last_activated_main_args;
    while (*str) {
      const char command = *str++;
      uint64_t sequence = 0, dummy = 0, last_activated_main = 0;
      switch (command) {
        case 'b':
        case 'd':
        case 'D':
        case 'p':
        case 'P':
        case 'n':
          ASSERT_EQ(*str, '(') << command;
          str = ParseNumber(++str, &sequence);
          ASSERT_EQ(*str, ')');
          ++str;
          break;

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
          collection_.NotifyBeginImplFrame(
              CreateBeginFrameArgs(source_id, sequence));
          break;

        case 'd': {
          auto args = CreateBeginFrameArgs(source_id, sequence);
          FrameInfo frame_info;
          frame_info.final_state = FrameInfo::FrameFinalState::kDropped;
          frame_info.smooth_thread = FrameInfo::SmoothThread::kSmoothCompositor;
          collection_.AddSortedFrame(args, frame_info);
          break;
        }

        case 'D': {
          auto args = CreateBeginFrameArgs(source_id, sequence);
          FrameInfo frame_info;
          frame_info.final_state = FrameInfo::FrameFinalState::kDropped;
          frame_info.smooth_thread = FrameInfo::SmoothThread::kSmoothMain;
          collection_.AddSortedFrame(args, frame_info);
          break;
        }

        case 'p': {
          auto args = CreateBeginFrameArgs(source_id, sequence);
          FrameInfo frame_info;
          frame_info.final_state = FrameInfo::FrameFinalState::kPresentedAll;
          frame_info.smooth_thread = FrameInfo::SmoothThread::kSmoothCompositor;
          collection_.AddSortedFrame(args, frame_info);
          break;
        }

        case 'P': {
          auto args = CreateBeginFrameArgs(source_id, sequence);
          FrameInfo frame_info;
          frame_info.final_state = FrameInfo::FrameFinalState::kPresentedAll;
          frame_info.smooth_thread = FrameInfo::SmoothThread::kSmoothMain;
          collection_.AddSortedFrame(args, frame_info);
          break;
        }
        case 'R':
          collection_.NotifyPauseFrameProduction();
          break;

        case 'n': {
          auto args = CreateBeginFrameArgs(source_id, sequence);
          FrameInfo frame_info;
          frame_info.final_state = FrameInfo::FrameFinalState::kNoUpdateDesired;
          collection_.AddSortedFrame(args, frame_info);
          break;
        }

        case 'e': {
          auto args = CreateBeginFrameArgs(source_id, sequence);
          collection_.NotifyFrameEnd(args, args);
          break;
        }

        case 'N': {
          auto args = CreateBeginFrameArgs(source_id, sequence);
          FrameInfo frame_info;
          frame_info.final_state = FrameInfo::FrameFinalState::kNoUpdateDesired;
          frame_info.smooth_thread = FrameInfo::SmoothThread::kSmoothMain;
          collection_.AddSortedFrame(args, frame_info);
          break;
        }

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

  void IncrementFramesExpected(uint32_t frames) {
    tracker_->metrics_->v3_.frames_expected += frames;
  }

  uint32_t frames_expected() const {
    return tracker_->metrics_->v3_.frames_expected;
  }

  uint32_t frames_produced() const {
    return tracker_->metrics_->v3_.frames_expected -
           tracker_->metrics_->v3_.frames_dropped;
  }

  FrameSequenceTracker::TerminationStatus GetTerminationStatus(
      FrameSequenceTracker* tracker) {
    return tracker->termination_status_;
  }
  FrameSequenceTracker::TerminationStatus GetTerminationStatus() {
    return tracker_->termination_status_;
  }

  // FrameSorter callback.
  void OnFrameResult(const viz::BeginFrameArgs& args,
                     const FrameInfo& frame_info) {
    collection_.AddSortedFrame(args, frame_info);
  }

 protected:
  std::unique_ptr<CompositorFrameReportingController>
      compositor_frame_reporting_controller_;
  FrameSequenceTrackerCollection collection_;
  raw_ptr<FrameSequenceTracker, DanglingUntriaged> tracker_;
  FrameSorter sorter_;
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

TEST_F(FrameSequenceTrackerTest, ReportMetricsAtFixedInterval) {
  const uint64_t source = 1;
  uint64_t sequence = 0;
  base::TimeDelta first_time_delta = base::Seconds(1);
  auto args = CreateBeginFrameArgs(source, ++sequence,
                                   base::TimeTicks::Now() + first_time_delta);

  // args.frame_time is less than 5s of the tracker creation time, so won't
  // schedule this tracker to report its throughput.
  collection_.NotifyBeginImplFrame(args);
  collection_.NotifyFrameEnd(args, args);

  EXPECT_EQ(NumberOfTrackers(), 1u);
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);

  IncrementFramesExpected(101u);
  // Now args.frame_time is 5s since the tracker creation time, so this tracker
  // should be scheduled to report its throughput.
  args = CreateBeginFrameArgs(source, ++sequence,
                              args.frame_time + TimeDeltaToReport());
  collection_.NotifyBeginImplFrame(args);
  collection_.NotifyFrameEnd(args, args);
  FrameInfo frame_info;
  frame_info.final_state = FrameInfo::FrameFinalState::kPresentedAll;
  collection_.AddSortedFrame(args, frame_info);
  EXPECT_EQ(NumberOfTrackers(), 1u);
  // At NotifyFrameEnd, the tracker is removed from removal_tracker_ list.
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);
}

TEST_F(FrameSequenceTrackerTest, MainFrameNoDamageTracking) {
  const uint64_t source = 1;
  uint64_t sequence = 0;

  const auto first_args = CreateBeginFrameArgs(source, ++sequence);
  DispatchCompleteFrame(first_args, kImplDamage | kMainDamage);

  // Now, start the next frame, but for main, respond with the previous args.
  const auto second_args = CreateBeginFrameArgs(source, ++sequence);
  StartFrames(second_args);

  collection_.NotifyFrameEnd(second_args, second_args);

  // Start and submit the next frame, with no damage from main.
  auto args = CreateBeginFrameArgs(source, ++sequence);
  collection_.NotifyBeginImplFrame(args);
  collection_.NotifyFrameEnd(args, args);

  // Now, submit a frame with damage from main from |second_args|.
  args = CreateBeginFrameArgs(source, ++sequence);
  StartFrames(args);
  collection_.NotifyFrameEnd(args, args);
}

TEST_F(FrameSequenceTrackerTest, SimpleSequenceOneFrame) {
  const char sequence[] = "b(1)e(1,0)P(1)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 1u);
  EXPECT_EQ(frames_produced(), 1u);
}

TEST_F(FrameSequenceTrackerTest, SimpleSequenceOneFrameNoDamage) {
  const char sequence[] = "b(1)N(1,1)n(1)e(1,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 2u);
  EXPECT_EQ(frames_produced(), 2u);

  const char second_sequence[] = "b(2)n(2)N(2,2)e(2,0)";
  GenerateSequence(second_sequence);
  EXPECT_EQ(frames_expected(), 4u);
  EXPECT_EQ(frames_produced(), 4u);
}

TEST_F(FrameSequenceTrackerTest, MultipleNoDamageNotifications) {
  const char sequence[] = "b(1)n(1)n(1)e(1,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 2u);
  EXPECT_EQ(frames_produced(), 2u);
}

TEST_F(FrameSequenceTrackerTest, MultipleNoDamageNotificationsFromMain) {
  const char sequence[] = "b(1)N(1,1)n(1)N(0,1)e(1,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 3u);
  EXPECT_EQ(frames_produced(), 3u);
}

TEST_F(FrameSequenceTrackerTest, DelayedMainFrameNoDamage) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] = "b(1)n(1)e(1,0)b(2)n(2)e(2,0)b(3)N(0,1)n(3)e(3,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 0u);
  EXPECT_EQ(frames_produced(), 0u);
}

TEST_F(FrameSequenceTrackerTest, DelayedMainFrameNoDamageFromOlderFrame) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  // Start a sequence, and receive a 'no damage' from an earlier frame.
  const char second_sequence[] = "b(2)N(2,1)n(2)N(2,2)e(2,0)";
  GenerateSequence(second_sequence);
  EXPECT_EQ(frames_expected(), 0u);
  EXPECT_EQ(frames_produced(), 0u);
}

// This tests when a BeginMainFrame leads to No Damage, after the next Main
// Frame has started. This should not crash.
TEST_F(FrameSequenceTrackerTest, DelayedMainFrameNoDamageAfterNextMainFrame) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] = "b(1)n(1)e(1,0)b(2)N(0,1)n(2)N(0,2)e(2,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 0u);
  EXPECT_EQ(frames_produced(), 0u);
}

TEST_F(FrameSequenceTrackerTest, StateResetDuringSequence) {
  const char sequence[] = "b(1)n(1)N(1,1)Re(1,0)b(2)n(2)e(2,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 3u);
  EXPECT_EQ(frames_produced(), 3u);
}

TEST_F(FrameSequenceTrackerTest, SequenceStateResetsDuringFrame) {
  const char sequence[] = "b(1)Rn(1)e(1,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 1u);
  EXPECT_EQ(frames_produced(), 1u);

  GenerateSequence("b(2)e(2,0)P(1)b(4)");
  EXPECT_EQ(frames_expected(), 2u);
  EXPECT_EQ(frames_produced(), 2u);
}

// b(2417)B(0,2417)E(2417)n(2417)N(2417,2417)
TEST_F(FrameSequenceTrackerTest, SequenceNumberReset) {
  const char sequence[] = "b(6)n(6)e(6,0)Rb(1)N(1,1)n(1)e(1,0)b(2)n(2)e(2,0)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 1u);
}

TEST_F(FrameSequenceTrackerTest, MainThroughputWithHighLatency) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] = "b(1)n(1)e(1,0)b(2)e(2,1)P(1)D(1)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 2u);
  EXPECT_EQ(frames_produced(), 1u);
}

TEST_F(FrameSequenceTrackerTest, TrackLastImplFrame1) {
  GenerateSequence("b(1)e(1,0)b(4)e(4,0)");
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

// All the following cases are for one complete impl + one incomplete:
// b(1)e(1,0)xxxxxxxxP(1)
// The 'xxxxx' is an incomplete impl frame that has no damage, it could be
// 1. b(2)e(2,0)P(1)n(2), and StopSequence happens anywhere after e and
//    before P.
// 2. b(2)e(2,0)P(1)n(2), and StopSequence can happen anywhere after e and
//    before P. In this case, the tracker is not ready for termination yet at P
//    because the sorted n(2) has not been called yet.
TEST_F(FrameSequenceTrackerTest, TrackLastImplFrame10) {
  GenerateSequence("b(1)e(1,0)b(2)e(2,0)");
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

TEST_F(FrameSequenceTrackerTest, TrackLastImplFrame11) {
  GenerateSequence("b(1)e(1,0)b(2)e(2,0)");
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
  GenerateSequence("b(1)P(1)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  GenerateSequence("e(1,0)p(1)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);
}

// Termination is triggered after a new tree is committed and activated. Due to
// this we will have started a BeginImplFrame that is not actually a part of the
// sequence. When this occurs we terminate as soon as the most recently
// submitted frame has bee processed.
TEST_F(FrameSequenceTrackerTest, IgnoreImplFrameBeforeTermination) {
  GenerateSequence("b(1)e(1,0)b(2)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  FrameSequenceTracker* removal_tracker =
      collection_.GetRemovalTrackerForTesting(
          FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
  GenerateSequence("e(2,0)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  EXPECT_EQ(GetTerminationStatus(removal_tracker),
            FrameSequenceTracker::TerminationStatus::kScheduledForTermination);
  GenerateSequence("P(1)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);
}

TEST_F(FrameSequenceTrackerTest, TerminationWithNullPresentationTimeStamp) {
  GenerateSequence("b(1)");
  collection_.StopSequence(FrameSequenceTrackerType::kTouchScroll);
  EXPECT_EQ(NumberOfRemovalTrackers(), 1u);
  // Even if the presentation timestamp is null, as long as this presentation
  // is acking the last impl frame, we consider that impl frame completed and
  // so the tracker is ready for termination.
  GenerateSequence("e(1,0)p(1)");
  EXPECT_EQ(NumberOfRemovalTrackers(), 0u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage1) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] = "b(1)n(1)e(1,0)b(2)n(2)e(2,1)b(3)n(3)e(3,2)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 0u);
  // At E(2), B(0,1) is treated no damage.
  EXPECT_EQ(frames_expected(), 0u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage2) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] =
      "b(1)n(1)e(1,0)b(2)n(2)e(2,1)b(3)n(3)e(3,1)b(4)n(4)e(4,1)b(8)n(8)e(8,2)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 0u);
  // At E(2), B(0,1) is treated as no damage.
  EXPECT_EQ(frames_expected(), 0u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage3) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] =
      "b(34)n(34)e(34,0)b(35)n(35)e(35,0)b(36)n(36)e(36,34)b(39)e(39,34)";
  GenerateSequence(sequence);
  // No damage frames are not expected.
  EXPECT_EQ(frames_expected(), 0u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage4) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] = "b(9)n(9)Re(9,0)b(11)n(11)e(11,9)b(12)e(12,11)b(13)";
  GenerateSequence(sequence);
  // No damage frames are not expected.
  EXPECT_EQ(frames_expected(), 0u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage5) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] =
      "b(1)e(1,0)b(2)n(2)e(2,0)b(3)n(3)e(3,0)b(4)n(4)e(4,3)";
  GenerateSequence(sequence);
  // At E(4), we treat B(1,3) as if it had no damage.
  EXPECT_EQ(frames_expected(), 0u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage6) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] = "b(1)e(1,1)b(2)n(2)N(2,2)e(2,2)b(3)n(3)N(3,3)e(3,3)";
  GenerateSequence(sequence);
  // No damage frames are not expected.
  EXPECT_EQ(frames_expected(), 0u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage7) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] = "b(8)n(8)e(8,0)b(9)e(9,9)b(10)e(10,9)";
  GenerateSequence(sequence);
  // No damage frames are not expected.
  EXPECT_EQ(frames_expected(), 0u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage8) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] =
      "b(18)n(18)N(18,18)Re(18,18)b(20)N(20,20)n(20)N(0,20)e(20,18)b(21)e(21,"
      "21)";
  GenerateSequence(sequence);
  // No damage frames are not expected.
  EXPECT_EQ(frames_expected(), 0u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage9) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] =
      "b(78)n(78)Re(78,0)Rb(82)n(82)N(82,82)Re(82,82)b(86)n(86)e(86,86)b(87)e("
      "87,86)";
  GenerateSequence(sequence);
  // No damage frames are not expected.
  EXPECT_EQ(frames_expected(), 0u);
}

TEST_F(FrameSequenceTrackerTest, OffScreenMainDamage10) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] =
      "b(2)n(2)N(2,2)e(2,2)b(3)n(3)N(3,3)e(3,3)b(4)n(4)N(4,4)e(4,4)b(5)n(5)N(5,"
      "5)e(5,5)b(6)n(6)e(6,5)Rb(8)n(8)N(8,8)e(8,8)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 0u);
}

// A presentation with a frame token that is > the main frame token submitted.
TEST_F(FrameSequenceTrackerTest, MainThreadPresentWithNonMatchedToken) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] = "b(1)e(1,0)b(2)e(2,1)P(2)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 1u);
  EXPECT_EQ(frames_produced(), 1u);
}

TEST_F(FrameSequenceTrackerTest, CoalescedMainThreadPresent) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] = "b(1)e(1,1)b(2)e(2,2)D(1)P(2)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 2u);
  EXPECT_EQ(frames_produced(), 1u);
}

TEST_F(FrameSequenceTrackerTest, MainThreadPresentWithNullTimeStamp) {
  CreateNewTracker(FrameInfo::SmoothEffectDrivingThread::kMain);
  const char sequence[] = "b(1)e(1,1)D(1)";
  GenerateSequence(sequence);
  EXPECT_EQ(frames_expected(), 1u);
  // No presentation, no main frame produced.
  EXPECT_EQ(frames_produced(), 0u);
  GenerateSequence("b(2)e(2,0)P(2)");
  // We are creating a second frame, it will be expected
  EXPECT_EQ(frames_expected(), 2u);
  // The main frame update is caught up here.
  EXPECT_EQ(frames_produced(), 1u);
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
  EXPECT_EQ(0u, results.size());

  // Start custom tracker 2 and 3 in addition to 1.
  collection_.StartCustomSequence(2);
  collection_.StartCustomSequence(3);
  EXPECT_EQ(3u, NumberOfCustomTrackers());

  // All custom trackers are running. No reports.
  EXPECT_EQ(0u, results.size());

  // Tracker 2 is stopped and scheduled to terminate.
  collection_.StopCustomSequence(2);
  EXPECT_EQ(2u, NumberOfCustomTrackers());

  // Tracker 2 has zero expected frames.
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(0u, results[2].frames_expected_v3);

  // Simple sequence of one frame.
  const char sequence[] = "b(1)e(1,0)P(1)";
  GenerateSequence(sequence);

  // Stop all custom trackers.
  collection_.StopCustomSequence(1);
  collection_.StopCustomSequence(3);
  EXPECT_EQ(0u, NumberOfCustomTrackers());

  // Tracker 1 and 3 and should report.
  EXPECT_EQ(3u, results.size());
  EXPECT_EQ(0u, results[1].frames_dropped_v3);
  EXPECT_EQ(1u, results[1].frames_expected_v3);
  EXPECT_EQ(0u, results[2].frames_dropped_v3);
  EXPECT_EQ(0u, results[2].frames_expected_v3);
  EXPECT_EQ(0u, results[3].frames_dropped_v3);
  EXPECT_EQ(1u, results[3].frames_expected_v3);
}

TEST_F(FrameSequenceTrackerTest, CustomTrackerOutOfOrderFramesMissingV3Data) {
  CustomTrackerResults results;
  collection_.set_custom_tracker_results_added_callback(
      base::BindLambdaForTesting([&](const CustomTrackerResults& reported) {
        for (const auto& pair : reported) {
          results[pair.first] = pair.second;
        }
      }));

  // Start custom tracker 1.
  collection_.StartCustomSequence(1);
  EXPECT_EQ(1u, NumberOfCustomTrackers());

  const uint64_t source = 1;
  uint64_t sequence = 0;

  // Dispatch 2 frames: frame 0 and frame 1.
  auto frame0_args = CreateBeginFrameArgs(source, ++sequence);
  DispatchCompleteFrame(frame0_args, kImplDamage | kMainDamage);
  sorter_.AddNewFrame(frame0_args);

  auto frame1_args = CreateBeginFrameArgs(source, ++sequence);
  DispatchCompleteFrame(frame1_args, kImplDamage | kMainDamage);
  sorter_.AddNewFrame(frame1_args);

  // Frame 1 gets its result before frame 0.
  FrameInfo frame_info;
  frame_info.final_state = FrameInfo::FrameFinalState::kPresentedAll;
  frame_info.smooth_thread = FrameInfo::SmoothThread::kSmoothMain;
  frame_info.scroll_thread = FrameInfo::SmoothEffectDrivingThread::kMain;
  frame_info.sequence_number = frame1_args.frame_id.sequence_number;
  sorter_.AddFrameResult(frame1_args, frame_info);

  // Stop the tracker.
  collection_.StopCustomSequence(1);

  // Frame 0 gets its result after tracker is stopped. FrameSorter flushes all
  // frames and metrics for both frames should be recorded for v3.
  sorter_.AddFrameResult(frame0_args, frame_info);

  // Frame 2 is dispatched after the tracker is stopped and should be ignored.
  auto frame2_args = CreateBeginFrameArgs(source, ++sequence);
  DispatchCompleteFrame(frame2_args, kImplDamage | kMainDamage);
  sorter_.AddNewFrame(frame2_args);
  sorter_.AddFrameResult(frame2_args, frame_info);

  // Trigger metrics report.
  collection_.ClearAll();

  // There is one report for tracker id 1 and 2 expected frames (frame 0 and 1).
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(2u, results[1].frames_expected_v3);
}

}  // namespace cc
