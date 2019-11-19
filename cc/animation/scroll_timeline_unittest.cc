// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_timeline.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {

namespace {

// Only expect precision up to 1 microsecond for double conversion error (mainly
// due to timeline time getting converted between double and TimeTick).
static constexpr double time_error_ms = 0.001;

#define EXPECT_SCROLL_TIMELINE_TIME_NEAR(expected, value) \
  EXPECT_NEAR(expected, ToDouble(value), time_error_ms)

void SetScrollOffset(PropertyTrees* property_trees,
                     ElementId scroller_id,
                     gfx::ScrollOffset offset) {
  // Update both scroll and transform trees
  property_trees->scroll_tree.SetScrollOffset(scroller_id, offset);
  TransformNode* transform_node =
      property_trees->transform_tree.FindNodeFromElementId(scroller_id);
  transform_node->scroll_offset = offset;
  transform_node->needs_local_transform_update = true;
}

void CreateScrollingElement(PropertyTrees* property_trees,
                            ElementId scroller_id,
                            gfx::Size content_size,
                            gfx::Size container_size) {
  // Create a corresponding TransformNode for the scrolling element.
  TransformNode transform_node;
  transform_node.scrolls = true;
  int transform_node_id =
      property_trees->transform_tree.Insert(transform_node, 0);
  property_trees->element_id_to_transform_node_index[scroller_id] =
      transform_node_id;

  // Add the scrolling node for the scrolling and link it to the above transform
  // node.
  ScrollNode scroll_node;
  scroll_node.scrollable = true;
  scroll_node.bounds = content_size;
  scroll_node.container_bounds = container_size;
  scroll_node.element_id = scroller_id;
  scroll_node.transform_id = transform_node_id;
  int scroll_node_id = property_trees->scroll_tree.Insert(scroll_node, 0);

  property_trees->element_id_to_scroll_node_index[scroller_id] = scroll_node_id;
}

// Helper method to calculate the current time, implementing only step 5 of
// https://wicg.github.io/scroll-animations/#current-time-algorithm
double CalculateCurrentTime(double current_scroll_offset,
                            double start_scroll_offset,
                            double end_scroll_offset,
                            double effective_time_range) {
  return ((current_scroll_offset - start_scroll_offset) /
          (end_scroll_offset - start_scroll_offset)) *
         effective_time_range;
}

// Helper method to convert base::TimeTicks to double.
// Returns double milliseconds if the input value is resolved or
// std::numeric_limits<double>::quiet_NaN() otherwise.
double ToDouble(base::Optional<base::TimeTicks> time_ticks) {
  if (time_ticks)
    return (time_ticks.value() - base::TimeTicks()).InMillisecondsF();
  return std::numeric_limits<double>::quiet_NaN();
}

}  // namespace

class ScrollTimelineTest : public ::testing::Test {
 public:
  ScrollTimelineTest()
      : scroller_id_(1), container_size_(100, 100), content_size_(500, 500) {
    // For simplicity we make the property_tree main thread; this avoids the
    // need to deal with the synced scroll offset code.
    property_trees_.is_main_thread = true;
    property_trees_.is_active = false;

    // Create a single scroller that is scrolling a 500x500 contents inside a
    // 100x100 container.
    CreateScrollingElement(&property_trees_, scroller_id_, content_size_,
                           container_size_);
  }

  PropertyTrees& property_trees() { return property_trees_; }

  ScrollTree& scroll_tree() { return property_trees_.scroll_tree; }
  ElementId scroller_id() const { return scroller_id_; }
  gfx::Size container_size() const { return container_size_; }
  gfx::Size content_size() const { return content_size_; }

 private:
  PropertyTrees property_trees_;
  ElementId scroller_id_;
  gfx::Size container_size_;
  gfx::Size content_size_;
};

TEST_F(ScrollTimelineTest, BasicCurrentTimeCalculations) {
  // For simplicity, we set the time range such that the current time maps
  // directly to the scroll offset. We have a square scroller/contents, so can
  // just compute one edge and use it for vertical/horizontal.
  double time_range = content_size().height() - container_size().height();

  ScrollTimeline vertical_timeline(scroller_id(), ScrollTimeline::ScrollDown,
                                   base::nullopt, base::nullopt, time_range,
                                   KeyframeModel::FillMode::NONE);
  ScrollTimeline horizontal_timeline(scroller_id(), ScrollTimeline::ScrollRight,
                                     base::nullopt, base::nullopt, time_range,
                                     KeyframeModel::FillMode::NONE);

  // Unscrolled, both timelines should read a current time of 0.
  SetScrollOffset(&property_trees(), scroller_id(), gfx::ScrollOffset());
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      0, vertical_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      0, horizontal_timeline.CurrentTime(scroll_tree(), false));

  // Now do some scrolling and make sure that the ScrollTimelines update.
  SetScrollOffset(&property_trees(), scroller_id(), gfx::ScrollOffset(75, 50));

  // As noted above, we have mapped the time range such that current time should
  // just be the scroll offset.
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      50, vertical_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      75, horizontal_timeline.CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeIsAdjustedForTimeRange) {
  // Here we set a time range to 100, which gives the current time the form of
  // 'percentage scrolled'.
  ScrollTimeline timeline(scroller_id(), ScrollTimeline::ScrollDown,
                          base::nullopt, base::nullopt, 100,
                          KeyframeModel::FillMode::NONE);

  double halfwayY = (content_size().height() - container_size().height()) / 2.;
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::ScrollOffset(0, halfwayY));

  EXPECT_SCROLL_TIMELINE_TIME_NEAR(50,
                                   timeline.CurrentTime(scroll_tree(), false));
}

// This test ensures that the ScrollTimeline's active scroller id is correct. We
// had a few crashes caused by assuming that the id would be available in the
// active tree before the activation happened; see http://crbug.com/853231
TEST_F(ScrollTimelineTest, ActiveTimeIsSetOnlyAfterPromotion) {
  PropertyTrees pending_tree;
  PropertyTrees active_tree;

  pending_tree.is_active = false;
  active_tree.is_active = true;

  // For simplicity we pretend the trees are main thread; this avoids the need
  // to deal with the synced scroll offset code.
  pending_tree.is_main_thread = true;
  active_tree.is_main_thread = true;

  // Initially only the pending tree has the scroll node.
  ElementId scroller_id(1);
  CreateScrollingElement(&pending_tree, scroller_id, content_size(),
                         container_size());

  double halfwayY = (content_size().height() - container_size().height()) / 2.;
  SetScrollOffset(&pending_tree, scroller_id, gfx::ScrollOffset(0, halfwayY));

  ScrollTimeline main_timeline(scroller_id, ScrollTimeline::ScrollDown,
                               base::nullopt, base::nullopt, 100,
                               KeyframeModel::FillMode::NONE);

  // Now create an impl version of the ScrollTimeline. Initially this should
  // only have a pending scroller id, as the active tree may not yet have the
  // scroller in it (as in this case).
  std::unique_ptr<ScrollTimeline> impl_timeline =
      main_timeline.CreateImplInstance();

  EXPECT_TRUE(std::isnan(
      ToDouble(impl_timeline->CurrentTime(active_tree.scroll_tree, true))));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      50, impl_timeline->CurrentTime(pending_tree.scroll_tree, false));

  // Now fake a tree activation; this should cause the ScrollTimeline to update
  // its active scroller id. Note that we deliberately pass in the pending_tree
  // and just claim it is the active tree; this avoids needing to properly
  // implement tree swapping just for the test.
  impl_timeline->PromoteScrollTimelinePendingToActive();
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      50, impl_timeline->CurrentTime(pending_tree.scroll_tree, true));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      50, impl_timeline->CurrentTime(pending_tree.scroll_tree, false));
}

TEST_F(ScrollTimelineTest, CurrentTimeIsAdjustedForPixelSnapping) {
  double time_range = content_size().height() - container_size().height();
  ScrollTimeline timeline(scroller_id(), ScrollTimeline::ScrollDown,
                          base::nullopt, base::nullopt, time_range,
                          KeyframeModel::FillMode::NONE);

  SetScrollOffset(&property_trees(), scroller_id(), gfx::ScrollOffset(0, 50));

  // For simplicity emulate snapping by directly setting snap_amount of
  // transform node.
  TransformNode* transform_node =
      property_trees().transform_tree.FindNodeFromElementId(scroller_id());
  transform_node->snap_amount = gfx::Vector2dF(0, 0.5);

  EXPECT_SCROLL_TIMELINE_TIME_NEAR(49.5,
                                   timeline.CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesStartScrollOffset) {
  double time_range = content_size().height() - container_size().height();
  const double start_scroll_offset = 20;
  ScrollTimeline timeline(scroller_id(), ScrollTimeline::ScrollDown,
                          start_scroll_offset, base::nullopt, time_range,
                          KeyframeModel::FillMode::NONE);

  // Unscrolled, the timeline should read a current time of unresolved, since
  // the current offset (0) will be less than the startScrollOffset.
  SetScrollOffset(&property_trees(), scroller_id(), gfx::ScrollOffset());
  EXPECT_TRUE(std::isnan(ToDouble(timeline.CurrentTime(scroll_tree(), false))));

  SetScrollOffset(&property_trees(), scroller_id(), gfx::ScrollOffset(0, 19));
  EXPECT_TRUE(std::isnan(ToDouble(timeline.CurrentTime(scroll_tree(), false))));
  SetScrollOffset(&property_trees(), scroller_id(), gfx::ScrollOffset(0, 20));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(0,
                                   timeline.CurrentTime(scroll_tree(), false));
  SetScrollOffset(&property_trees(), scroller_id(), gfx::ScrollOffset(0, 50));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(50, start_scroll_offset, time_range, time_range),
      timeline.CurrentTime(scroll_tree(), false));
  SetScrollOffset(&property_trees(), scroller_id(), gfx::ScrollOffset(0, 200));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(200, start_scroll_offset, time_range, time_range),
      timeline.CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesEndScrollOffset) {
  double time_range = content_size().height() - container_size().height();
  const double end_scroll_offset = time_range - 20;
  ScrollTimeline timeline(scroller_id(), ScrollTimeline::ScrollDown,
                          base::nullopt, end_scroll_offset, time_range,
                          KeyframeModel::FillMode::NONE);

  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::ScrollOffset(0, time_range));
  EXPECT_TRUE(std::isnan(ToDouble(timeline.CurrentTime(scroll_tree(), false))));
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::ScrollOffset(0, time_range - 20));
  EXPECT_TRUE(std::isnan(ToDouble(timeline.CurrentTime(scroll_tree(), false))));
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::ScrollOffset(0, time_range - 50));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(time_range - 50, 0, end_scroll_offset, time_range),
      timeline.CurrentTime(scroll_tree(), false));
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::ScrollOffset(0, time_range - 200));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(time_range - 200, 0, end_scroll_offset, time_range),
      timeline.CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesEndScrollOffsetInclusive) {
  double time_range = 100;
  const double end_scroll_offset =
      content_size().height() - container_size().height();
  ScrollTimeline timeline(scroller_id(), ScrollTimeline::ScrollDown,
                          base::nullopt, end_scroll_offset, time_range,
                          KeyframeModel::FillMode::NONE);

  const double current_offset = end_scroll_offset;
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::ScrollOffset(0, current_offset));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(current_offset, 0, end_scroll_offset, time_range),
      timeline.CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesCombinedStartAndEndScrollOffset) {
  double time_range = content_size().height() - container_size().height();
  double start_scroll_offset = 20;
  double end_scroll_offset = time_range - 50;
  ScrollTimeline timeline(scroller_id(), ScrollTimeline::ScrollDown,
                          start_scroll_offset, end_scroll_offset, time_range,
                          KeyframeModel::FillMode::NONE);
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::ScrollOffset(0, time_range - 150));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(time_range - 150, start_scroll_offset,
                           end_scroll_offset, time_range),
      timeline.CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesEqualStartAndEndScrollOffset) {
  double time_range = content_size().height() - container_size().height();
  ScrollTimeline timeline(scroller_id(), ScrollTimeline::ScrollDown, 20, 20,
                          time_range, KeyframeModel::FillMode::NONE);
  SetScrollOffset(&property_trees(), scroller_id(), gfx::ScrollOffset(0, 150));
  EXPECT_TRUE(std::isnan(ToDouble(timeline.CurrentTime(scroll_tree(), false))));
}

TEST_F(ScrollTimelineTest,
       CurrentTimeHandlesStartOffsetLargerThanEndScrollOffset) {
  double time_range = content_size().height() - container_size().height();
  ScrollTimeline timeline(scroller_id(), ScrollTimeline::ScrollDown, 50, 10,
                          time_range, KeyframeModel::FillMode::NONE);
  SetScrollOffset(&property_trees(), scroller_id(), gfx::ScrollOffset(0, 150));
  EXPECT_TRUE(std::isnan(ToDouble(timeline.CurrentTime(scroll_tree(), false))));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesFillMode) {
  const double time_range = 100;
  const double start_scroll_offset = 20;
  const double scroller_height =
      content_size().height() - container_size().height();
  const double end_scroll_offset = scroller_height - 20;

  ScrollTimeline fill_forwards_timeline(
      scroller_id(), ScrollTimeline::ScrollDown, start_scroll_offset,
      end_scroll_offset, time_range, KeyframeModel::FillMode::FORWARDS);
  ScrollTimeline fill_backwards_timeline(
      scroller_id(), ScrollTimeline::ScrollDown, start_scroll_offset,
      end_scroll_offset, time_range, KeyframeModel::FillMode::BACKWARDS);
  ScrollTimeline fill_both_timeline(scroller_id(), ScrollTimeline::ScrollDown,
                                    start_scroll_offset, end_scroll_offset,
                                    time_range, KeyframeModel::FillMode::BOTH);
  // AUTO should be equivalent to BOTH.
  ScrollTimeline fill_auto_timeline(scroller_id(), ScrollTimeline::ScrollDown,
                                    start_scroll_offset, end_scroll_offset,
                                    time_range, KeyframeModel::FillMode::AUTO);

  // Before the start_scroll_offset the current time should be 0 for backwards
  // or both, and unresolved otherwise.
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::ScrollOffset(0, start_scroll_offset - 10));
  EXPECT_FALSE(fill_forwards_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      0, fill_backwards_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      0, fill_both_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      0, fill_auto_timeline.CurrentTime(scroll_tree(), false));

  // At the end_scroll_offset the current time should be time-range for
  // forwards or both, and unresolved otherwise.
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::ScrollOffset(0, end_scroll_offset));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      time_range, fill_forwards_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_FALSE(fill_backwards_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      time_range, fill_both_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      time_range, fill_auto_timeline.CurrentTime(scroll_tree(), false));

  // After the end_scroll_offset the current time should be time-range for
  // forwards or both, and unresolved otherwise.
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::ScrollOffset(0, end_scroll_offset + 10));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      time_range, fill_forwards_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_FALSE(fill_backwards_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      time_range, fill_both_timeline.CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      time_range, fill_auto_timeline.CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, Activeness) {
  // ScrollTimeline with zero scroller id is inactive.
  ScrollTimeline inactive_timeline1(base::nullopt, ScrollTimeline::ScrollDown,
                                    base::nullopt, base::nullopt, 100,
                                    KeyframeModel::FillMode::NONE);
  EXPECT_FALSE(
      inactive_timeline1.IsActive(scroll_tree(), false /*is_active_tree*/));
  EXPECT_FALSE(
      inactive_timeline1.IsActive(scroll_tree(), true /*is_active_tree*/));

  // ScrollTimeline with a scroller that is not in the scroll tree is
  // inactive.
  ScrollTimeline inactive_timeline2(ElementId(2), ScrollTimeline::ScrollDown,
                                    base::nullopt, base::nullopt, 100,
                                    KeyframeModel::FillMode::NONE);
  EXPECT_FALSE(
      inactive_timeline2.IsActive(scroll_tree(), false /*is_active_tree*/));
  // Activate the scroll tree.
  inactive_timeline2.PromoteScrollTimelinePendingToActive();
  EXPECT_FALSE(
      inactive_timeline2.IsActive(scroll_tree(), true /*is_active_tree*/));

  ScrollTimeline active_timeline(scroller_id(), ScrollTimeline::ScrollDown,
                                 base::nullopt, base::nullopt, 100,
                                 KeyframeModel::FillMode::NONE);
  EXPECT_TRUE(
      active_timeline.IsActive(scroll_tree(), false /*is_active_tree*/));
  EXPECT_FALSE(
      active_timeline.IsActive(scroll_tree(), true /*is_active_tree*/));

  // Activate the scroll tree.
  active_timeline.PromoteScrollTimelinePendingToActive();
  EXPECT_TRUE(
      active_timeline.IsActive(scroll_tree(), false /*is_active_tree*/));
  EXPECT_TRUE(active_timeline.IsActive(scroll_tree(), true /*is_active_tree*/));
}

}  // namespace cc
