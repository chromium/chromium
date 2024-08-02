// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/fast_ink/fast_ink_points.h"

#include <algorithm>
#include <array>
#include <limits>

#include "base/containers/adapters.h"
#include "base/containers/circular_deque.h"
#include "base/ranges/algorithm.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ash {
namespace {

constexpr SkColor kDefaultPointColor = SkColorSetRGB(0x42, 0x85, 0xF4);
constexpr int kDefaultOpacity = 0xCC;

}  // namespace

const SkColor FastInkPoints::kDefaultColor =
    SkColorSetA(kDefaultPointColor, kDefaultOpacity);

FastInkPoints::FastInkPoints(base::TimeDelta life_duration)
    : life_duration_(life_duration) {}

FastInkPoints::~FastInkPoints() = default;

void FastInkPoints::AddPoint(const gfx::PointF& point,
                             const base::TimeTicks& time) {
  FastInkPoint new_point;
  new_point.location = point;
  new_point.time = time;
  points_.push_back(new_point);
}

void FastInkPoints::AddPoint(const gfx::PointF& point,
                             const base::TimeTicks& time,
                             SkColor color) {
  FastInkPoint new_point;
  new_point.location = point;
  new_point.time = time;
  new_point.color = color;
  points_.push_back(new_point);
}

void FastInkPoints::AddGap() {
  // Not doing anything special regarding prediction, as in real usage there
  // will be a gap in timestamps, and the prediction algorithm will reject the
  // points that are too old.
  points_.back().gap_after = true;
}

void FastInkPoints::MoveForwardToTime(const base::TimeTicks& latest_time) {
  DCHECK_GE(latest_time, collection_latest_time_);
  collection_latest_time_ = latest_time;

  if (!points_.empty() && !life_duration_.is_zero()) {
    // Remove obsolete points.
    const base::TimeTicks expiration = latest_time - life_duration_;
    auto first_alive_point = base::ranges::lower_bound(
        points_, expiration, base::ranges::less_equal(), &FastInkPoint::time);
    points_.erase(points_.begin(), first_alive_point);
  }
}

gfx::Rect FastInkPoints::UndoLastStroke() {
  if (points_.empty())
    return gfx::Rect();

  gfx::PointF min_point = GetNewest().location;
  gfx::PointF max_point = min_point;
  // Skip the last gap to delete until the penultimate gap.
  if (points_.back().gap_after)
    points_.pop_back();

  while (!points_.empty() && !points_.back().gap_after) {
    const gfx::PointF& location = points_.back().location;
    min_point.SetToMin(location);
    max_point.SetToMax(location);
    points_.pop_back();
  }

  return gfx::ToEnclosingRect(gfx::BoundingRect(min_point, max_point));
}

void FastInkPoints::Clear() {
  points_.clear();
}

gfx::Rect FastInkPoints::GetBoundingBox() const {
  return gfx::ToEnclosingRect(GetBoundingBoxF());
}

gfx::RectF FastInkPoints::GetBoundingBoxF() const {
  if (IsEmpty())
    return gfx::RectF();

  gfx::PointF min_point = GetOldest().location;
  gfx::PointF max_point = min_point;
  for (const FastInkPoint& point : points_) {
    min_point.SetToMin(point.location);
    max_point.SetToMax(point.location);
  }
  return gfx::BoundingRect(min_point, max_point);
}

FastInkPoints::FastInkPoint FastInkPoints::GetOldest() const {
  DCHECK(!IsEmpty());
  return points_.front();
}

FastInkPoints::FastInkPoint FastInkPoints::GetNewest() const {
  DCHECK(!IsEmpty());
  return points_.back();
}

bool FastInkPoints::IsEmpty() const {
  return points_.empty();
}

int FastInkPoints::GetNumberOfPoints() const {
  return points_.size();
}

const base::circular_deque<FastInkPoints::FastInkPoint>& FastInkPoints::points()
    const {
  return points_;
}

float FastInkPoints::GetFadeoutFactor(int index) const {
  DCHECK(!life_duration_.is_zero());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, GetNumberOfPoints());
  const base::TimeDelta age = collection_latest_time_ - points_[index].time;
  return std::min(age / life_duration_, 1.0);
}

void FastInkPoints::Predict(const FastInkPoints& real_points,
                            const base::TimeTicks& current_time,
                            base::TimeDelta prediction_duration,
                            const gfx::Size& screen_size) {
  Clear();

  if (real_points.IsEmpty() || prediction_duration.is_zero())
    return;

  gfx::Vector2dF scale(1.0f / screen_size.width(), 1.0f / screen_size.height());

  // Create a new set of predicted points based on the last four points added.
  // We add enough predicted points to fill the time between the new point and
  // the expected presentation time. Note that estimated presentation time is
  // based on current time and inefficient rendering of points can result in an
  // actual presentation time that is later.

  // TODO(reveman): Determine interval based on history when event time stamps
  // are accurate. b/36137953
  const float kPredictionIntervalMs = 5.0f;
  const float kMaxPointIntervalMs = 10.0f;
  base::TimeDelta prediction_interval =
      base::Milliseconds(kPredictionIntervalMs);
  base::TimeDelta max_point_interval = base::Milliseconds(kMaxPointIntervalMs);
  const FastInkPoint newest_real_point = real_points.GetNewest();
  base::TimeTicks last_point_time = newest_real_point.time;
  gfx::PointF last_point_location =
      gfx::ScalePoint(newest_real_point.location, scale.x(), scale.y());

  // Use the last four points for prediction.
  using PositionArray = std::array<gfx::PointF, 4>;
  PositionArray position;
  PositionArray::iterator it = position.begin();
  for (const auto& point : base::Reversed(real_points.points())) {
    // Stop adding positions if interval between points is too large to provide
    // an accurate history for prediction.
    if ((last_point_time - point.time) > max_point_interval)
      break;

    last_point_time = point.time;
    last_point_location = gfx::ScalePoint(point.location, scale.x(), scale.y());
    *it++ = last_point_location;

    // Stop when no more positions are needed.
    if (it == position.end())
      break;
  }

  const size_t valid_positions = it - position.begin();
  if (valid_positions < 2)  // Not enough reliable data, bail out.
    return;

  // Note: Currently there's no need to divide by the time delta between
  // points as we assume a constant delta between points that matches the
  // prediction point interval.
  gfx::Vector2dF velocity[3];
  for (size_t i = 0; i < valid_positions - 1; ++i)
    velocity[i] = position[i] - position[i + 1];
  // velocity[0] is always valid, since |valid_positions| >=2

  gfx::Vector2dF acceleration[2];
  for (size_t i = 0; i < valid_positions - 2; ++i)
    acceleration[i] = velocity[i] - velocity[i + 1];
  // acceleration[0] is always valid (zero if |valid_positions| < 3).

  gfx::Vector2dF jerk;
  if (valid_positions > 3)
    jerk = acceleration[0] - acceleration[1];
  // |jerk| is aways valid (zero if |valid_positions| < 4).

  // Adjust max prediction time based on speed as prediction data is not great
  // at lower speeds.
  const float kMaxPredictionScaleSpeed = 1e-5;
  double speed = velocity[0].LengthSquared();
  base::TimeTicks max_prediction_time =
      current_time +
      std::min(prediction_duration * (speed / kMaxPredictionScaleSpeed),
               prediction_duration);

  // Add predicted points until we reach the max prediction time.
  gfx::PointF location = position[0];
  for (base::TimeTicks time = newest_real_point.time + prediction_interval;
       time < max_prediction_time; time += prediction_interval) {
    // Note: Currently there's no need to multiply by the prediction interval
    // as the velocity is calculated based on a time delta between points that
    // is the same as the prediction interval.
    velocity[0] += acceleration[0];
    acceleration[0] += jerk;
    location += velocity[0];

    AddPoint(gfx::ScalePoint(location, 1 / scale.x(), 1 / scale.y()), time,
             newest_real_point.color);

    // Always stop at three predicted points as a four point history doesn't
    // provide accurate prediction of more points.
    if (GetNumberOfPoints() == 3)
      break;
  }
}

}  // namespace ash
