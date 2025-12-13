// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_decision_queue.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "cc/metrics/scroll_jank_v4_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

constexpr base::TimeTicks MillisSinceEpoch(int64_t millis) {
  return base::TimeTicks() + base::Milliseconds(millis);
}

using DamagingFrame = ScrollJankV4Frame::DamagingFrame;
using ScrollUpdates = ScrollJankV4FrameStage::ScrollUpdates;
using Real = ScrollUpdates::Real;
using Synthetic = ScrollUpdates::Synthetic;
using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using BeginFrameArgsForScrollJank =
    ScrollJankV4Frame::BeginFrameArgsForScrollJank;

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::StrictMock;

/* Matches a result iff `matcher` matches `result->missed_vsyncs_per_reason`. */
::testing::Matcher<const ScrollJankV4Result&> HasMissedVsyncsPerReasonMatching(
    ::testing::Matcher<const JankReasonArray<int>&> matcher) {
  return ::testing::Field(&ScrollJankV4Result::missed_vsyncs_per_reason,
                          matcher);
}

const ::testing::Matcher<const ScrollJankV4Result&> kHasNoMissedVsyncs =
    HasMissedVsyncsPerReasonMatching(::testing::Each(::testing::Eq(0)));

::testing::Matcher<const ScrollJankV4Result&> HasMissedVsyncs(
    JankReason reason,
    int missed_vsyncs) {
  JankReasonArray<int> expected_missed_vsyncs = {};
  expected_missed_vsyncs[static_cast<int>(reason)] = missed_vsyncs;
  return HasMissedVsyncsPerReasonMatching(
      ::testing::ElementsAreArray(expected_missed_vsyncs));
}

class MockResultConsumer : public ScrollJankV4DecisionQueue::ResultConsumer {
 public:
  MOCK_METHOD(void,
              OnFrameResult,
              (const ScrollUpdates& updates,
               const ScrollDamage& damage,
               const BeginFrameArgsForScrollJank& args,
               const ScrollJankV4Result& result),
              (override));
  MOCK_METHOD(void, OnScrollStarted, (), (override));
  MOCK_METHOD(void, OnScrollEnded, (), (override));
};

// A result consumer which forwards all method calls to another result consumer.
// This consumer doesn't own the other consumer.
class ForwardingResultConsumer
    : public ScrollJankV4DecisionQueue::ResultConsumer {
 public:
  explicit ForwardingResultConsumer(
      ScrollJankV4DecisionQueue::ResultConsumer* forwardee)
      : forwardee_(forwardee) {}

  void OnFrameResult(const ScrollUpdates& updates,
                     const ScrollDamage& damage,
                     const BeginFrameArgsForScrollJank& args,
                     const ScrollJankV4Result& result) override {
    forwardee_->OnFrameResult(updates, damage, args, result);
  }
  void OnScrollStarted() override { forwardee_->OnScrollStarted(); }
  void OnScrollEnded() override { forwardee_->OnScrollEnded(); }

 private:
  raw_ptr<ScrollJankV4DecisionQueue::ResultConsumer> forwardee_;
};

class ScrollJankV4DecisionQueueTest : public testing::Test {
 protected:
  ScrollJankV4DecisionQueueTest()
      : result_consumer_(std::make_unique<StrictMock<MockResultConsumer>>()),
        decision_queue_(std::make_unique<ScrollJankV4DecisionQueue>(
            std::make_unique<ForwardingResultConsumer>(
                result_consumer_.get()))) {}

  static ScrollJankV4Frame::BeginFrameArgsForScrollJank CreateBeginFrameArgs(
      base::TimeTicks frame_time) {
    return {.frame_time = frame_time, .interval = kVsyncInterval};
  }

  // `result_consumer_` must be declared above `decision_queue_` because
  // `decision_queue_` owns a `ForwardingResultConsumer` which has a raw pointer
  // to `result_consumer_` (so `decision_queue_` must be destroyed before
  // `result_consumer_`).
  std::unique_ptr<StrictMock<MockResultConsumer>> result_consumer_;
  std::unique_ptr<ScrollJankV4DecisionQueue> decision_queue_;
};

TEST_F(ScrollJankV4DecisionQueueTest, ImmediatelyReportsResultsForRealFrames) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F1: Real frame.
  ScrollUpdates updates1 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(103),
                         .last_input_generation_ts = MillisSinceEpoch(111),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage1 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(132)};
  BeginFrameArgsForScrollJank args1 =
      CreateBeginFrameArgs(MillisSinceEpoch(116));
  {
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates1, damage1, args1, kHasNoMissedVsyncs));
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, damage1, args1);
    EXPECT_TRUE(success1);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F2: Real frame with no missed VSyncs.
  ScrollUpdates updates2 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(119),
                         .last_input_generation_ts = MillisSinceEpoch(127),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage2 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(148)};
  BeginFrameArgsForScrollJank args2 =
      CreateBeginFrameArgs(MillisSinceEpoch(132));
  {
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates2, damage2, args2, kHasNoMissedVsyncs));
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, damage2, args2);
    EXPECT_TRUE(success2);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F3: Real frame with 1 VSync missed due to fast scroll continuity rule.
  ScrollUpdates updates3 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(151),
                         .last_input_generation_ts = MillisSinceEpoch(159),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage3 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(180)};
  BeginFrameArgsForScrollJank args3 =
      CreateBeginFrameArgs(MillisSinceEpoch(164));
  {
    EXPECT_CALL(
        *result_consumer_,
        OnFrameResult(
            updates3, damage3, args3,
            HasMissedVsyncs(JankReason::kMissedVsyncDuringFastScroll, 1)));
    bool success3 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates3, damage3, args3);
    EXPECT_TRUE(success3);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  {
    EXPECT_CALL(*result_consumer_, OnScrollEnded());
    decision_queue_->OnScrollEnded();
    Mock::VerifyAndClear(result_consumer_.get());
  }
}

TEST_F(ScrollJankV4DecisionQueueTest,
       DefersReportingResultsForSyntheticFramesUntilNextRealFrame) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F1: Synthetic frame.
  ScrollUpdates updates1 = ScrollUpdates(
      /* real= */ std::nullopt,
      Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(116)});
  ScrollDamage damage1 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(132)};
  BeginFrameArgsForScrollJank args1 =
      CreateBeginFrameArgs(MillisSinceEpoch(116));
  {
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F1).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _, _, _)).Times(0);
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, damage1, args1);
    EXPECT_TRUE(success1);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F2: Real frame with no missed VSyncs.
  ScrollUpdates updates2 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(135),
                         .last_input_generation_ts = MillisSinceEpoch(143),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage2 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(164)};
  BeginFrameArgsForScrollJank args2 =
      CreateBeginFrameArgs(MillisSinceEpoch(148));
  {
    // The deferred decision for the synthetic frame F1 should be flushed
    // before F2's result.
    InSequence in_sequence;
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates1, damage1, args1, kHasNoMissedVsyncs));
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates2, damage2, args2, kHasNoMissedVsyncs));
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, damage2, args2);
    EXPECT_TRUE(success2);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F3: Synthetic frame with no missed VSyncs.
  ScrollUpdates updates3 = ScrollUpdates(
      /* real= */ std::nullopt,
      Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(164)});
  ScrollDamage damage3 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(180)};
  BeginFrameArgsForScrollJank args3 =
      CreateBeginFrameArgs(MillisSinceEpoch(164));
  {
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F3).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _, _, _)).Times(0);
    bool success3 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates3, damage3, args3);
    EXPECT_TRUE(success3);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F4: Synthetic frame with 1 VSync missed due to fast scroll continuity rule.
  ScrollUpdates updates4 = ScrollUpdates(
      /* real= */ std::nullopt,
      Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(196)});
  ScrollDamage damage4 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(212)};
  BeginFrameArgsForScrollJank args4 =
      CreateBeginFrameArgs(MillisSinceEpoch(196));
  {
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F4).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _, _, _)).Times(0);
    bool success4 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates4, damage4, args4);
    EXPECT_TRUE(success4);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F5: Real frame with 2 VSyncs missed due to the fling continuity rule.
  ScrollUpdates updates5 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(236),
                         .last_input_generation_ts = MillisSinceEpoch(236),
                         .has_inertial_input = true,
                         .abs_total_raw_delta_pixels = 1.0f,
                         .max_abs_inertial_raw_delta_pixels = 1.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage5 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(260)};
  BeginFrameArgsForScrollJank args5 =
      CreateBeginFrameArgs(MillisSinceEpoch(244));
  {
    // The deferred decisions for the synthetic frames F3 and F4 should be
    // flushed (in chronological order) before F5's result.
    InSequence in_sequence;
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates3, damage3, args3, kHasNoMissedVsyncs));
    EXPECT_CALL(
        *result_consumer_,
        OnFrameResult(
            updates4, damage4, args4,
            HasMissedVsyncs(JankReason::kMissedVsyncDuringFastScroll, 1)));
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates5, damage5, args5,
                              HasMissedVsyncs(
                                  JankReason::kMissedVsyncAtStartOfFling, 2)));
    bool success5 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates5, damage5, args5);
    EXPECT_TRUE(success5);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  {
    EXPECT_CALL(*result_consumer_, OnScrollEnded());
    decision_queue_->OnScrollEnded();
    Mock::VerifyAndClear(result_consumer_.get());
  }
}

TEST_F(ScrollJankV4DecisionQueueTest,
       DefersReportingResultsForSyntheticFramesUntilEndOfScroll) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F1: Real frame.
  ScrollUpdates updates1 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(135),
                         .last_input_generation_ts = MillisSinceEpoch(143),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage1 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(164)};
  BeginFrameArgsForScrollJank args1 =
      CreateBeginFrameArgs(MillisSinceEpoch(148));
  {
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates1, damage1, args1, kHasNoMissedVsyncs));
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, damage1, args1);
    EXPECT_TRUE(success1);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F2: Synthetic frame with no missed VSyncs (because we're not in a fast
  // scroll).
  ScrollUpdates updates2 = ScrollUpdates(
      /* real= */ std::nullopt,
      Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(196)});
  ScrollDamage damage2 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(212)};
  BeginFrameArgsForScrollJank args2 =
      CreateBeginFrameArgs(MillisSinceEpoch(196));
  {
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F2).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _, _, _)).Times(0);
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, damage2, args2);
    EXPECT_TRUE(success2);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  {
    // The deferred decision for the synthetic frame F2 should be flushed before
    // the scroll end.
    InSequence in_sequence;
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates2, damage2, args2, kHasNoMissedVsyncs));
    EXPECT_CALL(*result_consumer_, OnScrollEnded());
    decision_queue_->OnScrollEnded();
    Mock::VerifyAndClear(result_consumer_.get());
  }
}

TEST_F(ScrollJankV4DecisionQueueTest,
       DefersReportingResultsForSyntheticFramesUntilStartOfNextScroll) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F1: Real frame.
  ScrollUpdates updates1 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(135),
                         .last_input_generation_ts = MillisSinceEpoch(143),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage1 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(164)};
  BeginFrameArgsForScrollJank args1 =
      CreateBeginFrameArgs(MillisSinceEpoch(148));
  {
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates1, damage1, args1, kHasNoMissedVsyncs));
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, damage1, args1);
    EXPECT_TRUE(success1);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F2: Synthetic frame with no missed VSyncs (because we're not in a fast
  // scroll).
  ScrollUpdates updates2 = ScrollUpdates(
      /* real= */ std::nullopt,
      Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(196)});
  ScrollDamage damage2 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(212)};
  BeginFrameArgsForScrollJank args2 =
      CreateBeginFrameArgs(MillisSinceEpoch(196));
  {
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F2).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _, _, _)).Times(0);
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, damage2, args2);
    EXPECT_TRUE(success2);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  {
    // The deferred decision for the synthetic frame F2 should be flushed before
    // the next scroll start.
    InSequence in_sequence;
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates2, damage2, args2, kHasNoMissedVsyncs));
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
    Mock::VerifyAndClear(result_consumer_.get());
  }
}

TEST_F(ScrollJankV4DecisionQueueTest,
       DefersReportingResultsForSyntheticFramesUntilDestruction) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F1: Real frame.
  ScrollUpdates updates1 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(135),
                         .last_input_generation_ts = MillisSinceEpoch(143),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage1 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(164)};
  BeginFrameArgsForScrollJank args1 =
      CreateBeginFrameArgs(MillisSinceEpoch(148));
  {
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates1, damage1, args1, kHasNoMissedVsyncs));
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, damage1, args1);
    EXPECT_TRUE(success1);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F2: Synthetic frame with no missed VSyncs (because we're not in a fast
  // scroll).
  ScrollUpdates updates2 = ScrollUpdates(
      /* real= */ std::nullopt,
      Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(196)});
  ScrollDamage damage2 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(212)};
  BeginFrameArgsForScrollJank args2 =
      CreateBeginFrameArgs(MillisSinceEpoch(196));
  {
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F2).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _, _, _)).Times(0);
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, damage2, args2);
    EXPECT_TRUE(success2);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  {
    // The deferred decision for the synthetic frame F2 should be flushed before
    // the queue is destroyed.
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates2, damage2, args2, kHasNoMissedVsyncs));
    delete decision_queue_.release();
    Mock::VerifyAndClear(result_consumer_.get());
  }
}

TEST_F(ScrollJankV4DecisionQueueTest, HandlesInvalidFramesGracefully) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F1: Valid real frame.
  ScrollUpdates updates1 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(103),
                         .last_input_generation_ts = MillisSinceEpoch(111),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage1 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(148)};
  BeginFrameArgsForScrollJank args1 =
      CreateBeginFrameArgs(MillisSinceEpoch(116));
  {
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates1, damage1, args1, kHasNoMissedVsyncs));
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, damage1, args1);
    EXPECT_TRUE(success1);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F2: Invalid real frame (because last_input_generation_ts is after
  // presentation_ts).
  ScrollUpdates updates2 = ScrollUpdates(
      Real{.first_input_generation_ts = MillisSinceEpoch(119),
           .last_input_generation_ts = MillisSinceEpoch(165) /* wrong */,
           .has_inertial_input = false,
           .abs_total_raw_delta_pixels = 5.0f,
           .max_abs_inertial_raw_delta_pixels = 0.0f},
      /* synthetic= */ std::nullopt);
  ScrollDamage damage2 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(164)};
  BeginFrameArgsForScrollJank args2 =
      CreateBeginFrameArgs(MillisSinceEpoch(132));
  {
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _, _, _)).Times(0);
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, damage2, args2);
    EXPECT_FALSE(success2);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F3: Invalid real frame (because its args.frame_time is before F1's, i.e.
  // it's not chronologically ordered).
  ScrollUpdates updates3 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(119),
                         .last_input_generation_ts = MillisSinceEpoch(127),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage3 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(164)};
  BeginFrameArgsForScrollJank args3 =
      CreateBeginFrameArgs(MillisSinceEpoch(115) /* wrong */);
  {
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _, _, _)).Times(0);
    bool success3 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates3, damage3, args3);
    EXPECT_FALSE(success3);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F4: Invalid real frame (because its presentation_ts is before F1's, i.e.
  // it's not chronologically ordered).
  ScrollUpdates updates4 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(119),
                         .last_input_generation_ts = MillisSinceEpoch(127),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage4 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(147) /* wrong */};
  BeginFrameArgsForScrollJank args4 =
      CreateBeginFrameArgs(MillisSinceEpoch(132));
  {
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _, _, _)).Times(0);
    bool success4 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates4, damage4, args4);
    EXPECT_FALSE(success4);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  // F5: Valid real frame.
  ScrollUpdates updates5 =
      ScrollUpdates(Real{.first_input_generation_ts = MillisSinceEpoch(119),
                         .last_input_generation_ts = MillisSinceEpoch(127),
                         .has_inertial_input = false,
                         .abs_total_raw_delta_pixels = 5.0f,
                         .max_abs_inertial_raw_delta_pixels = 0.0f},
                    /* synthetic= */ std::nullopt);
  ScrollDamage damage5 =
      DamagingFrame{.presentation_ts = MillisSinceEpoch(164)};
  BeginFrameArgsForScrollJank args5 =
      CreateBeginFrameArgs(MillisSinceEpoch(132));
  {
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(updates5, damage5, args5, kHasNoMissedVsyncs));
    bool success5 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates5, damage5, args5);
    EXPECT_TRUE(success5);
    Mock::VerifyAndClear(result_consumer_.get());
  }

  {
    EXPECT_CALL(*result_consumer_, OnScrollEnded());
    decision_queue_->OnScrollEnded();
    Mock::VerifyAndClear(result_consumer_.get());
  }
}

}  // namespace
}  // namespace cc
