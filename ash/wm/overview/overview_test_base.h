// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_TEST_BASE_H_
#define ASH_WM_OVERVIEW_OVERVIEW_TEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/shelf/shelf_view_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/desks/templates/saved_desk_test_helper.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class CloseButton;
class OverviewController;
class OverviewGrid;
class OverviewItem;
class OverviewItemBase;
class OverviewSession;
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

  // Enters/Leaves tablet mode.
  void EnterTabletMode();
  void LeaveTabletMode();

  bool InOverviewSession() const;

  bool WindowsOverlapping(aura::Window* window1, aura::Window* window2) const;

  // Creates a window which cannot be snapped by splitview.
  std::unique_ptr<aura::Window> CreateUnsnappableWindow(
      const gfx::Rect& bounds = gfx::Rect());

  void ClickWindow(aura::Window* window);

  OverviewController* GetOverviewController();
  OverviewSession* GetOverviewSession();

  SplitViewController* GetSplitViewController();

  gfx::Rect GetTransformedBounds(aura::Window* window) const;

  gfx::Rect GetTransformedTargetBounds(aura::Window* window) const;

  gfx::Rect GetTransformedBoundsInRootWindow(aura::Window* window) const;

  const OverviewItemBase* GetDropTarget(int grid_index) const;

  CloseButton* GetCloseButton(OverviewItemBase* item);

  views::Label* GetLabelView(OverviewItemBase* item);

  views::View* GetBackdropView(OverviewItemBase* item);

  WindowPreviewView* GetPreviewView(OverviewItemBase* item);

  gfx::Rect GetShadowBounds(const OverviewItemBase* item) const;

  views::Widget* GetCannotSnapWidget(OverviewItemBase* item);

  void SetAnimatingToClose(OverviewItemBase* item, bool val);

  float GetCloseButtonOpacity(OverviewItemBase* item);

  float GetTitlebarOpacity(OverviewItemBase* item);

  bool HasRoundedCorner(OverviewItemBase* item);

  // Tests that a window is contained within a given OverviewItem, and that both
  // the window and its matching close button are within the same screen.
  void CheckWindowAndCloseButtonInScreen(aura::Window* window,
                                         OverviewItemBase* window_item);

  void CheckOverviewEnterExitHistogram(const std::string& trace,
                                       const std::vector<int>& enter_counts,
                                       const std::vector<int>& exit_counts);

  gfx::Rect GetGridBounds();
  void SetGridBounds(OverviewGrid* grid, const gfx::Rect& bounds);

  SavedDeskTestHelper* saved_desk_test_helper() {
    return ash_test_helper()->saved_desk_test_helper();
  }

  desks_storage::DeskModel* desk_model() {
    return saved_desk_test_helper()->desk_model();
  }

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

 private:
  void CheckOverviewHistogram(const std::string& histogram,
                              const std::vector<int>& counts);

  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kOverviewSessionInitOptimizations};
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_api_;
  std::vector<std::string> trace_names_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_TEST_BASE_H_
