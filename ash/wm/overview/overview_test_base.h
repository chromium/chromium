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
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/account_id/account_id.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class CloseButton;
class OverviewController;
class OverviewGrid;
class OverviewItem;
class OverviewSession;
class ScopedOverviewTransformWindow;
class SplitViewController;
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

  CloseButton* GetCloseButton(OverviewItem* item);

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

  void CheckOverviewEnterExitHistogram(const std::string& trace,
                                       const std::vector<int>& enter_counts,
                                       const std::vector<int>& exit_counts);

  gfx::Rect GetGridBounds();
  void SetGridBounds(OverviewGrid* grid, const gfx::Rect& bounds);

  desks_storage::DeskModel* desk_model() { return desk_model_.get(); }

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  void CheckForDuplicateTraceName(const std::string& trace);

  // Takes in a current widget and checks if the accessibility next
  // and previous focus widgets match the given.
  void CheckA11yOverrides(const std::string& trace,
                          views::Widget* widget,
                          views::Widget* expected_previous,
                          views::Widget* expected_next);

  base::HistogramTester histograms_;
  std::unique_ptr<apps::AppRegistryCache> cache_;
  AccountId account_id_;

 private:
  void CheckOverviewHistogram(const std::string& histogram,
                              const std::vector<int>& counts);

  std::unique_ptr<desks_storage::LocalDeskDataManager> desk_model_;
  base::ScopedTempDir user_data_temp_dir_;
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_api_;
  std::vector<std::string> trace_names_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_TEST_BASE_H_
