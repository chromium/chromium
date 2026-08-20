// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_timing_emitter.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_timeline_calculator.h"
#include "cc/metrics/scroll_timing_info.h"
#include "cc/paint/element_id.h"
#include "cc/test/event_metrics_test_creator.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

namespace {

using BeginFrameArgsForScrollJank =
    ScrollJankV4Frame::BeginFrameArgsForScrollJank;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;
using DispatchBeginFrameArgs = ScrollEventMetrics::DispatchBeginFrameArgs;
using NonDamagingFrame = ScrollJankV4Frame::NonDamagingFrame;
using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using ScrollEnd = ScrollJankV4Frame::Stage::ScrollEnd;
using ScrollStart = ScrollJankV4Frame::Stage::ScrollStart;
using ScrollUpdates = ScrollJankV4Frame::Stage::ScrollUpdates;
using Stage = ScrollJankV4Frame::Stage;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr uint64_t kSourceId = 999;
constexpr base::TimeDelta kVsyncInterval = base::Milliseconds(16);

constexpr ElementId kScroller(101);
constexpr ElementId kOtherScroller(202);

constexpr base::TimeTicks MillisecondsTicks(int ms) {
  return base::TimeTicks() + base::Milliseconds(ms);
}

// The gesture's GestureScrollBegin hardware timestamp, i.e. the expected
// `start_time` of records below.
constexpr base::TimeTicks kScrollBeginGenerated = MillisecondsTicks(10);

// The gesture's GestureScrollBegin renderer-compositor arrival timestamp, which
// scroll jank v4 uses as a scroll ID. Deliberately different from
// `kScrollBeginGenerated` so tests can tell the two apart.
constexpr base::TimeTicks kScrollId = MillisecondsTicks(15);

}  // namespace

class ScrollTimingEmitterTest : public testing::Test {
 protected:
  static ScrollUpdates CreateUpdatesStage(base::TimeTicks scroll_id) {
    return ScrollUpdates(ScrollUpdates::Real{}, /*synthetic=*/std::nullopt,
                         scroll_id);
  }

  // Creates the timeline of a single frame identified by `result_id`. Timelines
  // of several frames are covered by `ConsumesCalculatedTimeline` below.
  static ScrollJankV4Frame::Timeline CreateTimeline(
      uint64_t result_id,
      ScrollDamage damage,
      ScrollJankV4Frame::StageList stages) {
    ScrollJankV4Frame::Timeline timeline;
    timeline.push_back(ScrollJankV4Frame(
        BeginFrameArgsForScrollJank{.frame_time = MillisecondsTicks(100),
                                    .interval = kVsyncInterval,
                                    .result_id = result_id},
        std::move(damage), std::move(stages)));
    return timeline;
  }

  static viz::BeginFrameArgs CreateBeginFrameArgs(int sequence_id,
                                                  base::TimeTicks frame_time) {
    return viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, kSourceId, sequence_id, frame_time,
        /*deadline=*/frame_time + kVsyncInterval / 3, kVsyncInterval,
        viz::BeginFrameArgs::BeginFrameArgsType::NORMAL);
  }

  static DispatchBeginFrameArgs CreateDispatchBeginFrameArgs(
      int sequence_id,
      base::TimeTicks frame_time) {
    return {
        .frame_time = frame_time,
        .interval = kVsyncInterval,
        .frame_id = viz::BeginFrameId(kSourceId, sequence_id),
    };
  }

  // Creates a scroll update which the stage calculator attributed to the frame
  // identified by `result_id` and to the gesture identified by `scroll_id`, and
  // which moved `element_ids` in that order.
  std::unique_ptr<ScrollUpdateEventMetrics> CreateUpdate(
      uint64_t result_id,
      base::TimeTicks timestamp,
      const std::vector<ElementId>& element_ids,
      base::TimeTicks scroll_id = kScrollId,
      base::TimeTicks scroll_begin_generated = kScrollBeginGenerated,
      ui::ScrollInputType input_type = ui::ScrollInputType::kTouchscreen) {
    EventMetricsTestCreator::ScrollUpdateEventBuilder builder =
        metrics_creator_.GestureScrollUpdateBuilder()
            .SetDelta(1.0f)
            .SetDidScroll(true)
            .SetCausedFrameUpdate(true)
            .SetTimestamp(timestamp)
            .SetScrollInputType(input_type)
            .SetScrollJankV4ResultId(result_id)
            .SetScrollBeginArrivalTimestamp(scroll_id)
            .SetScrollBeginGeneratedTimestamp(scroll_begin_generated);
    for (ElementId element_id : element_ids) {
      builder.AddAppliedScrollObservation(element_id);
    }
    return builder.Build();
  }

  // Wraps `update` in the event list of a frame which resolved one update.
  static EventMetrics::List CreateEvents(
      std::unique_ptr<ScrollUpdateEventMetrics> update) {
    EventMetrics::List events;
    events.push_back(std::move(update));
    return events;
  }

  EventMetricsTestCreator metrics_creator_;
  ScrollTimingEmitter emitter_;
};

TEST_F(ScrollTimingEmitterTest, CompleteGestureProducesOneRecord) {
  // The first movement frame opens the segment and names the target.
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  // A later movement frame extends the end time but not the target.
  emitter_.ProcessTimeline(
      CreateTimeline(2, DamagingFrame{.presentation_ts = MillisecondsTicks(56)},
                     {Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(2, MillisecondsTicks(36), {kOtherScroller})));

  emitter_.ProcessTimeline(
      CreateTimeline(3, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(56),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, NonDamagingMovementFrameDoesNotExtendEndTime) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  emitter_.ProcessTimeline(
      CreateTimeline(2, NonDamagingFrame{},
                     {Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(2, MillisecondsTicks(36), {kScroller})));

  emitter_.ProcessTimeline(
      CreateTimeline(3, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(40),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest,
       DamagingFrameWithoutObservedMovementDoesNotExtendEndTime) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  // The update consumed delta without moving content, e.g. it only moved
  // browser controls, so it recorded no observation.
  emitter_.ProcessTimeline(
      CreateTimeline(2, DamagingFrame{.presentation_ts = MillisecondsTicks(56)},
                     {Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(2, MillisecondsTicks(36), /*element_ids=*/{})));

  emitter_.ProcessTimeline(
      CreateTimeline(3, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(40),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, MovementFromAnotherScrollDoesNotExtendEndTime) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  // Two scrolls overlap in this frame. The stage covers the older scroll, so
  // the newer scroll's movement must not be attributed to the open segment.
  emitter_.ProcessTimeline(
      CreateTimeline(2, DamagingFrame{.presentation_ts = MillisecondsTicks(56)},
                     {Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(2, MillisecondsTicks(36), {kOtherScroller},
                                /*scroll_id=*/MillisecondsTicks(30))));

  emitter_.ProcessTimeline(
      CreateTimeline(3, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(40),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, InertialMovementExtendsEndTime) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  emitter_.ProcessTimeline(
      CreateTimeline(2, DamagingFrame{.presentation_ts = MillisecondsTicks(56)},
                     {Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(metrics_creator_.InertialGestureScrollUpdateBuilder()
                       .SetDelta(1.0f)
                       .SetDidScroll(true)
                       .SetCausedFrameUpdate(true)
                       .SetTimestamp(MillisecondsTicks(36))
                       .SetScrollJankV4ResultId(2)
                       .SetScrollBeginArrivalTimestamp(kScrollId)
                       .SetScrollBeginGeneratedTimestamp(kScrollBeginGenerated)
                       .AddAppliedScrollObservation(kScroller)
                       .Build()));

  emitter_.ProcessTimeline(
      CreateTimeline(3, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(56),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, NextScrollStartFinalizesPreviousSegment) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  // A new gesture starts without the previous gesture's end reaching a
  // presented frame.
  const base::TimeTicks next_scroll_id = MillisecondsTicks(50);
  emitter_.ProcessTimeline(
      CreateTimeline(
          2, DamagingFrame{.presentation_ts = MillisecondsTicks(80)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(next_scroll_id)}}),
      CreateEvents(CreateUpdate(2, MillisecondsTicks(60), {kOtherScroller},
                                next_scroll_id,
                                /*scroll_begin_generated=*/
                                MillisecondsTicks(45))));

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(40),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, GestureWithoutAppliedMovementProducesNoRecord) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), /*element_ids=*/{})));

  emitter_.ProcessTimeline(
      CreateTimeline(2, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(), IsEmpty());
}

TEST_F(ScrollTimingEmitterTest, UnsupportedInputTypesProduceNoRecord) {
  for (ui::ScrollInputType input_type :
       {ui::ScrollInputType::kScrollbar, ui::ScrollInputType::kAutoscroll}) {
    emitter_.ProcessTimeline(
        CreateTimeline(
            1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
            {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
        CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller},
                                  kScrollId, kScrollBeginGenerated,
                                  input_type)));

    emitter_.ProcessTimeline(
        CreateTimeline(2, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
        /*events_metrics=*/{});

    EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(), IsEmpty())
        << "input type " << static_cast<int>(input_type);
  }
}

TEST_F(ScrollTimingEmitterTest, NullScrollBeginTimestampProducesNoRecord) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller},
                                kScrollId,
                                /*scroll_begin_generated=*/base::TimeTicks())));

  emitter_.ProcessTimeline(
      CreateTimeline(2, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(), IsEmpty());
}

TEST_F(ScrollTimingEmitterTest,
       ScrollBeginTimestampAfterPresentationProducesNoRecord) {
  // The propagated scroll begin timestamp can be later than the presentation
  // timestamp, which would give the record a negative duration; see
  // `ScrollTimingEmitter::FlushActiveSegment()`.
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(
          CreateUpdate(1, MillisecondsTicks(20), {kScroller}, kScrollId,
                       /*scroll_begin_generated=*/MillisecondsTicks(100))));

  emitter_.ProcessTimeline(
      CreateTimeline(2, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(), IsEmpty());
}

TEST_F(ScrollTimingEmitterTest, EarliestObservationDefinesTheTarget) {
  // The event list is not required to be sorted, so the later update comes
  // first.
  EventMetrics::List events;
  events.push_back(CreateUpdate(1, MillisecondsTicks(30), {kOtherScroller}));
  events.push_back(CreateUpdate(1, MillisecondsTicks(20), {kScroller}));
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      events);

  emitter_.ProcessTimeline(
      CreateTimeline(2, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(40),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest,
       AnotherScrollInTheSameFrameDoesNotDefineTheTarget) {
  // One frame resolved updates from two gestures, which the calculator reports
  // as a single stage built for the lower scroll ID. The other gesture's update
  // moved earlier, so it would win the earliest-movement race if the stage's
  // scroll ID did not select the updates first.
  constexpr base::TimeTicks kNextScrollId = MillisecondsTicks(50);
  constexpr base::TimeTicks kNextScrollBeginGenerated = MillisecondsTicks(45);
  EventMetrics::List events;
  events.push_back(CreateUpdate(1, MillisecondsTicks(10), {kOtherScroller},
                                kNextScrollId, kNextScrollBeginGenerated));
  events.push_back(CreateUpdate(1, MillisecondsTicks(20), {kScroller}));
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(60)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      events);

  emitter_.ProcessTimeline(
      CreateTimeline(2, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(60),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, FirstObservationOnAnUpdateDefinesTheTarget) {
  // A single update moved two scrollers, e.g. a scroll which chained to an
  // ancestor. The observations share the update's input timestamp, so the
  // earliest movement is the one recorded first.
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(
          CreateUpdate(1, MillisecondsTicks(20), {kScroller, kOtherScroller})));

  emitter_.ProcessTimeline(
      CreateTimeline(2, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(40),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, FlushActiveSegmentFinalizesOpenSegment) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  emitter_.FlushActiveSegment();

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(40),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, FlushActiveSegmentIsIdempotent) {
  emitter_.FlushActiveSegment();
  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(), IsEmpty());

  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  emitter_.FlushActiveSegment();
  emitter_.FlushActiveSegment();
  emitter_.ProcessTimeline(
      CreateTimeline(2, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(), SizeIs(1));
}

TEST_F(ScrollTimingEmitterTest, MovementAfterAFlushDoesNotReopenTheGesture) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  emitter_.FlushActiveSegment();

  // The gesture keeps moving after the flush recorded it. Its updates still
  // carry the same propagated scroll begin timestamp, so a second segment would
  // report a record nested inside the first.
  emitter_.ProcessTimeline(
      CreateTimeline(2, DamagingFrame{.presentation_ts = MillisecondsTicks(72)},
                     {Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(2, MillisecondsTicks(56), {kScroller})));

  emitter_.ProcessTimeline(
      CreateTimeline(3, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(40),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, AFlushSuppressesOnlyTheRecordedGesture) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  emitter_.FlushActiveSegment();

  constexpr base::TimeTicks kNextScrollId = MillisecondsTicks(50);
  constexpr base::TimeTicks kNextScrollBeginGenerated = MillisecondsTicks(45);
  emitter_.ProcessTimeline(
      CreateTimeline(
          2, DamagingFrame{.presentation_ts = MillisecondsTicks(80)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kNextScrollId)}}),
      CreateEvents(CreateUpdate(2, MillisecondsTicks(60), {kOtherScroller},
                                kNextScrollId, kNextScrollBeginGenerated)));

  emitter_.ProcessTimeline(
      CreateTimeline(3, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(
                  ScrollTimingInfo{
                      .start_time = kScrollBeginGenerated,
                      .end_time = MillisecondsTicks(40),
                      .input_type = ui::ScrollInputType::kTouchscreen,
                      .element_id = kScroller,
                  },
                  ScrollTimingInfo{
                      .start_time = kNextScrollBeginGenerated,
                      .end_time = MillisecondsTicks(80),
                      .input_type = ui::ScrollInputType::kTouchscreen,
                      .element_id = kOtherScroller,
                  }));
}

TEST_F(ScrollTimingEmitterTest,
       AFlushWithoutARecordKeepsTheGestureAndItsTarget) {
  // The segment opens on observed movement, but no frame has presented that
  // movement yet, so the flush finalizes nothing.
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, NonDamagingFrame{},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  emitter_.FlushActiveSegment();
  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(), IsEmpty());

  // The gesture keeps moving and chains to another scroller. The record still
  // names the gesture's first moved scroller, not this frame's.
  emitter_.ProcessTimeline(
      CreateTimeline(2, DamagingFrame{.presentation_ts = MillisecondsTicks(72)},
                     {Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(2, MillisecondsTicks(56), {kOtherScroller})));

  emitter_.ProcessTimeline(
      CreateTimeline(3, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(72),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, AKeptSegmentDoesNotOutliveItsGesture) {
  // Nothing presented this gesture's movement, so the flush keeps its segment.
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, NonDamagingFrame{},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  emitter_.FlushActiveSegment();

  // The next gesture opens its own segment rather than extending the kept one.
  constexpr base::TimeTicks kNextScrollId = MillisecondsTicks(50);
  constexpr base::TimeTicks kNextScrollBeginGenerated = MillisecondsTicks(45);
  emitter_.ProcessTimeline(
      CreateTimeline(
          2, DamagingFrame{.presentation_ts = MillisecondsTicks(80)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kNextScrollId)}}),
      CreateEvents(CreateUpdate(2, MillisecondsTicks(60), {kOtherScroller},
                                kNextScrollId, kNextScrollBeginGenerated)));

  emitter_.ProcessTimeline(
      CreateTimeline(3, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kNextScrollBeginGenerated,
                  .end_time = MillisecondsTicks(80),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kOtherScroller,
              }));
}

TEST_F(ScrollTimingEmitterTest, TakeCompletedScrollTimingInfoDrains) {
  emitter_.ProcessTimeline(
      CreateTimeline(
          1, DamagingFrame{.presentation_ts = MillisecondsTicks(40)},
          {Stage{ScrollStart{}}, Stage{CreateUpdatesStage(kScrollId)}}),
      CreateEvents(CreateUpdate(1, MillisecondsTicks(20), {kScroller})));

  emitter_.ProcessTimeline(
      CreateTimeline(2, NonDamagingFrame{}, {Stage{ScrollEnd{}}}),
      /*events_metrics=*/{});

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(), SizeIs(1));
  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(), IsEmpty());
}

// Feeds the emitter a timeline produced by the real timeline calculator, which
// is what assigns the result IDs the emitter uses to attribute updates to
// frames.
TEST_F(ScrollTimingEmitterTest, ConsumesCalculatedTimeline) {
  ScrollJankV4FrameTimelineCalculator timeline_calculator;

  // A non-damaging update in one frame, followed by a damaging update which
  // moved content in the next frame. The two land in separate timeline frames.
  EventMetrics::List events;
  events.push_back(
      metrics_creator_.FirstGestureScrollUpdateBuilder()
          .SetDelta(1.0f)
          .SetDidScroll(false)
          .SetCausedFrameUpdate(false)
          .SetTimestamp(MillisecondsTicks(20))
          .SetDispatchArgs(CreateDispatchBeginFrameArgs(
              /*sequence_id=*/31, /*frame_time=*/MillisecondsTicks(24)))
          .SetScrollBeginArrivalTimestamp(kScrollId)
          .SetScrollBeginGeneratedTimestamp(kScrollBeginGenerated)
          .Build());
  events.push_back(metrics_creator_.GestureScrollUpdateBuilder()
                       .SetDelta(2.0f)
                       .SetDidScroll(true)
                       .SetCausedFrameUpdate(true)
                       .SetTimestamp(MillisecondsTicks(36))
                       .SetDispatchArgs(CreateDispatchBeginFrameArgs(
                           /*sequence_id=*/32,
                           /*frame_time=*/MillisecondsTicks(40)))
                       .SetScrollBeginArrivalTimestamp(kScrollId)
                       .SetScrollBeginGeneratedTimestamp(kScrollBeginGenerated)
                       .AddAppliedScrollObservation(kScroller)
                       .Build());

  // `CalculateTimeline()` stamps the result IDs into `events`, so it has to run
  // before `events` is handed to the emitter.
  const ScrollJankV4Frame::Timeline timeline =
      timeline_calculator.CalculateTimeline(
          events,
          CreateBeginFrameArgs(/*sequence_id=*/33,
                               /*frame_time=*/MillisecondsTicks(56)),
          /*presentation_ts=*/MillisecondsTicks(72));
  emitter_.ProcessTimeline(timeline, events);

  // The gesture ends without damaging a later frame.
  EventMetrics::List end_events;
  end_events.push_back(
      metrics_creator_.GestureScrollEndBuilder()
          .SetTimestamp(MillisecondsTicks(80))
          .SetCausedFrameUpdate(false)
          .SetDispatchArgs(CreateDispatchBeginFrameArgs(
              /*sequence_id=*/34, /*frame_time=*/MillisecondsTicks(88)))
          .SetScrollBeginArrivalTimestamp(kScrollId)
          .SetScrollBeginGeneratedTimestamp(kScrollBeginGenerated)
          .Build());
  const ScrollJankV4Frame::Timeline end_timeline =
      timeline_calculator.CalculateTimeline(
          end_events,
          CreateBeginFrameArgs(/*sequence_id=*/35,
                               /*frame_time=*/MillisecondsTicks(104)),
          /*presentation_ts=*/MillisecondsTicks(120));
  emitter_.ProcessTimeline(end_timeline, end_events);

  EXPECT_THAT(emitter_.TakeCompletedScrollTimingInfos(),
              ElementsAre(ScrollTimingInfo{
                  .start_time = kScrollBeginGenerated,
                  .end_time = MillisecondsTicks(72),
                  .input_type = ui::ScrollInputType::kTouchscreen,
                  .element_id = kScroller,
              }));
}

}  // namespace cc
