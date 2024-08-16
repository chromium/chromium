// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/accessibility/ui/accessibility_focus_ring_group.h"

#include <memory>
#include <vector>

#include "ash/accessibility/ui/accessibility_focus_ring.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/accessibility/ui/accessibility_layer.h"
#include "ash/test/ash_test_base.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class TestableAccessibilityFocusRingGroup : public AccessibilityFocusRingGroup {
 public:
  TestableAccessibilityFocusRingGroup() : AccessibilityFocusRingGroup() {
    // By default use an easy round number for testing.
    margin_ = 10;
  }

  TestableAccessibilityFocusRingGroup(
      const TestableAccessibilityFocusRingGroup&) = delete;
  TestableAccessibilityFocusRingGroup& operator=(
      const TestableAccessibilityFocusRingGroup&) = delete;

  ~TestableAccessibilityFocusRingGroup() override = default;

  void RectsToRings(const std::vector<gfx::Rect>& rects,
                    std::vector<AccessibilityFocusRing>* rings) const {
    AccessibilityFocusRingGroup::RectsToRings(rects, rings);
  }

  int GetMargin() const override { return margin_; }

 private:
  int margin_;
};

class AccessibilityFocusRingGroupTest : public AshTestBase {
 public:
  AccessibilityFocusRingGroupTest() {
    AccessibilityFocusRing::set_screen_bounds_for_testing(
        gfx::Rect(0, 0, 1000, 500));
  }
  ~AccessibilityFocusRingGroupTest() override = default;

 protected:
  gfx::Rect AddMargin(gfx::Rect r) {
    r.Inset(-group_.GetMargin());
    return r;
  }

  TestableAccessibilityFocusRingGroup group_;
};

TEST_F(AccessibilityFocusRingGroupTest, ClipToBounds) {
  // Test the ClipToBounds function, which takes an offscreen rect and preserves
  // the dimensions that are within bounds, and snaps the dimensions that are
  // out of bounds to the edges. This lets the code to create points work
  // properly, while ensuring all sides of the focus ring are visible.
  int length = 100;
  gfx::Rect screen(0, 0, 1000, 1000);

  gfx::Rect offTop(100, -1000, length, length);
  gfx::Rect expected(100, 0, length, 0);
  AccessibilityFocusRing::ClipToBounds(&offTop, screen);
  ASSERT_EQ(expected, offTop);

  gfx::Rect offBottom(100, 5000, length, length);
  expected = gfx::Rect(100, 1000, length, 0);
  AccessibilityFocusRing::ClipToBounds(&offBottom, screen);
  ASSERT_EQ(expected, offBottom);

  gfx::Rect offLeft(-1000, 100, length, length);
  expected = gfx::Rect(0, 100, 0, length);
  AccessibilityFocusRing::ClipToBounds(&offLeft, screen);
  ASSERT_EQ(expected, offLeft);

  gfx::Rect offRight(5000, 100, length, length);
  expected = gfx::Rect(1000, 100, 0, length);
  AccessibilityFocusRing::ClipToBounds(&offRight, screen);
  ASSERT_EQ(expected, offRight);

  gfx::Rect offCorner(-1000, -1000, length, length);
  expected = gfx::Rect(0, 0, 0, 0);
  AccessibilityFocusRing::ClipToBounds(&offCorner, screen);
  ASSERT_EQ(expected, offCorner);
}

TEST_F(AccessibilityFocusRingGroupTest, RectsToRingsSimpleBoundsCheck) {
  // Easy confidence check. Given a single rectangle, make sure we get back
  // a focus ring with the same bounds.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(20, 30, 70, 150));
  std::vector<AccessibilityFocusRing> rings;
  group_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(AddMargin(rects[0]), rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingGroupTest, RectsToRingsTinyRect) {
  // A single, very small rect can lead to error conditions.
  // Test that it displays as expected.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(100, 100, group_.GetMargin(), group_.GetMargin()));
  std::vector<AccessibilityFocusRing> rings;
  group_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(AddMargin(rects[0]), rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingGroupTest, RectsToRingsSnapToScreen) {
  // Given a rectangle that goes all the way to the screen edges, or off the
  // edge, check that we snap the focus ring to the screen, rather than going
  // offscreen.
  int x = -10;  // offscreen
  int y = -20;  // offscreen
  int length = 50;
  int padding = AccessibilityFocusRing::GetScreenPaddingForTesting();
  std::vector<gfx::Rect> rects;

  rects.push_back(gfx::Rect(x, y, length, length));
  std::vector<AccessibilityFocusRing> rings;
  group_.RectsToRings(rects, &rings);
  gfx::Rect result(padding, padding, length + x + group_.GetMargin() - padding,
                   length + y + group_.GetMargin() - padding);

  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(result, rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingGroupTest, RectsToRingsVerticalStack) {
  // Given two rects, one on top of each other, we should get back a
  // focus ring that surrounds them both.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(20, 20, 60, 30));
  rects.push_back(gfx::Rect(20, 50, 60, 30));
  std::vector<AccessibilityFocusRing> rings;
  group_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(AddMargin(gfx::Rect(20, 20, 60, 60)), rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingGroupTest, RectsToRingsHorizontalStack) {
  // Given two rects, one next to the other horizontally, we should get back a
  // focus ring that surrounds them both.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(20, 20, 60, 30));
  rects.push_back(gfx::Rect(80, 20, 60, 30));
  std::vector<AccessibilityFocusRing> rings;
  group_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  ASSERT_EQ(AddMargin(gfx::Rect(20, 20, 120, 30)), rings[0].GetBounds());
}

TEST_F(AccessibilityFocusRingGroupTest, RectsToRingsParagraphShape) {
  // Given a simple paragraph shape, make sure we get something that
  // outlines it correctly.
  AccessibilityFocusRing::set_screen_bounds_for_testing(
      gfx::Rect(0, 0, 1000, 1000));
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(110, 110, 180, 80));
  rects.push_back(gfx::Rect(110, 210, 580, 280));
  rects.push_back(gfx::Rect(510, 510, 180, 80));
  std::vector<AccessibilityFocusRing> rings;
  group_.RectsToRings(rects, &rings);
  ASSERT_EQ(1U, rings.size());
  EXPECT_EQ(gfx::Rect(100, 100, 600, 500), rings[0].GetBounds());

  const gfx::Point* points = rings[0].points;
  EXPECT_EQ(gfx::Point(100, 190), points[0]);
  EXPECT_EQ(gfx::Point(100, 110), points[1]);
  EXPECT_EQ(gfx::Point(100, 100), points[2]);
  EXPECT_EQ(gfx::Point(110, 100), points[3]);
  EXPECT_EQ(gfx::Point(290, 100), points[4]);
  EXPECT_EQ(gfx::Point(300, 100), points[5]);
  EXPECT_EQ(gfx::Point(300, 110), points[6]);
  EXPECT_EQ(gfx::Point(300, 190), points[7]);
  EXPECT_EQ(gfx::Point(300, 200), points[8]);
  EXPECT_EQ(gfx::Point(310, 200), points[9]);
  EXPECT_EQ(gfx::Point(690, 200), points[10]);
  EXPECT_EQ(gfx::Point(700, 200), points[11]);
  EXPECT_EQ(gfx::Point(700, 210), points[12]);
  EXPECT_EQ(gfx::Point(700, 490), points[13]);
  EXPECT_EQ(gfx::Point(700, 500), points[14]);
  EXPECT_EQ(gfx::Point(700, 500), points[15]);
  EXPECT_EQ(gfx::Point(700, 500), points[16]);
  EXPECT_EQ(gfx::Point(700, 500), points[17]);
  EXPECT_EQ(gfx::Point(700, 510), points[18]);
  EXPECT_EQ(gfx::Point(700, 590), points[19]);
  EXPECT_EQ(gfx::Point(700, 600), points[20]);
  EXPECT_EQ(gfx::Point(690, 600), points[21]);
  EXPECT_EQ(gfx::Point(510, 600), points[22]);
  EXPECT_EQ(gfx::Point(500, 600), points[23]);
  EXPECT_EQ(gfx::Point(500, 590), points[24]);
  EXPECT_EQ(gfx::Point(500, 510), points[25]);
  EXPECT_EQ(gfx::Point(500, 500), points[26]);
  EXPECT_EQ(gfx::Point(490, 500), points[27]);
  EXPECT_EQ(gfx::Point(110, 500), points[28]);
  EXPECT_EQ(gfx::Point(100, 500), points[29]);
  EXPECT_EQ(gfx::Point(100, 490), points[30]);
  EXPECT_EQ(gfx::Point(100, 210), points[31]);
  EXPECT_EQ(gfx::Point(100, 200), points[32]);
  EXPECT_EQ(gfx::Point(100, 200), points[33]);
  EXPECT_EQ(gfx::Point(100, 200), points[34]);
  EXPECT_EQ(gfx::Point(100, 200), points[35]);
}

}  // namespace ash
