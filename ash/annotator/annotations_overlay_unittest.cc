// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "ash/annotator/annotations_overlay_controller.h"
#include "ash/annotator/annotator_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/annotator/test/mock_annotator_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

namespace {

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

    auto* annotator_controller = Shell::Get()->annotator_controller();
    annotator_controller->SetToolClient(&client_);
    window_ = CreateTestWindow(gfx::Rect(20, 30, 200, 200));
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

  aura::Window* window() const { return window_.get(); }

  static constexpr gfx::Rect kUserRegion{20, 50, 60, 70};

 protected:
  MockAnnotatorClient client_;
  std::unique_ptr<aura::Window> window_;
};

// Create annotations overlay on top of the root window and verify the overlay
// is a child of menu container.
TEST_F(AnnotationsOverlayTest, CreateOverlayOnRootWindow) {
  auto bounds = gfx::Rect(Shell::GetPrimaryRootWindow()->bounds().size());
  auto overlay_controller = std::make_unique<AnnotationsOverlayController>(
      Shell::GetPrimaryRootWindow(), bounds);
  VerifyWindowStackingOnRoot(overlay_controller->GetOverlayNativeWindow(),
                             Shell::GetPrimaryRootWindow());
  VerifyToggleOverlay(overlay_controller.get());
}

// Create annotations overlay on top of a new window and verify the overlay is a
// child of the window.
TEST_F(AnnotationsOverlayTest, CreateOverlayOnTestWindow) {
  auto bounds = gfx::Rect(window()->bounds().size());
  auto overlay_controller =
      std::make_unique<AnnotationsOverlayController>(window(), bounds);
  VerifyWindowStackingOnTestWindow(overlay_controller->GetOverlayNativeWindow(),
                                   window());
  VerifyToggleOverlay(overlay_controller.get());
}

// Create annotations overlay on top of a region and verify the overlay is a
// child of menu container with the bounds of the region.
TEST_F(AnnotationsOverlayTest, CreateOverlayOnRegion) {
  auto overlay_controller = std::make_unique<AnnotationsOverlayController>(
      Shell::GetPrimaryRootWindow(), kUserRegion);
  VerifyWindowStackingOnRegion(overlay_controller->GetOverlayNativeWindow(),
                               Shell::GetPrimaryRootWindow(), kUserRegion);
  VerifyToggleOverlay(overlay_controller.get());
}

}  // namespace ash
