// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_gesture_util.h"
#include "ash/fast_ink/fast_ink_points.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ash {

namespace {

constexpr float kLength = 400;

constexpr gfx::RectF kFrame(50, 50, 300, 200);

constexpr gfx::SizeF kPenTipSize(4.0, 14);

constexpr int kPointsPerLine = 10;

float LinearInterpolate(float from, float to, int position, int range) {
  return from + ((to - from) * position) / range;
}

class HighlighterGestureUtilTest : public AshTestBase {
 public:
  HighlighterGestureUtilTest() : points_(base::TimeDelta()) {}

  HighlighterGestureUtilTest(const HighlighterGestureUtilTest&) = delete;
  HighlighterGestureUtilTest& operator=(const HighlighterGestureUtilTest&) =
      delete;

  ~HighlighterGestureUtilTest() override = default;

 protected:
  fast_ink::FastInkPoints points_;

  void MoveTo(float x, float y) { AddPoint(x, y); }

  void LineTo(float x, float y) {
    const gfx::PointF origin = points_.GetNewest().location;
    for (int i = 1; i <= kPointsPerLine; i++) {
      AddPoint(LinearInterpolate(origin.x(), x, i, kPointsPerLine),
               LinearInterpolate(origin.y(), y, i, kPointsPerLine));
    }
  }

  void TraceLine(float x0, float y0, float x1, float y1) {
    MoveTo(x0, y0);
    LineTo(x1, y1);
  }

  void TraceRect(const gfx::RectF& rect, float tilt) {
    MoveTo(rect.x() + tilt, rect.y());
    LineTo(rect.right(), rect.y() + tilt);
    LineTo(rect.right() - tilt, rect.bottom());
    LineTo(rect.x(), rect.bottom() - tilt);
    LineTo(rect.x() + tilt, rect.y());
  }

  void TraceZigZag(float w, float h) {
    MoveTo(0, 0);
    LineTo(w / 4, h);
    LineTo(w / 2, 0);
    LineTo(w * 3 / 4, h);
    LineTo(w, 0);
  }

  HighlighterGestureType DetectGesture() {
    return DetectHighlighterGesture(points_.GetBoundingBoxF(), kPenTipSize,
                                    points_);
  }

 private:
  void AddPoint(float x, float y) {
    points_.AddPoint(gfx::PointF(x, y), base::TimeTicks());
  }
};

}  // namespace

TEST_F(HighlighterGestureUtilTest, HorizontalStrokeTooShort) {
  TraceLine(0, 0, 1, 0);
  EXPECT_EQ(HighlighterGestureType::kNotRecognized, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, HorizontalStrokeFlat) {
  TraceLine(0, 0, kLength, 0);
  EXPECT_EQ(HighlighterGestureType::kHorizontalStroke, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, HorizontalStrokeTiltedLeftSlightly) {
  const float kTilt = kLength / 20;
  TraceLine(0, kTilt, kLength, 0);
  EXPECT_EQ(HighlighterGestureType::kHorizontalStroke, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, HorizontalStrokeTiltedRightSlightly) {
  const float kTilt = kLength / 20;
  TraceLine(0, 0, kLength, kTilt);
  EXPECT_EQ(HighlighterGestureType::kHorizontalStroke, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, HorizontalStrokeTiltedLeftTooMuch) {
  const float kTilt = kLength / 5;
  TraceLine(0, kTilt, kLength, 0);
  EXPECT_EQ(HighlighterGestureType::kNotRecognized, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, HorizontalStrokeTiltedRightTooMuch) {
  const float kTilt = kLength / 5;
  TraceLine(0, 0, kLength, kTilt);
  EXPECT_EQ(HighlighterGestureType::kNotRecognized, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, HorizontalStrokeZigZagSlightly) {
  const float kHeight = kLength / 20;
  TraceZigZag(kLength, kHeight);
  EXPECT_EQ(HighlighterGestureType::kHorizontalStroke, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, HorizontalStrokeZigZagTooMuch) {
  const float kHeight = kLength / 5;
  TraceZigZag(kLength, kHeight);
  EXPECT_EQ(HighlighterGestureType::kNotRecognized, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, ClosedShapeRectangle) {
  TraceRect(kFrame, 0);
  EXPECT_EQ(HighlighterGestureType::kClosedShape, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, ClosedShapeRectangleThin) {
  TraceRect(gfx::RectF(0, 0, kLength, kLength / 20), 0);
  EXPECT_EQ(HighlighterGestureType::kHorizontalStroke, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, ClosedShapeRectangleTilted) {
  TraceRect(kFrame, 10.0);
  EXPECT_EQ(HighlighterGestureType::kClosedShape, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, ClosedShapeWithOverlap) {
  // 1/4 overlap
  MoveTo(kFrame.right(), kFrame.y());
  LineTo(kFrame.x(), kFrame.y());
  LineTo(kFrame.x(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.y());
  LineTo(kFrame.x(), kFrame.y());
  EXPECT_EQ(HighlighterGestureType::kClosedShape, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, ClosedShapeLikeU) {
  // 1/4 open shape
  MoveTo(kFrame.x(), kFrame.y());
  LineTo(kFrame.x(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.y());
  EXPECT_EQ(HighlighterGestureType::kNotRecognized, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, ClosedShapeLikeG) {
  // 1/8 open shape
  MoveTo(kFrame.right(), kFrame.y());
  LineTo(kFrame.x(), kFrame.y());
  LineTo(kFrame.x(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.CenterPoint().y());
  EXPECT_EQ(HighlighterGestureType::kClosedShape, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, ClosedShapeWithLittleBackAndForth) {
  MoveTo(kFrame.right(), kFrame.y());
  LineTo(kFrame.x(), kFrame.y());
  // Go back one pixel, then go forward again.
  MoveTo(kFrame.x() + 1, kFrame.y());
  MoveTo(kFrame.x(), kFrame.y());
  LineTo(kFrame.x(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.CenterPoint().y());
  EXPECT_EQ(HighlighterGestureType::kClosedShape, DetectGesture());
}

TEST_F(HighlighterGestureUtilTest, ClosedShapeWithTooMuchBackAndForth) {
  MoveTo(kFrame.right(), kFrame.y());
  LineTo(kFrame.x(), kFrame.y());
  LineTo(kFrame.right(), kFrame.y());
  LineTo(kFrame.x(), kFrame.y());
  LineTo(kFrame.x(), kFrame.bottom());
  LineTo(kFrame.x(), kFrame.y());
  LineTo(kFrame.x(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.bottom());
  LineTo(kFrame.x(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.bottom());
  LineTo(kFrame.right(), kFrame.CenterPoint().y());
  EXPECT_EQ(HighlighterGestureType::kNotRecognized, DetectGesture());
}

}  // namespace ash
