// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_icon_cache.h"

#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_loader.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/grit/app_icon_resources.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

namespace apps {

const PackageId kTestPackageId(PackageType::kArc, "test.package.name");

SkColor kRed = SkColorSetRGB(255, 0, 0);
SkColor kGreen = SkColorSetRGB(0, 255, 0);
SkColor kBlue = SkColorSetRGB(0, 0, 255);

class PromiseAppIconCacheTest : public testing::Test {
 public:
  void SetUp() override { cache_ = std::make_unique<PromiseAppIconCache>(); }

  PromiseAppIconCache* icon_cache() { return cache_.get(); }

  PromiseAppIconPtr CreatePromiseAppIcon(int width, SkColor color = kRed) {
    PromiseAppIconPtr icon = std::make_unique<PromiseAppIcon>();
    icon->icon = gfx::test::CreateBitmap(width, color);
    icon->width_in_pixels = width;
    return icon;
  }

  const SkBitmap GetPlaceholderIconBitmap(int size_in_dip,
                                          IconEffects icon_effects) {
    base::test::TestFuture<std::unique_ptr<apps::IconValue>> callback;
    apps::LoadIconFromResource(
        /*profile=*/nullptr, std::nullopt, IconType::kStandard, size_in_dip,
        IDR_APP_ICON_PLACEHOLDER_CUBE,
        /*is_placeholder_icon=*/true, icon_effects, callback.GetCallback());
    apps::IconValue* placeholder_iv = callback.Get().get();
    return *placeholder_iv->uncompressed.bitmap();
  }

  SkBitmap ApplyEffectsToBitmap(SkBitmap bitmap, IconEffects effects) {
    auto iv = std::make_unique<apps::IconValue>();
    iv->uncompressed = gfx::ImageSkia::CreateFromBitmap(bitmap, 1.0f);
    iv->icon_type = apps::IconType::kUncompressed;

    base::test::TestFuture<IconValuePtr> image_with_effects;
    ApplyIconEffects(/*profile=*/nullptr, /*app_id=*/std::nullopt, effects,
                     bitmap.width(), std::move(iv),
                     image_with_effects.GetCallback());

    return *image_with_effects.Get()->uncompressed.bitmap();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
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
                                   gfx::test::CreateBitmap(/*size=*/50, kRed)));
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
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      icons_saved[0]->icon, gfx::test::CreateBitmap(/*size=*/128, kBlue)));

  EXPECT_EQ(icons_saved[1]->width_in_pixels, 512);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      icons_saved[1]->icon, gfx::test::CreateBitmap(/*size=*/512, kRed)));

  EXPECT_EQ(icons_saved[2]->width_in_pixels, 1024);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      icons_saved[2]->icon, gfx::test::CreateBitmap(/*size=*/1024, kGreen)));
}

TEST_F(PromiseAppIconCacheTest, GetIconWithEffects) {
  PromiseAppIconPtr icon = CreatePromiseAppIcon(512, kRed);
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon));

  // Confirm that we have an icon in the icon cache.
  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 1u);

  // Attempt to load icon for test package.
  base::test::TestFuture<std::unique_ptr<apps::IconValue>> test_callback;
  icon_cache()->GetIconAndApplyEffects(kTestPackageId, 128,
                                       IconEffects::kCrOsStandardMask,
                                       test_callback.GetCallback());

  // Confirm the details of the icon in the callback.
  IconValue* result_icon = test_callback.Get().get();
  EXPECT_FALSE(result_icon->uncompressed.isNull());
  EXPECT_EQ(result_icon->icon_type, IconType::kStandard);
  EXPECT_FALSE(result_icon->is_placeholder_icon);
  EXPECT_TRUE(result_icon->is_maskable_icon);
  EXPECT_EQ(result_icon->uncompressed.bitmap()->width(), 128);

  // Confirm that the icon has the correct effects applied to it.
  SkBitmap expected_bitmap = ApplyEffectsToBitmap(
      gfx::test::CreateBitmap(/*size=*/128, kRed), kCrOsStandardMask);
  EXPECT_TRUE(gfx::BitmapsAreEqual(*result_icon->uncompressed.bitmap(),
                                   expected_bitmap));
}

TEST_F(PromiseAppIconCacheTest, GetPlaceholderIconWhenNoIconsAvailable) {
  // Confirm that we have no icons in the icon cache.
  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 0u);

  // Load icon for test package.
  base::test::TestFuture<std::unique_ptr<apps::IconValue>> test_callback;
  icon_cache()->GetIconAndApplyEffects(kTestPackageId, 96,
                                       IconEffects::kCrOsStandardMask,
                                       test_callback.GetCallback());

  // Confirm the details of the icon in the callback.
  IconValue* result_icon = test_callback.Get().get();
  EXPECT_FALSE(result_icon->uncompressed.isNull());
  EXPECT_EQ(result_icon->icon_type, IconType::kStandard);
  EXPECT_TRUE(result_icon->is_placeholder_icon);
  EXPECT_EQ(result_icon->uncompressed.bitmap()->width(), 96);

  // Confirm that the icon is the correct one.
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      *result_icon->uncompressed.bitmap(),
      GetPlaceholderIconBitmap(/*size_in_dip=*/96,
                               IconEffects::kCrOsStandardMask)));
}

TEST_F(PromiseAppIconCacheTest, GetLargestIconIfAllIconsTooSmall) {
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
  base::test::TestFuture<std::unique_ptr<apps::IconValue>> test_callback;
  icon_cache()->GetIconAndApplyEffects(kTestPackageId, 128, IconEffects::kNone,
                                       test_callback.GetCallback());
  IconValue* icon_value = test_callback.Get().get();
  gfx::ImageSkia icon = icon_value->uncompressed;
  EXPECT_FALSE(icon.isNull());

  gfx::ImageSkiaRep image_rep = icon.GetRepresentation(1.0f);
  EXPECT_EQ(image_rep.pixel_width(), 128);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      image_rep.GetBitmap(), gfx::test::CreateBitmap(/*size=*/128, kBlue)));

  image_rep = icon.GetRepresentation(2.0f);
  EXPECT_EQ(image_rep.pixel_width(), 256);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      image_rep.GetBitmap(), gfx::test::CreateBitmap(/*size=*/256, kBlue)));
}

TEST_F(PromiseAppIconCacheTest, GetCorrectIconRepresentationsForScaleFactors) {
  PromiseAppIconPtr icon_small = CreatePromiseAppIcon(/*width=*/128, kRed);
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_small));
  PromiseAppIconPtr icon_large = CreatePromiseAppIcon(/*width=*/512, kGreen);
  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_large));

  EXPECT_EQ(icon_cache()->GetIconsForTesting(kTestPackageId).size(), 2u);

  base::test::TestFuture<std::unique_ptr<apps::IconValue>> test_callback;
  icon_cache()->GetIconAndApplyEffects(kTestPackageId, 128, IconEffects::kNone,
                                       test_callback.GetCallback());
  IconValue* icon_value = test_callback.Get().get();
  gfx::ImageSkia icon = icon_value->uncompressed;
  EXPECT_FALSE(icon.isNull());

  gfx::ImageSkiaRep image_rep_default = icon.GetRepresentation(1.0f);
  EXPECT_FALSE(image_rep_default.is_null());
  EXPECT_EQ(image_rep_default.pixel_width(), 128);
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(image_rep_default.GetBitmap(),
                           gfx::test::CreateBitmap(/*size=*/128, kRed)));

  // Verify that the large icon gets resized to become smaller for the 2.0 scale
  // factor (instead of the small icon being resized up).
  gfx::ImageSkiaRep image_rep_larger = icon.GetRepresentation(2.0f);
  EXPECT_FALSE(image_rep_larger.is_null());
  EXPECT_EQ(image_rep_larger.pixel_width(), 256);
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(image_rep_larger.GetBitmap(),
                           gfx::test::CreateBitmap(/*size=*/256, kGreen)));
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
