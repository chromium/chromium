// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_icon_cache.h"

#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

namespace apps {

const PackageId kTestPackageId(AppType::kArc, "test.package.name");

SkColor kRed = SkColorSetRGB(255, 0, 0);
SkColor kGreen = SkColorSetRGB(0, 255, 0);
SkColor kBlue = SkColorSetRGB(0, 0, 255);

class PromiseAppIconCacheTest : public testing::Test {
 public:
  void SetUp() override { cache_ = std::make_unique<PromiseAppIconCache>(); }

  PromiseAppIconCache* icon_cache() { return cache_.get(); }

  PromiseAppIconPtr CreatePromiseAppIcon(int width, SkColor color = kRed) {
    PromiseAppIconPtr icon = std::make_unique<PromiseAppIcon>();
    icon->icon = CreateBitmapWithColor(width, color);
    icon->width_in_pixels = width;
    return icon;
  }

  // Create a colored bitmap for testing so that we can verify whether
  // operations have been applied to the correct icon.
  SkBitmap CreateBitmapWithColor(int width, SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, width);
    bitmap.eraseColor(color);
    return bitmap;
  }

 private:
  std::unique_ptr<PromiseAppIconCache> cache_;
};

TEST_F(PromiseAppIconCacheTest, SaveIcon) {
  PromiseAppIconPtr icon = CreatePromiseAppIcon(/*width=*/50, kRed);
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon));
  EXPECT_TRUE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));
  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 1u);
  EXPECT_EQ(icons_saved[0]->width_in_pixels, 50);
  EXPECT_TRUE(gfx::BitmapsAreEqual(icons_saved[0]->icon,
                                   CreateBitmapWithColor(50, kRed)));
}

TEST_F(PromiseAppIconCacheTest, SaveMultipleIcons) {
  PromiseAppIconPtr icon_small = CreatePromiseAppIcon(/*width=*/512, kRed);
  PromiseAppIconPtr icon_large = CreatePromiseAppIcon(/*width=*/1024, kGreen);
  PromiseAppIconPtr icon_smallest = CreatePromiseAppIcon(/*width=*/128, kBlue);

  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_small));
  EXPECT_EQ(icon_cache()->GetIconsForTesting(kTestPackageId).size(), 1u);

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_large));
  EXPECT_EQ(icon_cache()->GetIconsForTesting(kTestPackageId).size(), 2u);

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_smallest));

  // We should have 3 icons for the same package ID in ascending order.
  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 3u);
  EXPECT_EQ(icons_saved[0]->width_in_pixels, 128);
  EXPECT_TRUE(gfx::BitmapsAreEqual(icons_saved[0]->icon,
                                   CreateBitmapWithColor(128, kBlue)));

  EXPECT_EQ(icons_saved[1]->width_in_pixels, 512);
  EXPECT_TRUE(gfx::BitmapsAreEqual(icons_saved[1]->icon,
                                   CreateBitmapWithColor(512, kRed)));

  EXPECT_EQ(icons_saved[2]->width_in_pixels, 1024);
  EXPECT_TRUE(gfx::BitmapsAreEqual(icons_saved[2]->icon,
                                   CreateBitmapWithColor(1024, kGreen)));
}

TEST_F(PromiseAppIconCacheTest, GetIcon_NoIcons) {
  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 0u);

  gfx::ImageSkia returned_icon = icon_cache()->GetIcon(kTestPackageId, 512);
  EXPECT_TRUE(returned_icon.isNull());
}

TEST_F(PromiseAppIconCacheTest, GetIcon_Simple) {
  PromiseAppIconPtr icon = CreatePromiseAppIcon(/*width=*/512, kRed);
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon));

  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 1u);

  gfx::ImageSkia returned_icon = icon_cache()->GetIcon(kTestPackageId, 128);

  // Verify we have an icon of the correct dip size.
  EXPECT_FALSE(returned_icon.isNull());
  EXPECT_EQ(returned_icon.width(), 128);
  EXPECT_TRUE(gfx::BitmapsAreEqual(*returned_icon.bitmap(),
                                   CreateBitmapWithColor(128, kRed)));
}

TEST_F(PromiseAppIconCacheTest, GetIcon_ReturnsLargestIconIfAllIconsTooSmall) {
  PromiseAppIconPtr icon_small = CreatePromiseAppIcon(/*width=*/10, kRed);
  PromiseAppIconPtr icon_small_2 = CreatePromiseAppIcon(/*width=*/30, kGreen);
  PromiseAppIconPtr icon_small_3 = CreatePromiseAppIcon(/*width=*/50, kBlue);
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_small));
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_small_2));
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_small_3));

  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 3u);

  // All icons returned should be the largest icon resized for our requested
  // scales.
  gfx::ImageSkia returned_icon = icon_cache()->GetIcon(kTestPackageId, 128);
  EXPECT_FALSE(returned_icon.isNull());

  gfx::ImageSkiaRep image_rep = returned_icon.GetRepresentation(1.0f);
  EXPECT_EQ(image_rep.pixel_width(), 128);
  EXPECT_TRUE(gfx::BitmapsAreEqual(image_rep.GetBitmap(),
                                   CreateBitmapWithColor(128, kBlue)));

  image_rep = returned_icon.GetRepresentation(2.0f);
  EXPECT_EQ(image_rep.pixel_width(), 256);
  EXPECT_TRUE(gfx::BitmapsAreEqual(image_rep.GetBitmap(),
                                   CreateBitmapWithColor(256, kBlue)));
}

TEST_F(PromiseAppIconCacheTest,
       GetIcon_ReturnsCorrectRepresentationsForScaleFactors) {
  PromiseAppIconPtr icon_small = CreatePromiseAppIcon(/*width=*/128, kRed);
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_small));
  PromiseAppIconPtr icon_large = CreatePromiseAppIcon(/*width=*/512, kGreen);
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_large));

  EXPECT_EQ(icon_cache()->GetIconsForTesting(kTestPackageId).size(), 2u);

  gfx::ImageSkia returned_icon = icon_cache()->GetIcon(kTestPackageId, 128);
  EXPECT_FALSE(returned_icon.isNull());

  gfx::ImageSkiaRep image_rep_default = returned_icon.GetRepresentation(1.0f);
  EXPECT_FALSE(image_rep_default.is_null());
  EXPECT_EQ(image_rep_default.pixel_width(), 128);
  EXPECT_TRUE(gfx::BitmapsAreEqual(image_rep_default.GetBitmap(),
                                   CreateBitmapWithColor(128, kRed)));

  // Verify that the large icon gets resized to become smaller for the 2.0 scale
  // factor (instead of the small icon being resized up).
  gfx::ImageSkiaRep image_rep_larger = returned_icon.GetRepresentation(2.0f);
  EXPECT_FALSE(image_rep_larger.is_null());
  EXPECT_EQ(image_rep_larger.pixel_width(), 256);
  EXPECT_TRUE(gfx::BitmapsAreEqual(image_rep_larger.GetBitmap(),
                                   CreateBitmapWithColor(256, kGreen)));
}

TEST_F(PromiseAppIconCacheTest, RemoveIconsForPackageId) {
  PromiseAppIconPtr icon_small = CreatePromiseAppIcon(/*width=*/100);
  PromiseAppIconPtr icon_med = CreatePromiseAppIcon(/*width=*/200);
  PromiseAppIconPtr icon_large = CreatePromiseAppIcon(/*width=*/300);

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_small));
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_med));
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_large));

  // Confirm we have 3 icons.
  EXPECT_EQ(icon_cache()->GetIconsForTesting(kTestPackageId).size(), 3u);

  // Remove all icons for package ID.
  icon_cache()->RemoveIconsForPackageId(kTestPackageId);

  // Confirm we have no icons.
  EXPECT_EQ(icon_cache()->GetIconsForTesting(kTestPackageId).size(), 0u);
}

}  // namespace apps
