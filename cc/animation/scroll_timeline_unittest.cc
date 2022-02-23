// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_timeline.h"

#include <limits>
#include <vector>

#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"

namespace cc {

namespace {

// Only expect precision up to 1 microsecond for double conversion error (mainly
// due to timeline time getting converted between double and TimeTick).
static constexpr double time_error_ms = 0.001;

#define EXPECT_SCROLL_TIMELINE_TIME_NEAR(expected, value) \
  EXPECT_NEAR(expected, ToDouble(value), time_error_ms)

void SetScrollOffset(PropertyTrees* property_trees,
                     ElementId scroller_id,
                     gfx::PointF offset) {
  // Update both scroll and transform trees
  property_trees->scroll_tree_mutable().SetScrollOffset(scroller_id, offset);
  TransformNode* transform_node =
      property_trees->transform_tree_mutable().FindNodeFromElementId(
          scroller_id);
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
      property_trees->transform_tree_mutable().Insert(transform_node, 0);
  property_trees->transform_tree_mutable().SetElementIdForNodeId(
      transform_node_id, scroller_id);

  // Add the scrolling node for the scrolling and link it to the above transform
  // node.
  ScrollNode scroll_node;
  scroll_node.scrollable = true;
  scroll_node.bounds = content_size;
  scroll_node.container_bounds = container_size;
  scroll_node.element_id = scroller_id;
  scroll_node.transform_id = transform_node_id;
  int scroll_node_id =
      property_trees->scroll_tree_mutable().Insert(scroll_node, 0);

  property_trees->scroll_tree_mutable().SetElementIdForNodeId(scroll_node_id,
                                                              scroller_id);
}

// Helper method to calculate the current time, implementing only step 5 of
// https://wicg.github.io/scroll-animations/#current-time-algorithm
double CalculateCurrentTime(double current_scroll_offset,
                            double start_scroll_offset,
                            double end_scroll_offset) {
  return ((current_scroll_offset - start_scroll_offset) /
          (end_scroll_offset - start_scroll_offset)) *
         ScrollTimeline::kScrollTimelineDurationMs;
}

// Helper method to convert base::TimeTicks to double.
// Returns double milliseconds if the input value is resolved or
// std::numeric_limits<double>::quiet_NaN() otherwise.
double ToDouble(absl::optional<base::TimeTicks> time_ticks) {
  if (time_ticks)
    return (time_ticks.value() - base::TimeTicks()).InMillisecondsF();
  return std::numeric_limits<double>::quiet_NaN();
}

}  // namespace

class ScrollTimelineTest : public ::testing::Test,
                           public ProtectedSequenceSynchronizer {
 public:
  ScrollTimelineTest()
      : property_trees_(*this),
        scroller_id_(1),
        container_size_(100, 100),
        content_size_(500, 500) {
    // For simplicity we make the property_tree main thread; this avoids the
    // need to deal with the synced scroll offset code.
    property_trees_.set_is_main_thread(true);
    property_trees_.set_is_active(false);

    // Create a single scroller that is scrolling a 500x500 contents inside a
    // 100x100 container.
    CreateScrollingElement(&property_trees_, scroller_id_, content_size_,
                           container_size_);
  }

  PropertyTrees& property_trees() { return property_trees_; }

  ScrollTree& scroll_tree() { return property_trees_.scroll_tree_mutable(); }
  ElementId scroller_id() const { return scroller_id_; }
  gfx::Size container_size() const { return container_size_; }
  gfx::Size content_size() const { return content_size_; }

  // ProtectedSequenceSynchronizer implementation
  bool IsOwnerThread() const override { return true; }
  bool InProtectedSequence() const override { return false; }
  void WaitForProtectedSequenceCompletion() const override {}

 private:
  PropertyTrees property_trees_;
  ElementId scroller_id_;
  gfx::Size container_size_;
  gfx::Size content_size_;
};

TEST_F(ScrollTimelineTest, BasicCurrentTimeCalculations) {
  std::vector<double> scroll_offsets;
  scroll_offsets.push_back(0);
  scroll_offsets.push_back(100);

  scoped_refptr<ScrollTimeline> vertical_timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);
  scoped_refptr<ScrollTimeline> horizontal_timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollRight, scroll_offsets);

  // Unscrolled, both timelines should read a current time of 0.
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF());
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      0, vertical_timeline->CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      0, horizontal_timeline->CurrentTime(scroll_tree(), false));

  // Now do some scrolling and make sure that the ScrollTimelines update.
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(75, 50));

  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      0.5 * ScrollTimeline::kScrollTimelineDurationMs,
      vertical_timeline->CurrentTime(scroll_tree(), false));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      0.75 * ScrollTimeline::kScrollTimelineDurationMs,
      horizontal_timeline->CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, MultipleScrollOffsetsCurrentTimeCalculations) {
  double scroll_size =
      content_size().height() - container_size().height();  // 400

  std::vector<double> scroll_offsets;
  scroll_offsets.push_back(0);
  scroll_offsets.push_back(100.0);
  scroll_offsets.push_back(250.0);
  scroll_offsets.push_back(scroll_size);

  scoped_refptr<ScrollTimeline> vertical_timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);

  unsigned int offset = 0;
  double w = 1.0 / 3.0;  // offset weight
  double p = 0;          // progress within the offset

  // Scale necessary to convert absolute unit times to progress based values
  double scale = ScrollTimeline::kScrollTimelineDurationMs / scroll_size;

  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF());
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      (offset + p) * w * scroll_size * scale,
      vertical_timeline->CurrentTime(scroll_tree(), false));

  p = (70.0 - 0.0) / (100.0 - 0.0);
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 70));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      (offset + p) * w * scroll_size * scale,
      vertical_timeline->CurrentTime(scroll_tree(), false));

  offset = 1;
  p = 0;
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 100));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      (offset + p) * w * scroll_size * scale,
      vertical_timeline->CurrentTime(scroll_tree(), false));

  p = (150.0 - 100.0) / (250.0 - 100.0);
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 150));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      (offset + p) * w * scroll_size * scale,
      vertical_timeline->CurrentTime(scroll_tree(), false));

  offset = 2;
  p = 0;
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 250));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      (offset + p) * w * scroll_size * scale,
      vertical_timeline->CurrentTime(scroll_tree(), false));

  p = (350.0 - 250.0) / (400.0 - 250.0);
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 350));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      (offset + p) * w * scroll_size * scale,
      vertical_timeline->CurrentTime(scroll_tree(), false));

  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 400));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      ScrollTimeline::kScrollTimelineDurationMs,
      vertical_timeline->CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, OverlappingScrollOffsets) {
  double scroll_size = 100.0;

  // Start offset is greater than end offset ==> animation progress is
  // either 0% or 100%.
  std::vector<double> scroll_offsets = {350.0, 200.0, 50.0};

  scoped_refptr<ScrollTimeline> vertical_timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);

  // Offset is less than start offset ==> current time is 0.
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 300));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      0, vertical_timeline->CurrentTime(scroll_tree(), false));

  // Scale necessary to convert absolute unit times to progress based values
  double scale = ScrollTimeline::kScrollTimelineDurationMs / scroll_size;

  // Offset is greater than end offset ==> current time is 100%.
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 360));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      scroll_size * scale,
      vertical_timeline->CurrentTime(scroll_tree(), false));

  scroll_offsets = {0.0, 400.0, 200.0};

  vertical_timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);

  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 100));
  // Scroll offset is 25% of [0, 400) range, which maps to [0% 50%) of the
  // entire scroll range.
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      scroll_size * 0.5 * 0.25 * scale,
      vertical_timeline->CurrentTime(scroll_tree(), false));

  scroll_offsets = {200.0, 0.0, 400.0};

  vertical_timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);

  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 300));
  // Scroll offset is 75% of [0, 400) range, which maps to [50% 100%) of the
  // entire scroll range.
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      scroll_size * (0.5 + 0.5 * 0.75) * scale,
      vertical_timeline->CurrentTime(scroll_tree(), false));
}

// This test ensures that the ScrollTimeline's active scroller id is correct. We
// had a few crashes caused by assuming that the id would be available in the
// active tree before the activation happened; see http://crbug.com/853231
TEST_F(ScrollTimelineTest, ActiveTimeIsSetOnlyAfterPromotion) {
  PropertyTrees pending_tree(*this);
  PropertyTrees active_tree(*this);

  pending_tree.set_is_active(false);
  active_tree.set_is_active(true);

  // For simplicity we pretend the trees are main thread; this avoids the need
  // to deal with the synced scroll offset code.
  pending_tree.set_is_main_thread(true);
  active_tree.set_is_main_thread(true);

  // Initially only the pending tree has the scroll node.
  ElementId scroller_id(1);
  CreateScrollingElement(&pending_tree, scroller_id, content_size(),
                         container_size());

  double scroll_size = content_size().height() - container_size().height();
  std::vector<double> scroll_offsets;
  scroll_offsets.push_back(0);
  scroll_offsets.push_back(scroll_size);

  double halfwayY = scroll_size / 2.;
  double expectedTime = 0.5 * ScrollTimeline::kScrollTimelineDurationMs;
  SetScrollOffset(&pending_tree, scroller_id, gfx::PointF(0, halfwayY));

  scoped_refptr<ScrollTimeline> main_timeline = ScrollTimeline::Create(
      scroller_id, ScrollTimeline::ScrollDown, scroll_offsets);

  // Now create an impl version of the ScrollTimeline. Initially this should
  // only have a pending scroller id, as the active tree may not yet have the
  // scroller in it (as in this case).
  scoped_refptr<ScrollTimeline> impl_timeline = base::WrapRefCounted(
      ToScrollTimeline(main_timeline->CreateImplInstance().get()));

  EXPECT_TRUE(std::isnan(
      ToDouble(impl_timeline->CurrentTime(active_tree.scroll_tree(), true))));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      expectedTime,
      impl_timeline->CurrentTime(pending_tree.scroll_tree(), false));

  // Now fake a tree activation; this should cause the ScrollTimeline to update
  // its active scroller id. Note that we deliberately pass in the pending_tree
  // and just claim it is the active tree; this avoids needing to properly
  // implement tree swapping just for the test.
  impl_timeline->ActivateTimeline();
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      expectedTime,
      impl_timeline->CurrentTime(pending_tree.scroll_tree(), true));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      expectedTime,
      impl_timeline->CurrentTime(pending_tree.scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeIsAdjustedForPixelSnapping) {
  double scroll_size = content_size().height() - container_size().height();
  std::vector<double> scroll_offsets;
  scroll_offsets.push_back(0);
  scroll_offsets.push_back(scroll_size);
  scoped_refptr<ScrollTimeline> timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);

  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 50));

  // For simplicity emulate snapping by directly setting snap_amount of
  // transform node.
  TransformNode* transform_node =
      property_trees().transform_tree_mutable().FindNodeFromElementId(
          scroller_id());
  transform_node->snap_amount = gfx::Vector2dF(0, 0.5);

  // Scale necessary to convert absolute unit times to progress based values
  double scale = ScrollTimeline::kScrollTimelineDurationMs / scroll_size;

  EXPECT_SCROLL_TIMELINE_TIME_NEAR(49.5 * scale,
                                   timeline->CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesStartScrollOffset) {
  double scroll_size = content_size().height() - container_size().height();
  const double start_scroll_offset = 20;
  std::vector<double> scroll_offsets;
  scroll_offsets.push_back(start_scroll_offset);
  scroll_offsets.push_back(scroll_size);
  scoped_refptr<ScrollTimeline> timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);

  // Unscrolled, the timeline should read a current time of 0 since the current
  // offset (0) will be less than the startScrollOffset.
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF());
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(0,
                                   timeline->CurrentTime(scroll_tree(), false));

  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 19));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(0,
                                   timeline->CurrentTime(scroll_tree(), false));

  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 20));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(0,
                                   timeline->CurrentTime(scroll_tree(), false));

  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 50));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(50, start_scroll_offset, scroll_size),
      timeline->CurrentTime(scroll_tree(), false));

  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 200));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(200, start_scroll_offset, scroll_size),
      timeline->CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesEndScrollOffset) {
  double scroll_size = content_size().height() - container_size().height();
  const double end_scroll_offset = scroll_size - 20;
  std::vector<double> scroll_offsets;
  scroll_offsets.push_back(0);  // should be absl::nullopt
  scroll_offsets.push_back(end_scroll_offset);
  scoped_refptr<ScrollTimeline> timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);

  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::PointF(0, scroll_size));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(ScrollTimeline::kScrollTimelineDurationMs,
                                   timeline->CurrentTime(scroll_tree(), false));

  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::PointF(0, scroll_size - 20));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(ScrollTimeline::kScrollTimelineDurationMs,
                                   timeline->CurrentTime(scroll_tree(), false));

  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::PointF(0, scroll_size - 50));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(scroll_size - 50, 0, end_scroll_offset),
      timeline->CurrentTime(scroll_tree(), false));

  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::PointF(0, scroll_size - 200));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(scroll_size - 200, 0, end_scroll_offset),
      timeline->CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesCombinedStartAndEndScrollOffset) {
  double scroll_size = content_size().height() - container_size().height();
  double start_scroll_offset = 20;
  double end_scroll_offset = scroll_size - 50;
  std::vector<double> scroll_offsets;
  scroll_offsets.push_back(start_scroll_offset);
  scroll_offsets.push_back(end_scroll_offset);
  scoped_refptr<ScrollTimeline> timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::PointF(0, scroll_size - 150));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(
      CalculateCurrentTime(scroll_size - 150, start_scroll_offset,
                           end_scroll_offset),
      timeline->CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesEqualStartAndEndScrollOffset) {
  std::vector<double> scroll_offsets;
  scroll_offsets.push_back(20);
  scroll_offsets.push_back(20);
  scoped_refptr<ScrollTimeline> timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 150));

  EXPECT_SCROLL_TIMELINE_TIME_NEAR(ScrollTimeline::kScrollTimelineDurationMs,
                                   timeline->CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest,
       CurrentTimeHandlesStartOffsetLargerThanEndScrollOffset) {
  std::vector<double> scroll_offsets;
  scroll_offsets.push_back(50);
  scroll_offsets.push_back(10);
  scoped_refptr<ScrollTimeline> timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);
  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 40));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(0,
                                   timeline->CurrentTime(scroll_tree(), false));

  SetScrollOffset(&property_trees(), scroller_id(), gfx::PointF(0, 150));

  EXPECT_SCROLL_TIMELINE_TIME_NEAR(ScrollTimeline::kScrollTimelineDurationMs,
                                   timeline->CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, CurrentTimeHandlesScrollOffsets) {
  const double start_scroll_offset = 20;
  const double scroller_height =
      content_size().height() - container_size().height();
  const double end_scroll_offset = scroller_height - 20;
  std::vector<double> scroll_offsets;
  scroll_offsets.push_back(start_scroll_offset);
  scroll_offsets.push_back(end_scroll_offset);

  scoped_refptr<ScrollTimeline> timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);

  // Before the start_scroll_offset the current time should be 0
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::PointF(0, start_scroll_offset - 10));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(0,
                                   timeline->CurrentTime(scroll_tree(), false));

  // At the end_scroll_offset the current time should be 100%
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::PointF(0, end_scroll_offset));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(ScrollTimeline::kScrollTimelineDurationMs,
                                   timeline->CurrentTime(scroll_tree(), false));

  // After the end_scroll_offset the current time should be 100%
  SetScrollOffset(&property_trees(), scroller_id(),
                  gfx::PointF(0, end_scroll_offset + 10));
  EXPECT_SCROLL_TIMELINE_TIME_NEAR(ScrollTimeline::kScrollTimelineDurationMs,
                                   timeline->CurrentTime(scroll_tree(), false));
}

TEST_F(ScrollTimelineTest, Activeness) {
  // ScrollTimeline with zero scroller id is inactive.
  std::vector<double> scroll_offsets;
  double scroll_size = content_size().height() - container_size().height();
  scroll_offsets.push_back(0);
  scroll_offsets.push_back(scroll_size);
  scoped_refptr<ScrollTimeline> inactive_timeline1 = ScrollTimeline::Create(
      absl::nullopt, ScrollTimeline::ScrollDown, scroll_offsets);
  EXPECT_FALSE(
      inactive_timeline1->IsActive(scroll_tree(), false /*is_active_tree*/));
  EXPECT_FALSE(
      inactive_timeline1->IsActive(scroll_tree(), true /*is_active_tree*/));

  // ScrollTimeline with a scroller that is not in the scroll tree is
  // inactive.
  scoped_refptr<ScrollTimeline> inactive_timeline2 = ScrollTimeline::Create(
      ElementId(2), ScrollTimeline::ScrollDown, scroll_offsets);
  EXPECT_FALSE(
      inactive_timeline2->IsActive(scroll_tree(), false /*is_active_tree*/));
  // Activate the scroll tree.
  inactive_timeline2->ActivateTimeline();
  EXPECT_FALSE(
      inactive_timeline2->IsActive(scroll_tree(), true /*is_active_tree*/));

  // ScrollTimeline with empty scroll offsets is inactive.
  std::vector<double> empty_scroll_offsets;
  scoped_refptr<ScrollTimeline> inactive_timeline3 = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, empty_scroll_offsets);
  EXPECT_FALSE(
      inactive_timeline3->IsActive(scroll_tree(), false /*is_active_tree*/));
  EXPECT_FALSE(
      inactive_timeline3->IsActive(scroll_tree(), true /*is_active_tree*/));

  // Activate the scroll tree.
  inactive_timeline3->ActivateTimeline();
  EXPECT_FALSE(
      inactive_timeline3->IsActive(scroll_tree(), false /*is_active_tree*/));
  EXPECT_FALSE(
      inactive_timeline3->IsActive(scroll_tree(), true /*is_active_tree*/));

  scoped_refptr<ScrollTimeline> active_timeline = ScrollTimeline::Create(
      scroller_id(), ScrollTimeline::ScrollDown, scroll_offsets);
  EXPECT_TRUE(
      active_timeline->IsActive(scroll_tree(), false /*is_active_tree*/));
  EXPECT_FALSE(
      active_timeline->IsActive(scroll_tree(), true /*is_active_tree*/));

  // Activate the scroll tree.
  active_timeline->ActivateTimeline();
  EXPECT_TRUE(
      active_timeline->IsActive(scroll_tree(), false /*is_active_tree*/));
  EXPECT_TRUE(
      active_timeline->IsActive(scroll_tree(), true /*is_active_tree*/));
}

}  // namespace cc
