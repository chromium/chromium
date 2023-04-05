// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_points.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

const int kTestPointsLifetimeSeconds = 5;

class FastInkPointsTest : public testing::Test {
 public:
  FastInkPointsTest()
      : points_(base::Seconds(kTestPointsLifetimeSeconds)),
        predicted_(base::Seconds(kTestPointsLifetimeSeconds)),
        event_time_(base::TimeTicks()),
        screen_size_(1000, 1000) {}

  FastInkPointsTest(const FastInkPointsTest&) = delete;
  FastInkPointsTest& operator=(const FastInkPointsTest&) = delete;

  ~FastInkPointsTest() override = default;

 protected:
  FastInkPoints points_;
  FastInkPoints predicted_;
  base::TimeTicks event_time_;
  const gfx::Size screen_size_;

  base::TimeDelta prediction_duration_;

  void AddPoint(const gfx::PointF& point, base::TimeDelta interval) {
    event_time_ += interval;
    points_.AddPoint(point, event_time_);
    predicted_.Predict(points_, event_time_, prediction_duration_,
                       screen_size_);
    const base::TimeTicks presentation_time =
        event_time_ + prediction_duration_;
    points_.MoveForwardToTime(presentation_time);
    predicted_.MoveForwardToTime(presentation_time);
  }

  void AddStroke(int points,
                 base::TimeDelta interval,
                 const gfx::PointF& position,
                 const gfx::Vector2dF& velocity,
                 const gfx::Vector2dF& acceleration) {
    points_.Clear();
    gfx::PointF p = position;
    gfx::Vector2dF v = velocity;
    for (int i = 0; i < points; ++i) {
      AddPoint(p, interval);
      p += v;
      v += acceleration;
    }
  }

  void Diff(std::vector<gfx::Vector2dF>& dst,
            const std::vector<gfx::Vector2dF>& src) {
    dst.clear();
    if (src.size() < 2)
      return;
    for (size_t i = 1; i < src.size(); ++i)
      dst.push_back(src[i] - src[i - 1]);
  }

  void ComputeDeltas(std::vector<gfx::Vector2dF>& velocity,
                     std::vector<gfx::Vector2dF>& acceleration) {
    std::vector<gfx::Vector2dF> position;
    for (auto p : points_.points())
      position.push_back(p.location.OffsetFromOrigin());
    for (auto p : predicted_.points())
      position.push_back(p.location.OffsetFromOrigin());

    Diff(velocity, position);
    Diff(acceleration, velocity);
  }
};

}  // namespace

// Tests that the fast ink points internal collection handles receiving points
// and that the functions are returning the expected output.
TEST_F(FastInkPointsTest, FastInkPointsInternalCollection) {
  EXPECT_TRUE(points_.IsEmpty());
  EXPECT_EQ(gfx::Rect(), points_.GetBoundingBox());
  const gfx::PointF left(1, 1);
  const gfx::PointF bottom(1, 9);
  const gfx::PointF top_right(30, 0);
  const gfx::PointF last(2, 2);
  points_.AddPoint(left, base::TimeTicks());
  EXPECT_EQ(gfx::Rect(1, 1, 0, 0), points_.GetBoundingBox());

  // Should be the new bottom of the bounding box.
  points_.AddPoint(bottom, base::TimeTicks());
  EXPECT_EQ(gfx::Rect(1, 1, 0, bottom.y() - 1), points_.GetBoundingBox());

  // Should be the new top and right of the bounding box.
  points_.AddPoint(top_right, base::TimeTicks());
  EXPECT_EQ(3, points_.GetNumberOfPoints());
  EXPECT_FALSE(points_.IsEmpty());
  EXPECT_EQ(gfx::Rect(left.x(), top_right.y(), top_right.x() - left.x(),
                      bottom.y() - top_right.y()),
            points_.GetBoundingBox());

  // Should not expand bounding box.
  points_.AddPoint(last, base::TimeTicks());
  EXPECT_EQ(gfx::Rect(left.x(), top_right.y(), top_right.x() - left.x(),
                      bottom.y() - top_right.y()),
            points_.GetBoundingBox());

  // Points should be sorted in the order they are added.
  EXPECT_EQ(left, points_.GetOldest().location);
  EXPECT_EQ(last, points_.GetNewest().location);

  // Add a new point which will expand the bounding box.
  gfx::PointF new_left_bottom(0, 40);
  points_.AddPoint(new_left_bottom, base::TimeTicks());
  EXPECT_EQ(5, points_.GetNumberOfPoints());
  EXPECT_EQ(gfx::Rect(new_left_bottom.x(), top_right.y(),
                      top_right.x() - new_left_bottom.x(),
                      new_left_bottom.y() - top_right.y()),
            points_.GetBoundingBox());

  // Verify clearing works.
  points_.Clear();
  EXPECT_TRUE(points_.IsEmpty());
}

// Test the fast ink points collection to verify that old points are
// removed.
TEST_F(FastInkPointsTest, FastInkPointsInternalCollectionDeletion) {
  EXPECT_EQ(1, prediction_duration_.is_zero());
  // When a point older than kTestPointsLifetimeSeconds (5 sec) is added, it
  // should get removed. The age of the point is a number between 0.0 and 1.0,
  // with 0.0 specifying a newly added point and 1.0 specifying the age of a
  // point added |kTestPointsLifetimeSeconds| ago.
  AddPoint(gfx::PointF(), base::Seconds(1));
  EXPECT_EQ(1, points_.GetNumberOfPoints());
  EXPECT_FLOAT_EQ(0.0, points_.GetFadeoutFactor(0));

  // Verify when we move forward in time by one second, the age of the last
  // point, added one second ago is 1 / |kTestPointsLifetimeSeconds|.
  AddPoint(gfx::PointF(), base::Seconds(1));
  EXPECT_EQ(2, points_.GetNumberOfPoints());
  EXPECT_FLOAT_EQ(0.2, points_.GetFadeoutFactor(0));
  EXPECT_FLOAT_EQ(0.0, points_.GetFadeoutFactor(1));
  // Verify adding a point 10 seconds later will clear all other points, since
  // they are older than 5 seconds.
  AddPoint(gfx::PointF(), base::Seconds(10));
  EXPECT_EQ(1, points_.GetNumberOfPoints());

  // Verify adding 3 points one second apart each will add 3 points to the
  // collection, since all 4 points are younger than 5 seconds. All 4 points are
  // added 1 second apart so their age should be 0.2 apart.
  AddPoint(gfx::PointF(), base::Seconds(1));
  AddPoint(gfx::PointF(), base::Seconds(1));
  AddPoint(gfx::PointF(), base::Seconds(1));
  EXPECT_EQ(4, points_.GetNumberOfPoints());
  EXPECT_FLOAT_EQ(0.6, points_.GetFadeoutFactor(0));
  EXPECT_FLOAT_EQ(0.4, points_.GetFadeoutFactor(1));
  EXPECT_FLOAT_EQ(0.2, points_.GetFadeoutFactor(2));
  EXPECT_FLOAT_EQ(0.0, points_.GetFadeoutFactor(3));

  // Verify adding 1 point three seconds later will remove 2 points which are
  // older than 5 seconds.
  AddPoint(gfx::PointF(), base::Seconds(3));
  EXPECT_EQ(3, points_.GetNumberOfPoints());
}

// Test the fast ink prediction.
TEST_F(FastInkPointsTest, FastInkPointsPrediction) {
  prediction_duration_ = base::Milliseconds(18);

  const base::TimeDelta kTraceInterval = base::Milliseconds(5);

  const int kExpectedPredictionDepth = 3;

  // Using fairly generous error margin to allow for the accumulation
  // of rounding errors.
  const float kMaxPredictionError = 1e-4;

  std::vector<gfx::Vector2dF> computed_velocity;
  std::vector<gfx::Vector2dF> computed_acceleration;

  const gfx::Vector2dF zero;
  const gfx::PointF position(0, 0);

  // 0 points, no prediction.
  AddStroke(0, kTraceInterval, position, zero, zero);
  EXPECT_EQ(0, predicted_.GetNumberOfPoints());

  // 1 point, no prediction.
  AddStroke(1, kTraceInterval, position, zero, zero);
  EXPECT_EQ(0, predicted_.GetNumberOfPoints());

  // Fixed position, no prediction.
  for (int points = 2; points <= 4; ++points) {
    SCOPED_TRACE(points);
    AddStroke(points, kTraceInterval, position, zero, zero);
    EXPECT_EQ(0, predicted_.GetNumberOfPoints());
  }

  // Constant velocity, the predicted trajectory should maintain it.
  const gfx::Vector2dF velocity(10, 5);
  for (int points = 2; points <= 4; ++points) {
    SCOPED_TRACE(points);
    AddStroke(points, kTraceInterval, position, velocity, zero);
    EXPECT_EQ(kExpectedPredictionDepth, predicted_.GetNumberOfPoints());
    ComputeDeltas(computed_velocity, computed_acceleration);
    for (auto v : computed_velocity) {
      EXPECT_GT(kMaxPredictionError, (velocity - v).Length());
    }
  }

  // Constant acceleration, the predicted trajectory should maintain it.
  const gfx::Vector2dF acceleration(4, 2);
  for (int points = 3; points <= 4; ++points) {
    SCOPED_TRACE(points);
    AddStroke(points, kTraceInterval, position, velocity, acceleration);
    EXPECT_EQ(kExpectedPredictionDepth, predicted_.GetNumberOfPoints());
    ComputeDeltas(computed_velocity, computed_acceleration);
    for (auto a : computed_acceleration) {
      EXPECT_GT(kMaxPredictionError, (acceleration - a).Length());
    }
  }

  // Not testing with non-zero jerk, as the current prediction implementation
  // is not maintaining constant jerk on purpose.
}

// Test the interrupted stroke support.
TEST_F(FastInkPointsTest, AddGap) {
  points_.AddPoint(gfx::PointF(0, 0), base::TimeTicks());
  points_.AddPoint(gfx::PointF(1, 1), base::TimeTicks());
  points_.AddGap();
  points_.AddPoint(gfx::PointF(2, 2), base::TimeTicks());
  points_.AddPoint(gfx::PointF(3, 3), base::TimeTicks());
  points_.AddPoint(gfx::PointF(4, 4), base::TimeTicks());
  points_.AddGap();
  points_.AddPoint(gfx::PointF(5, 5), base::TimeTicks());

  auto points = points_.points();

  EXPECT_FALSE(points[0].gap_after);
  EXPECT_TRUE(points[1].gap_after);
  EXPECT_FALSE(points[2].gap_after);
  EXPECT_FALSE(points[3].gap_after);
  EXPECT_TRUE(points[4].gap_after);
  EXPECT_FALSE(points[5].gap_after);
}

// Tests deleting points from the last stroke.
TEST_F(FastInkPointsTest, UndoLastStroke) {
  // Calling undo with no points should not crash.
  gfx::Rect bounding_box = points_.UndoLastStroke();
  EXPECT_EQ(bounding_box, gfx::Rect());

  points_.AddPoint(gfx::PointF(0, 0), base::TimeTicks());
  points_.AddPoint(gfx::PointF(1, 1), base::TimeTicks());
  points_.AddGap();

  // Calling undo should clear all points.
  bounding_box = points_.UndoLastStroke();
  EXPECT_TRUE(points_.IsEmpty());
  EXPECT_EQ(bounding_box, gfx::Rect(0, 0, 1, 1));

  points_.AddPoint(gfx::PointF(0, 0), base::TimeTicks());
  points_.AddPoint(gfx::PointF(1, 1), base::TimeTicks());
  points_.AddGap();
  points_.AddPoint(gfx::PointF(2, 2), base::TimeTicks());
  points_.AddPoint(gfx::PointF(3, 3), base::TimeTicks());
  points_.AddPoint(gfx::PointF(4, 4), base::TimeTicks());
  points_.AddGap();

  // Calling undo should clear the second stroke only.
  bounding_box = points_.UndoLastStroke();
  EXPECT_EQ(points_.GetNumberOfPoints(), 2);
  EXPECT_TRUE(points_.GetNewest().gap_after);
  EXPECT_EQ(bounding_box, gfx::Rect(2, 2, 2, 2));

  points_.AddPoint(gfx::PointF(0, 0), base::TimeTicks());
  points_.AddPoint(gfx::PointF(1, 1), base::TimeTicks());
  points_.AddGap();
  points_.AddPoint(gfx::PointF(2, 2), base::TimeTicks());
  points_.AddPoint(gfx::PointF(3, 3), base::TimeTicks());
  points_.AddPoint(gfx::PointF(4, 4), base::TimeTicks());
  points_.AddGap();
  points_.AddPoint(gfx::PointF(5, 5), base::TimeTicks());

  // Calling undo twice should clear the third and second strokes.
  bounding_box = points_.UndoLastStroke();
  EXPECT_EQ(bounding_box, gfx::Rect(5, 5, 0, 0));
  bounding_box = points_.UndoLastStroke();
  EXPECT_EQ(bounding_box, gfx::Rect(2, 2, 2, 2));
  EXPECT_EQ(points_.GetNumberOfPoints(), 4);
  EXPECT_TRUE(points_.GetNewest().gap_after);
}

}  // namespace ash
