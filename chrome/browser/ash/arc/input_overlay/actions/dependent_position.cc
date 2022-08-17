// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/dependent_position.h"

#include "base/logging.h"

namespace arc {
namespace input_overlay {
namespace {

// Json strings.
constexpr char kAspectRatio[] = "aspect_ratio";
constexpr char kXonY[] = "x_on_y";
constexpr char kYonX[] = "y_on_x";

float CalculateDependent(const gfx::PointF& anchor,
                         const gfx::Vector2dF& anchor_to_target,
                         bool height_dependent,
                         float dependent,
                         const gfx::RectF& content_bounds) {
  float res;
  if (height_dependent) {
    float anchor_to_target_y =
        std::abs(anchor_to_target.y()) * content_bounds.height();
    res = anchor.x() * content_bounds.width() +
          (anchor_to_target.x() < 0 ? -1 : 1) * anchor_to_target_y * dependent;
    if (res >= content_bounds.width())
      res = content_bounds.width() - 1;
  } else {
    float anchor_to_target_x =
        std::abs(anchor_to_target.x()) * content_bounds.width();
    res = anchor.y() * content_bounds.height() +
          (std::signbit(anchor_to_target.y()) ? -1 : 1) * anchor_to_target_x *
              dependent;
    if (res >= content_bounds.height())
      res = content_bounds.height() - 1;
  }
  // Make sure it is inside of the window bounds.
  return std::max(0.f, res);
}
}  // namespace

bool ParsePositiveFraction(const base::Value& value,
                           const char* key,
                           absl::optional<float>* output) {
  *output = value.FindDoubleKey(key);
  if (*output && **output <= 0) {
    LOG(ERROR) << "Require positive value of " << key << ". But got {"
               << output->value() << "}.";
    return false;
  }
  return true;
}

DependentPosition::DependentPosition() = default;
DependentPosition::DependentPosition(const DependentPosition&) = default;
DependentPosition& DependentPosition::operator=(const DependentPosition&) =
    default;
DependentPosition::~DependentPosition() = default;

bool DependentPosition::ParseFromJson(const base::Value& value) {
  bool succeed = Position::ParseFromJson(value);
  if (!succeed)
    return false;
  if (!ParsePositiveFraction(value, kAspectRatio, &aspect_ratio_))
    return false;
  if (!ParsePositiveFraction(value, kXonY, &x_on_y_))
    return false;
  if (!ParsePositiveFraction(value, kYonX, &y_on_x_))
    return false;

  if (aspect_ratio_ && (!x_on_y_ || !y_on_x_)) {
    LOG(ERROR) << "Require both x_on_y and y_on_x is aspect_ratio is "
                  "set.";
    return false;
  }

  if (!aspect_ratio_ && ((x_on_y_ && y_on_x_) || (!x_on_y_ && !y_on_x_))) {
    LOG(ERROR)
        << "Require only one of x_on_y or y_on_x if aspect_ratio is not set.";
    return false;
  }

  if (!aspect_ratio_) {
    if (x_on_y_)
      aspect_ratio_ = 0;
    else
      aspect_ratio_ = std::numeric_limits<float>::max();
  }

  return true;
}

gfx::PointF DependentPosition::CalculatePosition(
    const gfx::RectF& content_bounds) const {
  gfx::PointF res = Position::CalculatePosition(content_bounds);
  float cur_aspect_ratio =
      1. * content_bounds.width() / content_bounds.height();
  if (cur_aspect_ratio >= *aspect_ratio_) {
    float x = CalculateDependent(anchor(), anchor_to_target(), true, *x_on_y_,
                                 content_bounds);
    res.set_x(x);
  } else {
    float y = CalculateDependent(anchor(), anchor_to_target(), false, *y_on_x_,
                                 content_bounds);
    res.set_y(y);
  }
  return res;
}

}  // namespace input_overlay
}  // namespace arc
