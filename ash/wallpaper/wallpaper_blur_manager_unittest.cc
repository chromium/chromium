// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_blur_manager.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class WallpaperBlurManagerTest : public AshTestBase {
 public:
  WallpaperBlurManagerTest() = default;

  WallpaperBlurManagerTest(const WallpaperBlurManagerTest&) = delete;
  WallpaperBlurManagerTest& operator=(const WallpaperBlurManagerTest&) = delete;

  WallpaperBlurManager* blur_manager() { return &blur_manager_; }

 private:
  WallpaperBlurManager blur_manager_;
};

TEST_F(WallpaperBlurManagerTest, IsBlurAllowedForLockStateOverride) {
  EXPECT_FALSE(
      blur_manager()->IsBlurAllowedForLockState(WallpaperType::kDevice));
  EXPECT_FALSE(
      blur_manager()->IsBlurAllowedForLockState(WallpaperType::kOneShot));

  blur_manager()->set_allow_blur_for_testing();

  EXPECT_FALSE(
      blur_manager()->IsBlurAllowedForLockState(WallpaperType::kDevice));
  EXPECT_TRUE(
      blur_manager()->IsBlurAllowedForLockState(WallpaperType::kOneShot));
}

}  // namespace ash
