// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/position.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace arc::input_overlay {
namespace {
// Key strings in the Json file.
constexpr char kAnchor[] = "anchor";
constexpr char kAnchorToTarget[] = "anchor_to_target";
constexpr char kMaxX[] = "max_x";
constexpr char kMaxY[] = "max_y";

// Key strings for dependent position only.
constexpr char kAspectRatio[] = "aspect_ratio";
constexpr char kXonY[] = "x_on_y";
constexpr char kYonX[] = "y_on_x";

std::optional<gfx::PointF> ParseTwoElementsArray(const base::Value::Dict& value,
                                                 const char* key,
                                                 bool required) {
  const base::Value::List* list = value.FindList(key);
  if (!list) {
    if (required) {
      LOG(ERROR) << "Require values for key " << key;
    }
    return std::nullopt;
  }
  if (list->size() != 2) {
    LOG(ERROR) << "Require two elements for " << key << ". But got "
               << list->size() << " elements.";
    return std::nullopt;
  }
  const double x = list->front().GetDouble();
  const double y = list->back().GetDouble();
  if (std::abs(x) > 1 || std::abs(y) > 1) {
    LOG(ERROR) << "Require normalized values for " << key << ". But got x{" << x
               << "}, y{" << y << "}";
    return std::nullopt;
  }
  return std::make_optional<gfx::PointF>(x, y);
}

std::optional<int> ParseIntValue(const base::Value::Dict& value,
                                 std::string key) {
  if (std::optional<int> val = value.FindInt(key)) {
    if (*val <= 0) {
      LOG(ERROR) << "Value for {" << key << "} should be positive, but got {"
                 << *val << "}.";
      return std::nullopt;
    }
    return val;
  }
  return std::nullopt;
}

float CalculateDependent(const gfx::PointF& anchor,
                         const gfx::Vector2dF& anchor_to_target,
                         bool height_dependent,
                         float dependent,
                         const gfx::RectF& content_bounds) {
  float res;
  if (height_dependent) {
    const float anchor_to_target_y =
        std::abs(anchor_to_target.y()) * content_bounds.height();
    res = anchor.x() * content_bounds.width() +
          (anchor_to_target.x() < 0 ? -1 : 1) * anchor_to_target_y * dependent;
    if (res >= content_bounds.width()) {
      res = content_bounds.width() - 1;
    }
  } else {
    const float anchor_to_target_x =
        std::abs(anchor_to_target.x()) * content_bounds.width();
    res = anchor.y() * content_bounds.height() +
          (std::signbit(anchor_to_target.y()) ? -1 : 1) * anchor_to_target_x *
              dependent;
    if (res >= content_bounds.height()) {
      res = content_bounds.height() - 1;
    }
  }
  // Make sure it is inside of the window bounds.
  return std::max(0.0f, res);
}

}  // namespace

bool ParsePositiveFraction(const base::Value::Dict& value,
                           const char* key,
                           std::optional<float>* output) {
  *output = value.FindDouble(key);
  if (*output && **output <= 0) {
    LOG(ERROR) << "Require positive value of " << key << ". But got {"
               << output->value() << "}.";
    return false;
  }
  return true;
}

Position::Position(PositionType type) : position_type_(type) {}
Position::Position(const Position&) = default;
Position& Position::operator=(const Position&) = default;
Position::~Position() = default;

// static
std::unique_ptr<Position> Position::ConvertFromProto(
    const PositionProto& proto) {
  DCHECK_EQ(proto.anchor_to_target().size(), 2);
  auto position = std::make_unique<Position>(PositionType::kDefault);
  position->set_anchor_to_target(
      gfx::Vector2dF(proto.anchor_to_target()[0], proto.anchor_to_target()[1]));
  return position;
}

bool Position::ParseFromJson(const base::Value::Dict& value) {
  switch (position_type_) {
    case PositionType::kDefault:
      return ParseDefaultFromJson(value);
    case PositionType::kDependent:
      return ParseDependentFromJson(value);
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

gfx::PointF Position::CalculatePosition(
    const gfx::RectF& content_bounds) const {
  switch (position_type_) {
    case PositionType::kDefault:
      return CalculateDefaultPosition(content_bounds);
    case PositionType::kDependent:
      return CalculateDependentPosition(content_bounds);
    default:
      NOTREACHED_IN_MIGRATION();
      return gfx::PointF();
  }
}

bool Position::ParseDefaultFromJson(const base::Value::Dict& value) {
  // Parse anchor point if existing, or the anchor point is (0, 0).
  if (auto anchor = ParseTwoElementsArray(value, kAnchor, false)) {
    anchor_.set_x(anchor.value().x());
    anchor_.set_y(anchor.value().y());
  } else {
    LOG(WARNING) << "Anchor is assigned to default (0, 0).";
  }
  // Parse the vector which starts from anchor point to the target position.
  auto anchor_to_target = ParseTwoElementsArray(value, kAnchorToTarget, true);
  if (!anchor_to_target) {
    return false;
  }
  anchor_to_target_.set_x(anchor_to_target.value().x());
  anchor_to_target_.set_y(anchor_to_target.value().y());

  if (const auto target = anchor_ + anchor_to_target_;
      !gfx::RectF(1.0, 1.0).Contains(target)) {
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

bool Position::ParseDependentFromJson(const base::Value::Dict& value) {
  if (!ParseDefaultFromJson(value)) {
    return false;
  }
  if (!ParsePositiveFraction(value, kAspectRatio, &aspect_ratio_)) {
    return false;
  }
  if (!ParsePositiveFraction(value, kXonY, &x_on_y_)) {
    return false;
  }
  if (!ParsePositiveFraction(value, kYonX, &y_on_x_)) {
    return false;
  }

  if (aspect_ratio_ && (!x_on_y_ || !y_on_x_)) {
    LOG(ERROR) << "Require both x_on_y and y_on_x is aspect_ratio is set.";
    return false;
  }

  if (!aspect_ratio_ && ((x_on_y_ && y_on_x_) || (!x_on_y_ && !y_on_x_))) {
    LOG(ERROR)
        << "Require only one of x_on_y or y_on_x if aspect_ratio is not set.";
    return false;
  }

  if (!aspect_ratio_) {
    if (x_on_y_) {
      aspect_ratio_ = 0;
    } else {
      aspect_ratio_ = std::numeric_limits<float>::max();
    }
  }

  return true;
}

gfx::PointF Position::CalculateDefaultPosition(
    const gfx::RectF& content_bounds) const {
  auto res = anchor_ + anchor_to_target_;
  res.Scale(content_bounds.width(), content_bounds.height());
  if (max_x_) {
    res.set_x(std::min((int)res.x(), *max_x_));
  }
  if (max_y_) {
    res.set_y(std::min((int)res.y(), *max_y_));
  }
  return res;
}

gfx::PointF Position::CalculateDependentPosition(
    const gfx::RectF& content_bounds) const {
  auto res = Position::CalculateDefaultPosition(content_bounds);
  const float cur_aspect_ratio =
      1.0f * content_bounds.width() / content_bounds.height();
  if (cur_aspect_ratio >= *aspect_ratio_) {
    const float x = CalculateDependent(anchor_, anchor_to_target_, true,
                                       *x_on_y_, content_bounds);
    res.set_x(x);
  } else {
    const float y = CalculateDependent(anchor_, anchor_to_target_, false,
                                       *y_on_x_, content_bounds);
    res.set_y(y);
  }
  return res;
}

void Position::Normalize(const gfx::Point& point,
                         const gfx::RectF& content_bounds) {
  position_type_ = PositionType::kDefault;
  anchor_.SetPoint(0, 0);
  anchor_to_target_ = gfx::Vector2dF(1.0 * point.x() / content_bounds.width(),
                                     1.0 * point.y() / content_bounds.height());
  x_on_y_.reset();
  y_on_x_.reset();
  aspect_ratio_.reset();
}

std::unique_ptr<PositionProto> Position::ConvertToProto() const {
  auto proto = std::make_unique<PositionProto>();
  proto->add_anchor_to_target(anchor_to_target_.x());
  proto->add_anchor_to_target(anchor_to_target_.y());
  return proto;
}

bool Position::operator==(const Position& other) const {
  if (this->position_type_ != other.position_type() ||
      this->anchor_ != other.anchor() ||
      this->anchor_to_target_ != other.anchor_to_target() ||
      this->x_on_y_ != other.x_on_y() || this->y_on_x_ != other.y_on_x() ||
      this->aspect_ratio_ != other.aspect_ratio()) {
    return false;
  }
  return true;
}

bool Position::operator!=(const Position& other) const {
  return !(*this == other);
}

}  // namespace arc::input_overlay
