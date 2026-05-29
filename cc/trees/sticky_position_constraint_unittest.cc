// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/sticky_position_constraint.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class StickyPositionConstraintTest : public ::testing::Test {};

TEST_F(StickyPositionConstraintTest, CanMergeHorizontal) {
  StickyPositionConstraint constraint1;
  constraint1.is_anchored_left = true;
  constraint1.left_offset = 10;
  constraint1.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  constraint1.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 100, 100);

  StickyPositionConstraint constraint2 = constraint1;

  // Identical constraints should be mergeable.
  EXPECT_TRUE(constraint1.CanMerge(constraint2));

  // Changing any of the fields that affect the sticky offset should make them
  // not mergeable.
  constraint2.left_offset = 20;
  EXPECT_FALSE(constraint1.CanMerge(constraint2));

  // Changing fields that don't affect the sticky offset should still be
  // mergeable.
  constraint2 = constraint1;
  constraint2.top_offset = 20;
  constraint2.scroll_container_relative_sticky_box_rect.set_height(200);
  EXPECT_TRUE(constraint1.CanMerge(constraint2));
}

TEST_F(StickyPositionConstraintTest, CanMergeVertical) {
  StickyPositionConstraint constraint1;
  constraint1.is_anchored_top = true;
  constraint1.top_offset = 10;
  constraint1.constraint_box_rect = gfx::RectF(0, 0, 100, 100);
  constraint1.scroll_container_relative_containing_block_rect =
      gfx::RectF(0, 0, 100, 100);

  StickyPositionConstraint constraint2 = constraint1;

  // Identical constraints should be mergeable.
  EXPECT_TRUE(constraint1.CanMerge(constraint2));

  // Changing any of the fields that affect the sticky offset should make them
  // not mergeable.
  constraint2.top_offset = 20;
  EXPECT_FALSE(constraint1.CanMerge(constraint2));

  // Changing fields that don't affect the sticky offset should still be
  // mergeable.
  constraint2 = constraint1;
  constraint2.left_offset = 20;
  constraint2.scroll_container_relative_sticky_box_rect.set_width(200);
  EXPECT_TRUE(constraint1.CanMerge(constraint2));
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
