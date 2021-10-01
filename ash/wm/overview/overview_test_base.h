// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_TEST_BASE_H_
#define ASH_WM_OVERVIEW_OVERVIEW_TEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/shelf/shelf_view_test_api.h"
#include "ash/test/ash_test_base.h"

namespace views {
class ImageButton;
class Label;
}  // namespace views

namespace ash {

class OverviewController;
class OverviewGrid;
class OverviewItem;
class OverviewSession;
class ScopedOverviewTransformWindow;
class SplitViewController;
class TestShellDelegate;
class WindowPreviewView;

// The base test fixture for testing Overview Mode.
class OverviewTestBase : public AshTestBase {
 public:
  template <typename... TaskEnvironmentTraits>
  explicit OverviewTestBase(TaskEnvironmentTraits&&... traits)
      : AshTestBase(std::forward<TaskEnvironmentTraits>(traits)...) {}
  OverviewTestBase(const OverviewTestBase&) = delete;
  OverviewTestBase& operator=(const OverviewTestBase&) = delete;
  ~OverviewTestBase() override;

  // Enters tablet mode. Needed by tests that test dragging and or splitview,
  // which are tablet mode only.
  void EnterTabletMode();

  bool InOverviewSession();

  bool WindowsOverlapping(aura::Window* window1, aura::Window* window2);

  // Creates a window which cannot be snapped by splitview.
  std::unique_ptr<aura::Window> CreateUnsnappableWindow(
      const gfx::Rect& bounds = gfx::Rect());

  void ClickWindow(aura::Window* window);

  OverviewController* GetOverviewController();
  OverviewSession* GetOverviewSession();

  SplitViewController* GetSplitViewController();

  gfx::Rect GetTransformedBounds(aura::Window* window);

  gfx::Rect GetTransformedTargetBounds(aura::Window* window);

  gfx::Rect GetTransformedBoundsInRootWindow(aura::Window* window);

  OverviewItem* GetDropTarget(int grid_index);

  views::ImageButton* GetCloseButton(OverviewItem* item);

  views::Label* GetLabelView(OverviewItem* item);

  views::View* GetBackdropView(OverviewItem* item);

  WindowPreviewView* GetPreviewView(OverviewItem* item);

  float GetCloseButtonOpacity(OverviewItem* item);

  float GetTitlebarOpacity(OverviewItem* item);
  const ScopedOverviewTransformWindow& GetTransformWindow(
      OverviewItem* item) const;
  bool HasRoundedCorner(OverviewItem* item);

  // Tests that a window is contained within a given OverviewItem, and that both
  // the window and its matching close button are within the same screen.
  void CheckWindowAndCloseButtonInScreen(aura::Window* window,
                                         OverviewItem* window_item);

  gfx::Rect GetGridBounds();
  void SetGridBounds(OverviewGrid* grid, const gfx::Rect& bounds);

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  // Sets up the test suite with a custom `delegate`. If nullptr is used, a
  // TestShellDelegate will be used by default.
  void SetUpInternal(std::unique_ptr<TestShellDelegate> delegate);

  void CheckForDuplicateTraceName(const char* trace);

 private:
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_api_;
  std::vector<std::string> trace_names_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_TEST_BASE_H_
