// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_decision_queue.h"

#include <concepts>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/test/event_metrics_test_creator.h"
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
using ScrollJankV4Result = ScrollUpdateEventMetrics::ScrollJankV4Result;

using ::testing::_;
using ::testing::InSequence;
using ::testing::StrictMock;

/* Matches a result iff `matcher` matches `result->missed_vsyncs_per_reason`. */
::testing::Matcher<ScrollJankV4Result> HasMissedVsyncsPerReasonMatching(
    ::testing::Matcher<const JankReasonArray<int>&> matcher) {
  return ::testing::Field(&ScrollJankV4Result::missed_vsyncs_per_reason,
                          matcher);
}

const ::testing::Matcher<ScrollJankV4Result> kHasNoMissedVsyncs =
    HasMissedVsyncsPerReasonMatching(::testing::Each(::testing::Eq(0)));

::testing::Matcher<ScrollJankV4Result> HasMissedVsyncs(JankReason reason,
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
              (ScrollUpdateEventMetrics::ScrollJankV4Result result,
               ScrollUpdateEventMetrics* earliest_event),
              (override));
  MOCK_METHOD(void, OnScrollStarted, (), (override));
  MOCK_METHOD(void, OnScrollEnded, (), (override));
};

// A result consumer which forwards all method calls to another result consumer
// (of type `R`). This consumer doesn't own the other consumer.
template <typename R>
  requires std::derived_from<R, ScrollJankV4DecisionQueue::ResultConsumer>
class ForwardingResultConsumer
    : public ScrollJankV4DecisionQueue::ResultConsumer {
 public:
  explicit ForwardingResultConsumer(R* forwardee) : forwardee_(forwardee) {}

  void OnFrameResult(ScrollUpdateEventMetrics::ScrollJankV4Result result,
                     ScrollUpdateEventMetrics* earliest_event) override {
    forwardee_->OnFrameResult(result, earliest_event);
  }
  void OnScrollStarted() override { forwardee_->OnScrollStarted(); }
  void OnScrollEnded() override { forwardee_->OnScrollEnded(); }

 private:
  raw_ptr<R> forwardee_;
};

class ScrollJankV4DecisionQueueTest : public testing::Test {
 protected:
  ScrollJankV4DecisionQueueTest()
      : result_consumer_(std::make_unique<StrictMock<MockResultConsumer>>()),
        decision_queue_(std::make_unique<ScrollJankV4DecisionQueue>(
            std::make_unique<
                ForwardingResultConsumer<StrictMock<MockResultConsumer>>>(
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
  }

  // F1: Real frame.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event1 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, earliest_event1.get()));
    ScrollUpdates updates1 =
        ScrollUpdates(earliest_event1.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(103),
                           .last_input_generation_ts = MillisSinceEpoch(111),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, DamagingFrame{.presentation_ts = MillisSinceEpoch(132)},
        CreateBeginFrameArgs(MillisSinceEpoch(116)));
    EXPECT_TRUE(success1);
  }

  // F2: Real frame with no missed VSyncs.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event2 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, earliest_event2.get()));
    ScrollUpdates updates2 =
        ScrollUpdates(earliest_event2.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(119),
                           .last_input_generation_ts = MillisSinceEpoch(127),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, DamagingFrame{.presentation_ts = MillisSinceEpoch(148)},
        CreateBeginFrameArgs(MillisSinceEpoch(132)));
    EXPECT_TRUE(success2);
  }

  // F3: Real frame with 1 VSync missed due to fast scroll continuity rule.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event3 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(HasMissedVsyncs(
                                  JankReason::kMissedVsyncDuringFastScroll, 1),
                              earliest_event3.get()));
    ScrollUpdates updates3 =
        ScrollUpdates(earliest_event3.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(151),
                           .last_input_generation_ts = MillisSinceEpoch(159),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success3 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates3, DamagingFrame{.presentation_ts = MillisSinceEpoch(180)},
        CreateBeginFrameArgs(MillisSinceEpoch(164)));
    EXPECT_TRUE(success3);
  }

  {
    EXPECT_CALL(*result_consumer_, OnScrollEnded());
    decision_queue_->OnScrollEnded();
  }
}

TEST_F(ScrollJankV4DecisionQueueTest,
       DefersReportingResultsForSyntheticFramesUntilNextRealFrame) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
  }

  // F1: Synthetic frame.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event1 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F1).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _)).Times(0);
    ScrollUpdates updates1 = ScrollUpdates(
        earliest_event1.get(),
        /* real= */ std::nullopt,
        Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(116)});
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, DamagingFrame{.presentation_ts = MillisSinceEpoch(132)},
        CreateBeginFrameArgs(MillisSinceEpoch(116)));
    EXPECT_TRUE(success1);
  }

  // F2: Real frame with no missed VSyncs.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event2 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    // The deferred decision for the synthetic frame F1 should be flushed
    // before F2's result.
    InSequence in_sequence;
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, /* F1 */ nullptr));
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, earliest_event2.get()));
    ScrollUpdates updates2 =
        ScrollUpdates(earliest_event2.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(135),
                           .last_input_generation_ts = MillisSinceEpoch(143),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
        CreateBeginFrameArgs(MillisSinceEpoch(148)));
    EXPECT_TRUE(success2);
  }

  // F3: Synthetic frame with no missed VSyncs.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event3 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F3).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _)).Times(0);
    ScrollUpdates updates3 = ScrollUpdates(
        earliest_event3.get(),
        /* real= */ std::nullopt,
        Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(164)});
    bool success3 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates3, DamagingFrame{.presentation_ts = MillisSinceEpoch(180)},
        CreateBeginFrameArgs(MillisSinceEpoch(164)));
    EXPECT_TRUE(success3);
  }

  // F4: Synthetic frame with 1 VSync missed due to fast scroll continuity rule.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event4 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F4).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _)).Times(0);
    ScrollUpdates updates3 = ScrollUpdates(
        earliest_event4.get(),
        /* real= */ std::nullopt,
        Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(196)});
    bool success4 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates3, DamagingFrame{.presentation_ts = MillisSinceEpoch(212)},
        CreateBeginFrameArgs(MillisSinceEpoch(196)));
    EXPECT_TRUE(success4);
  }

  // F5: Real frame with 2 VSyncs missed due to the fling continuity rule.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event5 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    // The deferred decisions for the synthetic frames F3 and F4 should be
    // flushed (in chronological order) before F5's result.
    InSequence in_sequence;
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, /* F3 */ nullptr));
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(HasMissedVsyncs(
                                  JankReason::kMissedVsyncDuringFastScroll, 1),
                              /* F4 */ nullptr));
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(
                    HasMissedVsyncs(JankReason::kMissedVsyncAtStartOfFling, 2),
                    earliest_event5.get()));
    ScrollUpdates updates5 =
        ScrollUpdates(earliest_event5.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(236),
                           .last_input_generation_ts = MillisSinceEpoch(236),
                           .has_inertial_input = true,
                           .abs_total_raw_delta_pixels = 1.0f,
                           .max_abs_inertial_raw_delta_pixels = 1.0f},
                      /* synthetic= */ std::nullopt);
    bool success5 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates5, DamagingFrame{.presentation_ts = MillisSinceEpoch(260)},
        CreateBeginFrameArgs(MillisSinceEpoch(244)));
    EXPECT_TRUE(success5);
  }

  {
    EXPECT_CALL(*result_consumer_, OnScrollEnded());
    decision_queue_->OnScrollEnded();
  }
}

TEST_F(ScrollJankV4DecisionQueueTest,
       DefersReportingResultsForSyntheticFramesUntilEndOfScroll) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
  }

  // F1: Real frame.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event1 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, earliest_event1.get()));
    ScrollUpdates updates1 =
        ScrollUpdates(earliest_event1.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(135),
                           .last_input_generation_ts = MillisSinceEpoch(143),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
        CreateBeginFrameArgs(MillisSinceEpoch(148)));
    EXPECT_TRUE(success1);
  }

  // F2: Synthetic frame with no missed VSyncs (because we're not in a fast
  // scroll).
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event2 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F2).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _)).Times(0);
    ScrollUpdates updates2 = ScrollUpdates(
        earliest_event2.get(),
        /* real= */ std::nullopt,
        Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(196)});
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, DamagingFrame{.presentation_ts = MillisSinceEpoch(212)},
        CreateBeginFrameArgs(MillisSinceEpoch(196)));
    EXPECT_TRUE(success2);
  }

  {
    // The deferred decision for the synthetic frame F2 should be flushed before
    // the scroll end.
    InSequence in_sequence;
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, /* F2 */ nullptr));
    EXPECT_CALL(*result_consumer_, OnScrollEnded());
    decision_queue_->OnScrollEnded();
  }
}

TEST_F(ScrollJankV4DecisionQueueTest,
       DefersReportingResultsForSyntheticFramesUntilStartOfNextScroll) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
  }

  // F1: Real frame.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event1 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, earliest_event1.get()));
    ScrollUpdates updates1 =
        ScrollUpdates(earliest_event1.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(135),
                           .last_input_generation_ts = MillisSinceEpoch(143),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
        CreateBeginFrameArgs(MillisSinceEpoch(148)));
    EXPECT_TRUE(success1);
  }

  // F2: Synthetic frame with no missed VSyncs (because we're not in a fast
  // scroll).
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event2 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F2).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _)).Times(0);
    ScrollUpdates updates2 = ScrollUpdates(
        earliest_event2.get(),
        /* real= */ std::nullopt,
        Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(196)});
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, DamagingFrame{.presentation_ts = MillisSinceEpoch(212)},
        CreateBeginFrameArgs(MillisSinceEpoch(196)));
    EXPECT_TRUE(success2);
  }

  {
    // The deferred decision for the synthetic frame F2 should be flushed before
    // the next scroll start.
    InSequence in_sequence;
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, /* F2 */ nullptr));
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
  }
}

TEST_F(ScrollJankV4DecisionQueueTest,
       DefersReportingResultsForSyntheticFramesUntilDestruction) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
  }

  // F1: Real frame.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event1 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, earliest_event1.get()));
    ScrollUpdates updates1 =
        ScrollUpdates(earliest_event1.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(135),
                           .last_input_generation_ts = MillisSinceEpoch(143),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
        CreateBeginFrameArgs(MillisSinceEpoch(148)));
    EXPECT_TRUE(success1);
  }

  // F2: Synthetic frame with no missed VSyncs (because we're not in a fast
  // scroll).
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event2 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    // The queue should defer the decision (i.e. no result should be reported
    // yet for F2).
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _)).Times(0);
    ScrollUpdates updates2 = ScrollUpdates(
        earliest_event2.get(),
        /* real= */ std::nullopt,
        Synthetic{.first_input_begin_frame_ts = MillisSinceEpoch(196)});
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, DamagingFrame{.presentation_ts = MillisSinceEpoch(212)},
        CreateBeginFrameArgs(MillisSinceEpoch(196)));
    EXPECT_TRUE(success2);
  }

  {
    // The deferred decision for the synthetic frame F2 should be flushed before
    // the queue is destroyed.
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, /* F2 */ nullptr));
    delete decision_queue_.release();
  }
}

TEST_F(ScrollJankV4DecisionQueueTest, HandlesInvalidFramesGracefully) {
  {
    EXPECT_CALL(*result_consumer_, OnScrollStarted());
    decision_queue_->OnScrollStarted();
  }

  // F1: Valid real frame.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event1 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, earliest_event1.get()));
    ScrollUpdates updates1 =
        ScrollUpdates(earliest_event1.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(103),
                           .last_input_generation_ts = MillisSinceEpoch(111),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success1 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates1, DamagingFrame{.presentation_ts = MillisSinceEpoch(148)},
        CreateBeginFrameArgs(MillisSinceEpoch(116)));
    EXPECT_TRUE(success1);
  }

  // F2: Invalid real frame (because last_input_generation_ts is after
  // presentation_ts).
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event2 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _)).Times(0);
    ScrollUpdates updates2 = ScrollUpdates(
        earliest_event2.get(),
        Real{.first_input_generation_ts = MillisSinceEpoch(119),
             .last_input_generation_ts = MillisSinceEpoch(165) /* wrong */,
             .has_inertial_input = false,
             .abs_total_raw_delta_pixels = 5.0f,
             .max_abs_inertial_raw_delta_pixels = 0.0f},
        /* synthetic= */ std::nullopt);
    bool success2 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates2, DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
        CreateBeginFrameArgs(MillisSinceEpoch(132)));
    EXPECT_FALSE(success2);
  }

  // F3: Invalid real frame (because its args.frame_time is before F1's, i.e.
  // it's not chronologically ordered).
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event3 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _)).Times(0);
    ScrollUpdates updates4 =
        ScrollUpdates(earliest_event3.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(119),
                           .last_input_generation_ts = MillisSinceEpoch(127),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success3 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates4, DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
        CreateBeginFrameArgs(MillisSinceEpoch(115) /* wrong */));
    EXPECT_FALSE(success3);
  }

  // F4: Invalid real frame (because its presentation_ts is before F1's, i.e.
  // it's not chronologically ordered).
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event4 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_, OnFrameResult(_, _)).Times(0);
    ScrollUpdates updates4 =
        ScrollUpdates(earliest_event4.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(119),
                           .last_input_generation_ts = MillisSinceEpoch(127),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success4 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates4,
        DamagingFrame{.presentation_ts = MillisSinceEpoch(147) /* wrong */},
        CreateBeginFrameArgs(MillisSinceEpoch(132)));
    EXPECT_FALSE(success4);
  }

  // F5: Valid real frame.
  {
    std::unique_ptr<ScrollUpdateEventMetrics> earliest_event5 =
        EventMetricsTestCreator().CreateGestureScrollUpdate({});
    EXPECT_CALL(*result_consumer_,
                OnFrameResult(kHasNoMissedVsyncs, earliest_event5.get()));
    ScrollUpdates updates5 =
        ScrollUpdates(earliest_event5.get(),
                      Real{.first_input_generation_ts = MillisSinceEpoch(119),
                           .last_input_generation_ts = MillisSinceEpoch(127),
                           .has_inertial_input = false,
                           .abs_total_raw_delta_pixels = 5.0f,
                           .max_abs_inertial_raw_delta_pixels = 0.0f},
                      /* synthetic= */ std::nullopt);
    bool success5 = decision_queue_->ProcessFrameWithScrollUpdates(
        updates5, DamagingFrame{.presentation_ts = MillisSinceEpoch(164)},
        CreateBeginFrameArgs(MillisSinceEpoch(132)));
    EXPECT_TRUE(success5);
  }

  {
    EXPECT_CALL(*result_consumer_, OnScrollEnded());
    decision_queue_->OnScrollEnded();
  }
}

}  // namespace
}  // namespace cc
