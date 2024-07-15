// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/annotator/annotations_overlay_controller.h"
#include "ash/annotator/annotator_controller.h"
#include "ash/annotator/annotator_test_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

namespace {

void VerifyToggleOverlay(AnnotationsOverlayController* overlay_controller) {
  EXPECT_FALSE(overlay_controller->is_enabled());
  overlay_controller->Toggle();
  EXPECT_TRUE(overlay_controller->is_enabled());
}
}  // namespace

class AnnotationsOverlayTest : public AshTestBase {
 public:
  AnnotationsOverlayTest() = default;

  AnnotationsOverlayTest(const AnnotationsOverlayTest&) = delete;
  AnnotationsOverlayTest& operator=(const AnnotationsOverlayTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    annotator_helper_.SetUp();

    window_ = CreateTestWindow(gfx::Rect(20, 30, 200, 200));
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

  aura::Window* window() const { return window_.get(); }

  static constexpr gfx::Rect kUserRegion{20, 50, 60, 70};

 protected:
  AnnotatorIntegrationHelper annotator_helper_;
  std::unique_ptr<aura::Window> window_;
};

// Create annotations overlay on top of the root window and verify the overlay
// is a child of menu container.
TEST_F(AnnotationsOverlayTest, CreateOverlayOnRootWindow) {
  auto overlay_controller = std::make_unique<AnnotationsOverlayController>(
      Shell::GetPrimaryRootWindow(), std::nullopt);
  VerifyWindowStackingOnRoot(overlay_controller->GetOverlayNativeWindow(),
                             Shell::GetPrimaryRootWindow());
  VerifyToggleOverlay(overlay_controller.get());
}

// Create annotations overlay on top of a new window and verify the overlay is a
// child of the window.
TEST_F(AnnotationsOverlayTest, CreateOverlayOnTestWindow) {
  auto overlay_controller =
      std::make_unique<AnnotationsOverlayController>(window(), std::nullopt);
  VerifyWindowStackingOnTestWindow(overlay_controller->GetOverlayNativeWindow(),
                                   window());
  VerifyToggleOverlay(overlay_controller.get());
}

// Create annotations overlay on top of a region and verify the overlay is a
// child of menu container with the bounds of the region.
TEST_F(AnnotationsOverlayTest, CreateOverlayOnRegion) {
  auto overlay_controller = std::make_unique<AnnotationsOverlayController>(
      Shell::GetPrimaryRootWindow(),
      std::make_optional<gfx::Rect>(kUserRegion));
  VerifyWindowStackingOnRegion(overlay_controller->GetOverlayNativeWindow(),
                               Shell::GetPrimaryRootWindow(), kUserRegion);
  VerifyToggleOverlay(overlay_controller.get());
}

// Change window's bounds and verify overlay widget size has updated.
TEST_F(AnnotationsOverlayTest, ChangeWindowBounds) {
  auto overlay_controller =
      std::make_unique<AnnotationsOverlayController>(window(), std::nullopt);
  window()->SetBoundsInScreen(
      gfx::Rect(900, 0, 600, 500),
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          Shell::GetPrimaryRootWindow()));
  ExpectSameWindowBounds(overlay_controller->GetOverlayNativeWindow(),
                         window());
}

// Move window to a second display and verify the overlay widget size has
// updated.
TEST_F(AnnotationsOverlayTest, ChangeDisplay) {
  auto overlay_controller =
      std::make_unique<AnnotationsOverlayController>(window(), std::nullopt);
  UpdateDisplay("800x700,801+0-800x700");
  aura::Window::Windows roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  const auto& displays = Shell::Get()->display_manager()->active_display_list();
  ASSERT_EQ(2U, displays.size());
  const gfx::Point point_in_second_display = gfx::Point(1000, 500);
  ASSERT_TRUE(displays[1].bounds().Contains(point_in_second_display));

  GetEventGenerator()->MoveMouseTo(point_in_second_display);
  window()->SetBoundsInScreen(
      gfx::Rect(900, 0, 600, 500),
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          Shell::GetAllRootWindows()[1]));
  ExpectSameWindowBounds(overlay_controller->GetOverlayNativeWindow(),
                         window());
}

}  // namespace ash
