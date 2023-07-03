// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_icon_cache.h"

#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

namespace apps {

const PackageId kTestPackageId(AppType::kArc, "test.package.name");

class PromiseAppIconCacheTest : public testing::Test {
 public:
  void SetUp() override { cache_ = std::make_unique<PromiseAppIconCache>(); }

  PromiseAppIconCache* icon_cache() { return cache_.get(); }

  PromiseAppIconPtr CreateIcon(int width) {
    PromiseAppIconPtr icon = std::make_unique<PromiseAppIcon>();
    icon->icon = gfx::test::CreateBitmap(width, width);
    icon->width_in_pixels = width;
    return icon;
  }

 private:
  std::unique_ptr<PromiseAppIconCache> cache_;
};

TEST_F(PromiseAppIconCacheTest, SaveIcon) {
  PromiseAppIconPtr icon = CreateIcon(/*width=*/50);
  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon));
  EXPECT_TRUE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));
  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 1u);
  EXPECT_EQ(icons_saved[0]->width_in_pixels, 50);
  EXPECT_TRUE(gfx::BitmapsAreEqual(icons_saved[0]->icon,
                                   gfx::test::CreateBitmap(50, 50)));
}

TEST_F(PromiseAppIconCacheTest, SaveMultipleIcons) {
  PromiseAppIconPtr icon_small = CreateIcon(/*width=*/512);
  PromiseAppIconPtr icon_large = CreateIcon(/*width=*/1024);
  PromiseAppIconPtr icon_smallest = CreateIcon(/*width=*/128);

  EXPECT_FALSE(icon_cache()->DoesPackageIdHaveIcons(kTestPackageId));

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_small));
  EXPECT_EQ(icon_cache()->GetIconsForTesting(kTestPackageId).size(), 1u);

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_large));
  EXPECT_EQ(icon_cache()->GetIconsForTesting(kTestPackageId).size(), 2u);

  icon_cache()->SaveIcon(kTestPackageId, std::move(icon_smallest));

  // We should have 2 icons for the same package ID.
  std::vector<PromiseAppIcon*> icons_saved =
      icon_cache()->GetIconsForTesting(kTestPackageId);
  EXPECT_EQ(icons_saved.size(), 3u);
  EXPECT_EQ(icons_saved[0]->width_in_pixels, 128);
  EXPECT_TRUE(gfx::BitmapsAreEqual(icons_saved[0]->icon,
                                   gfx::test::CreateBitmap(128, 128)));

  EXPECT_EQ(icons_saved[1]->width_in_pixels, 512);
  EXPECT_TRUE(gfx::BitmapsAreEqual(icons_saved[1]->icon,
                                   gfx::test::CreateBitmap(512, 512)));

  EXPECT_EQ(icons_saved[2]->width_in_pixels, 1024);
  EXPECT_TRUE(gfx::BitmapsAreEqual(icons_saved[2]->icon,
                                   gfx::test::CreateBitmap(1024, 1024)));
}

}  // namespace apps
