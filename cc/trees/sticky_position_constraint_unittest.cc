// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/sticky_position_constraint.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class StickyPositionConstraintTest : public ::testing::Test {};

TEST_F(StickyPositionConstraintTest, CanMergeHorizontal) {
  StickyPositionConstraint c1;
  c1.is_anchored_left = true;
  c1.left_offset = 10;
  c1.scroll_container_relative_sticky_box_rect = gfx::RectF(50, 50, 10, 10);
  c1.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  c1.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 200, 200);

  StickyPositionConstraint c2 = c1;

  // Identical constraints should be mergeable.
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 200, 500)));

  // Changing fields that don't affect the sticky offset should still be
  // mergeable.
  c2.top_offset = 20;
  c2.scroll_container_relative_sticky_box_rect.set_height(200);
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 200, 500)));

  // Changing any of the fields that affect the sticky offset should make them
  // only mergeable if the constraints can produce constant difference of sticky
  // offsets within the whole domain or a given scroll range.
  c2.left_offset = 20;
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 200, 500)));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(100, 0, 200, 500)));
  // Mergeable if both are always sticky in the scroll range.
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(40, 0, 100, 500)));
  // Or both are always constrained by the containing block in the scroll range.
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(200, 0, 200, 500)));
  c2.scroll_container_relative_containing_block_rect.set_width(210);
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 200, 500)));
  // Or they have the same transition points in the scroll range.
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(100, 0, 200, 500)));
}

TEST_F(StickyPositionConstraintTest, CanMergeVertical) {
  StickyPositionConstraint c1;
  c1.is_anchored_top = true;
  c1.top_offset = 10;
  c1.scroll_container_relative_sticky_box_rect = gfx::RectF(50, 50, 10, 10);
  c1.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  c1.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 200, 200);

  StickyPositionConstraint c2 = c1;

  // Identical constraints should be mergeable.
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 500, 200)));

  // Changing fields that don't affect the sticky offset should still be
  // mergeable.
  c2.left_offset = 20;
  c2.scroll_container_relative_sticky_box_rect.set_width(200);
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2, gfx::RectF(20, 0, 500, 200)));

  // Changing any of the fields that affect the sticky offset should make them
  // only mergeable if the constraints can produce constant difference of sticky
  // offsets within the whole domain or a given scroll range.
  c2.top_offset = 20;
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 500, 200)));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(0, 100, 500, 200)));
  // Mergeable if both are always sticky in the scroll range.
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(0, 40, 500, 100)));
  // Or both are always constrained by the containing block in the scroll range.
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(0, 200, 500, 200)));
  c2.scroll_container_relative_containing_block_rect.set_height(210);
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 500, 200)));
  // Or they have the same transition points in the scroll range.
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(0, 100, 500, 200)));
}

TEST_F(StickyPositionConstraintTest, CanMergeHorizontalAndVertical) {
  StickyPositionConstraint c1;
  c1.is_anchored_left = true;
  c1.left_offset = 10;
  c1.is_anchored_top = true;
  c1.top_offset = 20;
  c1.scroll_container_relative_sticky_box_rect = gfx::RectF(50, 50, 10, 10);
  c1.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  c1.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 200, 200);

  StickyPositionConstraint c2 = c1;

  // Identical constraints should be mergeable.
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 200, 200)));

  // Changing any of the fields that affect the sticky offset should make them
  // only mergeable if the constraints can produce constant difference of sticky
  // offsets within the whole domain or a given scroll range.
  c2.left_offset = 20;
  c2.top_offset = 10;
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 200, 200)));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(40, 100, 100, 200)));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(100, 40, 200, 100)));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(100, 100, 200, 200)));
  // Mergeable if both are always sticky in the scroll range.
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(40, 40, 100, 100)));
  // Or both are always constrained by the containing block in the scroll range.
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(200, 200, 200, 200)));
  // Or both are always sticky in one axis and always constrained in another.
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(200, 40, 200, 100)));
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(40, 200, 100, 200)));
  c2.scroll_container_relative_containing_block_rect.set_size(
      gfx::SizeF(210, 190));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 200, 200)));
  // Or they have the same transition points in the scroll range.
  EXPECT_EQ(
      StickyPositionConstraint::CanMergeResult::kCanMergeWithinScrollRange,
      c1.CanMerge(c2, gfx::RectF(100, 100, 200, 200)));
}

TEST_F(StickyPositionConstraintTest, CannotMergeWithDifferentScrollAncestors) {
  StickyPositionConstraint c1;
  c1.is_anchored_top = true;
  c1.top_offset = 10;
  c1.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  c1.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 100, 100);

  StickyPositionConstraint c2 = c1;
  c2.y_scroll_ancestor_element_id = ElementId(1);

  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 100, 100)));
}

TEST_F(StickyPositionConstraintTest, CanOrCannotMergeWithShiftingAncestors) {
  StickyPositionConstraint c1;
  c1.is_anchored_top = true;
  c1.top_offset = 10;
  c1.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  c1.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 100, 100);
  c1.nearest_element_shifting_containing_block = ElementId(1);

  StickyPositionConstraint c2 = c1;
  // Can merge if the constraints equal, regardless of the shifting ancestor.
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2));
  // Cannot merge otherwise.
  c2.top_offset = 20;
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 100, 100)));

  // Cannot merge if the shifting ancestors are different.
  c2 = c1;
  c2.nearest_element_shifting_sticky_box = ElementId(1);
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2));
  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCannotMerge,
            c1.CanMerge(c2, gfx::RectF(0, 0, 100, 100)));
}

TEST_F(StickyPositionConstraintTest, CanMergeWithEmptyTransitionRanges) {
  StickyPositionConstraint c1;
  c1.is_anchored_top = true;
  c1.top_offset = 10;
  c1.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  c1.scroll_container_relative_sticky_box_rect = gfx::RectF(50, 50, 100, 100);
  c1.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 100, 100);

  StickyPositionConstraint c2 = c1;
  c2.top_offset = 20;

  EXPECT_EQ(StickyPositionConstraint::CanMergeResult::kCanAlwaysMerge,
            c1.CanMerge(c2));
}

TEST_F(StickyPositionConstraintTest, StickyPositionOffsetLeft) {
  StickyPositionConstraint constraint;
  constraint.is_anchored_left = true;
  constraint.left_offset = 20;
  constraint.constraint_box_rect = gfx::RectF(0, 0, 300, 300);
  constraint.scroll_container_relative_sticky_box_rect =
      gfx::RectF(50, 0, 50, 100);
  constraint.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 130, 300);

  EXPECT_EQ(gfx::Vector2dF(0, 0), constraint.StickyPositionOffset(
                                      gfx::PointF(0, 0), gfx::Vector2dF(),
                                      gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, 0), constraint.StickyPositionOffset(
                                      gfx::PointF(30, 0), gfx::Vector2dF(),
                                      gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(10, 0), constraint.StickyPositionOffset(
                                       gfx::PointF(40, 0), gfx::Vector2dF(),
                                       gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(30, 0), constraint.StickyPositionOffset(
                                       gfx::PointF(60, 0), gfx::Vector2dF(),
                                       gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(30, 0), constraint.StickyPositionOffset(
                                       gfx::PointF(100, 0), gfx::Vector2dF(),
                                       gfx::Vector2dF(), gfx::Vector2dF()));
}

TEST_F(StickyPositionConstraintTest, StickyPositionOffsetRight) {
  StickyPositionConstraint constraint;
  constraint.is_anchored_right = true;
  constraint.right_offset = 20;
  constraint.constraint_box_rect = gfx::RectF(0, 0, 300, 300);
  constraint.scroll_container_relative_sticky_box_rect =
      gfx::RectF(330, 0, 50, 100);
  constraint.scroll_container_relative_containing_block_rect =
      gfx::RectF(300, 0, 130, 300);

  EXPECT_EQ(gfx::Vector2dF(-30, 0), constraint.StickyPositionOffset(
                                        gfx::PointF(0, 0), gfx::Vector2dF(),
                                        gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(-30, 0), constraint.StickyPositionOffset(
                                        gfx::PointF(70, 0), gfx::Vector2dF(),
                                        gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(-10, 0), constraint.StickyPositionOffset(
                                        gfx::PointF(90, 0), gfx::Vector2dF(),
                                        gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, 0), constraint.StickyPositionOffset(
                                      gfx::PointF(100, 0), gfx::Vector2dF(),
                                      gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, 0), constraint.StickyPositionOffset(
                                      gfx::PointF(150, 0), gfx::Vector2dF(),
                                      gfx::Vector2dF(), gfx::Vector2dF()));
}

TEST_F(StickyPositionConstraintTest, StickyPositionOffsetTop) {
  StickyPositionConstraint constraint;
  constraint.is_anchored_top = true;
  constraint.top_offset = 20;
  constraint.constraint_box_rect = gfx::RectF(0, 0, 300, 300);
  constraint.scroll_container_relative_sticky_box_rect =
      gfx::RectF(0, 50, 100, 50);
  constraint.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 300, 130);

  EXPECT_EQ(gfx::Vector2dF(0, 0), constraint.StickyPositionOffset(
                                      gfx::PointF(0, 0), gfx::Vector2dF(),
                                      gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, 0), constraint.StickyPositionOffset(
                                      gfx::PointF(0, 30), gfx::Vector2dF(),
                                      gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, 10), constraint.StickyPositionOffset(
                                       gfx::PointF(0, 40), gfx::Vector2dF(),
                                       gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, 30), constraint.StickyPositionOffset(
                                       gfx::PointF(0, 60), gfx::Vector2dF(),
                                       gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, 30), constraint.StickyPositionOffset(
                                       gfx::PointF(0, 100), gfx::Vector2dF(),
                                       gfx::Vector2dF(), gfx::Vector2dF()));
}

TEST_F(StickyPositionConstraintTest, StickyPositionOffsetBottom) {
  StickyPositionConstraint constraint;
  constraint.is_anchored_bottom = true;
  constraint.bottom_offset = 20;
  constraint.constraint_box_rect = gfx::RectF(0, 0, 300, 300);
  constraint.scroll_container_relative_sticky_box_rect =
      gfx::RectF(0, 330, 100, 50);
  constraint.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 300, 300, 130);

  EXPECT_EQ(gfx::Vector2dF(0, -30), constraint.StickyPositionOffset(
                                        gfx::PointF(0, 0), gfx::Vector2dF(),
                                        gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, -30), constraint.StickyPositionOffset(
                                        gfx::PointF(0, 70), gfx::Vector2dF(),
                                        gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, -10), constraint.StickyPositionOffset(
                                        gfx::PointF(0, 90), gfx::Vector2dF(),
                                        gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, 0), constraint.StickyPositionOffset(
                                      gfx::PointF(0, 100), gfx::Vector2dF(),
                                      gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, 0), constraint.StickyPositionOffset(
                                      gfx::PointF(0, 150), gfx::Vector2dF(),
                                      gfx::Vector2dF(), gfx::Vector2dF()));
}

TEST_F(StickyPositionConstraintTest, StickyPositionOffsetWithMultipleAnchors) {
  StickyPositionConstraint constraint;
  constraint.is_anchored_left = true;
  constraint.left_offset = 10;
  constraint.is_anchored_right = true;
  constraint.right_offset = 20;
  constraint.is_anchored_top = true;
  constraint.top_offset = 20;
  constraint.is_anchored_bottom = true;
  constraint.bottom_offset = 30;
  constraint.constraint_box_rect = gfx::RectF(0, 0, 300, 300);
  constraint.scroll_container_relative_sticky_box_rect =
      gfx::RectF(50, 50, 50, 50);
  constraint.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 130, 130);

  EXPECT_EQ(gfx::Vector2dF(0, 0), constraint.StickyPositionOffset(
                                      gfx::PointF(0, 0), gfx::Vector2dF(),
                                      gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(0, 0), constraint.StickyPositionOffset(
                                      gfx::PointF(40, 30), gfx::Vector2dF(),
                                      gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(10, 10), constraint.StickyPositionOffset(
                                        gfx::PointF(50, 40), gfx::Vector2dF(),
                                        gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(30, 30), constraint.StickyPositionOffset(
                                        gfx::PointF(70, 60), gfx::Vector2dF(),
                                        gfx::Vector2dF(), gfx::Vector2dF()));
  EXPECT_EQ(gfx::Vector2dF(30, 30), constraint.StickyPositionOffset(
                                        gfx::PointF(100, 100), gfx::Vector2dF(),
                                        gfx::Vector2dF(), gfx::Vector2dF()));
}

TEST_F(StickyPositionConstraintTest,
       StickyPositionOffsetWithExpansionAndAncestorOffsets) {
  StickyPositionConstraint constraint;
  constraint.is_anchored_left = true;
  constraint.left_offset = 10;
  constraint.is_anchored_top = true;
  constraint.top_offset = 15;
  constraint.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  constraint.scroll_container_relative_sticky_box_rect =
      gfx::RectF(20, 45, 40, 60);
  constraint.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 300, 300);

  gfx::Vector2dF constraint_box_expansion(20, 10);
  gfx::Vector2dF ancestor_sticky_box_offset(-5, 10);
  gfx::Vector2dF ancestor_containing_block_offset(15, -5);

  EXPECT_EQ(gfx::Vector2dF(10, 5),
            constraint.StickyPositionOffset(
                gfx::PointF(30, 40), constraint_box_expansion,
                ancestor_sticky_box_offset, ancestor_containing_block_offset));
}

}  // namespace
}  // namespace cc
