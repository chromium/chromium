// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_wallpaper_controller.h"

#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"

namespace ash {

namespace {

// Do not change the wallpaper when entering or exiting overview mode when this
// is true.
bool g_disable_wallpaper_change_for_tests = false;

constexpr base::TimeDelta kBlurSlideDuration = base::Milliseconds(250);

bool IsWallpaperChangeAllowed() {
  return !g_disable_wallpaper_change_for_tests;
}

WallpaperWidgetController* GetWallpaperWidgetController(aura::Window* root) {
  return RootWindowController::ForWindow(root)->wallpaper_widget_controller();
}

// Returns the wallpaper blur based on the current configuration.
float GetBlur(bool should_blur) {
  return should_blur ? wallpaper_constants::kOverviewBlur
                     : wallpaper_constants::kClear;
}

}  // namespace

OverviewWallpaperController::OverviewWallpaperController() {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

OverviewWallpaperController::~OverviewWallpaperController() = default;

// static
void OverviewWallpaperController::SetDisableChangeWallpaperForTest(
    bool disable) {
  g_disable_wallpaper_change_for_tests = disable;
}

void OverviewWallpaperController::Blur(bool animate) {
  UpdateWallpaper(/*should_blur=*/true, animate);
}

void OverviewWallpaperController::Unblur() {
  UpdateWallpaper(/*should_blur=*/false, /*animate=*/true);
}

void OverviewWallpaperController::OnTabletModeStarted() {
  UpdateWallpaper(wallpaper_blurred_, /*animate=*/absl::nullopt);
}

void OverviewWallpaperController::OnTabletModeEnded() {
  UpdateWallpaper(wallpaper_blurred_, /*animate=*/absl::nullopt);
}

void OverviewWallpaperController::OnTabletControllerDestroyed() {
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
}

void OverviewWallpaperController::UpdateWallpaper(
    bool should_blur,
    absl::optional<bool> animate) {
  if (!IsWallpaperChangeAllowed())
    return;

  // Don't apply wallpaper change while the session is blocked.
  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    return;

  float blur = GetBlur(should_blur);

  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    auto* wallpaper_widget_controller = GetWallpaperWidgetController(root);

    if (blur == wallpaper_widget_controller->GetWallpaperBlur())
      continue;

    if (!animate.has_value()) {
      wallpaper_widget_controller->SetWallpaperBlur(blur);
      continue;
    }

    const bool should_animate = ShouldAnimateWallpaper(root);
    // On adding blur, we want to blur immediately if there are no animations
    // and blur after the rest of the overview animations have completed if
    // there is to be wallpaper animations. `UpdateWallpaper` will get called
    // twice when blurring, but only change the wallpaper when `should_animate`
    // matches `animate`.
    if (should_blur && should_animate != animate.value())
      continue;

    wallpaper_widget_controller->SetWallpaperBlur(
        blur, should_animate ? kBlurSlideDuration : base::TimeDelta());
  }

  wallpaper_blurred_ = should_blur;
}

}  // namespace ash
