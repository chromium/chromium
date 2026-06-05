// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_timing_controller.h"

#include "cc/input/scroll_timing_info.h"
#include "cc/paint/element_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {
namespace {

constexpr ElementId kScrollerId(7);

class ScrollTimingControllerTest : public testing::Test {
 protected:
  ScrollTimingController controller_;
};

// A touchscreen gesture with a non-null hardware timestamp that latches a
// scroller produces exactly one ScrollTimingInfo with the latched element
// id, the original input type, the hardware start_time, and a non-decreasing
// end_time.
TEST_F(ScrollTimingControllerTest, TouchScrollEmitsRecord) {
  const base::TimeTicks event_timestamp =
      base::TimeTicks::Now() - base::Milliseconds(7);

  controller_.DidScrollBegin(ui::ScrollInputType::kTouchscreen,
                             event_timestamp);
  controller_.DidScrollUpdate(kScrollerId);
  controller_.DidScrollEnd(ui::ScrollInputType::kTouchscreen);

  const std::vector<ScrollTimingInfo> infos =
      controller_.TakeCompletedScrollTimingInfos();
  ASSERT_EQ(1u, infos.size());
  EXPECT_EQ(kScrollerId, infos.front().element_id);
  EXPECT_EQ(ui::ScrollInputType::kTouchscreen, infos.front().input_type);
  EXPECT_EQ(event_timestamp, infos.front().start_time);
  ASSERT_TRUE(infos.front().end_time.has_value());
  EXPECT_GE(*infos.front().end_time, infos.front().start_time);
}

// Wheel scrolls are also within scope of the API.
TEST_F(ScrollTimingControllerTest, WheelScrollEmitsRecord) {
  controller_.DidScrollBegin(ui::ScrollInputType::kWheel,
                             base::TimeTicks::Now());
  controller_.DidScrollUpdate(kScrollerId);
  controller_.DidScrollEnd(ui::ScrollInputType::kWheel);

  const std::vector<ScrollTimingInfo> infos =
      controller_.TakeCompletedScrollTimingInfos();
  ASSERT_EQ(1u, infos.size());
  EXPECT_EQ(ui::ScrollInputType::kWheel, infos.front().input_type);
}

// When DidScrollBegin is never called (the InputHandlerProxy's signal that
// the runtime feature is off), updates and ends are no-ops and no record is
// produced.
TEST_F(ScrollTimingControllerTest, NoBeginNoRecord) {
  controller_.DidScrollUpdate(kScrollerId);
  controller_.DidScrollEnd(ui::ScrollInputType::kWheel);

  EXPECT_TRUE(controller_.TakeCompletedScrollTimingInfos().empty());
}

// A gesture that begins but never gets an effective update (no latched
// element id) is dropped on end.
TEST_F(ScrollTimingControllerTest, NoUpdateNoRecord) {
  controller_.DidScrollBegin(ui::ScrollInputType::kTouchscreen,
                             base::TimeTicks::Now());
  controller_.DidScrollEnd(ui::ScrollInputType::kTouchscreen);

  EXPECT_TRUE(controller_.TakeCompletedScrollTimingInfos().empty());
}

// Input types outside the API's scope (e.g. scrollbar, autoscroll) are
// silently ignored at DidScrollBegin so no tracking starts and no record
// is produced.
TEST_F(ScrollTimingControllerTest, ScrollbarInputTypeNotTracked) {
  controller_.DidScrollBegin(ui::ScrollInputType::kScrollbar,
                             base::TimeTicks::Now());
  EXPECT_FALSE(controller_.ActiveScrollStartForTesting().has_value());

  controller_.DidScrollUpdate(kScrollerId);
  controller_.DidScrollEnd(ui::ScrollInputType::kScrollbar);

  EXPECT_TRUE(controller_.TakeCompletedScrollTimingInfos().empty());
}

// A wrong-device GestureScrollEnd that races a successful gesture must not
// finalize the in-flight record, and also drops the (now-suspect) in-flight
// tracking so a stale entry can't be emitted later.
TEST_F(ScrollTimingControllerTest, InputTypeMismatchOnEndDropsEnd) {
  controller_.DidScrollBegin(ui::ScrollInputType::kTouchscreen,
                             base::TimeTicks::Now());
  controller_.DidScrollUpdate(kScrollerId);

  controller_.DidScrollEnd(ui::ScrollInputType::kWheel);

  EXPECT_TRUE(controller_.TakeCompletedScrollTimingInfos().empty());
  EXPECT_FALSE(controller_.ActiveScrollStartForTesting().has_value());
}

// A null hardware timestamp at DidScrollBegin (e.g. synthetic/legacy callers)
// is silently dropped so no entry with an incomparable startTime is emitted.
TEST_F(ScrollTimingControllerTest, NullTimestampNotTracked) {
  controller_.DidScrollBegin(ui::ScrollInputType::kTouchscreen,
                             base::TimeTicks());
  EXPECT_FALSE(controller_.ActiveScrollStartForTesting().has_value());

  controller_.DidScrollUpdate(kScrollerId);
  controller_.DidScrollEnd(ui::ScrollInputType::kTouchscreen);

  EXPECT_TRUE(controller_.TakeCompletedScrollTimingInfos().empty());
}

// A second DidScrollBegin without an intervening DidScrollEnd overwrites the
// in-flight gesture. The resulting record reflects the second begin's
// start_time and the element latched after the second begin.
TEST_F(ScrollTimingControllerTest, SecondBeginOverwritesInFlightGesture) {
  constexpr ElementId kFirstScrollerId(11);
  constexpr ElementId kSecondScrollerId(22);
  const base::TimeTicks first_begin =
      base::TimeTicks::Now() - base::Milliseconds(20);
  const base::TimeTicks second_begin =
      base::TimeTicks::Now() - base::Milliseconds(5);

  controller_.DidScrollBegin(ui::ScrollInputType::kTouchscreen, first_begin);
  controller_.DidScrollUpdate(kFirstScrollerId);

  // No DidScrollEnd before the next begin.
  controller_.DidScrollBegin(ui::ScrollInputType::kTouchscreen, second_begin);
  controller_.DidScrollUpdate(kSecondScrollerId);
  controller_.DidScrollEnd(ui::ScrollInputType::kTouchscreen);

  const std::vector<ScrollTimingInfo> infos =
      controller_.TakeCompletedScrollTimingInfos();
  ASSERT_EQ(1u, infos.size());
  EXPECT_EQ(second_begin, infos.front().start_time);
  EXPECT_EQ(kSecondScrollerId, infos.front().element_id);
}

// TakeCompletedScrollTimingInfos drains the batch: a subsequent call before
// any further activity returns an empty vector.
TEST_F(ScrollTimingControllerTest, TakeDrainsCompletedRecords) {
  controller_.DidScrollBegin(ui::ScrollInputType::kTouchscreen,
                             base::TimeTicks::Now());
  controller_.DidScrollUpdate(kScrollerId);
  controller_.DidScrollEnd(ui::ScrollInputType::kTouchscreen);

  EXPECT_EQ(1u, controller_.TakeCompletedScrollTimingInfos().size());
  EXPECT_TRUE(controller_.TakeCompletedScrollTimingInfos().empty());
}

}  // namespace
}  // namespace cc
