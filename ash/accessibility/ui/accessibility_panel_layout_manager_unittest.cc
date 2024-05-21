// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_panel_layout_manager.h"

#include <memory>

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Shorten the name for better line wrapping.
constexpr int kDefaultPanelHeight =
    AccessibilityPanelLayoutManager::kDefaultPanelHeight;

AccessibilityPanelLayoutManager* GetLayoutManager() {
  aura::Window* container =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_AccessibilityPanelContainer);
  return static_cast<AccessibilityPanelLayoutManager*>(
      container->layout_manager());
}

// Simulates Chrome creating the ChromeVoxPanel widget.
std::unique_ptr<views::Widget> CreateChromeVoxPanel() {
  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  params.parent = Shell::GetContainer(
      root_window, kShellWindowId_AccessibilityPanelContainer);
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.bounds = gfx::Rect(0, 0, root_window->bounds().width(),
                            root_window->bounds().height());
  widget->Init(std::move(params));
  return widget;
}

using AccessibilityPanelLayoutManagerTest = AshTestBase;

TEST_F(AccessibilityPanelLayoutManagerTest, Basics) {
  AccessibilityPanelLayoutManager* layout_manager = GetLayoutManager();
  ASSERT_TRUE(layout_manager);

  // The layout manager doesn't track anything at startup.
  EXPECT_FALSE(layout_manager->panel_window_for_test());

  // Simulate chrome creating the ChromeVox widget. The layout manager starts
  // managing it.
  std::unique_ptr<views::Widget> widget = CreateChromeVoxPanel();
  widget->Show();
  EXPECT_EQ(widget->GetNativeWindow(), layout_manager->panel_window_for_test());

  // The layout manager doesn't track anything after the widget closes.
  widget.reset();
  EXPECT_FALSE(layout_manager->panel_window_for_test());
}

TEST_F(AccessibilityPanelLayoutManagerTest, Shutdown) {
  // Simulate chrome creating the ChromeVox widget.
  std::unique_ptr<views::Widget> widget = CreateChromeVoxPanel();
  widget->Show();

  // Don't close the window.
  widget.release();

  // Ash should not crash if the window is still open at shutdown.
}

TEST_F(AccessibilityPanelLayoutManagerTest, PanelFullscreen) {
  AccessibilityPanelLayoutManager* layout_manager = GetLayoutManager();
  display::Screen* screen = display::Screen::GetScreen();

  std::unique_ptr<views::Widget> widget = CreateChromeVoxPanel();
  widget->Show();

  layout_manager->SetPanelBounds(gfx::Rect(0, 0, 0, kDefaultPanelHeight),
                                 AccessibilityPanelState::FULL_WIDTH);

  gfx::Rect expected_work_area = screen->GetPrimaryDisplay().work_area();

  // When the panel is fullscreen it fills the display and clears the
  // work area.
  layout_manager->SetPanelBounds(gfx::Rect(),
                                 AccessibilityPanelState::FULLSCREEN);
  EXPECT_EQ(widget->GetNativeWindow()->bounds(),
            screen->GetPrimaryDisplay().bounds());
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area().y(), 0);

  // Restoring the panel to default size restores the bounds and sets
  // the work area.
  layout_manager->SetPanelBounds(gfx::Rect(0, 0, 0, kDefaultPanelHeight),
                                 AccessibilityPanelState::FULL_WIDTH);
  gfx::Rect expected_bounds(0, 0, screen->GetPrimaryDisplay().bounds().width(),
                            kDefaultPanelHeight);
  EXPECT_EQ(widget->GetNativeWindow()->bounds(), expected_bounds);
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area(), expected_work_area);
}

TEST_F(AccessibilityPanelLayoutManagerTest, SetBounds) {
  std::unique_ptr<views::Widget> widget = CreateChromeVoxPanel();
  widget->Show();

  gfx::Rect bounds(0, 0, 100, 100);
  GetLayoutManager()->SetPanelBounds(bounds, AccessibilityPanelState::BOUNDED);
  EXPECT_EQ(widget->GetNativeWindow()->bounds(), bounds);
}

TEST_F(AccessibilityPanelLayoutManagerTest, DisplayBoundsChange) {
  std::unique_ptr<views::Widget> widget = CreateChromeVoxPanel();
  widget->Show();
  GetLayoutManager()->SetPanelBounds(gfx::Rect(0, 0, 0, kDefaultPanelHeight),
                                     AccessibilityPanelState::FULL_WIDTH);

  // When the display resolution changes the panel still sits at the top of the
  // screen.
  UpdateDisplay("1200x700,1300x800");
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Rect expected_bounds(0, 0, screen->GetPrimaryDisplay().bounds().width(),
                            kDefaultPanelHeight);
  EXPECT_EQ(widget->GetNativeWindow()->bounds(), expected_bounds);

  gfx::Rect expected_work_area = screen->GetPrimaryDisplay().bounds();
  expected_work_area.Inset(gfx::Insets::TLBR(
      kDefaultPanelHeight, 0, ShelfConfig::Get()->shelf_size(), 0));
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area(), expected_work_area);
}

TEST_F(AccessibilityPanelLayoutManagerTest, DockedMagnifierEnabled) {
  auto* docked_magnifier_controller =
      Shell::Get()->docked_magnifier_controller();
  std::unique_ptr<views::Widget> widget = CreateChromeVoxPanel();
  widget->Show();
  GetLayoutManager()->SetPanelBounds(gfx::Rect(0, 0, 0, kDefaultPanelHeight),
                                     AccessibilityPanelState::FULL_WIDTH);

  // When Docked Magnifier is enabled, panel sits right below, and work area
  // sits below the panel.
  docked_magnifier_controller->SetEnabled(true);
  int magnifier_height = docked_magnifier_controller->GetTotalMagnifierHeight();

  display::Screen* screen = display::Screen::GetScreen();
  gfx::Rect expected_bounds(0, magnifier_height,
                            screen->GetPrimaryDisplay().bounds().width(),
                            kDefaultPanelHeight);
  EXPECT_EQ(widget->GetNativeWindow()->bounds(), expected_bounds);

  gfx::Rect expected_work_area = screen->GetPrimaryDisplay().bounds();
  expected_work_area.Inset(
      gfx::Insets::TLBR(magnifier_height + kDefaultPanelHeight, 0,
                        ShelfConfig::Get()->shelf_size(), 0));
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area(), expected_work_area);
}

}  // namespace
}  // namespace ash
