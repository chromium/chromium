// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/position.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace arc::input_overlay {

namespace {
// Epsilon value used to compare float values to zero.
const float kEpsilon = 1e-3f;

// For default position.
constexpr const char kValidJson[] =
    R"json({"anchor": [
      0,
      0
    ],
    "anchor_to_target": [
      0.5,
      0.5
    ]
    })json";

// The default value for anchor point is (0, 0) if it is not set.
constexpr const char kValidJsonNoAnchorPoint[] =
    R"json({"anchor_to_target": [
      0.1796875,
      0.25
    ]
    })json";

constexpr const char kInValidJsonWrongAnchorPoint[] =
    R"json({"anchor": [
      1.0,
      2
    ],
    "anchor_to_target": [
      -0.1796875,
      -0.25
    ]
    })json";

constexpr const char kInValidJsonIncompleteAnchorPoint[] =
    R"json({"anchor": [
      1.0
    ],
    "anchor_to_target": [
      -0.1796875,
      -0.25
    ]
    })json";

constexpr const char kInValidJsonTooMuchVectorToTarget[] =
    R"json({"anchor": [
      1.0,
      1.0
    ],
    "anchor_to_target": [
      -0.1796875,
      -2,
      3
    ]
    })json";

constexpr const char kInValidJsonWrongVectorToTarget[] =
    R"json({"anchor": [
      1.0,
      1.0
    ],
    "anchor_to_target": [
      -0.1796875,
      -2
    ]
    })json";

constexpr const char kInValidJsonOutSideWindow[] =
    R"json({"anchor": [
      1.0,
      0.0
    ],
    "anchor_to_target": [
      -0.1796875,
      -0.5
    ]
    })json";

constexpr const char kJsonCalculateTargetUpperLeft[] =
    R"json({"anchor": [
      1.0,
      1.0
    ],
    "anchor_to_target": [
      -0.8,
      -0.8
    ]
    })json";

// For dependent position.
// Aspect-ratio-dependent positiion with default anchor.
constexpr const char kValidJsonAspectRatio[] =
    R"json({
    "anchor_to_target": [
      0.5,
      0.5
    ],
    "aspect_ratio": 1.5,
    "x_on_y": 0.8,
    "y_on_x": 0.6
    })json";

// Invalid aspect-ratio-dependent position missing value of x_on_y.
constexpr const char kInValidJsonAspectRatioNoXonY[] =
    R"json({
    "anchor_to_target": [
      0.5,
      0.5
    ],
    "aspect_ratio": 1.5,
    "y_on_x": 0.6
    })json";

// Invalid position has both x_on_y and y_on_x.
constexpr const char kInvalidJsonBothDependent[] =
    R"json({
    "anchor_to_target": [
      0.5,
      0.5
    ],
    "x_on_y": 1.5,
    "y_on_x": 0.6
    })json";

// Height-dependent position with default anchor.
constexpr const char kValidJsonHeightDependent[] =
    R"json({
    "anchor_to_target": [
      0.5,
      0.5
    ],
    "x_on_y": 0.8
    })json";

// Width-dependent position with default anchor.
constexpr const char kValidJsonWidthDependent[] =
    R"json({
    "anchor_to_target": [
      0.5,
      0.5
    ],
    "y_on_x": 0.8
    })json";

// Invalid Json with x_on_y negative.
constexpr const char kInValidXonYJson[] =
    R"json({
    "anchor_to_target": [
      0.5,
      0.5
    ],
    "x_on_y": -1.8
    })json";

// Height-dependent position with anchor on bottom-right.
constexpr const char kValidJsonHeightDepAnchorBR[] =
    R"json({
    "anchor": [
      1,
      1
    ],
    "anchor_to_target": [
      -0.5,
      -0.5
    ],
    "x_on_y": 0.8
    })json";

// Height-dependent position with anchor on bottom-left.
constexpr const char kValidJsonHeightDepAnchorBL[] =
    R"json({
    "anchor": [
      0,
      1
    ],
    "anchor_to_target": [
      0.5,
      -0.5
    ],
    "x_on_y": 0.8
    })json";

// Height-dependent position with anchor on top-right.
constexpr const char kValidJsonHeightDepAnchorTR[] =
    R"json({
    "anchor": [
      1,
      0
    ],
    "anchor_to_target": [
      -0.5,
      0.5
    ],
    "x_on_y": 0.8
    })json";

// Width-dependent position with anchor on bottom-right.
constexpr const char kValidJsonWidthDepAnchorBR[] =
    R"json({
    "anchor": [
      1,
      1
    ],
    "anchor_to_target": [
      -0.5,
      -0.5
    ],
    "y_on_x": 0.8
    })json";

// Width-dependent position with anchor on bottom-left.
constexpr const char kValidJsonWidthDepAnchorBL[] =
    R"json({
    "anchor": [
      0,
      1
    ],
    "anchor_to_target": [
      0.5,
      -0.5
    ],
    "y_on_x": 0.8
    })json";

// Height-dependent position with anchor on top-right.
constexpr const char kValidJsonWidthDepAnchorTR[] =
    R"json({
    "anchor": [
      1,
      0
    ],
    "anchor_to_target": [
      -0.5,
      0.5
    ],
    "y_on_x": 0.8
    })json";

}  // namespace

class PositionTest : public testing::Test {
 protected:
  PositionTest() = default;
};

TEST(PositionTest, TestParseJson) {
  // For default position
  // Parse valid Json.
  auto pos = std::make_unique<Position>(PositionType::kDefault);
  auto json_value = base::JSONReader::ReadAndReturnValueWithError(kValidJson);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_TRUE(pos->ParseFromJson(json_value->GetDict()));
  EXPECT_EQ(pos->anchor(), gfx::PointF(0, 0));
  EXPECT_EQ(pos->anchor_to_target(), gfx::Vector2dF(0.5, 0.5));
  pos.reset();

  // Parse valid Json without anchor point.
  pos = std::make_unique<Position>(PositionType::kDefault);
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonNoAnchorPoint);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_TRUE(pos->ParseFromJson(json_value->GetDict()));
  EXPECT_EQ(pos->anchor(), gfx::PointF(0, 0));
  EXPECT_EQ(pos->anchor_to_target(), gfx::Vector2dF(0.1796875, 0.25));
  pos.reset();

  // Parse invalid Json with wrong anchor point.
  pos = std::make_unique<Position>(PositionType::kDefault);
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonWrongAnchorPoint);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value->GetDict()));
  pos.reset();

  // Parse invalid Json with incomplete anchor point.
  pos = std::make_unique<Position>(PositionType::kDefault);
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonIncompleteAnchorPoint);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value->GetDict()));
  pos.reset();

  // Parse invalid Json with too much values for vector to target.
  pos = std::make_unique<Position>(PositionType::kDefault);
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonTooMuchVectorToTarget);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value->GetDict()));
  pos.reset();

  // Parse invalid Json with wrong vector to target.
  pos = std::make_unique<Position>(PositionType::kDefault);
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonWrongVectorToTarget);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value->GetDict()));
  pos.reset();

  // Parse invalid Json with target position outside of the window.
  pos = std::make_unique<Position>(PositionType::kDefault);
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kInValidJsonOutSideWindow);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value->GetDict()));
  pos.reset();

  // Parse valid Json for aspect ratio dependent position.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonAspectRatio);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_TRUE(pos->ParseFromJson(json_value->GetDict()));
  EXPECT_LT(std::abs(*pos->aspect_ratio() - 1.5), kEpsilon);
  EXPECT_LT(std::abs(*pos->x_on_y() - 0.8), kEpsilon);
  EXPECT_LT(std::abs(*pos->y_on_x() - 0.6), kEpsilon);

  // Parse invalid Json for aspect ration dependent position - missing x_on_y.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonAspectRatioNoXonY);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value->GetDict()));

  // Parse valid Json for height dependent position.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonHeightDependent);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_TRUE(pos->ParseFromJson(json_value->GetDict()));
  EXPECT_LT(std::abs(*pos->x_on_y() - 0.8), kEpsilon);

  // Parse invalid Json for non-aspect-ratio-dependent position - present both
  // x_on_y and y_on_x.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kInvalidJsonBothDependent);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value->GetDict()));

  // Parse Json with invalid x_on_y value.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value = base::JSONReader::ReadAndReturnValueWithError(kInValidXonYJson);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value->GetDict()));
}

TEST(PositionTest, TestCalculateDefaultPosition) {
  // Calculate the target position in the center.
  auto pos = std::make_unique<Position>(PositionType::kDefault);
  auto json_value = base::JSONReader::ReadAndReturnValueWithError(kValidJson);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  auto bounds = gfx::RectF(200, 400);
  auto target = pos->CalculatePosition(bounds);
  EXPECT_EQ(target, gfx::PointF(100, 200));
  pos.reset();

  // Calculate the target position with anchor point at the bottom-right corner.
  pos = std::make_unique<Position>(PositionType::kDefault);
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kJsonCalculateTargetUpperLeft);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 40), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 80), kEpsilon);
  bounds.set_width(300);
  bounds.set_height(400);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 60), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 80), kEpsilon);
  pos.reset();
}

TEST(PositionTest, TestCalculatePositionHeightDependent) {
  // Parse the position with the default anchor.
  auto pos = std::make_unique<Position>(PositionType::kDependent);
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonHeightDependent);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  auto bounds = gfx::RectF(200, 400);
  auto target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 160), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 200), kEpsilon);
  // Give a height which may calculate the x value outside of the window bounds.
  // The result x should be inside of the window bounds.
  bounds.set_height(600);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 199), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 300), kEpsilon);

  // Parse the position with anchor on the bottom-right corner.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kValidJsonHeightDepAnchorBR);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  bounds.set_height(400);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 40), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 200), kEpsilon);
  // Give a height which may calculate the x value outside of the window bounds.
  // The result x should be inside of the window bounds.
  bounds.set_height(600);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x()), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 300), kEpsilon);

  // Parse the position with anchor on the bottom-left corner.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kValidJsonHeightDepAnchorBL);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  bounds.set_height(400);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 160), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 200), kEpsilon);

  // Parse the position with anchor on the top-right corner.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kValidJsonHeightDepAnchorTR);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  bounds.set_height(400);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 40), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 200), kEpsilon);
}

TEST(PositionTest, TestCalculatePositionWidthDependent) {
  // Parse the position with the default anchor.
  auto pos = std::make_unique<Position>(PositionType::kDependent);
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonWidthDependent);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  auto bounds = gfx::RectF(200, 400);
  auto target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 100), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 80), kEpsilon);
  // Give a width which may calculate the y value outside of the window bounds.
  // The result y should be inside of the window bounds.
  bounds.set_width(1200);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 600), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 399), kEpsilon);

  // Parse the position with anchor on the bottom-right corner.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonWidthDepAnchorBR);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  bounds.set_width(200);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 100), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 320), kEpsilon);
  // Give a width which may calculate the y value outside of the window bounds.
  // The result y should be inside of the window bounds.
  bounds.set_width(1200);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 600), kEpsilon);
  EXPECT_LT(std::abs(target.y()), kEpsilon);

  // Parse the position with anchor on the bottom-left corner.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonWidthDepAnchorBL);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  bounds.set_width(200);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 100), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 320), kEpsilon);

  // Parse the position with anchor on the top-right corner.
  pos = std::make_unique<Position>(PositionType::kDependent);
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonWidthDepAnchorTR);
  pos->ParseFromJson(json_value->GetDict());
  bounds.set_width(200);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 100), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 80), kEpsilon);
}

TEST(PositionTest, TestCalculatePositionAspectRatioDependent) {
  auto pos = std::make_unique<Position>(PositionType::kDependent);
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonAspectRatio);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  auto bounds = gfx::RectF(200, 400);
  auto target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 100), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 60), kEpsilon);
  bounds.set_width(800);
  target = pos->CalculatePosition(bounds);
  EXPECT_LT(std::abs(target.x() - 160), kEpsilon);
  EXPECT_LT(std::abs(target.y() - 200), kEpsilon);
}

TEST(PositionTest, TestPositionEquality) {
  auto pos1 = std::make_unique<Position>(PositionType::kDefault);
  auto pos2 = std::make_unique<Position>(PositionType::kDefault);
  EXPECT_EQ(*pos1, *pos2);
  auto pos3 = std::make_unique<Position>(PositionType::kDependent);
  EXPECT_NE(*pos1, *pos3);
  pos2->Normalize(gfx::Point(10, 20), gfx::RectF(100, 150));
  EXPECT_NE(*pos1, *pos2);
  pos1->Normalize(gfx::Point(10, 20), gfx::RectF(100, 150));
  EXPECT_EQ(*pos1, *pos2);
  pos3->Normalize(gfx::Point(10, 20), gfx::RectF(100, 150));
  EXPECT_EQ(*pos1, *pos3);
}

TEST(PositionTest, TestProtoConversion) {
  auto pos = std::make_unique<Position>(PositionType::kDefault);
  auto json_value = base::JSONReader::ReadAndReturnValueWithError(kValidJson);
  EXPECT_TRUE(json_value.has_value());
  EXPECT_TRUE(json_value->is_dict());
  pos->ParseFromJson(json_value->GetDict());
  auto proto = pos->ConvertToProto();
  EXPECT_FALSE(proto->anchor_to_target().empty());
  EXPECT_EQ(proto->anchor_to_target().size(), 2);
  EXPECT_FLOAT_EQ(proto->anchor_to_target()[0], 0.5);
  EXPECT_FLOAT_EQ(proto->anchor_to_target()[1], 0.5);
  auto deserialized = Position::ConvertFromProto(*proto);
  EXPECT_VECTOR2DF_EQ(deserialized->anchor_to_target(),
                      gfx::Vector2dF(0.5, 0.5));
}

}  // namespace arc::input_overlay
