// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_provider.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/rounded_display/rounded_display_gutter.h"
#include "ash/rounded_display/rounded_display_provider_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ash {
namespace {

using Gutters = std::vector<RoundedDisplayGutter*>;
using RoundedCorner = RoundedDisplayGutter::RoundedCorner;
using RoundedCornerPosition = RoundedDisplayGutter::RoundedCorner::Position;
using ProviderStrategy = RoundedDisplayProvider::Strategy;

constexpr ProviderStrategy kDefaultTestStrategy = ProviderStrategy::kScanout;

gfx::RoundedCornersF CreateHorizontallyUniformRadii(int radius_a,
                                                    int radius_b) {
  return gfx::RoundedCornersF(radius_a, radius_a, radius_b, radius_b);
}

// The matcher matches a RoundedDisplayGutter that has the rounded corners of
// `positions`.
template <typename... Matchers>
auto GutterWithMatchingCorners(Matchers&&... positions) {
  return testing::ResultOf(
      "positions",
      [](const RoundedDisplayGutter* gutter) {
        std::vector<RoundedCornerPosition> positions;
        const std::vector<RoundedCorner>& corners = gutter->GetGutterCorners();
        base::ranges::transform(
            corners.begin(), corners.end(), std::back_inserter(positions),
            [](const RoundedCorner& corner) { return corner.position(); });
        return positions;
      },
      testing::UnorderedElementsAre(positions...));
}

class RoundedDisplayProviderTest : public AshTestBase {
 public:
  RoundedDisplayProviderTest() = default;

  RoundedDisplayProviderTest(const RoundedDisplayProviderTest&) = delete;
  RoundedDisplayProviderTest& operator=(const RoundedDisplayProviderTest&) =
      delete;

  ~RoundedDisplayProviderTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    // Create the rounded display provider for the primary display.
    provider_ = RoundedDisplayProvider::Create(GetPrimaryDisplay().id());
  }

  void TearDown() override {
    provider_.reset();
    AshTestBase::TearDown();
  }

 protected:
  const display::Display& GetDisplay(int64_t display_id) {
    return display_manager()->GetDisplayForId(display_id);
  }

  std::unique_ptr<RoundedDisplayProvider> provider_;
};

TEST_F(RoundedDisplayProviderTest, InitializeWithNonUniformRadii) {
  RoundedDisplayProviderTestApi test_api(provider_.get());

  const gfx::RoundedCornersF not_valid_radii(10, 11, 12, 12);

  EXPECT_DCHECK_DEATH(
      { provider_->Init(not_valid_radii, kDefaultTestStrategy); });
}

TEST_F(RoundedDisplayProviderTest, CorrectNumberOfGuttersAreProvided) {
  RoundedDisplayProviderTestApi test_api(provider_.get());
  const gfx::RoundedCornersF radii = CreateHorizontallyUniformRadii(10, 12);

  // We expect 2 overlay gutters to be created.
  provider_->Init(radii, kDefaultTestStrategy);
  EXPECT_EQ(test_api.GetGutters().size(), 2u);
}

TEST_F(RoundedDisplayProviderTest,
       CorrectGutterCreatedForStrategy_ScanoutDirection) {
  RoundedDisplayProviderTestApi test_api(provider_.get());
  const gfx::RoundedCornersF radii = CreateHorizontallyUniformRadii(10, 12);

  provider_->Init(radii, ProviderStrategy::kScanout);

  const auto& gutters = test_api.GetGutters();

  EXPECT_EQ(gutters.size(), 2u);

  // Check that we have two overlay gutters that in the scanout direction.
  EXPECT_THAT(gutters, testing::Contains(GutterWithMatchingCorners(
                           RoundedCornerPosition::kUpperLeft,
                           RoundedCornerPosition::kUpperRight)));
  EXPECT_THAT(gutters, testing::Contains(GutterWithMatchingCorners(
                           RoundedCornerPosition::kLowerLeft,
                           RoundedCornerPosition::kLowerRight)));
}

TEST_F(RoundedDisplayProviderTest,
       CorrectGutterCreatedForStrategy_OtherDirection) {
  RoundedDisplayProviderTestApi test_api(provider_.get());
  const gfx::RoundedCornersF radii = CreateHorizontallyUniformRadii(10, 12);

  provider_->Init(radii, ProviderStrategy::kOther);

  const auto& gutters = test_api.GetGutters();

  EXPECT_EQ(gutters.size(), 2u);

  // Check that we have two overlay gutters that in the scanout direction.
  EXPECT_THAT(gutters, testing::Contains(GutterWithMatchingCorners(
                           RoundedCornerPosition::kUpperLeft,
                           RoundedCornerPosition::kLowerLeft)));
  // Right overlay gutter.
  EXPECT_THAT(gutters, testing::Contains(GutterWithMatchingCorners(
                           RoundedCornerPosition::kUpperRight,
                           RoundedCornerPosition::kLowerRight)));
}

class RoundedDisplayProviderSurfaceUpdateTest
    : public RoundedDisplayProviderTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, std::string, bool>> {
 public:
  RoundedDisplayProviderSurfaceUpdateTest()
      : initial_display_spec_(std::get<0>(GetParam())),
        updated_display_spec_(std::get<1>(GetParam())),
        expect_update_(std::get<2>(GetParam())) {}

  RoundedDisplayProviderSurfaceUpdateTest(
      const RoundedDisplayProviderSurfaceUpdateTest&) = delete;
  RoundedDisplayProviderSurfaceUpdateTest& operator=(
      const RoundedDisplayProviderSurfaceUpdateTest&) = delete;

  ~RoundedDisplayProviderSurfaceUpdateTest() override = default;

 protected:
  std::string initial_display_spec_;
  std::string updated_display_spec_;
  bool expect_update_;
};

TEST_P(RoundedDisplayProviderSurfaceUpdateTest,
       UpdatesSurfaceOnlyWhenNecessary) {
  RoundedDisplayProviderTestApi test_api(provider_.get());

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  auto display_id = primary_display.id();

  UpdateDisplay(initial_display_spec_);

  gfx::RoundedCornersF radii =
      display_manager()->GetDisplayInfo(display_id).panel_corners_radii();

  provider_->Init(radii, kDefaultTestStrategy);

  ASSERT_TRUE(provider_->UpdateRoundedDisplaySurface());

  UpdateDisplay(updated_display_spec_);

  Gutters before_update_gutters = test_api.GetGutters();
  ASSERT_EQ(provider_->UpdateRoundedDisplaySurface(), expect_update_);
  Gutters after_update_gutters = test_api.GetGutters();

  // Confirm that we did not change gutters.
  EXPECT_EQ(before_update_gutters, after_update_gutters);
}

const std::string kInitialDisplaySpec = "500x400~15";
const std::string kInitialDisplaySpecWithRotation = "500x400/r~15";

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    RoundedDisplayProviderSurfaceUpdateTest,
    testing::Values(
        // If nothing changes, we should skip surface update.
        std::make_tuple(kInitialDisplaySpec,
                        "500x400~15",
                        /*expect_update=*/false),
        // Change in device scale factor, should only cause a surface update.
        std::make_tuple(kInitialDisplaySpec,
                        "500x400*2~15",
                        /*expect_update=*/true),
        // Further display rotation, should only cause a surface update.
        std::make_tuple(kInitialDisplaySpecWithRotation,
                        "500x400/u~15",
                        /*expect_update=*/true),
        // Multiple spec changes, should appropriately cause a surface update.
        std::make_tuple(kInitialDisplaySpec,
                        "500x400*2~15",
                        /*expect_update=*/true)));

}  // namespace
}  // namespace ash
