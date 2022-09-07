// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_pixel_diff_test_helper.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"

namespace ash {

namespace {

Shelf* GetPrimaryShelf() {
  return Shell::GetPrimaryRootWindowController()->shelf();
}

gfx::Rect GetShelfWidgetScreenBounds() {
  return GetPrimaryShelf()->GetWindow()->GetBoundsInScreen();
}

}  // namespace

AshPixelDiffTestHelper::AshPixelDiffTestHelper() = default;

AshPixelDiffTestHelper::~AshPixelDiffTestHelper() = default;

bool AshPixelDiffTestHelper::ComparePrimaryFullScreen(
    const std::string& screenshot_name) {
  aura::Window* primary_root_window = Shell::Get()->GetPrimaryRootWindow();
  return ComparePrimaryScreenshotWithBoundsInScreen(
      screenshot_name, primary_root_window->bounds());
}

bool AshPixelDiffTestHelper::CompareUiComponentScreenshot(
    const std::string& screenshot_name,
    UiComponent ui_component) {
  return ComparePrimaryScreenshotWithBoundsInScreen(
      screenshot_name, GetUiComponentBoundsInScreen(ui_component));
}

bool AshPixelDiffTestHelper::ComparePrimaryScreenshotWithBoundsInScreen(
    const std::string& screenshot_name,
    const gfx::Rect& screen_bounds) {
  aura::Window* primary_root_window = Shell::Get()->GetPrimaryRootWindow();
  return pixel_diff_.CompareNativeWindowScreenshot(
      screenshot_name, primary_root_window, screen_bounds);
}

void AshPixelDiffTestHelper::InitSkiaGoldPixelDiff(
    const std::string& screenshot_prefix,
    const std::string& corpus) {
  pixel_diff_.Init(screenshot_prefix, corpus);
}

gfx::Rect AshPixelDiffTestHelper::GetUiComponentBoundsInScreen(
    UiComponent ui_component) const {
  switch (ui_component) {
    case UiComponent::kShelfWidget:
      return GetShelfWidgetScreenBounds();
  }
}

}  // namespace ash
