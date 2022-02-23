// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/dependent_position.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace input_overlay {

class DependentPositionTest : public testing::Test {
 protected:
  DependentPositionTest() = default;
};

namespace {
// Epsilon value used to compare float values to zero.
const float kEpsilon = 1e-3f;

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

TEST(DependentPositionTest, TestParseJson) {
  // Parse valid Json for aspect ratio dependent position.
  std::unique_ptr<DependentPosition> pos =
      std::make_unique<DependentPosition>();
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonAspectRatio);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_TRUE(pos->ParseFromJson(json_value.value.value()));
  EXPECT_TRUE(std::abs(*pos->aspect_ratio() - 1.5) < kEpsilon);
  EXPECT_TRUE(std::abs(*pos->x_on_y() - 0.8) < kEpsilon);
  EXPECT_TRUE(std::abs(*pos->y_on_x() - 0.6) < kEpsilon);

  // Parse invalid Json for aspect ration dependent position - missing x_on_y.
  pos = std::make_unique<DependentPosition>();
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonAspectRatioNoXonY);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value.value.value()));

  // Parse valid Json for height dependent position.
  pos = std::make_unique<DependentPosition>();
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonHeightDependent);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_TRUE(pos->ParseFromJson(json_value.value.value()));
  EXPECT_TRUE(std::abs(*pos->x_on_y() - 0.8) < kEpsilon);

  // Parse invalid Json for non-aspect-ratio-dependent position - present both
  // x_on_y and y_on_x.
  pos = std::make_unique<DependentPosition>();
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kInvalidJsonBothDependent);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value.value.value()));

  // Parse Json with invalid x_on_y value.
  pos = std::make_unique<DependentPosition>();
  json_value = base::JSONReader::ReadAndReturnValueWithError(kInValidXonYJson);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value.value.value()));
}

TEST(DependentPositionTest, TestCalculatePositionHeightDependent) {
  // Parse the position with the default anchor.
  auto pos = std::make_unique<DependentPosition>();
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonHeightDependent);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  pos->ParseFromJson(json_value.value.value());
  gfx::RectF bounds(200, 400);
  gfx::PointF target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 160) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 200) < kEpsilon);
  // Give a height which may calculate the x value outside of the window bounds.
  // The result x should be inside of the window bounds.
  bounds.set_height(600);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 199) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 300) < kEpsilon);

  // Parse the position with anchor on the bottom-right corner.
  pos = std::make_unique<DependentPosition>();
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kValidJsonHeightDepAnchorBR);
  pos->ParseFromJson(json_value.value.value());
  bounds.set_height(400);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 40) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 200) < kEpsilon);
  // Give a height which may calculate the x value outside of the window bounds.
  // The result x should be inside of the window bounds.
  bounds.set_height(600);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x()) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 300) < kEpsilon);

  // Parse the position with anchor on the bottom-left corner.
  pos = std::make_unique<DependentPosition>();
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kValidJsonHeightDepAnchorBL);
  pos->ParseFromJson(json_value.value.value());
  bounds.set_height(400);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 160) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 200) < kEpsilon);

  // Parse the position with anchor on the top-right corner.
  pos = std::make_unique<DependentPosition>();
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kValidJsonHeightDepAnchorTR);
  pos->ParseFromJson(json_value.value.value());
  bounds.set_height(400);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 40) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 200) < kEpsilon);
}

TEST(DependentPositionTest, TestCalculatePositionWidthDependent) {
  // Parse the position with the default anchor.
  auto pos = std::make_unique<DependentPosition>();
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonWidthDependent);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  pos->ParseFromJson(json_value.value.value());
  gfx::RectF bounds(200, 400);
  gfx::PointF target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 100) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 80) < kEpsilon);
  // Give a width which may calculate the y value outside of the window bounds.
  // The result y should be inside of the window bounds.
  bounds.set_width(1200);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 600) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 399) < kEpsilon);

  // Parse the position with anchor on the bottom-right corner.
  pos = std::make_unique<DependentPosition>();
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonWidthDepAnchorBR);
  pos->ParseFromJson(json_value.value.value());
  bounds.set_width(200);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 100) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 320) < kEpsilon);
  // Give a width which may calculate the y value outside of the window bounds.
  // The result y should be inside of the window bounds.
  bounds.set_width(1200);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 600) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y()) < kEpsilon);

  // Parse the position with anchor on the bottom-left corner.
  pos = std::make_unique<DependentPosition>();
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonWidthDepAnchorBL);
  pos->ParseFromJson(json_value.value.value());
  bounds.set_width(200);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 100) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 320) < kEpsilon);

  // Parse the position with anchor on the top-right corner.
  pos = std::make_unique<DependentPosition>();
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonWidthDepAnchorTR);
  pos->ParseFromJson(json_value.value.value());
  bounds.set_width(200);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 100) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 80) < kEpsilon);
}

TEST(DependentPositionTest, TestCalculatePositionAspectRatioDependent) {
  auto pos = std::make_unique<DependentPosition>();
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonAspectRatio);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  pos->ParseFromJson(json_value.value.value());
  gfx::RectF bounds(200, 400);
  gfx::PointF target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 100) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 60) < kEpsilon);
  bounds.set_width(800);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 160) < kEpsilon);
  EXPECT_TRUE(std::abs(target.y() - 200) < kEpsilon);
}

}  // namespace input_overlay
}  // namespace arc
