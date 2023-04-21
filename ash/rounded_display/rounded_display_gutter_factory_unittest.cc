// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_gutter_factory.h"

#include <memory>
#include <vector>

#include "ash/rounded_display/rounded_display_gutter.h"
#include "base/ranges/algorithm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

using RoundedCorner = RoundedDisplayGutter::RoundedCorner;
using RoundedCornerPosition = RoundedDisplayGutter::RoundedCorner::Position;

constexpr gfx::RoundedCornersF kDefaultTestRadii(10, 12, 10, 12);
constexpr gfx::Size kTestDisplaySize(1920, 1080);

// The matcher matches a RoundedDisplayGutter that has the rounded corners of
// `positions`.
template <typename... Matchers>
auto GutterWithMatchingCorners(Matchers&&... positions) {
  return testing::ResultOf(
      "positions",
      [](const std::unique_ptr<RoundedDisplayGutter>& gutter) {
        std::vector<RoundedCornerPosition> positions;
        const std::vector<RoundedCorner>& corners = gutter->GetGutterCorners();
        base::ranges::transform(
            corners.begin(), corners.end(), std::back_inserter(positions),
            [](const RoundedCorner& corner) { return corner.position(); });
        return positions;
      },
      testing::UnorderedElementsAre(positions...));
}

class RoundedDisplayGutterFactoryTest : public testing::Test {
 public:
  RoundedDisplayGutterFactoryTest() = default;

  RoundedDisplayGutterFactoryTest(const RoundedDisplayGutterFactoryTest&) =
      delete;
  RoundedDisplayGutterFactoryTest& operator=(
      const RoundedDisplayGutterFactoryTest&) = delete;

  // testing::Test
  void SetUp() override {
    factory_ = std::make_unique<RoundedDisplayGutterFactory>();
  }

  std::unique_ptr<RoundedDisplayGutterFactory> factory_;
};

TEST_F(RoundedDisplayGutterFactoryTest,
       CorrectOverlayGuttersCreatedForOrientationHint) {
  {
    auto overlay_gutters = factory_->CreateOverlayGutters(
        kTestDisplaySize, kDefaultTestRadii, /*create_vertical_gutters=*/false);

    EXPECT_EQ(overlay_gutters.size(), 2u);
    // Upper overlay gutter.
    EXPECT_THAT(overlay_gutters, testing::Contains(GutterWithMatchingCorners(
                                     RoundedCornerPosition::kUpperLeft,
                                     RoundedCornerPosition::kUpperRight)));
    // Lower overlay gutter.
    EXPECT_THAT(overlay_gutters, testing::Contains(GutterWithMatchingCorners(
                                     RoundedCornerPosition::kLowerLeft,
                                     RoundedCornerPosition::kLowerRight)));
  }
  {
    auto overlay_gutters = factory_->CreateOverlayGutters(
        kTestDisplaySize, kDefaultTestRadii, /*create_vertical_gutters=*/true);

    EXPECT_EQ(overlay_gutters.size(), 2u);
    // Left overlay gutter.
    EXPECT_THAT(overlay_gutters, testing::Contains(GutterWithMatchingCorners(
                                     RoundedCornerPosition::kUpperLeft,
                                     RoundedCornerPosition::kLowerLeft)));
    // Right overlay gutter.
    EXPECT_THAT(overlay_gutters, testing::Contains(GutterWithMatchingCorners(
                                     RoundedCornerPosition::kUpperRight,
                                     RoundedCornerPosition::kLowerRight)));
  }
}

TEST_F(RoundedDisplayGutterFactoryTest,
       OverlayGuttersAreNotCreatedIfNotNeeded) {
  {
    const gfx::RoundedCornersF radii(10, 10, 0, 0);
    auto overlay_gutters = factory_->CreateOverlayGutters(
        kTestDisplaySize, radii, /*create_vertical_gutters=*/false);

    EXPECT_EQ(overlay_gutters.size(), 1u);

    EXPECT_THAT(overlay_gutters, testing::Contains(GutterWithMatchingCorners(
                                     RoundedCornerPosition::kUpperLeft,
                                     RoundedCornerPosition::kUpperRight)));

    // We do not create the lower overlay gutter since both the rounded
    // corners drawn by the gutter are zero.
    EXPECT_THAT(overlay_gutters,
                testing::Not(testing::Contains(GutterWithMatchingCorners(
                    RoundedCornerPosition::kLowerLeft,
                    RoundedCornerPosition::kLowerRight))));
  }
  {
    const gfx::RoundedCornersF radii(10, 10, 0, 0);
    auto overlay_gutters = factory_->CreateOverlayGutters(
        kTestDisplaySize, radii, /*create_vertical_gutters=*/true);

    EXPECT_EQ(overlay_gutters.size(), 2u);

    // We need to create both vertical overlay gutter as one of corners have
    // not zero radius.
    EXPECT_THAT(overlay_gutters, testing::Contains(GutterWithMatchingCorners(
                                     RoundedCornerPosition::kUpperLeft,
                                     RoundedCornerPosition::kLowerLeft)));
    EXPECT_THAT(overlay_gutters, testing::Contains(GutterWithMatchingCorners(
                                     RoundedCornerPosition::kUpperRight,
                                     RoundedCornerPosition::kLowerRight)));
  }
}

}  // namespace
}  // namespace ash
