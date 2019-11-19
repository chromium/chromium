// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/multi_user_window_manager_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "ash/window_factory.h"
#include "ash/wm/desks/close_desk_button.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/display.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

void NewDesk() {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
}

std::unique_ptr<aura::Window> CreateTransientWindow(
    aura::Window* transient_parent,
    const gfx::Rect& bounds) {
  std::unique_ptr<aura::Window> window =
      window_factory::NewWindow(nullptr, aura::client::WINDOW_TYPE_POPUP);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(bounds);
  ::wm::AddTransientChild(transient_parent, window.get());
  aura::client::ParentWindowWithContext(
      window.get(), transient_parent->GetRootWindow(), bounds);
  window->Show();
  return window;
}

std::unique_ptr<aura::Window> CreateTransientModalChildWindow(
    aura::Window* transient_parent) {
  auto child =
      CreateTransientWindow(transient_parent, gfx::Rect(20, 30, 200, 150));
  child->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  ::wm::SetModalParent(child.get(), transient_parent);
  return child;
}

bool DoesActiveDeskContainWindow(aura::Window* window) {
  return base::Contains(DesksController::Get()->active_desk()->windows(),
                        window);
}

OverviewGrid* GetOverviewGridForRoot(aura::Window* root) {
  DCHECK(root->IsRootWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());

  return overview_controller->overview_session()->GetGridWithRootWindow(root);
}

void CloseDeskFromMiniView(const DeskMiniView* desk_mini_view,
                           ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);

  // Move to the center of the mini view so that the close button shows up.
  const gfx::Point mini_view_center =
      desk_mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  EXPECT_TRUE(desk_mini_view->close_desk_button()->GetVisible());
  // Move to the center of the close button and click.
  event_generator->MoveMouseTo(
      desk_mini_view->close_desk_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
}

void ClickOnMiniView(const DeskMiniView* desk_mini_view,
                     ui::test::EventGenerator* event_generator) {
  DCHECK(desk_mini_view);

  const gfx::Point mini_view_center =
      desk_mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  event_generator->ClickLeftButton();
}

void LongGestureTap(const gfx::Point& screen_location,
                    ui::test::EventGenerator* event_generator,
                    bool release_touch = true) {
  event_generator->set_current_screen_location(screen_location);
  event_generator->PressTouch();
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  const int long_press_delay_ms = gesture_config->long_press_time_in_ms() +
                                  gesture_config->show_press_delay_in_ms();
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(long_press_delay_ms));
  run_loop.Run();
  if (release_touch)
    event_generator->ReleaseTouch();
}

void GestureTapOnView(const views::View* view,
                      ui::test::EventGenerator* event_generator) {
  event_generator->GestureTapAt(view->GetBoundsInScreen().CenterPoint());
}

// If |drop| is false, the dragged |item| won't be dropped; giving the caller
// a chance to do some validations before the item is dropped.
void DragItemToPoint(OverviewItem* item,
                     const gfx::Point& screen_location,
                     ui::test::EventGenerator* event_generator,
                     bool by_touch_gestures = false,
                     bool drop = true) {
  DCHECK(item);

  const gfx::Point item_center =
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint());
  event_generator->set_current_screen_location(item_center);
  if (by_touch_gestures) {
    event_generator->PressTouch();
    // Move the touch by an enough amount in X to engage in the normal drag mode
    // rather than the drag to close mode.
    event_generator->MoveTouchBy(50, 0);
    event_generator->MoveTouch(screen_location);
    if (drop)
      event_generator->ReleaseTouch();
  } else {
    event_generator->PressLeftButton();
    event_generator->MoveMouseTo(screen_location);
    if (drop)
      event_generator->ReleaseLeftButton();
  }
}

BackdropController* GetDeskBackdropController(const Desk* desk,
                                              aura::Window* root) {
  auto* workspace_controller =
      GetWorkspaceController(desk->GetDeskContainerForRoot(root));
  WorkspaceLayoutManager* layout_manager =
      workspace_controller->layout_manager();
  return layout_manager->backdrop_controller();
}

// Returns true if |win1| is stacked (not directly) below |win2|.
bool IsStackedBelow(aura::Window* win1, aura::Window* win2) {
  DCHECK_NE(win1, win2);
  DCHECK_EQ(win1->parent(), win2->parent());

  const auto& children = win1->parent()->children();
  auto win1_iter = std::find(children.begin(), children.end(), win1);
  auto win2_iter = std::find(children.begin(), children.end(), win2);
  DCHECK(win1_iter != children.end());
  DCHECK(win2_iter != children.end());
  return win1_iter < win2_iter;
}

// Verifies DesksBarView layout under different screen sizes
void DesksBarViewLayoutTestHelper(const DesksBarView* desks_bar_view,
                                  bool use_compact_layout) {
  DCHECK(desks_bar_view);
  const NewDeskButton* button = desks_bar_view->new_desk_button();
  EXPECT_EQ(button->IsLabelVisibleForTesting(), !use_compact_layout);

  for (const auto& mini_view : desks_bar_view->mini_views()) {
    EXPECT_EQ(mini_view->GetDeskPreviewForTesting()->height(),
              DeskPreviewView::GetHeight(use_compact_layout));
    EXPECT_EQ(mini_view->IsLabelVisibleForTesting(), !use_compact_layout);
  }
}

// Defines an observer to test DesksController notifications.
class TestObserver : public DesksController::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  const std::vector<const Desk*>& desks() const { return desks_; }

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override {
    desks_.emplace_back(desk);
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskRemoved(const Desk* desk) override {
    base::Erase(desks_, desk);
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {
    EXPECT_TRUE(DesksController::Get()->AreDesksBeingModified());
  }
  void OnDeskSwitchAnimationLaunching() override {}
  void OnDeskSwitchAnimationFinished() override {
    EXPECT_FALSE(DesksController::Get()->AreDesksBeingModified());
  }

 private:
  std::vector<const Desk*> desks_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestDeskObserver : public Desk::Observer {
 public:
  TestDeskObserver() = default;
  ~TestDeskObserver() override = default;

  int notify_counts() const { return notify_counts_; }

  // Desk::Observer:
  void OnContentChanged() override { ++notify_counts_; }
  void OnDeskDestroyed(const Desk* desk) override {}

 private:
  int notify_counts_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestDeskObserver);
};

class DesksTest : public AshTestBase,
                  public ::testing::WithParamInterface<bool> {
 public:
  DesksTest() = default;
  ~DesksTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVirtualDesks);

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DesksTest);
};

TEST_F(DesksTest, LongPressOverviewItemInClamshellModeWithOnlyOneVirtualDesk) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  ASSERT_TRUE(overview_controller->StartOverview());
  OverviewSession* overview_session = overview_controller->overview_session();
  OverviewItem* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  LongGestureTap(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()),
      event_generator, /*release_touch=*/false);
  EXPECT_TRUE(overview_item->IsDragItem());
  EXPECT_EQ(
      OverviewWindowDragController::DragBehavior::kUndefined,
      overview_session->window_drag_controller()->current_drag_behavior());
  event_generator->ReleaseTouch();
  EXPECT_FALSE(overview_item->IsDragItem());
}

TEST_F(DesksTest, DesksCreationAndRemoval) {
  TestObserver observer;
  auto* controller = DesksController::Get();
  controller->AddObserver(&observer);

  // There's always a default pre-existing desk that cannot be removed.
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());

  // Add desks until no longer possible.
  while (controller->CanCreateDesks())
    NewDesk();

  // Expect we've reached the max number of desks, and we've been notified only
  // with the newly created desks.
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_EQ(desks_util::kMaxNumberOfDesks - 1, observer.desks().size());
  EXPECT_TRUE(controller->CanRemoveDesks());

  // Remove all desks until no longer possible, and expect that there's always
  // one default desk remaining.
  while (controller->CanRemoveDesks())
    RemoveDesk(observer.desks().back());

  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(observer.desks().empty());

  controller->RemoveObserver(&observer);
}

TEST_F(DesksTest, DesksBarViewDeskCreation) {
  auto* controller = DesksController::Get();

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  // Initially the grid is not offset down when there are no desk mini_views
  // once animations are added.
  EXPECT_FALSE(overview_grid->IsDesksBarViewActive());

  const auto* desks_bar_view = overview_grid->desks_bar_view();

  // Since we have a single default desk, there should be no mini_views, and the
  // new desk button is enabled.
  DCHECK(desks_bar_view);
  EXPECT_TRUE(desks_bar_view->mini_views().empty());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->GetEnabled());

  // Click many times on the new desk button and expect only the max number of
  // desks will be created, and the button is no longer enabled.
  const gfx::Point button_center =
      desks_bar_view->new_desk_button()->GetBoundsInScreen().CenterPoint();

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(button_center);
  for (size_t i = 0; i < desks_util::kMaxNumberOfDesks + 2; ++i)
    event_generator->ClickLeftButton();

  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_FALSE(controller->CanCreateDesks());
  EXPECT_TRUE(controller->CanRemoveDesks());
  EXPECT_FALSE(desks_bar_view->new_desk_button()->GetEnabled());

  // Hover over one of the mini_views, and expect that the close button becomes
  // visible.
  const auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_FALSE(mini_view->close_desk_button()->GetVisible());
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  EXPECT_TRUE(mini_view->close_desk_button()->GetVisible());

  // Use the close button to close the desk.
  event_generator->MoveMouseTo(
      mini_view->close_desk_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  // The new desk button is now enabled again.
  EXPECT_EQ(desks_util::kMaxNumberOfDesks - 1, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->GetEnabled());

  // Exit overview mode and re-enter. Since we have more than one pre-existing
  // desks, their mini_views should be created upon construction of the desks
  // bar.
  overview_controller->EndOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Get the new grid and the new desk_bar_view.
  overview_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  desks_bar_view = overview_grid->desks_bar_view();

  DCHECK(desks_bar_view);
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->GetEnabled());
}

TEST_F(DesksTest, DesksBarViewScreenLayoutTest) {
  UpdateDisplay("1600x1200");
  DesksController* controller = DesksController::Get();
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  EXPECT_FALSE(overview_grid->IsDesksBarViewActive());
  const DesksBarView* desks_bar_view = overview_grid->desks_bar_view();
  while (controller->CanCreateDesks()) {
    NewDesk();
    DesksBarViewLayoutTestHelper(desks_bar_view,
                                 /*use_compact_layout=*/false);
  };

  UpdateDisplay("500x480");
  ASSERT_TRUE(overview_controller->InOverviewSession());
  while (controller->CanRemoveDesks()) {
    DesksBarViewLayoutTestHelper(desks_bar_view,
                                 /*use_compact_layout=*/true);
    RemoveDesk(controller->desks().back().get());
  }

  UpdateDisplay("1600x480");
  ASSERT_TRUE(overview_controller->InOverviewSession());
  while (controller->CanCreateDesks()) {
    DesksBarViewLayoutTestHelper(desks_bar_view,
                                 /*use_compact_layout=*/false);
    NewDesk();
  }
}

TEST_F(DesksTest, DeskActivation) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  auto* root = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desk_1->GetDeskContainerForRoot(root));

  // Create three new desks, and activate one of the middle ones.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  const Desk* desk_3 = controller->desks()[2].get();
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ActivateDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());

  // Remove the active desk, which is in the middle, activation should move to
  // the left, so desk 1 should be activated.
  EXPECT_FALSE(controller->AreDesksBeingModified());
  RemoveDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ASSERT_EQ(3u, controller->desks().size());
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_TRUE(desk_1->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());

  // Remove the active desk, it's the first one on the left, so desk_3 (on the
  // right) will be activated.
  EXPECT_FALSE(controller->AreDesksBeingModified());
  RemoveDesk(desk_1);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_EQ(desk_3, controller->active_desk());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());
  EXPECT_TRUE(desk_3->GetDeskContainerForRoot(root)->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(root)->IsVisible());
}

TEST_F(DesksTest, TestWindowPositioningPaused) {
  auto* controller = DesksController::Get();
  NewDesk();

  // Create two windows whose window positioning is managed.
  const auto win0_bounds = gfx::Rect{10, 20, 250, 100};
  const auto win1_bounds = gfx::Rect{50, 50, 200, 200};
  auto win0 = CreateAppWindow(win0_bounds);
  auto win1 = CreateAppWindow(win1_bounds);
  WindowState* window_state = WindowState::Get(win0.get());
  window_state->SetWindowPositionManaged(true);
  window_state = WindowState::Get(win1.get());
  window_state->SetWindowPositionManaged(true);
  EXPECT_EQ(win0_bounds, win0->GetBoundsInScreen());
  EXPECT_EQ(win1_bounds, win1->GetBoundsInScreen());

  // Moving one window to the second desk should not affect the bounds of either
  // windows.
  Desk* desk_2 = controller->desks()[1].get();
  controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk_2, DesksMoveWindowFromActiveDeskSource::kDragAndDrop);
  EXPECT_EQ(win0_bounds, win0->GetBoundsInScreen());
  EXPECT_EQ(win1_bounds, win1->GetBoundsInScreen());

  // Removing a desk, which results in moving its windows to another desk should
  // not affect the positions of those managed windows.
  RemoveDesk(desk_2);
  EXPECT_EQ(win0_bounds, win0->GetBoundsInScreen());
  EXPECT_EQ(win1_bounds, win1->GetBoundsInScreen());
}

// This test makes sure we have coverage for that desk switch animation when run
// with multiple displays.
TEST_F(DesksTest, DeskActivationDualDisplay) {
  UpdateDisplay("600x600,400x500");

  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create three new desks, and activate one of the middle ones.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  const Desk* desk_3 = controller->desks()[2].get();
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(controller->AreDesksBeingModified());
  ActivateDesk(desk_2);
  EXPECT_FALSE(controller->AreDesksBeingModified());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(desk_3->is_active());
  EXPECT_FALSE(desk_4->is_active());

  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_1->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_TRUE(desk_2->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_3->GetDeskContainerForRoot(roots[1])->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(roots[0])->IsVisible());
  EXPECT_FALSE(desk_4->GetDeskContainerForRoot(roots[1])->IsVisible());
}

TEST_F(DesksTest, TransientWindows) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create two windows, one is a transient child of the other.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTransientWindow(win0.get(), gfx::Rect(100, 100, 100, 100));

  EXPECT_EQ(2u, desk_1->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));

  auto* root = Shell::GetPrimaryRootWindow();
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desks_util::GetDeskContainerForContext(win0.get()));
  EXPECT_EQ(desks_util::GetActiveDeskContainerForRoot(root),
            desks_util::GetDeskContainerForContext(win1.get()));

  // Create a new desk and activate it.
  NewDesk();
  const Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_2->windows().empty());
  ActivateDesk(desk_2);
  EXPECT_FALSE(desk_1->is_active());
  EXPECT_TRUE(desk_2->is_active());

  // Create another transient child of the earlier transient child, and confirm
  // it's tracked in desk_1 (even though desk_2 is the currently active one).
  // This is because the transient parent exists in desk_1.
  auto win2 = CreateTransientWindow(win1.get(), gfx::Rect(100, 100, 50, 50));
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win2.get()));
  auto* desk_1_container = desk_1->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_1_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_1_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_1_container->children()[2]);

  // Remove the inactive desk 1, and expect that its windows, including
  // transient will move to desk 2.
  RemoveDesk(desk_1);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_EQ(3u, desk_2->windows().size());
  auto* desk_2_container = desk_2->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_2_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_2_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_2_container->children()[2]);
}

TEST_F(DesksTest, TransientModalChildren) {
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(desk_1->is_active());

  // Create three windows, one of them is a modal transient child.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateTransientModalChildWindow(win0.get());
  EXPECT_EQ(win1.get(), ::wm::GetModalTransient(win0.get()));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 200, 100));
  ASSERT_EQ(3u, desk_1->windows().size());
  auto* root = Shell::GetPrimaryRootWindow();
  auto* desk_1_container = desk_1->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_1_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_1_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_1_container->children()[2]);

  // Remove desk_1, and expect that all its windows (including the transient
  // modal child and its parent) are moved to desk_2, and that their z-order
  // within the container is preserved.
  RemoveDesk(desk_1);
  EXPECT_EQ(desk_2, controller->active_desk());
  ASSERT_EQ(3u, desk_2->windows().size());
  auto* desk_2_container = desk_2->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_2_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_2_container->children()[1]);
  EXPECT_EQ(win2.get(), desk_2_container->children()[2]);
  EXPECT_EQ(win1.get(), ::wm::GetModalTransient(win0.get()));

  // Move only the modal child window to desk_3, and expect that its parent will
  // move along with it, and their z-order is preserved.
  controller->MoveWindowFromActiveDeskTo(
      win1.get(), desk_3, DesksMoveWindowFromActiveDeskSource::kDragAndDrop);
  ASSERT_EQ(1u, desk_2->windows().size());
  ASSERT_EQ(2u, desk_3->windows().size());
  EXPECT_EQ(win2.get(), desk_2_container->children()[0]);
  auto* desk_3_container = desk_3->GetDeskContainerForRoot(root);
  EXPECT_EQ(win0.get(), desk_3_container->children()[0]);
  EXPECT_EQ(win1.get(), desk_3_container->children()[1]);

  // The modality remains the same.
  ActivateDesk(desk_3);
  EXPECT_EQ(win1.get(), ::wm::GetModalTransient(win0.get()));
}

TEST_F(DesksTest, WindowActivation) {
  // Create three windows.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateAppWindow(gfx::Rect(100, 100, 100, 100));

  EXPECT_TRUE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win2.get()));

  // Activate win0 and expects that it remains activated until we switch desks.
  wm::ActivateWindow(win0.get());

  // Create a new desk and activate it. Expect it's not tracking any windows
  // yet.
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // Activate the newly-added desk. Expect that the tracked windows per each
  // desk will remain the same.
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  EXPECT_EQ(3u, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());

  // `desk_2` has no windows, so now no window should be active. However,
  // windows on `desk_1` are activateable.
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
  EXPECT_TRUE(wm::CanActivateWindow(win0.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win1.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win2.get()));

  // Create two new windows, they should now go to desk_2.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win4 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  wm::ActivateWindow(win3.get());
  EXPECT_EQ(2u, desk_2->windows().size());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win3.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win4.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_FALSE(DoesActiveDeskContainWindow(win2.get()));
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Delete `win0` and expect that `desk_1`'s windows will be updated.
  win0.reset();
  EXPECT_EQ(2u, desk_1->windows().size());
  EXPECT_EQ(2u, desk_2->windows().size());
  // No change in the activation.
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Switch back to `desk_1`. Now we can activate its windows.
  ActivateDesk(desk_1);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_TRUE(wm::CanActivateWindow(win1.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win2.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win3.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win4.get()));

  // After `win0` has been deleted, `win2` is next on the MRU list.
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // Remove `desk_2` and expect that its windows will be moved to the active
  // desk.
  TestDeskObserver observer;
  desk_1->AddObserver(&observer);
  RemoveDesk(desk_2);
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_EQ(4u, desk_1->windows().size());
  // Even though two new windows have been added to desk_1, observers should be
  // notified only once.
  EXPECT_EQ(1, observer.notify_counts());
  desk_1->RemoveObserver(&observer);
  EXPECT_TRUE(DoesActiveDeskContainWindow(win3.get()));
  EXPECT_TRUE(DoesActiveDeskContainWindow(win4.get()));

  // `desk_2`'s windows moved to `desk_1`, but that should not change the
  // already active window.
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // Moved windows can still be activated.
  EXPECT_TRUE(wm::CanActivateWindow(win3.get()));
  EXPECT_TRUE(wm::CanActivateWindow(win4.get()));
}

TEST_F(DesksTest, ActivateDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Create two windows on desk_1.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());

  // Enter overview mode, and expect the desk bar is shown with exactly four
  // desks mini views, and there are exactly two windows in the overview mode
  // grid.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());
  EXPECT_EQ(2u, overview_grid->window_list().size());

  // Activate desk_4 (last one on the right) by clicking on its mini view.
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(desk_4->is_active());
  auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_EQ(desk_4, mini_view->desk());
  EXPECT_FALSE(mini_view->close_desk_button()->GetVisible());
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(mini_view_center);
  DeskSwitchAnimationWaiter waiter;
  event_generator->ClickLeftButton();
  waiter.Wait();

  // Expect that desk_4 is now active, and overview mode exited.
  EXPECT_TRUE(desk_4->is_active());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  // Exiting overview mode should not restore focus to a window on a
  // now-inactive desk. Run a loop since the overview session is destroyed async
  // and until that happens, focus will be on the dummy
  // "OverviewModeFocusedWidget".
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Create one window in desk_4 and enter overview mode. Expect the grid is
  // showing exactly one window.
  auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win2.get());
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->window_list().size());

  // When exiting overview mode without changing desks, the focus should be
  // restored to the same window.
  overview_controller->EndOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  // Run a loop since the overview session is destroyed async and until that
  // happens, focus will be on the dummy "OverviewModeFocusedWidget".
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());
}

// This test makes sure we have coverage for that desk switch animation when run
// with multiple displays while overview mode is active.
TEST_F(DesksTest, ActivateDeskFromOverviewDualDisplay) {
  UpdateDisplay("600x600,400x500");

  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Enter overview mode.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());
  // Use secondary display grid.
  const auto* overview_grid = GetOverviewGridForRoot(roots[1]);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());

  // Activate desk_4 (last one on the right) by clicking on its mini view.
  const Desk* desk_4 = controller->desks()[3].get();
  EXPECT_FALSE(desk_4->is_active());
  const auto* mini_view = desks_bar_view->mini_views().back().get();
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(mini_view_center);
  DeskSwitchAnimationWaiter waiter;
  event_generator->ClickLeftButton();
  waiter.Wait();

  // Expect that desk_4 is now active, and overview mode exited.
  EXPECT_TRUE(desk_4->is_active());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_F(DesksTest, RemoveInactiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create three desks other than the default initial desk.
  NewDesk();
  NewDesk();
  NewDesk();
  ASSERT_EQ(4u, controller->desks().size());

  // Create two windows on desk_1.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // Active desk_4 and enter overview mode. Expect that the grid is currently
  // empty.
  Desk* desk_4 = controller->desks()[3].get();
  ActivateDesk(desk_4);
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(overview_grid->window_list().empty());

  // Remove desk_1 using the close button on its mini view. desk_1 is currently
  // inactive. Its windows should be moved to desk_4 and added to the overview
  // grid in the MRU order (win0, and win1).
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(4u, desks_bar_view->mini_views().size());
  Desk* desk_1 = controller->desks()[0].get();
  auto* mini_view = desks_bar_view->mini_views().front().get();
  EXPECT_EQ(desk_1, mini_view->desk());

  // Setup observers of both the active and inactive desks to make sure
  // refreshing the mini_view is requested only *once* for the active desk, and
  // never for the to-be-removed inactive desk.
  TestDeskObserver desk_4_observer;
  desk_4->AddObserver(&desk_4_observer);
  TestDeskObserver desk_1_observer;
  desk_1->AddObserver(&desk_1_observer);

  CloseDeskFromMiniView(mini_view, GetEventGenerator());

  EXPECT_EQ(0, desk_1_observer.notify_counts());
  EXPECT_EQ(1, desk_4_observer.notify_counts());

  ASSERT_EQ(3u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_EQ(2u, overview_grid->window_list().size());
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win0.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win1.get()));
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win0.get()),
            overview_grid->window_list()[0].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win1.get()),
            overview_grid->window_list()[1].get());

  // Make sure overview mode remains active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Removing a desk with no windows should not result in any new mini_views
  // updates.
  mini_view = desks_bar_view->mini_views().front().get();
  EXPECT_TRUE(mini_view->desk()->windows().empty());
  CloseDeskFromMiniView(mini_view, GetEventGenerator());
  EXPECT_EQ(1, desk_4_observer.notify_counts());

  // Exiting overview mode should not cause any mini_views refreshes, since the
  // destroyed overview-specific windows do not show up in the mini_view.
  overview_controller->EndOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1, desk_4_observer.notify_counts());
  desk_4->RemoveObserver(&desk_4_observer);
}

TEST_F(DesksTest, RemoveActiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create one desk other than the default initial desk.
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Create two windows on desk_1.
  Desk* desk_1 = controller->desks()[0].get();
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // Activate desk_2 and create one more window.
  Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win2.get());
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // Enter overview mode, and remove desk_2 from its mini-view close button.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->window_list().size());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_EQ(desk_2, mini_view->desk());

  // Setup observers of both the active and inactive desks to make sure
  // refreshing the mini_view is requested only *once* for the soon-to-be active
  // desk_1, and never for the to-be-removed currently active desk_2.
  TestDeskObserver desk_1_observer;
  desk_1->AddObserver(&desk_1_observer);
  TestDeskObserver desk_2_observer;
  desk_2->AddObserver(&desk_2_observer);

  CloseDeskFromMiniView(mini_view, GetEventGenerator());

  EXPECT_EQ(1, desk_1_observer.notify_counts());
  EXPECT_EQ(0, desk_2_observer.notify_counts());

  // desk_1 will become active, and windows from desk_2 and desk_1 will merge
  // and added in the overview grid in the order of MRU.
  ASSERT_EQ(1u, controller->desks().size());
  ASSERT_EQ(1u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(3u, overview_grid->window_list().size());
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win0.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win1.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(win2.get()));

  // The MRU order is {win2, win0, win1}.
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win2.get()),
            overview_grid->window_list()[0].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win0.get()),
            overview_grid->window_list()[1].get());
  EXPECT_EQ(overview_grid->GetOverviewItemContaining(win1.get()),
            overview_grid->window_list()[2].get());

  // Make sure overview mode remains active.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Exiting overview mode should not cause any mini_views refreshes, since the
  // destroyed overview-specific windows do not show up in the mini_view.
  overview_controller->EndOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1, desk_1_observer.notify_counts());
  desk_1->RemoveObserver(&desk_1_observer);
}

TEST_F(DesksTest, ActivateActiveDeskFromOverview) {
  auto* controller = DesksController::Get();

  // Create one more desk other than the default initial desk, so the desks bar
  // shows up in overview mode.
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  // Enter overview mode, and click on `desk_1`'s mini_view, and expect that
  // overview mode exits since this is the already active desk.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  const Desk* desk_1 = controller->desks()[0].get();
  const auto* mini_view = desks_bar_view->mini_views().front().get();
  ClickOnMiniView(mini_view, GetEventGenerator());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_EQ(desk_1, controller->active_desk());
}

TEST_F(DesksTest, MinimizedWindow) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  auto* window_state = WindowState::Get(win0.get());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  // Minimized windows on the active desk show up in the MRU list...
  EXPECT_EQ(1u, Shell::Get()
                    ->mru_window_tracker()
                    ->BuildMruWindowList(kActiveDesk)
                    .size());

  ActivateDesk(desk_2);

  // ... But they don't once their desk is inactive.
  EXPECT_TRUE(Shell::Get()
                  ->mru_window_tracker()
                  ->BuildMruWindowList(kActiveDesk)
                  .empty());

  // Switching back to their desk should neither activate them nor unminimize
  // them.
  ActivateDesk(desk_1);
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_NE(win0.get(), window_util::GetActiveWindow());
}

TEST_P(DesksTest, DragWindowToDesk) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();

  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 200, 150));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());

  ui::Shadow* shadow = ::wm::ShadowController::GetShadowForWindow(win1.get());
  ASSERT_TRUE(shadow);
  ASSERT_TRUE(shadow->layer());
  EXPECT_TRUE(shadow->layer()->GetTargetVisibility());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(2u, overview_grid->size());

  // While in overview mode, the window's shadow is hidden.
  EXPECT_FALSE(shadow->layer()->GetTargetVisibility());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item = overview_session->GetOverviewItemForWindow(win1.get());
  ASSERT_TRUE(overview_item);
  const gfx::RectF target_bounds_before_drag = overview_item->target_bounds();

  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  auto* desk_1_mini_view = desks_bar_view->mini_views()[0].get();
  EXPECT_EQ(desk_1, desk_1_mini_view->desk());
  // Drag it and drop it on its same desk's mini_view. Nothing happens, it
  // should be returned back to its original target bounds.
  auto* event_generator = GetEventGenerator();
  DragItemToPoint(overview_item,
                  desk_1_mini_view->GetBoundsInScreen().CenterPoint(),
                  event_generator,
                  /*by_touch_gestures=*/GetParam());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(2u, overview_grid->size());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(win1.get()));

  // Now drag it to desk_2's mini_view. The overview grid should now have only
  // `win2`, and `win1` should move to desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  event_generator,
                  /*by_touch_gestures=*/GetParam());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->size());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), win1.get()));
  EXPECT_FALSE(overview_grid->drop_target_widget());

  // After dragging an item outside of overview to another desk, the focus
  // should not be given to another window in overview, and should remain on the
  // OverviewModeFocusedWidget.
  EXPECT_EQ(overview_session->GetOverviewFocusWindow(),
            window_util::GetActiveWindow());

  // It is possible to select the remaining window which will activate it and
  // exit overview.
  overview_item = overview_session->GetOverviewItemForWindow(win2.get());
  ASSERT_TRUE(overview_item);
  const auto window_center =
      gfx::ToFlooredPoint(overview_item->target_bounds().CenterPoint());
  if (GetParam() /* uses gestures */) {
    event_generator->GestureTapAt(window_center);
  } else {
    event_generator->MoveMouseTo(window_center);
    event_generator->ClickLeftButton();
  }
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // After the window is dropped onto another desk, its shadow should be
  // restored properly.
  EXPECT_TRUE(shadow->layer()->GetTargetVisibility());
}

TEST_P(DesksTest, DragMinimizedWindowToDesk) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Minimize the window before entering Overview Mode.
  auto* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Drag the window to desk_2's mini_view and activate desk_2. Expect that the
  // window will be in an unminimized state and all its visibility and layer
  // opacity attributes are correct.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator(),
                  /*by_touch_gestures=*/GetParam());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
  EXPECT_FALSE(overview_grid->drop_target_widget());
  DeskSwitchAnimationWaiter waiter;
  ClickOnMiniView(desk_2_mini_view, GetEventGenerator());
  waiter.Wait();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(desk_2->is_active());

  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->layer()->GetTargetVisibility());
  EXPECT_EQ(1.f, window->layer()->GetTargetOpacity());
}

TEST_P(DesksTest, DragWindowToNonMiniViewPoints) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->size());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const gfx::RectF target_bounds_before_drag = overview_item->target_bounds();

  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Drag it and drop it on the new desk button. Nothing happens, it should be
  // returned back to its original target bounds.
  DragItemToPoint(
      overview_item,
      desks_bar_view->new_desk_button()->GetBoundsInScreen().CenterPoint(),
      GetEventGenerator(),
      /*by_touch_gestures=*/GetParam());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->size());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));

  // Drag it and drop it on the bottom right corner of the display. Also,
  // nothing should happen.
  DragItemToPoint(overview_item,
                  window->GetRootWindow()->GetBoundsInScreen().bottom_right(),
                  GetEventGenerator(),
                  /*by_touch_gestures=*/GetParam());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(1u, overview_grid->size());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));
}

TEST_F(DesksTest, MruWindowTracker) {
  // Create two desks with two windows in each.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  // Build active desk's MRU window list.
  auto* mru_window_tracker = Shell::Get()->mru_window_tracker();
  auto window_list = mru_window_tracker->BuildWindowForCycleList(kActiveDesk);
  ASSERT_EQ(2u, window_list.size());
  EXPECT_EQ(win3.get(), window_list[0]);
  EXPECT_EQ(win2.get(), window_list[1]);

  // Build the global MRU window list.
  window_list = mru_window_tracker->BuildWindowForCycleList(kAllDesks);
  ASSERT_EQ(4u, window_list.size());
  EXPECT_EQ(win3.get(), window_list[0]);
  EXPECT_EQ(win2.get(), window_list[1]);
  EXPECT_EQ(win1.get(), window_list[2]);
  EXPECT_EQ(win0.get(), window_list[3]);

  // Switch back to desk_1 and test both MRU list types.
  Desk* desk_1 = controller->desks()[0].get();
  ActivateDesk(desk_1);
  window_list = mru_window_tracker->BuildWindowForCycleList(kActiveDesk);
  ASSERT_EQ(2u, window_list.size());
  EXPECT_EQ(win1.get(), window_list[0]);
  EXPECT_EQ(win0.get(), window_list[1]);
  // TODO(afakhry): Check with UX if we should favor active desk's windows in
  // the global MRU list (i.e. put them first in the list).
  window_list = mru_window_tracker->BuildWindowForCycleList(kAllDesks);
  ASSERT_EQ(4u, window_list.size());
  EXPECT_EQ(win1.get(), window_list[0]);
  EXPECT_EQ(win3.get(), window_list[1]);
  EXPECT_EQ(win2.get(), window_list[2]);
  EXPECT_EQ(win0.get(), window_list[3]);
}

TEST_F(DesksTest, NextActivatable) {
  // Create two desks with two windows in each.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // When deactivating a window, the next activatable window should be on the
  // same desk.
  wm::DeactivateWindow(win3.get());
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());
  wm::DeactivateWindow(win2.get());
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Similarly for desk_1.
  Desk* desk_1 = controller->desks()[0].get();
  ActivateDesk(desk_1);
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());
  win1.reset();
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  win0.reset();
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
}

TEST_F(DesksTest, NoMiniViewsUpdateOnOverviewEnter) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  auto* desk_1 = controller->desks()[0].get();
  auto* desk_2 = controller->desks()[1].get();

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  wm::ActivateWindow(win1.get());
  EXPECT_EQ(win1.get(), window_util::GetActiveWindow());

  TestDeskObserver desk_1_observer;
  TestDeskObserver desk_2_observer;
  desk_1->AddObserver(&desk_1_observer);
  desk_2->AddObserver(&desk_2_observer);
  EXPECT_EQ(0, desk_1_observer.notify_counts());
  EXPECT_EQ(0, desk_2_observer.notify_counts());

  // The widgets created by overview mode, whose windows are added to the active
  // desk's container, should never result in mini_views updates since they're
  // not mirrored there at all.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  EXPECT_EQ(0, desk_1_observer.notify_counts());
  EXPECT_EQ(0, desk_2_observer.notify_counts());

  desk_1->RemoveObserver(&desk_1_observer);
  desk_2->RemoveObserver(&desk_2_observer);
}

// Tests that the new desk button's state and color are as expected.
TEST_F(DesksTest, NewDeskButtonStateAndColor) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto* new_desk_button = desks_bar_view->new_desk_button();

  // Tests that with one or two desks, the new desk button has an enabled state
  // and color.
  const SkColor background_color =
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kInactiveControlBackground,
          AshColorProvider::AshColorMode::kDark);
  const SkColor disabled_background_color =
      AshColorProvider::GetDisabledColor(background_color);
  EXPECT_TRUE(new_desk_button->GetEnabled());
  EXPECT_EQ(background_color, new_desk_button->GetBackgroundColorForTesting());

  const gfx::Point button_center =
      new_desk_button->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(button_center);
  event_generator->ClickLeftButton();
  EXPECT_TRUE(new_desk_button->GetEnabled());
  EXPECT_EQ(background_color, new_desk_button->GetBackgroundColorForTesting());

  // Tests that adding desks until we reach the desks limit should change the
  // state and color of the new desk button.
  size_t prev_size = controller->desks().size();
  while (controller->CanCreateDesks()) {
    event_generator->ClickLeftButton();
    EXPECT_EQ(prev_size + 1, controller->desks().size());
    prev_size = controller->desks().size();
  }
  EXPECT_FALSE(new_desk_button->GetEnabled());
  EXPECT_EQ(disabled_background_color,
            new_desk_button->GetBackgroundColorForTesting());
}

class TabletModeDesksTest : public DesksTest {
 public:
  TabletModeDesksTest() = default;
  ~TabletModeDesksTest() override = default;

  // DesksTest:
  void SetUp() override {
    DesksTest::SetUp();

    // Enter tablet mode. Avoid TabletModeController::OnGetSwitchStates() from
    // disabling tablet mode.
    base::RunLoop().RunUntilIdle();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabletModeDesksTest);
};

TEST_F(TabletModeDesksTest, Backdrops) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_1 = controller->desks()[0].get();
  const Desk* desk_2 = controller->desks()[1].get();

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Enter tablet mode and expect that the backdrop is created only for desk_1,
  // since it's the one that has a window in it.
  auto* desk_1_backdrop_controller =
      GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
  auto* desk_2_backdrop_controller =
      GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_TRUE(desk_1_backdrop_controller->backdrop_window()->IsVisible());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());

  // Enter overview and expect that the backdrop is still present for desk_1 but
  // hidden.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window()->IsVisible());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->size());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Now drag it to desk_2's mini_view, so that it moves to desk_2. Expect that
  // desk_1's backdrop is destroyed, while created (but still hidden) for
  // desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  EXPECT_EQ(desk_2, desk_2_mini_view->desk());
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // Exit overview, and expect that desk_2's backdrop remains hidden since the
  // desk is not activated yet.
  overview_controller->EndOverview(
      OverviewSession::EnterExitOverviewType::kImmediateExit);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // Activate desk_2 and expect that its backdrop is now visible.
  ActivateDesk(desk_2);
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_TRUE(desk_2_backdrop_controller->backdrop_window()->IsVisible());

  // No backdrops after exiting tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
}

TEST_F(TabletModeDesksTest, NoDesksBarInTabletModeWithOneDesk) {
  // Initially there's only one desk.
  auto* controller = DesksController::Get();
  ASSERT_EQ(1u, controller->desks().size());

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Enter overview and expect that the DesksBar widget won't be created.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_FALSE(desks_bar_view);

  // It's possible to drag the window without any crashes.
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  DragItemToPoint(overview_item, window->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator(), /*drop=*/true);

  // Exit overview and add a new desk, then re-enter overview. Expect that now
  // the desks bar is visible.
  overview_controller->EndOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  NewDesk();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
}

TEST_F(TabletModeDesksTest, DesksCreationRemovalCycle) {
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Create and remove desks in a cycle while in overview mode. Expect as the
  // containers are reused for new desks, their backdrop state are always
  // correct, and there are no crashes as desks are removed.
  auto* desks_controller = DesksController::Get();
  for (size_t i = 0; i < 2 * desks_util::kMaxNumberOfDesks; ++i) {
    NewDesk();
    ASSERT_EQ(2u, desks_controller->desks().size());
    const Desk* desk_1 = desks_controller->desks()[0].get();
    const Desk* desk_2 = desks_controller->desks()[1].get();
    auto* desk_1_backdrop_controller =
        GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
    auto* desk_2_backdrop_controller =
        GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
    {
      SCOPED_TRACE("Check backdrops after desk creation");
      ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
      EXPECT_TRUE(desk_1_backdrop_controller->backdrop_window()->IsVisible());
      EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
    }
    // Remove the active desk, and expect that now desk_2 should have a hidden
    // backdrop, while the container of the removed desk_1 should have none.
    RemoveDesk(desk_1);
    {
      SCOPED_TRACE("Check backdrops after desk removal");
      EXPECT_TRUE(desk_2->is_active());
      EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));
      EXPECT_FALSE(desk_1_backdrop_controller->backdrop_window());
      ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
      EXPECT_TRUE(desk_2_backdrop_controller->backdrop_window()->IsVisible());
    }
  }
}

TEST_F(TabletModeDesksTest, RestoreSplitViewOnDeskSwitch) {
  // Create two desks with two snapped windows in each.
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->desks()[0].get();
  Desk* desk_2 = desks_controller->desks()[1].get();

  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win2.get(), SplitViewController::RIGHT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());

  // Desk 2 has no windows, so the SplitViewController should be tracking no
  // windows.
  ActivateDesk(desk_2);
  EXPECT_EQ(nullptr, split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());
  // However, the snapped windows on desk 1 should retain their snapped state.
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win2.get())->IsSnapped());

  // Snap two other windows in desk 2.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win4.get(), SplitViewController::RIGHT);
  EXPECT_EQ(win3.get(), split_view_controller()->left_window());
  EXPECT_EQ(win4.get(), split_view_controller()->right_window());

  // Switch back to desk 1, and expect the snapped windows are restored.
  ActivateDesk(desk_1);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());
  EXPECT_TRUE(WindowState::Get(win3.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win4.get())->IsSnapped());
}

TEST_F(TabletModeDesksTest, SnappedStateRetainedOnSwitchingDesksFromOverview) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win2.get(), SplitViewController::RIGHT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());

  // Enter overview and switch to desk_2 using its mini_view. Overview should
  // end, but TabletModeWindowManager should not maximize the snapped windows
  // and they should retain their snapped state.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  auto* desks_bar_view = overview_grid->desks_bar_view();
  auto* mini_view = desks_bar_view->mini_views()[1].get();
  Desk* desk_2 = desks_controller->desks()[1].get();
  EXPECT_EQ(desk_2, mini_view->desk());
  {
    DeskSwitchAnimationWaiter waiter;
    ClickOnMiniView(mini_view, GetEventGenerator());
    waiter.Wait();
  }
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win2.get())->IsSnapped());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Snap two other windows in desk_2, and switch back to desk_1 from overview.
  // The snapped state should be retained for windows in both source and
  // destination desks.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win4.get(), SplitViewController::RIGHT);
  EXPECT_EQ(win3.get(), split_view_controller()->left_window());
  EXPECT_EQ(win4.get(), split_view_controller()->right_window());
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  desks_bar_view = overview_grid->desks_bar_view();
  mini_view = desks_bar_view->mini_views()[0].get();
  Desk* desk_1 = desks_controller->desks()[0].get();
  EXPECT_EQ(desk_1, mini_view->desk());
  {
    DeskSwitchAnimationWaiter waiter;
    ClickOnMiniView(mini_view, GetEventGenerator());
    waiter.Wait();
  }
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win2.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win3.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(win4.get())->IsSnapped());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_F(
    TabletModeDesksTest,
    SnappedStateRetainedOnSwitchingDesksWithOverviewFullOfUnsnappableWindows) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          Shell::GetPrimaryRootWindow());
  const gfx::Size big(work_area.width() * 2 / 3, work_area.height() * 2 / 3);
  const gfx::Size small(250, 100);
  std::unique_ptr<aura::Window> win1 = CreateTestWindow(gfx::Rect(small));
  aura::test::TestWindowDelegate win2_delegate;
  win2_delegate.set_minimum_size(big);
  std::unique_ptr<aura::Window> win2(CreateTestWindowInShellWithDelegate(
      &win2_delegate, /*id=*/-1, gfx::Rect(big)));
  aura::test::TestWindowDelegate win3_delegate;
  win3_delegate.set_minimum_size(big);
  std::unique_ptr<aura::Window> win3(CreateTestWindowInShellWithDelegate(
      &win3_delegate, /*id=*/-1, gfx::Rect(big)));
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->StartOverview());
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_FALSE(CanSnapInSplitview(win2.get()));
  EXPECT_FALSE(CanSnapInSplitview(win3.get()));

  // Switch to |desk_2| using its |mini_view|. Split view and overview should
  // end, but |win1| should retain its snapped state.
  ASSERT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  auto* desks_bar_view = overview_grid->desks_bar_view();
  auto* mini_view = desks_bar_view->mini_views()[1].get();
  Desk* desk_2 = desks_controller->desks()[1].get();
  EXPECT_EQ(desk_2, mini_view->desk());
  {
    DeskSwitchAnimationWaiter waiter;
    ClickOnMiniView(mini_view, GetEventGenerator());
    waiter.Wait();
  }
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Switch back to |desk_1| and verify that split view is arranged as before.
  EXPECT_TRUE(overview_controller->StartOverview());
  ASSERT_TRUE(overview_controller->InOverviewSession());
  overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  desks_bar_view = overview_grid->desks_bar_view();
  mini_view = desks_bar_view->mini_views()[0].get();
  Desk* desk_1 = desks_controller->desks()[0].get();
  EXPECT_EQ(desk_1, mini_view->desk());
  {
    DeskSwitchAnimationWaiter waiter;
    ClickOnMiniView(mini_view, GetEventGenerator());
    waiter.Wait();
  }
  EXPECT_TRUE(WindowState::Get(win1.get())->IsSnapped());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

TEST_F(TabletModeDesksTest, OverviewStateOnSwitchToDeskWithSplitView) {
  // Setup two desks, one (desk_1) with two snapped windows, and the other
  // (desk_2) with only one snapped window.
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->desks()[0].get();
  Desk* desk_2 = desks_controller->desks()[1].get();
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win2.get(), SplitViewController::RIGHT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());
  auto* overview_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_2);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SplitViewController::LEFT);
  EXPECT_EQ(win3.get(), split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());

  // Switching to the desk that has only one snapped window to be restored in
  // SplitView should enter overview mode, whereas switching to one that has two
  // snapped windows should exit overview.
  ActivateDesk(desk_1);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_2);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ActivateDesk(desk_1);
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_F(TabletModeDesksTest, RemovingDesksWithSplitView) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_2 = desks_controller->desks()[1].get();
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());
  ActivateDesk(desk_2);
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win2.get(), SplitViewController::RIGHT);
  EXPECT_EQ(nullptr, split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());

  // Removing desk_2 will cause both snapped windows to merge in SplitView.
  RemoveDesk(desk_2);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(win2.get(), split_view_controller()->right_window());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
}

TEST_F(TabletModeDesksTest, RemoveDeskWithMaximizedWindowAndMergeWithSnapped) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_2 = desks_controller->desks()[1].get();
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());
  ActivateDesk(desk_2);
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  EXPECT_EQ(nullptr, split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());
  EXPECT_TRUE(WindowState::Get(win2.get())->IsMaximized());

  // Removing desk_2 will cause us to enter overview mode without any crashes.
  // SplitView will remain left snapped.
  RemoveDesk(desk_2);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(win1.get(), split_view_controller()->left_window());
  EXPECT_EQ(nullptr, split_view_controller()->right_window());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
}

TEST_F(TabletModeDesksTest, BackdropsStacking) {
  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->desks()[0].get();
  Desk* desk_2 = desks_controller->desks()[1].get();

  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win2.get(), SplitViewController::RIGHT);
  auto* desk_1_backdrop_controller =
      GetDeskBackdropController(desk_1, Shell::GetPrimaryRootWindow());
  auto* desk_2_backdrop_controller =
      GetDeskBackdropController(desk_2, Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());

  // The backdrop window should be stacked below both snapped windows.
  auto* desk_1_backdrop = desk_1_backdrop_controller->backdrop_window();
  EXPECT_TRUE(IsStackedBelow(desk_1_backdrop, win1.get()));
  EXPECT_TRUE(IsStackedBelow(desk_1_backdrop, win2.get()));

  // Switching to another desk doesn't change the backdrop state of the inactive
  // desk.
  ActivateDesk(desk_2);
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  EXPECT_FALSE(desk_2_backdrop_controller->backdrop_window());
  EXPECT_TRUE(IsStackedBelow(desk_1_backdrop, win1.get()));
  EXPECT_TRUE(IsStackedBelow(desk_1_backdrop, win2.get()));

  // Snapping new windows in desk_2 should update the backdrop state of desk_2,
  // but should not affect desk_1.
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  split_view_controller()->SnapWindow(win3.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(win4.get(), SplitViewController::RIGHT);
  ASSERT_TRUE(desk_1_backdrop_controller->backdrop_window());
  ASSERT_TRUE(desk_2_backdrop_controller->backdrop_window());
  auto* desk_2_backdrop = desk_2_backdrop_controller->backdrop_window();
  EXPECT_TRUE(IsStackedBelow(desk_2_backdrop, win3.get()));
  EXPECT_TRUE(IsStackedBelow(desk_2_backdrop, win4.get()));
}

namespace {

// A client view that returns a given minimum size to be used on the widget's
// native window.
class TestClientView : public views::ClientView {
 public:
  TestClientView(views::Widget* widget, const gfx::Size& minimum_size)
      : views::ClientView(widget, widget->widget_delegate()->GetContentsView()),
        minimum_size_(minimum_size) {}
  ~TestClientView() override = default;

  // views::ClientView:
  gfx::Size GetMinimumSize() const override { return minimum_size_; }

 private:
  const gfx::Size minimum_size_;

  DISALLOW_COPY_AND_ASSIGN(TestClientView);
};

}  // namespace

TEST_F(TabletModeDesksTest, RestoringUnsnappableWindowsInSplitView) {
  UpdateDisplay("600x400");
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  // Setup an app window that cannot be snapped in landscape orientation, but
  // can be snapped in portrait orientation.
  auto window = CreateAppWindow(gfx::Rect(350, 350));
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  widget->non_client_view()->set_client_view(
      new TestClientView(widget, gfx::Size(350, 100)));
  EXPECT_FALSE(CanSnapInSplitview(window.get()));

  // Change to a portrait orientation and expect it's possible to snap the
  // window.
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);
  EXPECT_TRUE(CanSnapInSplitview(window.get()));

  // Snap the window in this orientation.
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_EQ(window.get(), split_view_controller()->left_window());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Create a second desk, switch to it, and change back the orientation to
  // landscape, in which the window is not snappable. The window still exists on
  // the first desk, so nothing should change.
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  const Desk* desk_2 = controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, controller->active_desk());
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  // Switch back to the first desk, and expect that SplitView is not restored,
  // since the only available window on that desk is not snappable.
  const Desk* desk_1 = controller->desks()[0].get();
  ActivateDesk(desk_1);
  EXPECT_EQ(desk_1, controller->active_desk());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
}

TEST_F(DesksTest, MiniViewsTouchGestures) {
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(3u, desks_bar_view->mini_views().size());
  auto* desk_1_mini_view = desks_bar_view->mini_views()[0].get();
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  auto* desk_3_mini_view = desks_bar_view->mini_views()[2].get();

  // Override the long-tap delays.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_long_press_time_in_ms(50);
  gesture_config->set_show_press_delay_in_ms(50);

  // Long gesture tapping on one mini_view shows its close button, and hides
  // those of other mini_views.
  auto* event_generator = GetEventGenerator();
  LongGestureTap(desk_1_mini_view->GetBoundsInScreen().CenterPoint(),
                 event_generator);
  EXPECT_TRUE(desk_1_mini_view->close_desk_button()->GetVisible());
  EXPECT_FALSE(desk_2_mini_view->close_desk_button()->GetVisible());
  EXPECT_FALSE(desk_3_mini_view->close_desk_button()->GetVisible());
  LongGestureTap(desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                 event_generator);
  EXPECT_FALSE(desk_1_mini_view->close_desk_button()->GetVisible());
  EXPECT_TRUE(desk_2_mini_view->close_desk_button()->GetVisible());
  EXPECT_FALSE(desk_3_mini_view->close_desk_button()->GetVisible());

  // Tapping on the visible close button, closes the desk rather than switches
  // to that desk.
  GestureTapOnView(desk_2_mini_view->close_desk_button(), event_generator);
  ASSERT_EQ(2u, controller->desks().size());
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Tapping on the invisible close button should not result in closing that
  // desk; rather activating that desk.
  EXPECT_FALSE(desk_1_mini_view->close_desk_button()->GetVisible());
  GestureTapOnView(desk_1_mini_view->close_desk_button(), event_generator);
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(controller->desks()[0]->is_active());
}

TEST_F(DesksTest, AutohiddenShelfAnimatesAfterDeskSwitch) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  const gfx::Rect shown_shelf_bounds = shelf_widget->GetWindowBoundsInScreen();

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Enable animations so that we can make sure that they occur.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  NewDesk();

  // Create a window on the first desk so that the shelf will auto-hide there.
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->Maximize();
  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  shelf->shelf_layout_manager()->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  const gfx::Rect hidden_shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_NE(shown_shelf_bounds, hidden_shelf_bounds);

  // Go to the second desk.
  ActivateDesk(DesksController::Get()->desks()[1].get());
  // The shelf should now want to show itself, but as the shelf animation is
  // just starting, it should still be hidden. If this fails, the change was
  // not animated.
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(shelf_widget->GetWindowBoundsInScreen(), hidden_shelf_bounds);
  // Let's wait until the shelf animates to a fully shown state.
  while (shelf_widget->GetWindowBoundsInScreen() != shown_shelf_bounds) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(200));
    run_loop.Run();
  }
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

class DesksWithSplitViewTest : public AshTestBase {
 public:
  DesksWithSplitViewTest() = default;
  ~DesksWithSplitViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kVirtualDesks,
                              features::kDragToSnapInClamshellMode},
        /*disabled_features=*/{});

    AshTestBase::SetUp();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DesksWithSplitViewTest);
};

TEST_F(DesksWithSplitViewTest, SwitchToDeskWithSnappedActiveWindow) {
  auto* desks_controller = DesksController::Get();
  auto* overview_controller = Shell::Get()->overview_controller();

  // Two virtual desks: |desk_1| (active) and |desk_2|.
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk_1 = desks_controller->desks()[0].get();
  Desk* desk_2 = desks_controller->desks()[1].get();

  // Two windows on |desk_1|: |win0| (snapped) and |win1|.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  WindowState* win0_state = WindowState::Get(win0.get());
  WMEvent snap_to_left(WM_EVENT_CYCLE_SNAP_LEFT);
  win0_state->OnWMEvent(&snap_to_left);
  EXPECT_EQ(WindowStateType::kLeftSnapped, win0_state->GetStateType());

  // Switch to |desk_2| and then back to |desk_1|. Verify that neither split
  // view nor overview arises.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_2);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  ActivateDesk(desk_1);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_F(DesksWithSplitViewTest, SuccessfulDragToDeskRemovesSplitViewIndicators) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());

  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());

  // Drag it to desk_2's mini_view. The overview grid should now show the
  // "no-windows" widget, and the window should move to desk_2.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  DragItemToPoint(overview_item,
                  desk_2_mini_view->GetBoundsInScreen().CenterPoint(),
                  GetEventGenerator(),
                  /*by_touch_gestures=*/false,
                  /*drop=*/false);
  // Validate that before dropping, the SplitView indicators and the drop target
  // widget are created.
  EXPECT_TRUE(overview_grid->drop_target_widget());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            overview_session->grid_list()[0]
                ->split_view_drag_indicators()
                ->current_window_dragging_state());
  // Now drop the window, and validate the indicators and the drop target were
  // removed.
  GetEventGenerator()->ReleaseLeftButton();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  EXPECT_FALSE(DoesActiveDeskContainWindow(window.get()));
  EXPECT_TRUE(overview_session->no_windows_widget_for_testing());
  EXPECT_FALSE(overview_grid->drop_target_widget());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            overview_session->grid_list()[0]
                ->split_view_drag_indicators()
                ->current_window_dragging_state());
}

namespace {

constexpr char kUser1Email[] = "user1@desks";
constexpr char kUser2Email[] = "user2@desks";

}  // namespace

class DesksMultiUserTest : public NoSessionAshTestBase,
                           public MultiUserWindowManagerDelegate {
 public:
  DesksMultiUserTest() = default;
  ~DesksMultiUserTest() override = default;

  MultiUserWindowManager* multi_user_window_manager() {
    return multi_user_window_manager_.get();
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVirtualDesks);

    NoSessionAshTestBase::SetUp();
    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();
    session_controller->AddUserSession(kUser1Email);
    session_controller->AddUserSession(kUser2Email);

    // Simulate user 1 login.
    SwitchActiveUser(GetUser1AccountId());
    multi_user_window_manager_ =
        MultiUserWindowManager::Create(this, GetUser1AccountId());
    MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
        MultiUserWindowManagerImpl::ANIMATION_SPEED_DISABLED);
    session_controller->SetSessionState(session_manager::SessionState::ACTIVE);
  }

  void TearDown() override {
    multi_user_window_manager_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // MultiUserWindowManagerDelegate:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override {}
  void OnTransitionUserShelfToNewAccount() override {}

  AccountId GetUser1AccountId() const {
    return AccountId::FromUserEmail(kUser1Email);
  }

  AccountId GetUser2AccountId() const {
    return AccountId::FromUserEmail(kUser2Email);
  }

  void SwitchActiveUser(const AccountId& account_id) {
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MultiUserWindowManager> multi_user_window_manager_;

  DISALLOW_COPY_AND_ASSIGN(DesksMultiUserTest);
};

TEST_F(DesksMultiUserTest, SwitchUsersBackAndForth) {
  auto* controller = DesksController::Get();
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  multi_user_window_manager()->SetWindowOwner(win0.get(), GetUser1AccountId());
  EXPECT_TRUE(win0->IsVisible());
  ActivateDesk(desk_2);
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  multi_user_window_manager()->SetWindowOwner(win1.get(), GetUser1AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());

  // Switch to user_2 and expect no windows from user_1 is visible regardless of
  // the desk.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());

  // Since this is the first time this user logs in, desk_1 will be activated
  // for this user.
  EXPECT_TRUE(desk_1->is_active());

  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win2.get(), GetUser2AccountId());
  EXPECT_TRUE(win2->IsVisible());
  ActivateDesk(desk_3);
  auto win3 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win3.get(), GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_TRUE(win3->IsVisible());

  // When switching back to user_1, the active desk should be restored to
  // desk_2.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());

  // When switching to user_2, the active desk should be restored to desk_3.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_TRUE(win3->IsVisible());
}

TEST_F(DesksMultiUserTest, RemoveDesks) {
  // Create two desks with several windows with different app types that
  // belong to different users.
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  multi_user_window_manager()->SetWindowOwner(win0.get(), GetUser1AccountId());
  EXPECT_TRUE(win0->IsVisible());
  ActivateDesk(desk_2);
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200), AppType::ARC_APP);
  // Non-app window.
  auto win3 = CreateAppWindow(gfx::Rect(50, 50, 200, 200), AppType::NON_APP);
  multi_user_window_manager()->SetWindowOwner(win2.get(), GetUser1AccountId());
  multi_user_window_manager()->SetWindowOwner(win1.get(), GetUser1AccountId());
  multi_user_window_manager()->SetWindowOwner(win3.get(), GetUser1AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());
  EXPECT_TRUE(win2->IsVisible());
  EXPECT_TRUE(win3->IsVisible());

  // Switch to user_2 and expect no windows from user_1 is visible regardless of
  // the desk.
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());

  auto win4 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win4.get(), GetUser2AccountId());
  EXPECT_TRUE(win4->IsVisible());
  ActivateDesk(desk_2);
  auto win5 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win5.get(), GetUser2AccountId());
  EXPECT_FALSE(win0->IsVisible());
  EXPECT_FALSE(win1->IsVisible());
  EXPECT_FALSE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());
  EXPECT_FALSE(win4->IsVisible());
  EXPECT_TRUE(win5->IsVisible());

  // Delete desk_2, and expect all app windows move to desk_1.
  RemoveDesk(desk_2);
  EXPECT_TRUE(desk_1->is_active());
  auto* desk_1_container =
      desk_1->GetDeskContainerForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(desk_1_container, win0->parent());
  EXPECT_EQ(desk_1_container, win1->parent());
  EXPECT_EQ(desk_1_container, win2->parent());
  // The non-app window didn't move.
  EXPECT_NE(desk_1_container, win3->parent());
  EXPECT_EQ(desk_1_container, win4->parent());
  EXPECT_EQ(desk_1_container, win5->parent());

  // Only user_2's window are visible.
  EXPECT_TRUE(win4->IsVisible());
  EXPECT_TRUE(win5->IsVisible());

  // The non-app window will always be hidden.
  EXPECT_FALSE(win3->IsVisible());

  // Switch to user_1 and expect the correct windows' visibility.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(win0->IsVisible());
  EXPECT_TRUE(win1->IsVisible());
  EXPECT_TRUE(win2->IsVisible());
  EXPECT_FALSE(win3->IsVisible());

  // Create two more desks, switch to user_2, and activate the third desk.
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  SwitchActiveUser(GetUser2AccountId());
  ActivateDesk(desk_3);
  auto win6 = CreateAppWindow(gfx::Rect(0, 0, 250, 200));
  multi_user_window_manager()->SetWindowOwner(win5.get(), GetUser2AccountId());

  // Switch back to user_1, and remove the first desk. When switching back to
  // user_2 after that, we should see that what used to be the third desk is now
  // active.
  SwitchActiveUser(GetUser1AccountId());
  EXPECT_TRUE(desk_1->is_active());
  RemoveDesk(desk_1);
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_TRUE(win6->IsVisible());
}

TEST_F(DesksMultiUserTest, SwitchingUsersEndsOverview) {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->StartOverview());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  SwitchActiveUser(GetUser2AccountId());
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

}  // namespace

// Simulates the same behavior of event rewriting that key presses go through.
class DesksAcceleratorsTest : public DesksTest,
                              public ui::EventRewriterChromeOS::Delegate {
 public:
  DesksAcceleratorsTest() = default;
  ~DesksAcceleratorsTest() override = default;

  // DesksTest:
  void SetUp() override {
    DesksTest::SetUp();

    auto* event_rewriter_controller = EventRewriterController::Get();
    event_rewriter_controller->AddEventRewriter(
        std::make_unique<ui::EventRewriterChromeOS>(
            this, Shell::Get()->sticky_keys_controller()));
  }

  // ui::EventRewriterChromeOS::Delegate:
  bool RewriteModifierKeys() override { return true; }
  bool GetKeyboardRemappedPrefValue(const std::string& pref_name,
                                    int* result) const override {
    return false;
  }
  bool TopRowKeysAreFunctionKeys() const override { return false; }
  bool IsExtensionCommandRegistered(ui::KeyboardCode key_code,
                                    int flags) const override {
    return false;
  }
  bool IsSearchKeyAcceleratorReserved() const override { return true; }

  void SendAccelerator(ui::KeyboardCode key_code, int flags) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->PressKey(key_code, flags);
    generator->ReleaseKey(key_code, flags);
  }

  // Moves the overview highlight to the next item.
  void MoveOverviewHighlighter(OverviewSession* session) {
    session->Move(/*reverse=*/false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesksAcceleratorsTest);
};

namespace {

TEST_F(DesksAcceleratorsTest, NewDesk) {
  auto* controller = DesksController::Get();
  // It's possible to add up to `kMaxNumberOfDesks` desks using the shortcut.
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  for (size_t num_desks = 1; num_desks < desks_util::kMaxNumberOfDesks;
       ++num_desks) {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_PLUS, flags);
    waiter.Wait();
    // The newly created desk should be activated.
    ASSERT_EQ(num_desks + 1, controller->desks().size());
    EXPECT_TRUE(controller->desks().back()->is_active());
  }

  // When we reach the limit, the shortcut does nothing.
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
  SendAccelerator(ui::VKEY_OEM_PLUS, flags);
  EXPECT_EQ(desks_util::kMaxNumberOfDesks, controller->desks().size());
}

TEST_F(DesksAcceleratorsTest, CannotRemoveLastDesk) {
  auto* controller = DesksController::Get();
  // Removing the last desk is not possible.
  ASSERT_EQ(1u, controller->desks().size());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  SendAccelerator(ui::VKEY_OEM_MINUS, flags);
  ASSERT_EQ(1u, controller->desks().size());
}

TEST_F(DesksAcceleratorsTest, RemoveDesk) {
  auto* controller = DesksController::Get();
  // Create a few desks and remove them outside and inside overview using the
  // shortcut.
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  EXPECT_TRUE(desk_1->is_active());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  DeskSwitchAnimationWaiter waiter;
  SendAccelerator(ui::VKEY_OEM_MINUS, flags);
  waiter.Wait();
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_TRUE(desk_2->is_active());

  // Using the accelerator doesn't result in exiting overview.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  SendAccelerator(ui::VKEY_OEM_MINUS, flags);
  ASSERT_EQ(1u, controller->desks().size());
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

TEST_F(DesksAcceleratorsTest, RemoveRightmostDesk) {
  auto* controller = DesksController::Get();
  // Create a few desks and remove them outside and inside overview using the
  // shortcut.
  NewDesk();
  NewDesk();
  ASSERT_EQ(3u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  Desk* desk_3 = controller->desks()[2].get();
  ActivateDesk(desk_3);
  EXPECT_TRUE(desk_3->is_active());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_MINUS, flags);
    waiter.Wait();
  }
  ASSERT_EQ(2u, controller->desks().size());
  EXPECT_TRUE(desk_2->is_active());
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_MINUS, flags);
    waiter.Wait();
  }
  ASSERT_EQ(1u, controller->desks().size());
  EXPECT_TRUE(desk_1->is_active());
}

TEST_F(DesksAcceleratorsTest, LeftRightDeskActivation) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_1->is_active());
  // No desk on left, nothing should happen.
  const int flags = ui::EF_COMMAND_DOWN;
  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_TRUE(desk_1->is_active());

  // Go right until there're no more desks on the right.
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_6, flags);
    waiter.Wait();
    EXPECT_TRUE(desk_2->is_active());
  }

  // Nothing happens.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_TRUE(desk_2->is_active());

  // Go back left.
  {
    DeskSwitchAnimationWaiter waiter;
    SendAccelerator(ui::VKEY_OEM_4, flags);
    waiter.Wait();
    EXPECT_TRUE(desk_1->is_active());
  }
}

TEST_F(DesksAcceleratorsTest, MoveWindowLeftRightDesk) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_1->is_active());

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Moving window left when this is the left-most desk. Nothing happens.
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
  EXPECT_TRUE(DoesActiveDeskContainWindow(window.get()));

  // Move window right, it should be deactivated.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
  EXPECT_TRUE(desk_1->windows().empty());
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));

  // No more active windows on this desk, nothing happens.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_TRUE(desk_1->windows().empty());
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Activate desk 2, and do the same set of tests.
  ActivateDesk(desk_2);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Move right does nothing.
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_TRUE(desk_2->windows().empty());
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());
  EXPECT_TRUE(base::Contains(desk_1->windows(), window.get()));
}

TEST_F(DesksAcceleratorsTest, MoveWindowLeftRightDeskOverview) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_1->is_active());

  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  // In overview, while no window is highlighted, nothing should happen.
  const size_t num_windows_before = desk_1->windows().size();
  EXPECT_TRUE(desk_2->windows().empty());
  SendAccelerator(ui::VKEY_OEM_6, flags);
  ASSERT_EQ(num_windows_before, desk_1->windows().size());
  EXPECT_TRUE(desk_2->windows().empty());

  auto* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(overview_session);
  // It's possible to move the highlighted window. |Move()| will cycle through
  // the desk items first, so call it until we are highlighting an OverviewItem.
  while (!overview_session->GetHighlightedWindow())
    MoveOverviewHighlighter(overview_session);
  EXPECT_EQ(win0.get(), overview_session->GetHighlightedWindow());
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), win0.get()));
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // The highlight widget should move to the next window if we call
  // |MoveOverviewHighlighter()| again.
  MoveOverviewHighlighter(overview_session);
  EXPECT_EQ(win1.get(), overview_session->GetHighlightedWindow());
  SendAccelerator(ui::VKEY_OEM_6, flags);
  EXPECT_FALSE(DoesActiveDeskContainWindow(win1.get()));
  EXPECT_TRUE(base::Contains(desk_2->windows(), win1.get()));
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // No more highlighted windows.
  EXPECT_FALSE(overview_session->GetHighlightedWindow());
}

TEST_F(DesksAcceleratorsTest, CannotMoveAlwaysOnTopWindows) {
  auto* controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, controller->desks().size());
  Desk* desk_1 = controller->desks()[0].get();
  Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(desk_1->is_active());

  // An always-on-top window does not belong to any desk and hence cannot be
  // removed.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  win0->SetProperty(aura::client::kZOrderingKey,
                    ui::ZOrderLevel::kFloatingWindow);
  wm::ActivateWindow(win0.get());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  EXPECT_FALSE(DoesActiveDeskContainWindow(win0.get()));
  EXPECT_FALSE(controller->MoveWindowFromActiveDeskTo(
      win0.get(), desk_2, DesksMoveWindowFromActiveDeskSource::kDragAndDrop));
  const int flags = ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN;
  SendAccelerator(ui::VKEY_OEM_4, flags);
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
  EXPECT_TRUE(win0->IsVisible());

  // It remains visible even after switching desks.
  ActivateDesk(desk_2);
  EXPECT_TRUE(win0->IsVisible());
}

// TODO(afakhry): Add more tests:
// - Always on top windows are not tracked by any desk.
// - Reusing containers when desks are removed and created.

// Instantiate the parametrized tests.
INSTANTIATE_TEST_SUITE_P(, DesksTest, ::testing::Bool());

}  // namespace

}  // namespace ash
