// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/region.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

#define TEST_INSIDE_RECT(r, x, y, w, h)                      \
  EXPECT_TRUE(r.Contains(gfx::Point(x, y)));                 \
  EXPECT_TRUE(r.Contains(gfx::Point(x + w - 1, y)));         \
  EXPECT_TRUE(r.Contains(gfx::Point(x, y + h - 1)));         \
  EXPECT_TRUE(r.Contains(gfx::Point(x + w - 1, y + h - 1))); \
  EXPECT_TRUE(r.Contains(gfx::Point(x, y + h / 2)));         \
  EXPECT_TRUE(r.Contains(gfx::Point(x + w - 1, y + h / 2))); \
  EXPECT_TRUE(r.Contains(gfx::Point(x + w / 2, y)));         \
  EXPECT_TRUE(r.Contains(gfx::Point(x + w / 2, y + h - 1))); \
  EXPECT_TRUE(r.Contains(gfx::Point(x + w / 2, y + h / 2))); \

#define TEST_LEFT_OF_RECT(r, x, y, w, h)                  \
  EXPECT_FALSE(r.Contains(gfx::Point(x - 1, y)));         \
  EXPECT_FALSE(r.Contains(gfx::Point(x - 1, y + h - 1))); \

#define TEST_RIGHT_OF_RECT(r, x, y, w, h)                 \
  EXPECT_FALSE(r.Contains(gfx::Point(x + w, y)));         \
  EXPECT_FALSE(r.Contains(gfx::Point(x + w, y + h - 1))); \

#define TEST_TOP_OF_RECT(r, x, y, w, h)                   \
  EXPECT_FALSE(r.Contains(gfx::Point(x, y - 1)));         \
  EXPECT_FALSE(r.Contains(gfx::Point(x + w - 1, y - 1))); \

#define TEST_BOTTOM_OF_RECT(r, x, y, w, h)                \
  EXPECT_FALSE(r.Contains(gfx::Point(x, y + h)));         \
  EXPECT_FALSE(r.Contains(gfx::Point(x + w - 1, y + h))); \

TEST(RegionTest, ContainsPoint) {
  Region r;

  EXPECT_FALSE(r.Contains(gfx::Point(0, 0)));

  r.Union(gfx::Rect(35, 35, 1, 1));
  TEST_INSIDE_RECT(r, 35, 35, 1, 1);
  TEST_LEFT_OF_RECT(r, 35, 35, 1, 1);
  TEST_RIGHT_OF_RECT(r, 35, 35, 1, 1);
  TEST_TOP_OF_RECT(r, 35, 35, 1, 1);
  TEST_BOTTOM_OF_RECT(r, 35, 35, 1, 1);

  r.Union(gfx::Rect(30, 30, 10, 10));
  TEST_INSIDE_RECT(r, 30, 30, 10, 10);
  TEST_LEFT_OF_RECT(r, 30, 30, 10, 10);
  TEST_RIGHT_OF_RECT(r, 30, 30, 10, 10);
  TEST_TOP_OF_RECT(r, 30, 30, 10, 10);
  TEST_BOTTOM_OF_RECT(r, 30, 30, 10, 10);

  r.Union(gfx::Rect(31, 40, 10, 10));
  EXPECT_FALSE(r.Contains(gfx::Point(30, 40)));
  EXPECT_TRUE(r.Contains(gfx::Point(31, 40)));
  EXPECT_FALSE(r.Contains(gfx::Point(40, 39)));
  EXPECT_TRUE(r.Contains(gfx::Point(40, 40)));

  TEST_INSIDE_RECT(r, 30, 30, 10, 10);
  TEST_LEFT_OF_RECT(r, 30, 30, 10, 10);
  TEST_RIGHT_OF_RECT(r, 30, 30, 10, 10);
  TEST_TOP_OF_RECT(r, 30, 30, 10, 10);
  TEST_INSIDE_RECT(r, 31, 40, 10, 10);
  TEST_LEFT_OF_RECT(r, 31, 40, 10, 10);
  TEST_RIGHT_OF_RECT(r, 31, 40, 10, 10);
  TEST_BOTTOM_OF_RECT(r, 31, 40, 10, 10);

  r.Union(gfx::Rect(42, 40, 10, 10));

  TEST_INSIDE_RECT(r, 42, 40, 10, 10);
  TEST_LEFT_OF_RECT(r, 42, 40, 10, 10);
  TEST_RIGHT_OF_RECT(r, 42, 40, 10, 10);
  TEST_TOP_OF_RECT(r, 42, 40, 10, 10);
  TEST_BOTTOM_OF_RECT(r, 42, 40, 10, 10);

  TEST_INSIDE_RECT(r, 30, 30, 10, 10);
  TEST_LEFT_OF_RECT(r, 30, 30, 10, 10);
  TEST_RIGHT_OF_RECT(r, 30, 30, 10, 10);
  TEST_TOP_OF_RECT(r, 30, 30, 10, 10);
  TEST_INSIDE_RECT(r, 31, 40, 10, 10);
  TEST_LEFT_OF_RECT(r, 31, 40, 10, 10);
  TEST_RIGHT_OF_RECT(r, 31, 40, 10, 10);
  TEST_BOTTOM_OF_RECT(r, 31, 40, 10, 10);
}

TEST(RegionTest, EmptySpan) {
  Region r;
  r.Union(gfx::Rect(5, 0, 10, 10));
  r.Union(gfx::Rect(0, 5, 10, 10));
  r.Subtract(gfx::Rect(7, 7, 10, 0));

  for (gfx::Rect rect : r)
    EXPECT_FALSE(rect.IsEmpty());
}

#define TEST_NO_INTERSECT(a, b) {  \
  Region ar = a;                   \
  Region br = b;                   \
  EXPECT_FALSE(ar.Intersects(br)); \
  EXPECT_FALSE(br.Intersects(ar)); \
  EXPECT_FALSE(ar.Intersects(b));  \
  EXPECT_FALSE(br.Intersects(a));  \
}

#define TEST_INTERSECT(a, b) {    \
  Region ar = a;                  \
  Region br = b;                  \
  EXPECT_TRUE(ar.Intersects(br)); \
  EXPECT_TRUE(br.Intersects(ar)); \
  EXPECT_TRUE(ar.Intersects(b));  \
  EXPECT_TRUE(br.Intersects(a));  \
}

TEST(RegionTest, IntersectsRegion) {
  Region r;

  TEST_NO_INTERSECT(gfx::Rect(), gfx::Rect());
  TEST_NO_INTERSECT(gfx::Rect(), gfx::Rect(0, 0, 1, 1));
  TEST_NO_INTERSECT(gfx::Rect(), gfx::Rect(1, 1, 1, 1));

  TEST_NO_INTERSECT(gfx::Rect(-1, -1, 2, 2), gfx::Rect());

  r.Union(gfx::Rect(0, 0, 1, 1));
  TEST_NO_INTERSECT(r, gfx::Rect());
  TEST_INTERSECT(r, gfx::Rect(0, 0, 1, 1));
  TEST_INTERSECT(r, gfx::Rect(0, 0, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(-1, 0, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(-1, -1, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(0, -1, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(-1, -1, 3, 3));

  r.Union(gfx::Rect(0, 0, 3, 3));
  r.Union(gfx::Rect(10, 0, 3, 3));
  r.Union(gfx::Rect(0, 10, 13, 3));
  TEST_NO_INTERSECT(r, gfx::Rect());
  TEST_INTERSECT(r, gfx::Rect(1, 1, 1, 1));
  TEST_INTERSECT(r, gfx::Rect(0, 0, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(1, 0, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(1, 1, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(0, 1, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(0, 0, 3, 3));
  TEST_INTERSECT(r, gfx::Rect(-1, -1, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(2, -1, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(2, 2, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(-1, 2, 2, 2));

  TEST_INTERSECT(r, gfx::Rect(11, 1, 1, 1));
  TEST_INTERSECT(r, gfx::Rect(10, 0, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(11, 0, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(11, 1, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(10, 1, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(10, 0, 3, 3));
  TEST_INTERSECT(r, gfx::Rect(9, -1, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(12, -1, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(12, 2, 2, 2));
  TEST_INTERSECT(r, gfx::Rect(9, 2, 2, 2));

  TEST_INTERSECT(r, gfx::Rect(0, -1, 13, 5));
  TEST_INTERSECT(r, gfx::Rect(1, -1, 11, 5));
  TEST_INTERSECT(r, gfx::Rect(2, -1, 9, 5));
  TEST_INTERSECT(r, gfx::Rect(2, -1, 8, 5));
  TEST_INTERSECT(r, gfx::Rect(3, -1, 8, 5));
  TEST_NO_INTERSECT(r, gfx::Rect(3, -1, 7, 5));

  TEST_INTERSECT(r, gfx::Rect(0, 1, 13, 1));
  TEST_INTERSECT(r, gfx::Rect(1, 1, 11, 1));
  TEST_INTERSECT(r, gfx::Rect(2, 1, 9, 1));
  TEST_INTERSECT(r, gfx::Rect(2, 1, 8, 1));
  TEST_INTERSECT(r, gfx::Rect(3, 1, 8, 1));
  TEST_NO_INTERSECT(r, gfx::Rect(3, 1, 7, 1));

  TEST_INTERSECT(r, gfx::Rect(0, 0, 13, 13));
  TEST_INTERSECT(r, gfx::Rect(0, 1, 13, 11));
  TEST_INTERSECT(r, gfx::Rect(0, 2, 13, 9));
  TEST_INTERSECT(r, gfx::Rect(0, 2, 13, 8));
  TEST_INTERSECT(r, gfx::Rect(0, 3, 13, 8));
  TEST_NO_INTERSECT(r, gfx::Rect(0, 3, 13, 7));
}

TEST(RegionTest, ReadPastFullSpanVectorInIntersectsTest) {
  Region r;

  // This region has enough spans to fill its allocated Vector exactly.
  r.Union(gfx::Rect(400, 300, 1, 800));
  r.Union(gfx::Rect(785, 585, 1, 1));
  r.Union(gfx::Rect(787, 585, 1, 1));
  r.Union(gfx::Rect(0, 587, 16, 162));
  r.Union(gfx::Rect(26, 590, 300, 150));
  r.Union(gfx::Rect(196, 750, 1, 1));
  r.Union(gfx::Rect(0, 766, 1, 1));
  r.Union(gfx::Rect(0, 782, 1, 1));
  r.Union(gfx::Rect(745, 798, 1, 1));
  r.Union(gfx::Rect(795, 882, 10, 585));
  r.Union(gfx::Rect(100, 1499, 586, 1));
  r.Union(gfx::Rect(100, 1500, 585, 784));
  // This query rect goes past the bottom of the Region, causing the
  // test to reach the last span and try go past it. It should not read
  // memory off the end of the span Vector.
  TEST_NO_INTERSECT(r, gfx::Rect(0, 2184, 1, 150));
}

#define TEST_NO_CONTAINS(a, b)                  \
  {                                             \
    Region ar = a;                              \
    Region br = b;                              \
    EXPECT_FALSE(ar.Contains(br));              \
    EXPECT_FALSE(ar.Contains(b));               \
  }

#define TEST_CONTAINS(a, b)                     \
  {                                             \
    Region ar = a;                              \
    Region br = b;                              \
    EXPECT_TRUE(ar.Contains(br));               \
    EXPECT_TRUE(ar.Contains(b));                \
  }

TEST(RegionTest, ContainsRegion) {
  TEST_CONTAINS(gfx::Rect(), gfx::Rect());
  TEST_CONTAINS(gfx::Rect(0, 0, 1, 1), gfx::Rect());
  TEST_CONTAINS(gfx::Rect(10, 10, 1, 1), gfx::Rect());

  TEST_NO_CONTAINS(gfx::Rect(), gfx::Rect(0, 0, 1, 1));
  TEST_NO_CONTAINS(gfx::Rect(), gfx::Rect(1, 1, 1, 1));

  TEST_NO_CONTAINS(gfx::Rect(10, 10, 1, 1), gfx::Rect(11, 10, 1, 1));
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 1, 1), gfx::Rect(10, 11, 1, 1));
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 1, 1), gfx::Rect(9, 10, 1, 1));
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 1, 1), gfx::Rect(10, 9, 1, 1));
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 1, 1), gfx::Rect(9, 9, 2, 2));
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 1, 1), gfx::Rect(10, 9, 2, 2));
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 1, 1), gfx::Rect(9, 10, 2, 2));
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 1, 1), gfx::Rect(10, 10, 2, 2));
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 1, 1), gfx::Rect(9, 9, 3, 3));

  Region h_lines;
  for (int i = 10; i < 20; i += 2)
    h_lines.Union(gfx::Rect(i, 10, 1, 10));

  TEST_CONTAINS(gfx::Rect(10, 10, 9, 10), h_lines);
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 9, 9), h_lines);
  TEST_NO_CONTAINS(gfx::Rect(10, 11, 9, 9), h_lines);
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 8, 10), h_lines);
  TEST_NO_CONTAINS(gfx::Rect(11, 10, 8, 10), h_lines);

  Region v_lines;
  for (int i = 10; i < 20; i += 2)
    v_lines.Union(gfx::Rect(10, i, 10, 1));

  TEST_CONTAINS(gfx::Rect(10, 10, 10, 9), v_lines);
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 9, 9), v_lines);
  TEST_NO_CONTAINS(gfx::Rect(11, 10, 9, 9), v_lines);
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 10, 8), v_lines);
  TEST_NO_CONTAINS(gfx::Rect(10, 11, 10, 8), v_lines);

  Region grid;
  for (int i = 10; i < 20; i += 2)
    for (int j = 10; j < 20; j += 2)
      grid.Union(gfx::Rect(i, j, 1, 1));

  TEST_CONTAINS(gfx::Rect(10, 10, 9, 9), grid);
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 9, 8), grid);
  TEST_NO_CONTAINS(gfx::Rect(10, 11, 9, 8), grid);
  TEST_NO_CONTAINS(gfx::Rect(10, 10, 8, 9), grid);
  TEST_NO_CONTAINS(gfx::Rect(11, 10, 8, 9), grid);

  TEST_CONTAINS(h_lines, h_lines);
  TEST_CONTAINS(v_lines, v_lines);
  TEST_NO_CONTAINS(v_lines, h_lines);
  TEST_NO_CONTAINS(h_lines, v_lines);
  TEST_CONTAINS(grid, grid);
  TEST_CONTAINS(h_lines, grid);
  TEST_CONTAINS(v_lines, grid);
  TEST_NO_CONTAINS(grid, h_lines);
  TEST_NO_CONTAINS(grid, v_lines);

  for (int i = 10; i < 20; i += 2)
    TEST_CONTAINS(h_lines, gfx::Rect(i, 10, 1, 10));

  for (int i = 10; i < 20; i += 2)
    TEST_CONTAINS(v_lines, gfx::Rect(10, i, 10, 1));

  for (int i = 10; i < 20; i += 2)
    for (int j = 10; j < 20; j += 2)
      TEST_CONTAINS(grid, gfx::Rect(i, j, 1, 1));

  Region container;
  container.Union(gfx::Rect(0, 0, 40, 20));
  container.Union(gfx::Rect(0, 20, 41, 20));
  TEST_CONTAINS(container, gfx::Rect(5, 5, 30, 30));

  container.Clear();
  container.Union(gfx::Rect(0, 0, 10, 10));
  container.Union(gfx::Rect(0, 30, 10, 10));
  container.Union(gfx::Rect(30, 30, 10, 10));
  container.Union(gfx::Rect(30, 0, 10, 10));
  TEST_NO_CONTAINS(container, gfx::Rect(5, 5, 30, 30));

  container.Clear();
  container.Union(gfx::Rect(0, 0, 10, 10));
  container.Union(gfx::Rect(0, 30, 10, 10));
  container.Union(gfx::Rect(30, 0, 10, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(5, 5, 30, 30));

  container.Clear();
  container.Union(gfx::Rect(30, 0, 10, 10));
  container.Union(gfx::Rect(30, 30, 10, 10));
  container.Union(gfx::Rect(0, 0, 10, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(5, 5, 30, 30));

  container.Clear();
  container.Union(gfx::Rect(0, 0, 10, 40));
  container.Union(gfx::Rect(30, 0, 10, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(5, 5, 30, 30));

  container.Clear();
  container.Union(gfx::Rect(0, 0, 40, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(10, -1, 20, 10));

  container.Clear();
  container.Union(gfx::Rect(0, 0, 40, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(10, 31, 20, 10));

  container.Clear();
  container.Union(gfx::Rect(0, 0, 40, 20));
  container.Union(gfx::Rect(0, 20, 41, 20));
  TEST_NO_CONTAINS(container, gfx::Rect(-1, 10, 10, 20));

  container.Clear();
  container.Union(gfx::Rect(0, 0, 40, 20));
  container.Union(gfx::Rect(0, 20, 41, 20));
  TEST_NO_CONTAINS(container, gfx::Rect(31, 10, 10, 20));

  container.Clear();
  container.Union(gfx::Rect(0, 0, 40, 40));
  container.Subtract(gfx::Rect(0, 20, 60, 0));
  TEST_NO_CONTAINS(container, gfx::Rect(31, 10, 10, 20));

  container.Clear();
  container.Union(gfx::Rect(0, 0, 60, 20));
  container.Union(gfx::Rect(30, 20, 10, 20));
  TEST_NO_CONTAINS(container, gfx::Rect(0, 0, 10, 39));
  TEST_NO_CONTAINS(container, gfx::Rect(0, 0, 10, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(0, 0, 10, 41));
  TEST_NO_CONTAINS(container, gfx::Rect(29, 0, 10, 39));
  TEST_CONTAINS(container, gfx::Rect(30, 0, 10, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(31, 0, 10, 41));
  TEST_NO_CONTAINS(container, gfx::Rect(49, 0, 10, 39));
  TEST_NO_CONTAINS(container, gfx::Rect(50, 0, 10, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(51, 0, 10, 41));

  container.Clear();
  container.Union(gfx::Rect(30, 0, 10, 20));
  container.Union(gfx::Rect(0, 20, 60, 20));
  TEST_NO_CONTAINS(container, gfx::Rect(0, 0, 10, 39));
  TEST_NO_CONTAINS(container, gfx::Rect(0, 0, 10, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(0, 0, 10, 41));
  TEST_NO_CONTAINS(container, gfx::Rect(29, 0, 10, 39));
  TEST_CONTAINS(container, gfx::Rect(30, 0, 10, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(31, 0, 10, 41));
  TEST_NO_CONTAINS(container, gfx::Rect(49, 0, 10, 39));
  TEST_NO_CONTAINS(container, gfx::Rect(50, 0, 10, 40));
  TEST_NO_CONTAINS(container, gfx::Rect(51, 0, 10, 41));
}

TEST(RegionTest, Union) {
  Region r;
  Region r2;

  // A rect uniting a contained rect does not change the region.
  r2 = r = gfx::Rect(0, 0, 50, 50);
  r2.Union(gfx::Rect(20, 20, 10, 10));
  EXPECT_EQ(r, r2);

  // A rect uniting a containing rect gives back the containing rect.
  r = gfx::Rect(0, 0, 50, 50);
  r.Union(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(Region(gfx::Rect(0, 0, 100, 100)), r);

  // A complex region uniting a contained rect does not change the region.
  r = gfx::Rect(0, 0, 50, 50);
  r.Union(gfx::Rect(100, 0, 50, 50));
  r2 = r;
  r2.Union(gfx::Rect(20, 20, 10, 10));
  EXPECT_EQ(r, r2);

  // A complex region uniting a containing rect gives back the containing rect.
  r = gfx::Rect(0, 0, 50, 50);
  r.Union(gfx::Rect(100, 0, 50, 50));
  r.Union(gfx::Rect(0, 0, 500, 500));
  EXPECT_EQ(Region(gfx::Rect(0, 0, 500, 500)), r);
}

TEST(RegionTest, IsEmpty) {
  EXPECT_TRUE(Region().IsEmpty());
  EXPECT_TRUE(Region(gfx::Rect()).IsEmpty());
  EXPECT_TRUE(Region(Region()).IsEmpty());
  EXPECT_TRUE(Region(gfx::Rect(10, 10, 10, 0)).IsEmpty());
  EXPECT_TRUE(Region(gfx::Rect(10, 10, 0, 10)).IsEmpty());
  EXPECT_TRUE(Region(gfx::Rect(-10, 10, 10, 0)).IsEmpty());
  EXPECT_TRUE(Region(gfx::Rect(-10, 10, 0, 10)).IsEmpty());
  EXPECT_FALSE(Region(gfx::Rect(-1, -1, 1, 1)).IsEmpty());
  EXPECT_FALSE(Region(gfx::Rect(0, 0, 1, 1)).IsEmpty());
  EXPECT_FALSE(Region(gfx::Rect(0, 0, 2, 2)).IsEmpty());

  EXPECT_TRUE(SkIRect::MakeXYWH(10, 10, 10, 0).isEmpty());
  EXPECT_TRUE(SkIRect::MakeXYWH(10, 10, 0, 10).isEmpty());
  EXPECT_TRUE(SkIRect::MakeXYWH(-10, 10, 10, 0).isEmpty());
  EXPECT_TRUE(SkIRect::MakeXYWH(-10, 10, 0, 10).isEmpty());
  EXPECT_FALSE(SkIRect::MakeXYWH(-1, -1, 1, 1).isEmpty());
  EXPECT_FALSE(SkIRect::MakeXYWH(0, 0, 1, 1).isEmpty());
  EXPECT_FALSE(SkIRect::MakeXYWH(0, 0, 2, 2).isEmpty());
}

TEST(RegionTest, Clear) {
  Region r;

  r = gfx::Rect(0, 0, 50, 50);
  EXPECT_FALSE(r.IsEmpty());
  r.Clear();
  EXPECT_TRUE(r.IsEmpty());

  r = gfx::Rect(0, 0, 50, 50);
  r.Union(gfx::Rect(100, 0, 50, 50));
  r.Union(gfx::Rect(0, 0, 500, 500));
  EXPECT_FALSE(r.IsEmpty());
  r.Clear();
  EXPECT_TRUE(r.IsEmpty());
}

TEST(RegionSwap, Swap) {
  Region r1, r2, r3;

  r1 = gfx::Rect(0, 0, 50, 50);
  r1.Swap(&r2);
  EXPECT_TRUE(r1.IsEmpty());
  EXPECT_EQ(r2.ToString(), Region(gfx::Rect(0, 0, 50, 50)).ToString());

  r1 = gfx::Rect(0, 0, 50, 50);
  r1.Union(gfx::Rect(100, 0, 50, 50));
  r1.Union(gfx::Rect(0, 0, 500, 500));
  r3 = r1;
  r1.Swap(&r2);
  EXPECT_EQ(r1.ToString(), Region(gfx::Rect(0, 0, 50, 50)).ToString());
  EXPECT_EQ(r2.ToString(), r3.ToString());
}

}  // namespace
}  // namespace cc
