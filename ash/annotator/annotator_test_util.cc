// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/annotator/annotator_test_util.h"

#include "ash/annotator/annotator_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

void ExpectChildOfMenuContainer(aura::Window* overlay_window,
                                aura::Window* source_window) {
  auto* parent = overlay_window->parent();

  auto* menu_container = source_window->GetRootWindow()->GetChildById(
      kShellWindowId_MenuContainer);
  ASSERT_EQ(parent, menu_container);
  EXPECT_EQ(menu_container->children().front(), overlay_window);
}

void ExpectSameWindowBounds(aura::Window* overlay_window,
                            aura::Window* source_window) {
  EXPECT_EQ(overlay_window->bounds(),
            gfx::Rect(source_window->bounds().size()));
}

void VerifyWindowStackingOnRoot(aura::Window* overlay_window,
                                aura::Window* source_window) {
  ExpectChildOfMenuContainer(overlay_window, source_window);
  ExpectSameWindowBounds(overlay_window, source_window);
}

void VerifyWindowStackingOnTestWindow(aura::Window* overlay_window,
                                      aura::Window* source_window) {
  auto* parent = overlay_window->parent();
  ASSERT_EQ(parent, source_window);
  EXPECT_EQ(source_window->children().back(), overlay_window);
  ExpectSameWindowBounds(overlay_window, source_window);
}

void VerifyWindowStackingOnRegion(aura::Window* overlay_window,
                                  aura::Window* source_window,
                                  const gfx::Rect region_bounds) {
  ExpectChildOfMenuContainer(overlay_window, source_window);
  EXPECT_EQ(overlay_window->bounds(), region_bounds);
}

void VerifyOverlayEnabledState(aura::Window* overlay_window,
                               bool overlay_enabled_state) {
  if (overlay_enabled_state) {
    EXPECT_TRUE(overlay_window->IsVisible());
    EXPECT_EQ(overlay_window->event_targeting_policy(),
              aura::EventTargetingPolicy::kTargetAndDescendants);
  } else {
    EXPECT_FALSE(overlay_window->IsVisible());
    EXPECT_EQ(overlay_window->event_targeting_policy(),
              aura::EventTargetingPolicy::kNone);
  }
}

void VerifyOverlayWindowForCaptureMode(aura::Window* overlay_window,
                                       aura::Window* window_being_recorded,
                                       CaptureModeSource source,
                                       const gfx::Rect region_bounds) {
  switch (source) {
    case CaptureModeSource::kFullscreen:
      VerifyWindowStackingOnRoot(overlay_window, window_being_recorded);
      break;
    case CaptureModeSource::kWindow:
      VerifyWindowStackingOnTestWindow(overlay_window, window_being_recorded);
      break;
    case CaptureModeSource::kRegion:
      VerifyWindowStackingOnRegion(overlay_window, window_being_recorded,
                                   region_bounds);
      break;
  }
}

AnnotatorIntegrationHelper::AnnotatorIntegrationHelper() = default;

void AnnotatorIntegrationHelper::SetUp() {
  auto* annotator_controller = Shell::Get()->annotator_controller();
  annotator_controller->SetToolClient(&annotator_client_);
  scoped_feature_list_.InitAndEnableFeature(ash::features::kAnnotatorMode);
}

}  // namespace ash
