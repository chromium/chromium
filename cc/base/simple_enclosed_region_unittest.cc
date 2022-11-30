// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/simple_enclosed_region.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "cc/base/region.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

bool ExpectRegionEq(const gfx::Rect& rect, const SimpleEnclosedRegion& region) {
  std::vector<gfx::Rect> actual_rects;
  std::vector<gfx::Rect> expected_rects;

  if (!rect.IsEmpty())
    expected_rects.push_back(rect);

  for (size_t i = 0; i < region.GetRegionComplexity(); ++i)
    actual_rects.push_back(region.GetRect(i));

  if (rect.IsEmpty() != region.IsEmpty()) {
    LOG(ERROR) << "Expected: " << rect.IsEmpty()
               << " Actual: " << region.IsEmpty();
    return false;
  }

  if (expected_rects.size() != actual_rects.size()) {
    LOG(ERROR) << "Expected: " << expected_rects.size()
               << " Actual: " << actual_rects.size();
    return false;
  }

  std::sort(actual_rects.begin(), actual_rects.end());
  std::sort(expected_rects.begin(), expected_rects.end());

  for (size_t i = 0; i < expected_rects.size(); ++i) {
    if (expected_rects[i] != actual_rects[i]) {
      LOG(ERROR) << "Expected: " << expected_rects[i].ToString()
                 << " Actual: " << actual_rects[i].ToString();
      return false;
    }
  }

  return true;
}

TEST(SimpleEnclosedRegionTest, Create) {
  SimpleEnclosedRegion r1;
  EXPECT_TRUE(r1.IsEmpty());
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(), r1));

  SimpleEnclosedRegion r2(gfx::Rect(2, 3, 4, 5));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(2, 3, 4, 5), r2));

  SimpleEnclosedRegion r3(2, 3, 4, 5);
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(2, 3, 4, 5), r3));

  SimpleEnclosedRegion r4(4, 5);
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(4, 5), r4));

  SimpleEnclosedRegion r5(Region(gfx::Rect(2, 3, 4, 5)));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(2, 3, 4, 5), r5));

  SimpleEnclosedRegion r6(r5);
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(2, 3, 4, 5), r6));
}

TEST(SimpleEnclosedRegionTest, Assign) {
  SimpleEnclosedRegion r;
  EXPECT_TRUE(r.IsEmpty());

  r = gfx::Rect(2, 3, 4, 5);
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(2, 3, 4, 5), r));

  r = SimpleEnclosedRegion(3, 4, 5, 6);
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(3, 4, 5, 6), r));
}

TEST(SimpleEnclosedRegionTest, Clear) {
  SimpleEnclosedRegion r(1, 2, 3, 4);
  EXPECT_FALSE(r.IsEmpty());
  r.Clear();
  EXPECT_TRUE(r.IsEmpty());
}

TEST(SimpleEnclosedRegionTest, GetRegionComplexity) {
  SimpleEnclosedRegion empty;
  EXPECT_EQ(0u, empty.GetRegionComplexity());

  SimpleEnclosedRegion stuff;
  stuff.Union(gfx::Rect(1, 2, 3, 4));
  EXPECT_EQ(1u, stuff.GetRegionComplexity());

  // The SimpleEnclosedRegion only holds up to 1 rect.
  stuff.Union(gfx::Rect(5, 6, 7, 8));
  EXPECT_EQ(1u, stuff.GetRegionComplexity());
}

TEST(SimpleEnclosedRegionTest, Contains) {
  SimpleEnclosedRegion r(1, 2, 5, 6);

  EXPECT_FALSE(r.Contains(gfx::Point(0, 2)));
  EXPECT_FALSE(r.Contains(gfx::Point(1, 1)));
  EXPECT_TRUE(r.Contains(gfx::Point(1, 2)));

  EXPECT_FALSE(r.Contains(gfx::Point(6, 2)));
  EXPECT_FALSE(r.Contains(gfx::Point(5, 1)));
  EXPECT_TRUE(r.Contains(gfx::Point(5, 2)));

  EXPECT_FALSE(r.Contains(gfx::Point(0, 7)));
  EXPECT_FALSE(r.Contains(gfx::Point(1, 8)));
  EXPECT_TRUE(r.Contains(gfx::Point(1, 7)));

  EXPECT_FALSE(r.Contains(gfx::Point(6, 7)));
  EXPECT_FALSE(r.Contains(gfx::Point(5, 8)));
  EXPECT_TRUE(r.Contains(gfx::Point(5, 7)));

  EXPECT_FALSE(r.Contains(gfx::Rect(0, 2, 1, 1)));
  EXPECT_FALSE(r.Contains(gfx::Rect(1, 1, 1, 1)));
  EXPECT_TRUE(r.Contains(gfx::Rect(1, 2, 1, 1)));
  EXPECT_FALSE(r.Contains(gfx::Rect(0, 1, 2, 2)));

  EXPECT_FALSE(r.Contains(gfx::Rect(6, 2, 1, 1)));
  EXPECT_FALSE(r.Contains(gfx::Rect(5, 1, 1, 1)));
  EXPECT_TRUE(r.Contains(gfx::Rect(5, 2, 1, 1)));
  EXPECT_FALSE(r.Contains(gfx::Rect(5, 1, 2, 2)));

  EXPECT_FALSE(r.Contains(gfx::Rect(0, 7, 1, 1)));
  EXPECT_FALSE(r.Contains(gfx::Rect(1, 8, 1, 1)));
  EXPECT_TRUE(r.Contains(gfx::Rect(1, 7, 1, 1)));
  EXPECT_FALSE(r.Contains(gfx::Rect(0, 7, 2, 2)));

  EXPECT_FALSE(r.Contains(gfx::Rect(6, 7, 1, 1)));
  EXPECT_FALSE(r.Contains(gfx::Rect(5, 8, 1, 1)));
  EXPECT_TRUE(r.Contains(gfx::Rect(5, 7, 1, 1)));
  EXPECT_FALSE(r.Contains(gfx::Rect(5, 7, 2, 2)));

  gfx::Rect q(1, 2, 5, 6);
  EXPECT_TRUE(r.Contains(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, -1, 0, 0));
  EXPECT_FALSE(r.Contains(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(-1, 1, 0, 0));
  EXPECT_FALSE(r.Contains(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(1, 0, 0, -1));
  EXPECT_FALSE(r.Contains(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, 0, -1, 1));
  EXPECT_FALSE(r.Contains(q)) << q.ToString();

  q.Inset(gfx::Insets::TLBR(0, 1, 1, 0));
  EXPECT_TRUE(r.Contains(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(1, -1, 0, 0));
  EXPECT_TRUE(r.Contains(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(-1, 0, 0, 1));
  EXPECT_TRUE(r.Contains(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, 0, 1, -1));
  EXPECT_TRUE(r.Contains(q)) << q.ToString();

  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(0, 2, 1, 1)));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(1, 1, 1, 1)));
  EXPECT_TRUE(r.Contains(SimpleEnclosedRegion(1, 2, 1, 1)));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(0, 1, 2, 2)));

  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(6, 2, 1, 1)));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(5, 1, 1, 1)));
  EXPECT_TRUE(r.Contains(SimpleEnclosedRegion(5, 2, 1, 1)));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(5, 1, 2, 2)));

  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(0, 7, 1, 1)));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(1, 8, 1, 1)));
  EXPECT_TRUE(r.Contains(SimpleEnclosedRegion(1, 7, 1, 1)));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(0, 7, 2, 2)));

  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(6, 7, 1, 1)));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(5, 8, 1, 1)));
  EXPECT_TRUE(r.Contains(SimpleEnclosedRegion(5, 7, 1, 1)));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(5, 7, 2, 2)));

  q = gfx::Rect(1, 2, 5, 6);
  EXPECT_TRUE(r.Contains(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, -1, 0, 0));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(-1, 1, 0, 0));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(1, 0, 0, -1));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, 0, -1, 1));
  EXPECT_FALSE(r.Contains(SimpleEnclosedRegion(q))) << q.ToString();

  q.Inset(gfx::Insets::TLBR(0, 1, 1, 0));
  EXPECT_TRUE(r.Contains(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(1, -1, 0, 0));
  EXPECT_TRUE(r.Contains(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(-1, 0, 0, 1));
  EXPECT_TRUE(r.Contains(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, 0, 1, -1));
  EXPECT_TRUE(r.Contains(SimpleEnclosedRegion(q))) << q.ToString();
}

TEST(SimpleEnclosedRegionTest, Intersects) {
  SimpleEnclosedRegion r(1, 2, 5, 6);

  EXPECT_FALSE(r.Intersects(gfx::Rect(0, 2, 1, 1)));
  EXPECT_FALSE(r.Intersects(gfx::Rect(1, 1, 1, 1)));
  EXPECT_TRUE(r.Intersects(gfx::Rect(1, 2, 1, 1)));
  EXPECT_TRUE(r.Intersects(gfx::Rect(0, 1, 2, 2)));

  EXPECT_FALSE(r.Intersects(gfx::Rect(6, 2, 1, 1)));
  EXPECT_FALSE(r.Intersects(gfx::Rect(5, 1, 1, 1)));
  EXPECT_TRUE(r.Intersects(gfx::Rect(5, 2, 1, 1)));
  EXPECT_TRUE(r.Intersects(gfx::Rect(5, 1, 2, 2)));

  EXPECT_FALSE(r.Intersects(gfx::Rect(0, 7, 1, 1)));
  EXPECT_FALSE(r.Intersects(gfx::Rect(1, 8, 1, 1)));
  EXPECT_TRUE(r.Intersects(gfx::Rect(1, 7, 1, 1)));
  EXPECT_TRUE(r.Intersects(gfx::Rect(0, 7, 2, 2)));

  EXPECT_FALSE(r.Intersects(gfx::Rect(6, 7, 1, 1)));
  EXPECT_FALSE(r.Intersects(gfx::Rect(5, 8, 1, 1)));
  EXPECT_TRUE(r.Intersects(gfx::Rect(5, 7, 1, 1)));
  EXPECT_TRUE(r.Intersects(gfx::Rect(5, 7, 2, 2)));

  gfx::Rect q(1, 2, 5, 6);
  EXPECT_TRUE(r.Intersects(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, -1, 0, 0));
  EXPECT_TRUE(r.Intersects(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(-1, 1, 0, 0));
  EXPECT_TRUE(r.Intersects(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(1, 0, 0, -1));
  EXPECT_TRUE(r.Intersects(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, 0, -1, 1));
  EXPECT_TRUE(r.Intersects(q)) << q.ToString();

  q.Inset(gfx::Insets::TLBR(0, 1, 1, 0));
  EXPECT_TRUE(r.Intersects(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(1, -1, 0, 0));
  EXPECT_TRUE(r.Intersects(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(-1, 0, 0, 1));
  EXPECT_TRUE(r.Intersects(q)) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, 0, 1, -1));
  EXPECT_TRUE(r.Intersects(q)) << q.ToString();

  EXPECT_FALSE(r.Intersects(SimpleEnclosedRegion(0, 2, 1, 1)));
  EXPECT_FALSE(r.Intersects(SimpleEnclosedRegion(1, 1, 1, 1)));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(1, 2, 1, 1)));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(0, 1, 2, 2)));

  EXPECT_FALSE(r.Intersects(SimpleEnclosedRegion(6, 2, 1, 1)));
  EXPECT_FALSE(r.Intersects(SimpleEnclosedRegion(5, 1, 1, 1)));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(5, 2, 1, 1)));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(5, 1, 2, 2)));

  EXPECT_FALSE(r.Intersects(SimpleEnclosedRegion(0, 7, 1, 1)));
  EXPECT_FALSE(r.Intersects(SimpleEnclosedRegion(1, 8, 1, 1)));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(1, 7, 1, 1)));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(0, 7, 2, 2)));

  EXPECT_FALSE(r.Intersects(SimpleEnclosedRegion(6, 7, 1, 1)));
  EXPECT_FALSE(r.Intersects(SimpleEnclosedRegion(5, 8, 1, 1)));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(5, 7, 1, 1)));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(5, 7, 2, 2)));

  q = gfx::Rect(1, 2, 5, 6);
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, -1, 0, 0));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(-1, 1, 0, 0));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(1, 0, 0, -1));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, 0, -1, 1));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(q))) << q.ToString();

  q.Inset(gfx::Insets::TLBR(0, 1, 1, 0));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(1, -1, 0, 0));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(-1, 0, 0, 1));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(q))) << q.ToString();
  q.Inset(gfx::Insets::TLBR(0, 0, 1, -1));
  EXPECT_TRUE(r.Intersects(SimpleEnclosedRegion(q))) << q.ToString();
}

TEST(SimpleEnclosedRegionTest, Equals) {
  SimpleEnclosedRegion r(1, 2, 3, 4);
  EXPECT_TRUE(r.Equals(SimpleEnclosedRegion(1, 2, 3, 4)));
  EXPECT_FALSE(r.Equals(SimpleEnclosedRegion(2, 2, 3, 4)));
  EXPECT_FALSE(r.Equals(SimpleEnclosedRegion(1, 3, 3, 4)));
  EXPECT_FALSE(r.Equals(SimpleEnclosedRegion(1, 2, 4, 4)));
  EXPECT_FALSE(r.Equals(SimpleEnclosedRegion(1, 2, 3, 5)));
  EXPECT_FALSE(r.Equals(SimpleEnclosedRegion(2, 2, 2, 4)));
  EXPECT_FALSE(r.Equals(SimpleEnclosedRegion(1, 3, 3, 3)));
}

TEST(SimpleEnclosedRegionTest, Bounds) {
  SimpleEnclosedRegion r;
  EXPECT_EQ(gfx::Rect(), r.bounds());
  r = gfx::Rect(3, 4, 5, 6);
  EXPECT_EQ(gfx::Rect(3, 4, 5, 6), r.bounds());
  r.Union(gfx::Rect(1, 2, 12, 13));
  EXPECT_EQ(gfx::Rect(1, 2, 12, 13), r.bounds());
}

TEST(SimpleEnclosedRegionTest, GetRect) {
  SimpleEnclosedRegion r(3, 4, 5, 6);
  EXPECT_EQ(gfx::Rect(3, 4, 5, 6), r.GetRect(0));
  r.Union(gfx::Rect(1, 2, 12, 13));
  EXPECT_EQ(gfx::Rect(1, 2, 12, 13), r.GetRect(0));
}

TEST(SimpleEnclosedRegionTest, Union) {
  SimpleEnclosedRegion r;
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(), r));

  // Empty Union anything = anything.
  r.Union(gfx::Rect(4, 5, 6, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(4, 5, 6, 7), r));

  // Anything Union empty = anything.
  r.Union(gfx::Rect());
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(4, 5, 6, 7), r));

  // Anything Union contained rect = Anything.
  r.Union(gfx::Rect(5, 6, 4, 5));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(4, 5, 6, 7), r));

  // Anything Union containing rect = containing rect.
  r.Union(gfx::Rect(2, 3, 8, 9));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(2, 3, 8, 9), r));
  r.Union(gfx::Rect(2, 3, 9, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(2, 3, 9, 10), r));

  // Union with a second disjoint rect with area larger than half of the first
  // one.
  // +---+     +--+
  // |   |     |  |
  // +---+     +--+
  r.Union(gfx::Rect(20, 20, 6, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(20, 20, 6, 10), r));

  // Union with a second disjoint rect with area smaller than half of the first
  // one.
  // +----+     +--+
  // |    |     +--+
  // +----+
  r.Union(gfx::Rect(2, 3, 3, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(20, 20, 6, 10), r));

  // Union with a second disjoint rect with area larger than the first one.
  // +---+     +-------+
  // |   |     |       |
  // +---+     +-------+
  r.Union(gfx::Rect(2, 3, 15, 15));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(2, 3, 15, 15), r));

  // Union with rect which extends from the first one:
  // +----------+
  // |  1  |  2 |
  // +-----+----+
  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(20, 10, 5, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 15, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(10, 5, 10, 5));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 5, 10, 15), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(5, 10, 5, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(5, 10, 15, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(10, 20, 10, 5));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 15), r));

  // Union with rect which overlaps and extends from the first one:
  // +----+--+---+
  // |  1 |  |2  |
  // |    |  |   |
  // +----+--+---+
  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(10, 2, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 2, 10, 18), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(2, 10, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(2, 10, 18, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(10, 18, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 18), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(18, 10, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 18, 10), r));

  // Union with a second rect which overlaps with the first one and
  // area(rect 1) + area(overlap) > area(rect 2)*2 and
  // area(rect 1) < area(rect 2)*2.
  //       +---+
  //   +---|+ 2|
  //   |   +---+
  //   | 1  |
  //   +----+    (same figure for next test case.)
  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(14, 12, 8, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(11, 9, 8, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(9, 12, 8, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Union(gfx::Rect(13, 11, 8, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 10), r));

  // Union with a second rect which overlaps with the first one and
  // area(rect 1) + area(overlap) < area(rect 2)*2 and
  // area(rect 1) > area(rect 2).
  r = gfx::Rect(10, 10, 5, 5);
  r.Union(gfx::Rect(7, 7, 4, 4));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(7, 7, 4, 4), r));

  r = gfx::Rect(10, 10, 5, 5);
  r.Union(gfx::Rect(14, 7, 4, 4));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(14, 7, 4, 4), r));

  r = gfx::Rect(10, 10, 5, 5);
  r.Union(gfx::Rect(7, 14, 4, 4));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(7, 14, 4, 4), r));

  r = gfx::Rect(10, 10, 5, 5);
  r.Union(gfx::Rect(14, 14, 4, 4));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(14, 14, 4, 4), r));

  // Union with a second rect which overlaps with the first one and the new
  // unioned rect should combine both rect.
  //  +---+-+-----------+
  //  |  1| |    2      |
  //  |   +-|-----------+
  //  +-----+
  r = gfx::Rect(10, 10, 5, 5);
  r.Union(gfx::Rect(5, 11, 7, 4));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(5, 11, 10, 4), r));

  r = gfx::Rect(10, 10, 5, 5);
  r.Union(gfx::Rect(13, 10, 7, 4));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 4), r));

  r = gfx::Rect(10, 10, 5, 5);
  r.Union(gfx::Rect(10, 12, 4, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 4, 9), r));

  r = gfx::Rect(10, 10, 5, 5);
  r.Union(gfx::Rect(11, 11, 4, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(11, 10, 4, 8), r));
}

TEST(SimpleEnclosedRegionTest, Subtract) {
  SimpleEnclosedRegion r;
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(), r));

  // Empty Subtract anything = empty.
  r.Subtract(gfx::Rect(4, 5, 6, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(), r));

  // Subtracting an enclosing rect = empty.
  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(10, 10, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(9, 9, 12, 12));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(), r));

  // Subtracting a rect that covers one side of the region will shrink that
  // side.
  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(18, 10, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 8, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(18, 8, 10, 14));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 8, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(10, 18, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 8), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(8, 18, 14, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 8), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(2, 10, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(12, 10, 8, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(2, 8, 10, 14));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(12, 10, 8, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(10, 2, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 12, 10, 8), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(8, 2, 14, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 12, 10, 8), r));

  // Subtracting a rect that does not cover a full side will still shrink that
  // side.
  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(18, 12, 10, 8));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 8, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(18, 12, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 8, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(12, 18, 8, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 8), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(12, 18, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 8), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(2, 12, 10, 8));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(12, 10, 8, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(2, 12, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(12, 10, 8, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(12, 2, 8, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 12, 10, 8), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(12, 2, 10, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 12, 10, 8), r));

  // Subtracting a rect inside the region will make it choose the larger result.
  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(11, 11, 7, 8));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(18, 10, 2, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(11, 11, 8, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 18, 10, 2), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(12, 11, 7, 8));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 2, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(11, 12, 8, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 2), r));

  // Subtracting a rect that cuts the region in two will choose the larger side.
  // Here it's the top side.
  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(10, 14, 10, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 4), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(0, 14, 30, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 4), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(10, 14, 8, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 4), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(0, 14, 18, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 4), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(12, 14, 18, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 4), r));

  // Here it's the bottom side.
  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(10, 13, 10, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 16, 10, 4), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(0, 13, 30, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 16, 10, 4), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(10, 13, 8, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 16, 10, 4), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(0, 13, 18, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 16, 10, 4), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(12, 13, 18, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 16, 10, 4), r));

  // Here it's the left side.
  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(14, 10, 3, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 4, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(14, 10, 3, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 4, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(14, 10, 3, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 4, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(14, 10, 3, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 4, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(14, 10, 3, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 4, 10), r));

  // Here it's the right side.
  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(13, 10, 3, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(16, 10, 4, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(13, 10, 3, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(16, 10, 4, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(13, 10, 3, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(16, 10, 4, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(13, 10, 3, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(16, 10, 4, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(13, 10, 3, 10));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(16, 10, 4, 10), r));

  // Subtracting a rect that leaves three possible choices will choose the
  // larger.
  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(10, 14, 7, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 4), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(10, 14, 5, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(15, 10, 5, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(13, 14, 7, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 4), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(15, 14, 5, 3));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 5, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(14, 10, 3, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 4, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(14, 10, 3, 5));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 15, 10, 5), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(14, 13, 3, 7));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 4, 10), r));

  r = gfx::Rect(10, 10, 10, 10);
  r.Subtract(gfx::Rect(14, 15, 3, 5));
  EXPECT_TRUE(ExpectRegionEq(gfx::Rect(10, 10, 10, 5), r));
}

}  // namespace
}  // namespace cc
