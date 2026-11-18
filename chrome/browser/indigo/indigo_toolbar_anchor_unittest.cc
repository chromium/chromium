// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_toolbar_anchor.h"

#include <limits>
#include <optional>
#include <vector>

#include "base/token.h"
#include "components/viz/common/surfaces/tracked_element_rects.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/gfx/geometry/rect.h"

namespace indigo {
namespace {

class IndigoToolbarAnchorTest : public testing::Test {
 protected:
  // Returns the bounds of the selected anchor, or nullopt if there is none.
  std::optional<gfx::Rect> AnchorBounds(
      const std::vector<viz::TrackedElementRect>& rects,
      std::optional<base::Token> primary_token,
      float device_scale_factor = 1.0f) {
    const viz::TrackedElementRect* anchor = SelectToolbarAnchor(
        rects, primary_token, device_scale_factor, tracked_ids_);
    if (!anchor) {
      return std::nullopt;
    }
    return anchor->visible_bounds;
  }

  const base::Token primary_ = base::Token::CreateRandom();
  const base::Token copy_ = base::Token::CreateRandom();
  const base::Token copy2_ = base::Token::CreateRandom();
  const absl::flat_hash_set<base::Token> tracked_ids_ = {primary_, copy_,
                                                         copy2_};
};

TEST_F(IndigoToolbarAnchorTest, ReturnsNullWithoutRects) {
  EXPECT_FALSE(AnchorBounds({}, primary_));
}

TEST_F(IndigoToolbarAnchorTest, ReturnsPrimaryWithoutCopies) {
  const gfx::Rect primary_bounds(0, 0, 300, 300);
  EXPECT_EQ(AnchorBounds({{primary_, primary_bounds}}, primary_),
            primary_bounds);
}

TEST_F(IndigoToolbarAnchorTest, LargestCopyWhenPrimaryHasNoRect) {
  const gfx::Rect copy_bounds(0, 400, 400, 400);
  EXPECT_EQ(AnchorBounds(
                {{copy2_, gfx::Rect(0, 900, 300, 300)}, {copy_, copy_bounds}},
                primary_),
            copy_bounds);
}

TEST_F(IndigoToolbarAnchorTest, LargestCopyWithoutPrimaryToken) {
  const gfx::Rect copy_bounds(0, 400, 400, 400);
  EXPECT_EQ(AnchorBounds(
                {{copy2_, gfx::Rect(0, 900, 300, 300)}, {copy_, copy_bounds}},
                std::nullopt),
            copy_bounds);
}

TEST_F(IndigoToolbarAnchorTest, IgnoresUnknownIds) {
  EXPECT_FALSE(AnchorBounds(
      {{base::Token::CreateRandom(), gfx::Rect(0, 400, 400, 400)}}, primary_));
}

TEST_F(IndigoToolbarAnchorTest, ReturnsPrimaryWithEmptyRect) {
  const std::vector<viz::TrackedElementRect> rects = {
      {primary_, gfx::Rect()}, {copy_, gfx::Rect(0, 400, 400, 400)}};
  const viz::TrackedElementRect* anchor =
      SelectToolbarAnchor(rects, primary_, 1.0f, tracked_ids_);
  ASSERT_TRUE(anchor);
  EXPECT_EQ(anchor->id, primary_);
}

TEST_F(IndigoToolbarAnchorTest, FollowsMuchLargerOverlappingCopy) {
  const gfx::Rect primary_bounds(0, 0, 300, 300);
  {
    SCOPED_TRACE("Overlapping copy that isn't much larger");
    EXPECT_EQ(AnchorBounds({{primary_, primary_bounds},
                            {copy_, gfx::Rect(0, 0, 320, 320)}},
                           primary_),
              primary_bounds);
  }
  {
    SCOPED_TRACE("Much larger copy elsewhere on the page");
    EXPECT_EQ(AnchorBounds({{primary_, primary_bounds},
                            {copy_, gfx::Rect(0, 400, 400, 400)}},
                           primary_),
              primary_bounds);
  }
  {
    SCOPED_TRACE("Much larger overlapping copy, e.g. a zoom view");
    const gfx::Rect zoom_bounds(0, 0, 400, 400);
    EXPECT_EQ(AnchorBounds({{primary_, primary_bounds}, {copy_, zoom_bounds}},
                           primary_),
              zoom_bounds);
  }
}

TEST_F(IndigoToolbarAnchorTest, AppliesAreaRatioToOverlappingCopy) {
  // The primary's area is 40000, so the threshold is 60000.
  const gfx::Rect primary_bounds(0, 0, 200, 200);
  {
    SCOPED_TRACE("Just below 1.5x");
    EXPECT_EQ(AnchorBounds({{primary_, primary_bounds},
                            {copy_, gfx::Rect(0, 0, 244, 245)}},
                           primary_),
              primary_bounds);
  }
  {
    SCOPED_TRACE("Exactly 1.5x");
    EXPECT_EQ(AnchorBounds({{primary_, primary_bounds},
                            {copy_, gfx::Rect(0, 0, 200, 300)}},
                           primary_),
              primary_bounds);
  }
  {
    SCOPED_TRACE("Just above 1.5x");
    const gfx::Rect copy_bounds(0, 0, 245, 245);
    EXPECT_EQ(AnchorBounds({{primary_, primary_bounds}, {copy_, copy_bounds}},
                           primary_),
              copy_bounds);
  }
}

TEST_F(IndigoToolbarAnchorTest, PrefersOverlappingCopyOverLargerCopyElsewhere) {
  const gfx::Rect zoom_bounds(100, 100, 400, 400);
  EXPECT_EQ(AnchorBounds({{copy_, gfx::Rect(0, 1000, 600, 600)},
                          {primary_, gfx::Rect(0, 0, 300, 300)},
                          {copy2_, zoom_bounds}},
                         primary_),
            zoom_bounds);
}

TEST_F(IndigoToolbarAnchorTest, EqualAreasKeepFirstRect) {
  const gfx::Rect first_bounds(0, 0, 400, 400);
  EXPECT_EQ(AnchorBounds(
                {{copy_, first_bounds}, {copy2_, gfx::Rect(0, 500, 400, 400)}},
                primary_),
            first_bounds);
}

TEST_F(IndigoToolbarAnchorTest, IgnoresCopiesNarrowerThanMinWidth) {
  EXPECT_FALSE(AnchorBounds({{copy_, gfx::Rect(0, 0, 169, 400)}}, primary_));
  EXPECT_EQ(AnchorBounds({{copy_, gfx::Rect(0, 0, 170, 400)}}, primary_),
            gfx::Rect(0, 0, 170, 400));
}

TEST_F(IndigoToolbarAnchorTest, ScalesMinWidthByDeviceScaleFactor) {
  // At DSF 2, a copy needs 340 physical pixels.
  EXPECT_FALSE(
      AnchorBounds({{copy_, gfx::Rect(0, 0, 339, 400)}}, primary_, 2.0f));
  EXPECT_EQ(AnchorBounds({{copy_, gfx::Rect(0, 0, 340, 400)}}, primary_, 2.0f),
            gfx::Rect(0, 0, 340, 400));
}

TEST_F(IndigoToolbarAnchorTest, HandlesRectsWhoseAreaOverflowsInt) {
  constexpr int kMax = std::numeric_limits<int>::max();
  const gfx::Rect huge_bounds(0, 0, kMax, kMax);
  {
    SCOPED_TRACE("Huge overlapping copy");
    EXPECT_EQ(AnchorBounds(
                  {{primary_, gfx::Rect(0, 0, 300, 300)}, {copy_, huge_bounds}},
                  primary_),
              huge_bounds);
  }
  {
    SCOPED_TRACE("Huge primary and copy");
    const std::vector<viz::TrackedElementRect> rects = {{primary_, huge_bounds},
                                                        {copy_, huge_bounds}};
    const viz::TrackedElementRect* anchor =
        SelectToolbarAnchor(rects, primary_, 1.0f, tracked_ids_);
    ASSERT_TRUE(anchor);
    EXPECT_EQ(anchor->id, primary_);
  }
  {
    SCOPED_TRACE("Huge copies without a primary rect");
    EXPECT_EQ(AnchorBounds(
                  {{copy_, gfx::Rect(0, 0, kMax, 1000)}, {copy2_, huge_bounds}},
                  primary_),
              huge_bounds);
  }
}

}  // namespace
}  // namespace indigo
