// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_blur_manager.h"

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
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

TEST_F(WallpaperBlurManagerTest, BlurCanBeUpdatedForLockState) {
  blur_manager()->set_allow_blur_for_testing();
  EXPECT_TRUE(
      blur_manager()->IsBlurAllowedForLockState(WallpaperType::kOneShot));
  EXPECT_FALSE(blur_manager()->is_wallpaper_blurred_for_lock_state());

  blur_manager()->UpdateWallpaperBlurForLockState(true,
                                                  WallpaperType::kOneShot);
  EXPECT_TRUE(blur_manager()->is_wallpaper_blurred_for_lock_state());

  blur_manager()->UpdateWallpaperBlurForLockState(false,
                                                  WallpaperType::kOneShot);
  EXPECT_FALSE(blur_manager()->is_wallpaper_blurred_for_lock_state());
}

TEST_F(WallpaperBlurManagerTest, BlurCanBeUpdatedForRootWindows) {
  auto* wallpaper_view = Shell::GetPrimaryRootWindowController()
                             ->wallpaper_widget_controller()
                             ->wallpaper_view();
  auto* root_window =
      wallpaper_view->GetWidget()->GetNativeWindow()->GetRootWindow();
  blur_manager()->set_allow_blur_for_testing();

  blur_manager()->UpdateWallpaperBlurForLockState(true,
                                                  WallpaperType::kOneShot);
  EXPECT_TRUE(blur_manager()->is_wallpaper_blurred_for_lock_state());

  EXPECT_FALSE(blur_manager()->UpdateBlurForRootWindow(
      root_window, /*lock_state_changed=*/false, /*new_root=*/false,
      WallpaperType::kOneShot));
  EXPECT_TRUE(blur_manager()->UpdateBlurForRootWindow(
      root_window, /*lock_state_changed=*/true, /*new_root=*/true,
      WallpaperType::kOneShot));
}

}  // namespace ash
