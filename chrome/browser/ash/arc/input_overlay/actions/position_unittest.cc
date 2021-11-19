// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/actions/position.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace input_overlay {

class PositionTest : public testing::Test {
 protected:
  PositionTest() = default;
};

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

TEST(PositionTest, TestParseJson) {
  // Parse valid Json.
  std::unique_ptr<Position> pos = std::make_unique<Position>();
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJson);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_TRUE(pos->ParseFromJson(json_value.value.value()));
  EXPECT_TRUE(pos->anchor() == gfx::PointF(0, 0));
  EXPECT_TRUE(pos->anchor_to_target() == gfx::Vector2dF(0.5, 0.5));
  pos.reset();

  // Parse valid Json without anchor point.
  pos = std::make_unique<Position>();
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonNoAnchorPoint);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_TRUE(pos->ParseFromJson(json_value.value.value()));
  EXPECT_TRUE(pos->anchor() == gfx::PointF(0, 0));
  EXPECT_TRUE(pos->anchor_to_target() == gfx::Vector2dF(0.1796875, 0.25));
  pos.reset();

  // Parse invalid Json with wrong anchor point.
  pos = std::make_unique<Position>();
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonWrongAnchorPoint);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value.value.value()));
  pos.reset();

  // Parse invalid Json with incomplete anchor point.
  pos = std::make_unique<Position>();
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonIncompleteAnchorPoint);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value.value.value()));
  pos.reset();

  // Parse invalid Json with too much values for vector to target.
  pos = std::make_unique<Position>();
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonTooMuchVectorToTarget);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value.value.value()));
  pos.reset();

  // Parse invalid Json with wrong vector to target.
  pos = std::make_unique<Position>();
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kInValidJsonWrongVectorToTarget);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value.value.value()));
  pos.reset();

  // Parse invalid Json with target position outside of the window.
  pos = std::make_unique<Position>();
  json_value =
      base::JSONReader::ReadAndReturnValueWithError(kInValidJsonOutSideWindow);
  EXPECT_FALSE(!json_value.value || !json_value.value->is_dict());
  EXPECT_FALSE(pos->ParseFromJson(json_value.value.value()));
  pos.reset();
}

TEST(PositionTest, TestCalculatePosition) {
  // Calculate the target position in the center.
  std::unique_ptr<Position> pos = std::make_unique<Position>();
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJson);
  pos->ParseFromJson(json_value.value.value());
  gfx::RectF bounds(200, 400);
  gfx::PointF target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(target == gfx::PointF(100, 200));
  pos.reset();

  // Calculate the target position with anchor point at the bottom-right corner.
  pos = std::make_unique<Position>();
  json_value = base::JSONReader::ReadAndReturnValueWithError(
      kJsonCalculateTargetUpperLeft);
  pos->ParseFromJson(json_value.value.value());
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 40) < 0.0001);
  EXPECT_TRUE(std::abs(target.y() - 80) < 0.0001);
  bounds.set_width(300);
  bounds.set_height(400);
  target = pos->CalculatePosition(bounds);
  EXPECT_TRUE(std::abs(target.x() - 60) < 0.0001);
  EXPECT_TRUE(std::abs(target.y() - 80) < 0.0001);
  pos.reset();
}

}  // namespace input_overlay
}  // namespace arc
