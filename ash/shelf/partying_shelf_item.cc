// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/partying_shelf_item.h"

#include <array>
#define _USE_MATH_DEFINES
#include <cmath>
#include <limits>
#include <memory>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "base/check_op.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Conversion factor from degrees to radians.
constexpr float kRadiansPerDegree = static_cast<float>(M_PI / 180.0);

// After this, the centerpoint of the item moves at constant speed in a straight
// line, although there are still squash/stretch fluctuations around that point.
constexpr float kBounceEndFrame = 11.f;

// After this, there is no squash/stretch. The item just goes straight.
constexpr float kSquashStretchEndFrame = 29.f;

// Returns a random bool. Thread-safe.
bool RandBool() {
  uint8_t byte;
  base::RandBytes(&byte, 1u);
  return static_cast<bool>(byte & 0x1);
}

// Returns a random float in range [0, `length`). Thread-safe.
float SampleInterval(float length) {
  return length * base::RandFloat();
}

// Returns a random float in range [`a`, `b`). Thread-safe.
float SampleInterval(float a, float b) {
  const float t = base::RandFloat();
  return a * (1.f - t) + b * t;
}

// Returns a random float in range [0, `interval_length`), excluding range
// [`gap_left`, `gap_left`+`gap_length`). Thread-safe.
float SampleIntervalWithGap(float interval_length,
                            float gap_left,
                            float gap_length) {
  float result = SampleInterval(interval_length - gap_length);
  if (result >= gap_left)
    result += gap_length;
  return result;
}

// Evaluates an animation curve at `t_star`. `N` is the number of segments. `t`
// and `value` give the keyframes. Mostly, each segment is interpolated with
// `gfx::Tween::EASE_IN_OUT`. If `rush_in` is true, then the first segment (if
// any) is interpolated with `gfx::Tween::EASE_OUT` instead. `t_star` is assumed
// to be no earlier than the first keyframe. If `t_star` is later than the last
// keyframe, this function returns the value at the last keyframe. In other
// words, the curve goes flat after the final keyframe.
template <size_t N>
float AnimationCurveValue(float t_star,
                          const std::array<float, N + 1>& t,
                          const std::array<float, N + 1>& value,
                          bool rush_in) {
  DCHECK_GE(t_star, t[0]);
  for (size_t i = 0u; i < N; ++i) {
    if (t_star < t[i + 1]) {
      return gfx::Tween::FloatValueBetween(
          gfx::Tween::CalculateValue(rush_in && i == 0u
                                         ? gfx::Tween::EASE_OUT
                                         : gfx::Tween::EASE_IN_OUT,
                                     (t_star - t[i]) / (t[i + 1] - t[i])),
          value[i], value[i + 1]);
    }
  }
  return value[N];
}

}  // namespace

PartyingShelfItem::PartyingShelfItem(aura::Window* root_window,
                                     const gfx::ImageSkia& image_skia,
                                     int icon_size)
    : gfx::LinearAnimation(this),
      widget_(new views::Widget),
      icon_size_(icon_size),
      speed_(RandBool() ? SampleInterval(0.25f, 1.0f)
                        : SampleInterval(1.0f, 4.0f)),
      leftward_or_upward_(RandBool()),
      travel_angle_in_degrees_(SampleInterval(45.f)) {
  // Set up `widget_`.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_ShelfContainer);
  params.bounds = gfx::Rect(params.parent->GetBoundsInScreen().origin(),
                            gfx::Size(icon_size, icon_size));
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.name = "PartyingShelfItem";
  widget_->set_focus_on_creation(false);
  widget_->Init(std::move(params));
  widget_->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  views::View* view =
      widget_->SetContentsView(std::make_unique<views::ImageView>(
          ui::ImageModel::FromImageSkia(image_skia)));
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
  // As the item arrives through the perimeter of the work area and possibly the
  // perimeter of the root window, we do not want it to be shown on an adjacent
  // root window.
  widget_->GetNativeWindow()->SetProperty(kLockedToRootKey, true);
  widget_->Show();

  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(widget_->GetNativeView());
  const float distance_clockwise_from_origin =
      RandDistanceClockwiseFromOriginToEntryPoint();
  gfx::PointF entry_point;
  if (distance_clockwise_from_origin < work_area.width()) {
    bounce_side_ = kTop;
    entry_point = gfx::PointF(work_area.x() + distance_clockwise_from_origin,
                              work_area.y());
  } else if (distance_clockwise_from_origin <
             work_area.width() + work_area.height()) {
    bounce_side_ = kRight;
    entry_point = gfx::PointF(
        work_area.right(),
        work_area.y() + distance_clockwise_from_origin - work_area.width());
  } else if (distance_clockwise_from_origin <
             2 * work_area.width() + work_area.height()) {
    bounce_side_ = kBottom;
    entry_point =
        gfx::PointF(work_area.right() - distance_clockwise_from_origin +
                        work_area.width() + work_area.height(),
                    work_area.bottom());
  } else {
    bounce_side_ = kLeft;
    entry_point = gfx::PointF(
        work_area.x(), work_area.bottom() - distance_clockwise_from_origin +
                           2 * work_area.width() + work_area.height());
  }
  // Start just outside the work area and enter through the chosen entry point.
  center_point_at_0_in_parent_ =
      entry_point -
      ScaleVector2d(ComputeTravelDirection(),
                    0.5f * icon_size_ /
                        cosf(travel_angle_in_degrees_ * kRadiansPerDegree));
  JustGoStraight();
  // Initialize the transform. Otherwise the item will appear at the origin for
  // one frame. Use `kSquashStretchEndFrame` because `JustGoStraight` works by
  // starting the animation from there.
  UpdateTransform(kSquashStretchEndFrame);
}

PartyingShelfItem::~PartyingShelfItem() {
  widget_->Close();
}

void PartyingShelfItem::AnimateToState(double state) {
  UpdateTransform(static_cast<float>(state) * mockup_length_);
}

void PartyingShelfItem::AnimationEnded(const gfx::Animation* animation) {
  // Start a new animation from where the old animation ends.
  center_point_at_0_in_parent_ = ComputeCenterPointInParent(mockup_length_);
  // Compute the new direction as the item bounces off `target_side_`.
  switch (target_side_) {
    case kTop:
      if (bounce_side_ == kLeft)
        leftward_or_upward_ = false;
      if (bounce_side_ == kLeft || bounce_side_ == kRight)
        travel_angle_in_degrees_ = 90.f - travel_angle_in_degrees_;
      break;
    case kBottom:
      if (bounce_side_ == kRight)
        leftward_or_upward_ = true;
      if (bounce_side_ == kLeft || bounce_side_ == kRight)
        travel_angle_in_degrees_ = 90.f - travel_angle_in_degrees_;
      break;
    case kLeft:
      if (bounce_side_ == kTop)
        leftward_or_upward_ = false;
      if (bounce_side_ == kTop || bounce_side_ == kBottom)
        travel_angle_in_degrees_ = 90.f - travel_angle_in_degrees_;
      break;
    case kRight:
      if (bounce_side_ == kBottom)
        leftward_or_upward_ = true;
      if (bounce_side_ == kTop || bounce_side_ == kBottom)
        travel_angle_in_degrees_ = 90.f - travel_angle_in_degrees_;
      break;
  }
  bounce_side_ = target_side_;
  // Go ahead with the new animation.
  Go();
}

void PartyingShelfItem::Go() {
  // Given the travel direction, there are two possibilities to consider for
  // `target_side_`, one horizontal and one vertical. We choose the one that
  // results in a shorter `mockup_length_`.
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(widget_->GetNativeView());
  const gfx::Vector2dF velocity =
      ScaleVector2d(ComputeTravelDirection(), ComputeTravelSpeed());
  const gfx::PointF center_point_after_bounce =
      ComputeCenterPointInParent(kBounceEndFrame);
  if (velocity.y() < 0.f) {
    target_side_ = kTop;
    mockup_length_ = kBounceEndFrame + (work_area.y() + 0.5f * icon_size_ -
                                        center_point_after_bounce.y()) /
                                           velocity.y();
  } else if (velocity.y() > 0.f) {
    target_side_ = kBottom;
    mockup_length_ = kBounceEndFrame + (work_area.bottom() - 0.5f * icon_size_ -
                                        center_point_after_bounce.y()) /
                                           velocity.y();
  } else {
    DCHECK_EQ(0.f, velocity.y());
    DCHECK_NE(0.f, velocity.x());
    mockup_length_ = std::numeric_limits<float>::infinity();
  }
  if (velocity.x() < 0.f) {
    const float time_to_left =
        kBounceEndFrame +
        (work_area.x() + 0.5f * icon_size_ - center_point_after_bounce.x()) /
            velocity.x();
    if (time_to_left < mockup_length_) {
      target_side_ = kLeft;
      mockup_length_ = time_to_left;
    }
  } else if (velocity.x() > 0.f) {
    const float time_to_right =
        kBounceEndFrame + (work_area.right() - 0.5f * icon_size_ -
                           center_point_after_bounce.x()) /
                              velocity.x();
    if (time_to_right < mockup_length_) {
      target_side_ = kRight;
      mockup_length_ = time_to_right;
    }
  }

  if (mockup_length_ < kBounceEndFrame) {
    // Go without squash and stretch, because the duration is too short.
    JustGoStraight();
  } else {
    // Alright, let's go!
    SetDuration(base::Seconds(1.f / 24.f * mockup_length_ / speed_));
    Start();
  }
}

void PartyingShelfItem::JustGoStraight() {
  // Offset `center_point_at_0_in_parent_` so that the new centerpoint at mockup
  // frame 29 equals the old `center_point_at_0_in_parent_`. That means we can
  // start the animation from mockup frame 29 and the item will proceed from an
  // unchanged position.
  const gfx::PointF center_point = ComputeCenterPointInIcon();
  const absl::optional<gfx::PointF> transformed_center_point =
      ComputeTransform(kSquashStretchEndFrame).InverseMapPoint(center_point);
  CHECK(transformed_center_point.has_value());
  center_point_at_0_in_parent_ +=
      transformed_center_point.value() - center_point;
  // Start the animation from mockup frame 29. That is when the squash and
  // stretch has completely settled down.
  Go();
  SetCurrentValue(kSquashStretchEndFrame / mockup_length_);
}

void PartyingShelfItem::UpdateTransform(float mockup_frame) {
  gfx::Transform transform = ComputeTransform(mockup_frame);
  transform.PostTranslate(center_point_at_0_in_parent_ -
                          ComputeCenterPointInIcon());
  widget_->GetContentsView()->SetTransform(transform);
}

gfx::Transform PartyingShelfItem::ComputeTransform(float mockup_frame) const {
  // Until mockup frame 9, the pivot is on the perimeter of the work area, to
  // facilitate crashing into it and springing away.
  gfx::PointF pivot_until_9;
  switch (bounce_side_) {
    case kTop:
      pivot_until_9 = leftward_or_upward_ ? gfx::PointF(0.f, 0.f)
                                          : gfx::PointF(icon_size_, 0.f);
      break;
    case kBottom:
      pivot_until_9 = leftward_or_upward_ ? gfx::PointF(0.f, icon_size_)
                                          : gfx::PointF(icon_size_, icon_size_);
      break;
    case kLeft:
      pivot_until_9 = leftward_or_upward_ ? gfx::PointF(0.f, 0.f)
                                          : gfx::PointF(0.f, icon_size_);
      break;
    case kRight:
      pivot_until_9 = leftward_or_upward_ ? gfx::PointF(icon_size_, 0.f)
                                          : gfx::PointF(icon_size_, icon_size_);
      break;
  }
  gfx::Transform transform = ComputeTransformAboutOrigin(mockup_frame);
  if (mockup_frame <= 9.f)
    return gfx::TransformAboutPivot(pivot_until_9, transform);

  // After mockup frame 9, the pivot is at the centerpoint, to facilitate
  // squash/stretch fluctuations as the item begins to travel from
  // `bounce_side_` to `target_side_`.
  const gfx::PointF center_point = ComputeCenterPointInIcon();
  gfx::PointF pivot_after_9 = center_point;
  transform = gfx::TransformAboutPivot(pivot_after_9, transform);

  // Counteract the displacement caused by the instantaneous change in pivot.
  const gfx::Transform transform_at_9 = ComputeTransformAboutOrigin(9.f);
  transform.PostTranslate(
      gfx::TransformAboutPivot(pivot_until_9, transform_at_9)
          .MapPoint(center_point) -
      gfx::TransformAboutPivot(pivot_after_9, transform_at_9)
          .MapPoint(center_point));

  return transform;
}

gfx::Transform PartyingShelfItem::ComputeTransformAboutOrigin(
    float mockup_frame) const {
  const float degrees = AnimationCurveValue<1u>(
      mockup_frame, /*t=*/{0.f, kBounceEndFrame},
      /*value=*/{-travel_angle_in_degrees_, travel_angle_in_degrees_},
      /*rush_in=*/false);
  const float travel_distance =
      mockup_frame <= 7.f ? 0.f : (mockup_frame - 7.f) * ComputeTravelSpeed();
  const float skate_distance = AnimationCurveValue<1u>(
      mockup_frame, /*t=*/{0.f, kBounceEndFrame},
      /*value=*/{0.f, 2.25f / 90.f * travel_angle_in_degrees_ * icon_size_},
      /*rush_in=*/false);
  const float scale = AnimationCurveValue<7u>(
      mockup_frame,
      /*t=*/{0.f, 5.f, 9.f, 13.f, 17.f, 21.f, 25.f, kSquashStretchEndFrame},
      /*value=*/{1.f, 0.3f, 1.4f, 0.8f, 1.1f, 0.95f, 1.025f, 1.f},
      /*rush_in=*/true);
  gfx::Transform transform;
  switch (bounce_side_) {
    case kTop:
      if (leftward_or_upward_) {
        transform.Translate(-skate_distance, 0.f);
        transform.Rotate(degrees);
        transform.Translate(0.f, travel_distance);
        transform.Scale(1.f, scale);
        transform.Rotate(-degrees);
      } else {
        transform.Translate(skate_distance, 0.f);
        transform.Rotate(-degrees);
        transform.Translate(0.f, travel_distance);
        transform.Scale(1.f, scale);
        transform.Rotate(degrees);
      }
      break;
    case kBottom:
      if (leftward_or_upward_) {
        transform.Translate(-skate_distance, 0.f);
        transform.Rotate(-degrees);
        transform.Translate(0.f, -travel_distance);
        transform.Scale(1.f, scale);
        transform.Rotate(degrees);
      } else {
        transform.Translate(skate_distance, 0.f);
        transform.Rotate(degrees);
        transform.Translate(0.f, -travel_distance);
        transform.Scale(1.f, scale);
        transform.Rotate(-degrees);
      }
      break;
    case kLeft:
      if (leftward_or_upward_) {
        transform.Translate(0.f, -skate_distance);
        transform.Rotate(-degrees);
        transform.Translate(travel_distance, 0.f);
        transform.Scale(scale, 1.f);
        transform.Rotate(degrees);
      } else {
        transform.Translate(0.f, skate_distance);
        transform.Rotate(degrees);
        transform.Translate(travel_distance, 0.f);
        transform.Scale(scale, 1.f);
        transform.Rotate(-degrees);
      }
      break;
    case kRight:
      if (leftward_or_upward_) {
        transform.Translate(0.f, -skate_distance);
        transform.Rotate(degrees);
        transform.Translate(-travel_distance, 0.f);
        transform.Scale(scale, 1.f);
        transform.Rotate(-degrees);
      } else {
        transform.Translate(0.f, skate_distance);
        transform.Rotate(-degrees);
        transform.Translate(-travel_distance, 0.f);
        transform.Scale(scale, 1.f);
        transform.Rotate(degrees);
      }
      break;
  }
  return transform;
}

gfx::PointF PartyingShelfItem::ComputeCenterPointInIcon() const {
  return gfx::PointF(0.5f * icon_size_, 0.5f * icon_size_);
}

gfx::PointF PartyingShelfItem::ComputeCenterPointInParent(
    float mockup_frame) const {
  const gfx::PointF center_point = ComputeCenterPointInIcon();
  return center_point_at_0_in_parent_ +
         (ComputeTransform(mockup_frame).MapPoint(center_point) - center_point);
}

float PartyingShelfItem::ComputeTravelSpeed() const {
  return (5.f - 0.4f) / (0.8f * 24.f) * icon_size_;
}

gfx::Vector2dF PartyingShelfItem::ComputeTravelDirection() const {
  const float radians = travel_angle_in_degrees_ * kRadiansPerDegree;
  float s = sinf(radians);
  if (leftward_or_upward_)
    s = -s;
  const float c = cosf(radians);
  switch (bounce_side_) {
    case kTop:
      return gfx::Vector2dF(s, c);
    case kBottom:
      return gfx::Vector2dF(s, -c);
    case kLeft:
      return gfx::Vector2dF(c, s);
    case kRight:
      return gfx::Vector2dF(-c, s);
  }
}

float PartyingShelfItem::RandDistanceClockwiseFromOriginToEntryPoint() const {
  aura::Window* window = widget_->GetNativeView();
  const Shelf* shelf = Shelf::ForWindow(window);
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window);
  const float perimeter = 2 * (work_area.width() + work_area.height());
  switch (shelf->GetVisibilityState()) {
    case SHELF_VISIBLE: {
      const float min_distance_from_shelf_side =
          ShelfConfig::Get()->shelf_size() + 0.5f * icon_size_;
      switch (shelf->alignment()) {
        case ShelfAlignment::kBottom:
        case ShelfAlignment::kBottomLocked:
          return SampleIntervalWithGap(
              perimeter,
              work_area.width() + work_area.height() -
                  min_distance_from_shelf_side,
              work_area.width() + 2 * min_distance_from_shelf_side);
        case ShelfAlignment::kLeft:
          return SampleInterval(min_distance_from_shelf_side,
                                2 * work_area.width() + work_area.height() -
                                    min_distance_from_shelf_side);
        case ShelfAlignment::kRight:
          return SampleIntervalWithGap(
              perimeter, work_area.width() - min_distance_from_shelf_side,
              work_area.height() + 2 * min_distance_from_shelf_side);
      }
    } break;
    case SHELF_AUTO_HIDE:
    case SHELF_HIDDEN:
      return SampleInterval(perimeter);
  }
}

}  // namespace ash
