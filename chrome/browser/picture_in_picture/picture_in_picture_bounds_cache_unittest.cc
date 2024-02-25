// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_bounds_cache.h"
#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"

namespace {

class PictureInPictureBoundsCacheTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL("https://www.pictureinpicture.com"));
  }

  void TearDown() override {
    DeleteContents();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  display::Display& display() { return display_; }

 private:
  std::unique_ptr<content::WebContents> child_web_contents_;
  display::Display display_{1};
};

}  // namespace

TEST_F(PictureInPictureBoundsCacheTest, HasNoInitialGuess) {
  const gfx::Size requested_size(320, 240);
  auto initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display(), requested_size);
  EXPECT_FALSE(initial_bounds);
}

TEST_F(PictureInPictureBoundsCacheTest,
       RemembersMostRecentBoundsForEmptyInitialSize) {
  PictureInPictureBoundsCache::GetBoundsForNewWindow(web_contents(), display(),
                                                     {});
  const gfx::Rect bounds_1(10, 20, 30, 40);
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(), bounds_1);
  const gfx::Rect bounds_2(50, 60, 70, 80);
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(), bounds_2);

  // This also verifies that an empty request size matches with a cache that has
  // an empty request size.
  auto initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display(), {});
  EXPECT_TRUE(initial_bounds);
  EXPECT_EQ(*initial_bounds, bounds_2);
}

TEST_F(PictureInPictureBoundsCacheTest,
       RemembersMostRecentBoundsForNonEmptyInitialSize) {
  const gfx::Size requested_size(320, 240);
  PictureInPictureBoundsCache::GetBoundsForNewWindow(web_contents(), display(),
                                                     requested_size);
  const gfx::Rect bounds_1(10, 20, 30, 40);
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(), bounds_1);
  const gfx::Rect bounds_2(50, 60, 70, 80);
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(), bounds_2);
  auto initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display(), requested_size);
  EXPECT_TRUE(initial_bounds);
  EXPECT_EQ(*initial_bounds, bounds_2);
}

TEST_F(PictureInPictureBoundsCacheTest, DisplayMismatchDoesNotProvideBounds) {
  // If the display id changes, then the cache should miss.
  const gfx::Size requested_size(320, 240);
  PictureInPictureBoundsCache::GetBoundsForNewWindow(web_contents(), display(),
                                                     requested_size);
  const gfx::Rect bounds(10, 20, 30, 40);
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(), bounds);

  display::Display display_2(2);
  auto initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display_2, requested_size);
  EXPECT_FALSE(initial_bounds);
}

TEST_F(PictureInPictureBoundsCacheTest, SizeMismatchDoesNotProvideBounds) {
  // If the request specifies an exact size, then it must match the cached
  // requested size.
  const gfx::Size requested_size_1(320, 240);
  PictureInPictureBoundsCache::GetBoundsForNewWindow(web_contents(), display(),
                                                     requested_size_1);
  const gfx::Rect bounds(10, 20, 30, 40);
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(), bounds);

  const gfx::Size requested_size_2(100, 100);
  ASSERT_NE(requested_size_1, requested_size_2);
  auto initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display(), requested_size_2);
  EXPECT_FALSE(initial_bounds);
}

TEST_F(PictureInPictureBoundsCacheTest,
       EmptyRequestSizeMatchesWithNonEmptyCacheSize) {
  // If the request does not specify a size, then it should match with any
  // request size stored in the cache.
  PictureInPictureBoundsCache::GetBoundsForNewWindow(web_contents(), display(),
                                                     gfx::Size(320, 240));
  const gfx::Rect bounds(10, 20, 30, 40);
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(), bounds);

  auto initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display(), {});
  EXPECT_TRUE(initial_bounds);
  EXPECT_EQ(*initial_bounds, bounds);
}

TEST_F(PictureInPictureBoundsCacheTest,
       NonEmptyRequestSizeMismatchesWithEmptyCacheSize) {
  // A non-empty request size will not match with a cache for an empty size.
  PictureInPictureBoundsCache::GetBoundsForNewWindow(web_contents(), display(),
                                                     {});
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(),
                                                  gfx::Rect(10, 20, 30, 40));

  auto initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display(), gfx::Size(10, 20));
  EXPECT_FALSE(initial_bounds);
}

TEST_F(PictureInPictureBoundsCacheTest,
       DifferentOriginsMismatchAndOnlyOneIsCached) {
  GURL gurl("https://www.pictureinpicture.com");
  NavigateAndCommit(gurl);
  PictureInPictureBoundsCache::GetBoundsForNewWindow(web_contents(), display(),
                                                     {});
  const gfx::Rect bounds(10, 20, 30, 40);
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(), bounds);

  NavigateAndCommit(GURL("https://www.example.com"));
  auto initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display(), {});
  EXPECT_FALSE(initial_bounds);

  // Navigating back should also miss, because example.com opened a new pip
  // window.  The cache now reflects example.com .
  NavigateAndCommit(gurl);
  initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display(), {});
  EXPECT_FALSE(initial_bounds);
}

TEST_F(PictureInPictureBoundsCacheTest, NavigatingThereAndBackStillMatches) {
  GURL gurl("https://www.pictureinpicture.com");
  NavigateAndCommit(gurl);
  PictureInPictureBoundsCache::GetBoundsForNewWindow(web_contents(), display(),
                                                     {});
  const gfx::Rect bounds(10, 20, 30, 40);
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(), bounds);

  // Don't open a new pip window this time.
  NavigateAndCommit(GURL("https://www.example.com"));

  // Navigating back should not miss, because nobody opened a pip window after
  // we navigated away.
  NavigateAndCommit(gurl);
  auto initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display(), {});
  EXPECT_TRUE(initial_bounds);
  EXPECT_EQ(*initial_bounds, bounds);
}

TEST_F(PictureInPictureBoundsCacheTest, SameOriginsDifferentPathsMatch) {
  NavigateAndCommit(GURL("https://www.pictureinpicture.com/look_sword.html"));
  PictureInPictureBoundsCache::GetBoundsForNewWindow(web_contents(), display(),
                                                     {});
  const gfx::Rect bounds(10, 20, 30, 40);
  PictureInPictureBoundsCache::UpdateCachedBounds(web_contents(), bounds);

  NavigateAndCommit(GURL(
      "https://www.pictureinpicture.com/elvish_sword_of_great_antiquity.html"));
  auto initial_bounds = PictureInPictureBoundsCache::GetBoundsForNewWindow(
      web_contents(), display(), {});
  EXPECT_TRUE(initial_bounds);
  EXPECT_EQ(*initial_bounds, bounds);
}
