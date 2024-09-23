// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/trees/occlusion.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(OcclusionTest, HasOcclusion) {
  Occlusion empty;
  EXPECT_FALSE(empty.HasOcclusion());

  empty = Occlusion(
      gfx::Transform(), SimpleEnclosedRegion(), SimpleEnclosedRegion());
  EXPECT_FALSE(empty.HasOcclusion());

  Occlusion outside_nonempty(
      gfx::Transform(), SimpleEnclosedRegion(10, 10), SimpleEnclosedRegion());
  EXPECT_TRUE(outside_nonempty.HasOcclusion());

  Occlusion inside_nonempty(
      gfx::Transform(), SimpleEnclosedRegion(), SimpleEnclosedRegion(10, 10));
  EXPECT_TRUE(inside_nonempty.HasOcclusion());

  Occlusion both_nonempty(gfx::Transform(),
                          SimpleEnclosedRegion(10, 10),
                          SimpleEnclosedRegion(10, 10));
  EXPECT_TRUE(both_nonempty.HasOcclusion());
}

#define EXPECT_OCCLUSION(occlusion, rects, ...)              \
  {                                                          \
    bool expected[] = {__VA_ARGS__};                         \
    ASSERT_EQ(std::size(rects), std::size(expected));        \
    for (size_t i = 0; i < std::size(rects); ++i)            \
      EXPECT_EQ(expected[i], occlusion.IsOccluded(rects[i])) \
          << "Test failed for index " << i << ".";           \
  }

TEST(OcclusionTest, IsOccludedNoTransform) {
  gfx::Rect rects[] = {gfx::Rect(10, 10),
                       gfx::Rect(10, 0, 10, 10),
                       gfx::Rect(0, 10, 10, 10),
                       gfx::Rect(10, 10, 10, 10)};

  Occlusion no_occlusion;
  EXPECT_OCCLUSION(no_occlusion, rects, false, false, false, false);

  Occlusion all_occluded_outside(
      gfx::Transform(), SimpleEnclosedRegion(20, 20), SimpleEnclosedRegion());
  EXPECT_OCCLUSION(all_occluded_outside, rects, true, true, true, true);

  Occlusion all_occluded_inside(
      gfx::Transform(), SimpleEnclosedRegion(), SimpleEnclosedRegion(20, 20));
  EXPECT_OCCLUSION(all_occluded_inside, rects, true, true, true, true);

  Occlusion all_occluded_mixed(gfx::Transform(),
                               SimpleEnclosedRegion(10, 20),
                               SimpleEnclosedRegion(10, 0, 10, 20));
  EXPECT_OCCLUSION(all_occluded_mixed, rects, true, true, true, true);

  Occlusion some_occluded(gfx::Transform(),
                          SimpleEnclosedRegion(10, 10),
                          SimpleEnclosedRegion(10, 10, 10, 10));
  EXPECT_OCCLUSION(some_occluded, rects, true, false, false, true);
}

TEST(OcclusionTest, IsOccludedScaled) {
  gfx::Rect rects[] = {gfx::Rect(10, 10),
                       gfx::Rect(10, 0, 10, 10),
                       gfx::Rect(0, 10, 10, 10),
                       gfx::Rect(10, 10, 10, 10)};

  gfx::Transform half_scale;
  half_scale.Scale(0.5, 0.5);

  gfx::Transform double_scale;
  double_scale.Scale(2, 2);

  Occlusion all_occluded_outside_half(
      half_scale, SimpleEnclosedRegion(10, 10), SimpleEnclosedRegion());
  Occlusion all_occluded_outside_double(
      double_scale, SimpleEnclosedRegion(40, 40), SimpleEnclosedRegion());
  EXPECT_OCCLUSION(all_occluded_outside_half, rects, true, true, true, true);
  EXPECT_OCCLUSION(all_occluded_outside_double, rects, true, true, true, true);

  Occlusion all_occluded_inside_half(
      half_scale, SimpleEnclosedRegion(), SimpleEnclosedRegion(10, 10));
  Occlusion all_occluded_inside_double(
      double_scale, SimpleEnclosedRegion(), SimpleEnclosedRegion(40, 40));
  EXPECT_OCCLUSION(all_occluded_inside_half, rects, true, true, true, true);
  EXPECT_OCCLUSION(all_occluded_inside_double, rects, true, true, true, true);

  Occlusion all_occluded_mixed_half(half_scale,
                                    SimpleEnclosedRegion(5, 10),
                                    SimpleEnclosedRegion(5, 0, 5, 10));
  Occlusion all_occluded_mixed_double(double_scale,
                                      SimpleEnclosedRegion(20, 40),
                                      SimpleEnclosedRegion(20, 0, 20, 40));
  EXPECT_OCCLUSION(all_occluded_mixed_half, rects, true, true, true, true);
  EXPECT_OCCLUSION(all_occluded_mixed_double, rects, true, true, true, true);

  Occlusion some_occluded_half(
      half_scale, SimpleEnclosedRegion(5, 5), SimpleEnclosedRegion(5, 5, 5, 5));
  Occlusion some_occluded_double(double_scale,
                                 SimpleEnclosedRegion(20, 20),
                                 SimpleEnclosedRegion(20, 20, 20, 20));
  EXPECT_OCCLUSION(some_occluded_half, rects, true, false, false, true);
  EXPECT_OCCLUSION(some_occluded_double, rects, true, false, false, true);
}

TEST(OcclusionTest, IsOccludedTranslated) {
  gfx::Rect rects[] = {gfx::Rect(10, 10),
                       gfx::Rect(10, 0, 10, 10),
                       gfx::Rect(0, 10, 10, 10),
                       gfx::Rect(10, 10, 10, 10)};

  gfx::Transform move_left;
  move_left.Translate(-100, 0);

  gfx::Transform move_down;
  move_down.Translate(0, 100);

  Occlusion all_occluded_outside_left(
      move_left, SimpleEnclosedRegion(-100, 0, 20, 20), SimpleEnclosedRegion());
  Occlusion all_occluded_outside_down(
      move_down, SimpleEnclosedRegion(0, 100, 20, 20), SimpleEnclosedRegion());
  EXPECT_OCCLUSION(all_occluded_outside_left, rects, true, true, true, true);
  EXPECT_OCCLUSION(all_occluded_outside_down, rects, true, true, true, true);

  Occlusion all_occluded_inside_left(
      move_left, SimpleEnclosedRegion(), SimpleEnclosedRegion(-100, 0, 20, 20));
  Occlusion all_occluded_inside_down(
      move_down, SimpleEnclosedRegion(), SimpleEnclosedRegion(0, 100, 20, 20));
  EXPECT_OCCLUSION(all_occluded_inside_left, rects, true, true, true, true);
  EXPECT_OCCLUSION(all_occluded_inside_down, rects, true, true, true, true);

  Occlusion all_occluded_mixed_left(move_left,
                                    SimpleEnclosedRegion(-100, 0, 10, 20),
                                    SimpleEnclosedRegion(-90, 0, 10, 20));
  Occlusion all_occluded_mixed_down(move_down,
                                    SimpleEnclosedRegion(0, 100, 10, 20),
                                    SimpleEnclosedRegion(10, 100, 10, 20));
  EXPECT_OCCLUSION(all_occluded_mixed_left, rects, true, true, true, true);
  EXPECT_OCCLUSION(all_occluded_mixed_down, rects, true, true, true, true);

  Occlusion some_occluded_left(move_left,
                               SimpleEnclosedRegion(-100, 0, 10, 10),
                               SimpleEnclosedRegion(-90, 10, 10, 10));
  Occlusion some_occluded_down(move_down,
                               SimpleEnclosedRegion(0, 100, 10, 10),
                               SimpleEnclosedRegion(10, 110, 10, 10));
  EXPECT_OCCLUSION(some_occluded_left, rects, true, false, false, true);
  EXPECT_OCCLUSION(some_occluded_down, rects, true, false, false, true);
}

TEST(OcclusionTest, IsOccludedScaledAfterConstruction) {
  gfx::Rect rects[] = {gfx::Rect(10, 10),
                       gfx::Rect(10, 0, 10, 10),
                       gfx::Rect(0, 10, 10, 10),
                       gfx::Rect(10, 10, 10, 10)};

  gfx::Transform half_transform;
  half_transform.Scale(0.5, 0.5);

  gfx::Transform double_transform;
  double_transform.Scale(2, 2);

  Occlusion all_occluded_outside(
      gfx::Transform(), SimpleEnclosedRegion(10, 10), SimpleEnclosedRegion());
  Occlusion all_occluded_outside_half =
      all_occluded_outside.GetOcclusionWithGivenDrawTransform(half_transform);

  all_occluded_outside = Occlusion(
      gfx::Transform(), SimpleEnclosedRegion(40, 40), SimpleEnclosedRegion());
  Occlusion all_occluded_outside_double =
      all_occluded_outside.GetOcclusionWithGivenDrawTransform(double_transform);

  EXPECT_OCCLUSION(all_occluded_outside_half, rects, true, true, true, true);
  EXPECT_OCCLUSION(all_occluded_outside_double, rects, true, true, true, true);

  Occlusion some_occluded(gfx::Transform(),
                          SimpleEnclosedRegion(5, 5),
                          SimpleEnclosedRegion(5, 5, 5, 5));
  Occlusion some_occluded_half =
      some_occluded.GetOcclusionWithGivenDrawTransform(half_transform);

  some_occluded = Occlusion(gfx::Transform(),
                            SimpleEnclosedRegion(20, 20),
                            SimpleEnclosedRegion(20, 20, 20, 20));
  Occlusion some_occluded_double =
      some_occluded.GetOcclusionWithGivenDrawTransform(double_transform);

  EXPECT_OCCLUSION(some_occluded_half, rects, true, false, false, true);
  EXPECT_OCCLUSION(some_occluded_double, rects, true, false, false, true);
}

TEST(OcclusionTest, GetUnoccludedContentRectNoTransform) {
  Occlusion some_occluded(gfx::Transform(),
                          SimpleEnclosedRegion(10, 10),
                          SimpleEnclosedRegion(10, 10, 10, 10));

  gfx::Rect full_query_result =
      some_occluded.GetUnoccludedContentRect(gfx::Rect(20, 20));
  EXPECT_EQ(gfx::Rect(20, 20), full_query_result);

  gfx::Rect half_query_result =
      some_occluded.GetUnoccludedContentRect(gfx::Rect(10, 0, 10, 20));
  EXPECT_EQ(gfx::Rect(10, 0, 10, 10), half_query_result);
}

TEST(OcclusionTest, GetUnoccludedContentRectScaled) {
  gfx::Transform half_scale;
  half_scale.Scale(0.5, 0.5);

  gfx::Transform double_scale;
  double_scale.Scale(2, 2);

  Occlusion some_occluded_half(
      half_scale, SimpleEnclosedRegion(5, 5), SimpleEnclosedRegion(5, 5, 5, 5));
  Occlusion some_occluded_double(double_scale,
                                 SimpleEnclosedRegion(20, 20),
                                 SimpleEnclosedRegion(20, 20, 20, 20));
  gfx::Rect full_query_result_half =
      some_occluded_half.GetUnoccludedContentRect(gfx::Rect(20, 20));
  gfx::Rect full_query_result_double =
      some_occluded_double.GetUnoccludedContentRect(gfx::Rect(20, 20));
  EXPECT_EQ(gfx::Rect(20, 20), full_query_result_half);
  EXPECT_EQ(gfx::Rect(20, 20), full_query_result_double);

  gfx::Rect half_query_result_half =
      some_occluded_half.GetUnoccludedContentRect(gfx::Rect(10, 0, 10, 20));
  gfx::Rect half_query_result_double =
      some_occluded_half.GetUnoccludedContentRect(gfx::Rect(10, 0, 10, 20));
  EXPECT_EQ(gfx::Rect(10, 0, 10, 10), half_query_result_half);
  EXPECT_EQ(gfx::Rect(10, 0, 10, 10), half_query_result_double);
}

TEST(OcclusionTest, GetUnoccludedContentRectTranslated) {
  gfx::Transform move_left;
  move_left.Translate(-100, 0);

  gfx::Transform move_down;
  move_down.Translate(0, 100);

  Occlusion some_occluded_left(move_left,
                               SimpleEnclosedRegion(-100, 0, 10, 10),
                               SimpleEnclosedRegion(-90, 10, 10, 10));
  Occlusion some_occluded_down(move_down,
                               SimpleEnclosedRegion(0, 100, 0, 10),
                               SimpleEnclosedRegion(10, 110, 10, 10));

  gfx::Rect full_query_result_left =
      some_occluded_left.GetUnoccludedContentRect(gfx::Rect(20, 20));
  gfx::Rect full_query_result_down =
      some_occluded_down.GetUnoccludedContentRect(gfx::Rect(20, 20));
  EXPECT_EQ(gfx::Rect(20, 20), full_query_result_left);
  EXPECT_EQ(gfx::Rect(20, 20), full_query_result_down);

  gfx::Rect half_query_result_left =
      some_occluded_left.GetUnoccludedContentRect(gfx::Rect(10, 0, 10, 20));
  gfx::Rect half_query_result_down =
      some_occluded_down.GetUnoccludedContentRect(gfx::Rect(10, 0, 10, 20));
  EXPECT_EQ(gfx::Rect(10, 0, 10, 10), half_query_result_left);
  EXPECT_EQ(gfx::Rect(10, 0, 10, 10), half_query_result_down);
}

}  // namespace
}  // namespace cc
