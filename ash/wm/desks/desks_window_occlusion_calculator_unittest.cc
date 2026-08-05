// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/app_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_builder.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"

namespace ash {
namespace {

class DesksWindowOcclusionCalculatorTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DesksWindowOcclusionCalculatorTest() = default;
  DesksWindowOcclusionCalculatorTest(
      const DesksWindowOcclusionCalculatorTest&) = delete;
  DesksWindowOcclusionCalculatorTest& operator=(
      const DesksWindowOcclusionCalculatorTest&) = delete;
  ~DesksWindowOcclusionCalculatorTest() override = default;

  // AshTestBase:
  void SetUp() override { AshTestBase::SetUp(); }
};

bool HasLayerWithName(const ui::Layer* layer, const std::string& name) {
  if (layer->name() == name) {
    return true;
  }
  for (ui::Layer* child : layer->children()) {
    if (HasLayerWithName(child, name)) {
      return true;
    }
  }
  return false;
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         DesksWindowOcclusionCalculatorTest,
                         testing::Values(false));

// Tests that desk bar mini views accurately update and filter occluded windows
// from their mirrored layer trees during desk operations like moving windows
// between desks, adding new desks, switching active desks, and removing desks.
TEST_P(DesksWindowOcclusionCalculatorTest, DeskBarOcclusionIntegration) {
  auto* desk_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desk_controller->desks().size());

  auto win0 = CreateWindowWithAppType(chromeos::AppType::SYSTEM_APP,
                                      gfx::Rect(0, 0, 100, 100));  // bottom
  win0->SetName("Win0");
  auto win1 = CreateWindowWithAppType(
      chromeos::AppType::SYSTEM_APP,
      gfx::Rect(0, 0, 100, 100));  // top, fully occludes win0.
  win1->SetName("Win1");

  // 1. Enter Overview
  EnterOverview();
  auto* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  auto* root_window = win0->GetRootWindow();
  auto* desk1 = desk_controller->desks()[0].get();
  auto* tree_owner1 = DesksTestApi::GetMirroredContentsLayerTreeForRootAndDesk(
      root_window, desk1);
  ASSERT_TRUE(tree_owner1);
  EXPECT_TRUE(HasLayerWithName(tree_owner1->root(), "Win1"));
  EXPECT_FALSE(HasLayerWithName(tree_owner1->root(), "Win0"));

  // 2. Drag window to another desk (simulate moving)
  auto* desk2 = desk_controller->desks()[1].get();
  desk_controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk2, root_window,
      DesksMoveWindowFromActiveDeskSource::kDragAndDrop);

  auto* tree_owner2 = DesksTestApi::GetMirroredContentsLayerTreeForRootAndDesk(
      root_window, desk2);
  ASSERT_TRUE(tree_owner2);
  EXPECT_TRUE(HasLayerWithName(tree_owner2->root(), "Win1"));

  // win1 was moved. The new calculator fixes the stale active desk snapshot
  // bug, so win0 becomes visible. The legacy calculator has the bug, so
  // win0 remains occluded.
  tree_owner1 = DesksTestApi::GetMirroredContentsLayerTreeForRootAndDesk(
      root_window, desk1);
  ASSERT_TRUE(tree_owner1);
  if (GetParam()) {
    EXPECT_TRUE(HasLayerWithName(tree_owner1->root(), "Win0"));
  } else {
    EXPECT_FALSE(HasLayerWithName(tree_owner1->root(), "Win0"));
  }

  // 3. Add a new desk in overview
  auto* new_desk_button =
      GetOverviewGridForRoot(root_window)->desks_bar_view()->new_desk_button();
  LeftClickOn(new_desk_button);
  ASSERT_EQ(3u, desk_controller->desks().size());

  // 4. Switch desk while in overview
  auto* desk3 = desk_controller->desks()[2].get();
  ActivateDesk(desk3);

  auto win2 = CreateWindowWithAppType(chromeos::AppType::SYSTEM_APP,
                                      gfx::Rect(10, 10, 50, 50));
  win2->SetName("Win2");

  // 5. Delete a desk
  RemoveDesk(desk1);
  ASSERT_EQ(2u, desk_controller->desks().size());

  // Enter overview again to show the desk bar and verify mirror layers.
  EnterOverview();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // win0 from desk1 is merged into active desk (desk3).
  // win0 (100x100) and win2 (50x50) do not fully occlude each other.
  auto* tree_owner3 = DesksTestApi::GetMirroredContentsLayerTreeForRootAndDesk(
      root_window, desk3);
  ASSERT_TRUE(tree_owner3);
  EXPECT_TRUE(HasLayerWithName(tree_owner3->root(), "Win0"));
  EXPECT_TRUE(HasLayerWithName(tree_owner3->root(), "Win2"));

  // 6. Exit overview
  ExitOverview();
  ASSERT_FALSE(overview_controller->InOverviewSession());
}

TEST_P(DesksWindowOcclusionCalculatorTest, MirroredLayerTreeValidation) {
  auto* desk_controller = DesksController::Get();
  // Must have at least 2 desks to show mini views in clamshell overview.
  NewDesk();
  Desk* active_desk = desk_controller->desks()[0].get();

  auto* desk_container =
      active_desk->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow());

  // Create a fully occluded window underneath it.
  std::unique_ptr<aura::Window> occluded_window(
      aura::test::TestWindowBuilder()
          .SetColorWindowDelegate(SK_ColorBLUE)
          .SetParent(desk_container)
          .SetBounds(gfx::Rect(100, 100, 100, 100))
          .SetWindowType(aura::client::WINDOW_TYPE_NORMAL)
          .AllowAllWindowStates()
          .Build()
          .release());
  occluded_window->SetName("OccludedWindow");
  occluded_window->TrackOcclusionState();

  // Create a completely occluding window.
  std::unique_ptr<aura::Window> occluding_window(
      aura::test::TestWindowBuilder()
          .SetColorWindowDelegate(SK_ColorRED)
          .SetParent(desk_container)
          .SetBounds(gfx::Rect(0, 0, 1000, 1000))
          .SetWindowType(aura::client::WINDOW_TYPE_NORMAL)
          .AllowAllWindowStates()
          .Build()
          .release());
  occluding_window->SetName("OccludingWindow");
  occluding_window->TrackOcclusionState();

  // Compute occlusion BEFORE overview starts so the tracker caches the
  // non-overview states. This emulates the real environment which computes
  // the state before pausing and locking the tracker for overview mode
  // animations.

  // Enter overview mode. This triggers RecreateDeskContentsMirrorLayers().
  EnterOverview();
  auto* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Fetch the resulting graphical layer tree generated by DeskPreviewView.
  auto* layer_tree_owner =
      DesksTestApi::GetMirroredContentsLayerTreeForRootAndDesk(
          Shell::GetPrimaryRootWindow(), active_desk);
  ASSERT_TRUE(layer_tree_owner);
  const ui::Layer* root_layer = layer_tree_owner->root();
  ASSERT_TRUE(root_layer);

  // The occluding window must be in the mirrored layer tree.
  EXPECT_TRUE(HasLayerWithName(root_layer, "OccludingWindow"));

  // And the fully occluded window's mirrored layer was explicitly skipped.
  EXPECT_FALSE(HasLayerWithName(root_layer, "OccludedWindow"));
}

TEST_P(DesksWindowOcclusionCalculatorTest, ShowsBackgroundDeskWindows) {
  // Create Desk 2.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  Desk* desk1 = desks_controller->GetDeskAtIndex(0);
  Desk* desk2 = desks_controller->GetDeskAtIndex(1);

  auto* desk_container =
      desk1->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow());

  // Create a fully occluded window underneath.
  std::unique_ptr<aura::Window> occluded_window(
      aura::test::TestWindowBuilder()
          .SetColorWindowDelegate(SK_ColorBLUE)
          .SetParent(desk_container)
          .SetBounds(gfx::Rect(100, 100, 100, 100))
          .SetWindowType(aura::client::WINDOW_TYPE_NORMAL)
          .AllowAllWindowStates()
          .Build()
          .release());
  occluded_window->SetName("OccludedWindow");

  // Create a completely occluding window.
  std::unique_ptr<aura::Window> occluding_window(
      aura::test::TestWindowBuilder()
          .SetColorWindowDelegate(SK_ColorRED)
          .SetParent(desk_container)
          .SetBounds(gfx::Rect(0, 0, 1000, 1000))
          .SetWindowType(aura::client::WINDOW_TYPE_NORMAL)
          .AllowAllWindowStates()
          .Build()
          .release());
  occluding_window->SetName("OccludingWindow");

  occluded_window->TrackOcclusionState();
  occluding_window->TrackOcclusionState();

  // Compute occlusion BEFORE Desk 1 is deactivated/hidden.

  // Activate Desk 2.
  ActivateDesk(desk2);
  ASSERT_FALSE(desk1->is_active());

  // Enter overview.
  EnterOverview();

  // Verify that the mirror layer for window1 on Desk 1's mini view exists.
  auto* root = Shell::GetPrimaryRootWindow();
  const ui::LayerTreeOwner* layer_tree_owner =
      DesksTestApi::GetMirroredContentsLayerTreeForRootAndDesk(root, desk1);
  ASSERT_TRUE(layer_tree_owner);

  const ui::Layer* root_layer = layer_tree_owner->root();
  ASSERT_TRUE(root_layer);

  // The occluding window must be in the mirrored layer tree.
  EXPECT_TRUE(HasLayerWithName(root_layer, "OccludingWindow"));

  // And the fully occluded window's mirrored layer was explicitly skipped.
  EXPECT_FALSE(HasLayerWithName(root_layer, "OccludedWindow"));

  // Exit overview to avoid RecreateDeskContentsMirrorLayers firing during the
  // teardown/destruction of the custom test windows since it accesses the
  // global WindowOcclusionTracker.
  ExitOverview();
}

}  // namespace ash
