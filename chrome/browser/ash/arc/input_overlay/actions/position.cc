// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/position.h"

#include "base/logging.h"

namespace arc {
namespace input_overlay {
namespace {
// Key strings in the Json file.
constexpr char kAnchor[] = "anchor";
constexpr char kAnchorToTarget[] = "anchor_to_target";
constexpr char kMaxX[] = "max_x";
constexpr char kMaxY[] = "max_y";

absl::optional<gfx::PointF> ParseTwoElementsArray(const base::Value& value,
                                                  const char* key,
                                                  bool required) {
  const base::Value* list_value = value.FindListKey(key);
  if (!list_value) {
    if (required)
      LOG(ERROR) << "Require values for key " << key;
    return absl::nullopt;
  }
  if (!list_value->is_list()) {
    LOG(ERROR) << "Require list values for key " << key;
    return absl::nullopt;
  }
  auto list = list_value->GetListDeprecated();
  if (list.size() != 2) {
    LOG(ERROR) << "Require two elements for " << key << ". But got "
               << list.size() << " elements.";
    return absl::nullopt;
  }
  double x = list.front().GetDouble();
  double y = list.back().GetDouble();
  if (std::abs(x) > 1 || std::abs(y) > 1) {
    LOG(ERROR) << "Require normalized values for " << key << ". But got x{" << x
               << "}, y{" << y << "}";
    return absl::nullopt;
  }
  return absl::make_optional<gfx::PointF>(x, y);
}

absl::optional<int> ParseIntValue(const base::Value& value, std::string key) {
  auto val = value.FindIntKey(key);
  if (val) {
    if (*val <= 0) {
      LOG(ERROR) << "Value for {" << key << "} should be positive, but got {"
                 << *val << "}.";
      return absl::nullopt;
    }
    return val;
  }
  return absl::nullopt;
}

}  // namespace

Position::Position() {}
Position::~Position() = default;

bool Position::ParseFromJson(const base::Value& value) {
  // Parse anchor point if existing, or the anchor point is (0, 0).
  auto anchor = ParseTwoElementsArray(value, kAnchor, false);
  if (!anchor) {
    LOG(WARNING) << "Anchor is assigned to default (0, 0).";
  } else {
    anchor_.set_x(anchor.value().x());
    anchor_.set_y(anchor.value().y());
  }
  // Parse the vector which starts from anchor point to the target position.
  auto anchor_to_target = ParseTwoElementsArray(value, kAnchorToTarget, true);
  if (!anchor_to_target)
    return false;
  anchor_to_target_.set_x(anchor_to_target.value().x());
  anchor_to_target_.set_y(anchor_to_target.value().y());

  gfx::PointF target = anchor_ + anchor_to_target_;
  if (!gfx::RectF(1.0, 1.0).Contains(target)) {
    LOG(ERROR)
        << "The target position is located at outside of the window. The value "
           "should be within [0, 1]. But got target {"
        << target.x() << ", " << target.y() << "}.";
    return false;
  }

  max_x_ = ParseIntValue(value, kMaxX);
  max_y_ = ParseIntValue(value, kMaxY);

  return true;
}

gfx::PointF Position::CalculatePosition(const gfx::RectF& content_bounds) {
  gfx::PointF res = anchor_ + anchor_to_target_;
  res.Scale(content_bounds.width(), content_bounds.height());
  if (max_x_)
    res.set_x(std::min((int)res.x(), *max_x_));
  if (max_y_)
    res.set_y(std::min((int)res.y(), *max_y_));
  return res;
}

}  // namespace input_overlay
}  // namespace arc
