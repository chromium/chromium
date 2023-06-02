// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_icon_cache.h"

#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

const PackageId kTestPackageId(AppType::kArc, "test.package.name");

class PromiseAppIconCacheTest : public testing::Test {
 public:
  void SetUp() override { cache_ = std::make_unique<PromiseAppIconCache>(); }

  PromiseAppIconCache* icon_cache() { return cache_.get(); }

 private:
  std::unique_ptr<PromiseAppIconCache> cache_;
};

TEST_F(PromiseAppIconCacheTest, SaveIcon) {
  PromiseAppIconPtr icon = std::make_unique<PromiseAppIcon>();
  icon->icon = gfx::ImageSkia();
  icon->is_masking_allowed = true;
  icon->width_in_pixels = 50;

  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon));
  EXPECT_TRUE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));
  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 1u);
  EXPECT_EQ(icons_saved[0]->is_masking_allowed, true);
  EXPECT_EQ(icons_saved[0]->width_in_pixels, 50);
}

TEST_F(PromiseAppIconCacheTest, SaveMultipleIcons) {
  PromiseAppIconPtr icon_small = std::make_unique<PromiseAppIcon>();
  icon_small->icon = gfx::ImageSkia();
  icon_small->is_masking_allowed = true;
  icon_small->width_in_pixels = 512;

  PromiseAppIconPtr icon_large = std::make_unique<PromiseAppIcon>();
  icon_large->icon = gfx::ImageSkia();
  icon_large->is_masking_allowed = false;
  icon_large->width_in_pixels = 1024;

  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_small));
  EXPECT_TRUE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));
  EXPECT_EQ(icon_cache()->GetIconsForTesting(kTestPackageId).size(), 1u);

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_large));

  // We should have 2 icons for the same package ID.
  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 2u);
  EXPECT_EQ(icons_saved[0]->is_masking_allowed, true);
  EXPECT_EQ(icons_saved[0]->width_in_pixels, 512);
  EXPECT_EQ(icons_saved[1]->is_masking_allowed, false);
  EXPECT_EQ(icons_saved[1]->width_in_pixels, 1024);
}

}  // namespace apps
