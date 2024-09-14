// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wm/workspace_controller.h"

#include <map>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

using aura::Window;

namespace ash {

// Returns a string containing the names of all the children of |window| (in
// order). Each entry is separated by a space.
std::string GetWindowNames(const aura::Window* window) {
  std::string result;
  for (size_t i = 0; i < window->children().size(); ++i) {
    if (i != 0)
      result += " ";
    result += window->children()[i]->GetName();
  }
  return result;
}

// Returns a string containing the names of windows corresponding to each of the
// child layers of |window|'s layer. Any layers that don't correspond to a child
// Window of |window| are ignored. The result is ordered based on the layer
// ordering.
std::string GetLayerNames(const aura::Window* window) {
  typedef std::map<const ui::Layer*, std::string> LayerToWindowNameMap;
  LayerToWindowNameMap window_names;
  for (size_t i = 0; i < window->children().size(); ++i) {
    window_names[window->children()[i]->layer()] =
        window->children()[i]->GetName();
  }

  std::string result;
  const std::vector<raw_ptr<ui::Layer, VectorExperimental>>& layers(
      window->layer()->children());
  for (size_t i = 0; i < layers.size(); ++i) {
    LayerToWindowNameMap::iterator layer_i = window_names.find(layers[i]);
    if (layer_i != window_names.end()) {
      if (!result.empty())
        result += " ";
      result += layer_i->second;
    }
  }
  return result;
}

class WorkspaceControllerTest : public AshTestBase {
 public:
  WorkspaceControllerTest() = default;

  WorkspaceControllerTest(const WorkspaceControllerTest&) = delete;
  WorkspaceControllerTest& operator=(const WorkspaceControllerTest&) = delete;

  ~WorkspaceControllerTest() override = default;

  aura::Window* CreateTestWindowUnparented() {
    aura::Window* window = new aura::Window(nullptr);
    window->SetProperty(aura::client::kShowStateKey,
                        ui::mojom::WindowShowState::kNormal);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    return window;
  }

  aura::Window* CreateTestWindow() {
    aura::Window* window = new aura::Window(nullptr);
    window->SetProperty(aura::client::kShowStateKey,
                        ui::mojom::WindowShowState::kNormal);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    ParentWindowInPrimaryRootWindow(window);
    return window;
  }

  aura::Window* CreateBrowserLikeWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindow();
    window->SetBounds(bounds);
    WindowState* window_state = WindowState::Get(window);
    window_state->SetWindowPositionManaged(true);
    window->Show();
    return window;
  }

  aura::Window* CreatePopupLikeWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithBounds(bounds);
    window->Show();
    return window;
  }

  aura::Window* GetDesktop() {
    return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                               desks_util::GetActiveDeskContainerId());
  }

  gfx::Rect GetFullscreenBounds(aura::Window* window) {
    return display::Screen::GetScreen()
        ->GetDisplayNearestWindow(window)
        .bounds();
  }

  ShelfWidget* shelf_widget() { return GetPrimaryShelf()->shelf_widget(); }

  ShelfLayoutManager* shelf_layout_manager() {
    return GetPrimaryShelf()->shelf_layout_manager();
  }
};

// Assertions around adding a normal window.
TEST_F(WorkspaceControllerTest, AddNormalWindowWhenEmpty) {
  std::unique_ptr<Window> w1(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));

  WindowState* window_state = WindowState::Get(w1.get());

  EXPECT_FALSE(window_state->HasRestoreBounds());

  w1->Show();

  EXPECT_FALSE(window_state->HasRestoreBounds());

  ASSERT_TRUE(w1->layer() != nullptr);
  EXPECT_TRUE(w1->layer()->visible());

  EXPECT_EQ("0,0 250x251", w1->bounds().ToString());

  EXPECT_EQ(w1.get(), GetDesktop()->children()[0]);
}

// Assertions around maximizing/unmaximizing.
TEST_F(WorkspaceControllerTest, SingleMaximizeWindow) {
  std::unique_ptr<Window> w1(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));

  w1->Show();
  wm::ActivateWindow(w1.get());

  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  ASSERT_TRUE(w1->layer() != nullptr);
  EXPECT_TRUE(w1->layer()->visible());

  EXPECT_EQ("0,0 250x251", w1->bounds().ToString());

  // Maximize the window.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);

  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  EXPECT_EQ(w1.get(), GetDesktop()->children()[0]);
  EXPECT_EQ(screen_util::GetMaximizedWindowBoundsInParent(w1.get()).width(),
            w1->bounds().width());
  EXPECT_EQ(screen_util::GetMaximizedWindowBoundsInParent(w1.get()).height(),
            w1->bounds().height());

  // Restore the window.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);

  EXPECT_EQ(w1.get(), GetDesktop()->children()[0]);
  EXPECT_EQ("0,0 250x251", w1->bounds().ToString());
}

// Assertions around two windows and toggling one to be fullscreen.
TEST_F(WorkspaceControllerTest, FullscreenWithNormalWindow) {
  std::unique_ptr<Window> w1(CreateTestWindow());
  std::unique_ptr<Window> w2(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));
  w1->Show();

  ASSERT_TRUE(w1->layer() != nullptr);
  EXPECT_TRUE(w1->layer()->visible());

  w2->SetBounds(gfx::Rect(0, 0, 50, 51));
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kFullscreen);
  w2->Show();
  wm::ActivateWindow(w2.get());

  // Both windows should be in the same workspace.
  EXPECT_EQ(w1.get(), GetDesktop()->children()[0]);
  EXPECT_EQ(w2.get(), GetDesktop()->children()[1]);

  gfx::Rect work_area(screen_util::GetMaximizedWindowBoundsInParent(w1.get()));
  EXPECT_EQ(work_area.width(), w2->bounds().width());
  EXPECT_EQ(work_area.height(), w2->bounds().height());

  // Restore w2, which should then go back to one workspace.
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ(50, w2->bounds().width());
  EXPECT_EQ(51, w2->bounds().height());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
}

// Makes sure requests to change the bounds of a normal window go through.
TEST_F(WorkspaceControllerTest, ChangeBoundsOfNormalWindow) {
  std::unique_ptr<Window> w1(CreateTestWindow());
  w1->Show();

  // Setting the bounds should go through since the window is in the normal
  // workspace.
  w1->SetBounds(gfx::Rect(0, 0, 200, 500));
  EXPECT_EQ(200, w1->bounds().width());
  EXPECT_EQ(500, w1->bounds().height());
}

// Verifies the bounds is not altered when showing and grid is enabled.
TEST_F(WorkspaceControllerTest, SnapToGrid) {
  std::unique_ptr<Window> w1(CreateTestWindowUnparented());
  w1->SetBounds(gfx::Rect(1, 6, 25, 30));
  ParentWindowInPrimaryRootWindow(w1.get());
  // We are not aligning this anymore this way. When the window gets shown
  // the window is expected to be handled differently, but this cannot be
  // tested with this test. So the result of this test should be that the
  // bounds are exactly as passed in.
  EXPECT_EQ("1,6 25x30", w1->bounds().ToString());
}

// Assertions around a fullscreen window.
TEST_F(WorkspaceControllerTest, SingleFullscreenWindow) {
  std::unique_ptr<Window> w1(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));
  // Make the window fullscreen.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kFullscreen);
  w1->Show();
  wm::ActivateWindow(w1.get());

  EXPECT_EQ(w1.get(), GetDesktop()->children()[0]);
  EXPECT_EQ(GetFullscreenBounds(w1.get()).width(), w1->bounds().width());
  EXPECT_EQ(GetFullscreenBounds(w1.get()).height(), w1->bounds().height());

  // Restore the window. Use SHOW_STATE_DEFAULT as that is what we'll end up
  // with when using views::Widget.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kDefault);
  EXPECT_EQ("0,0 250x251", w1->bounds().ToString());

  EXPECT_EQ(w1.get(), GetDesktop()->children()[0]);
  EXPECT_EQ(250, w1->bounds().width());
  EXPECT_EQ(251, w1->bounds().height());

  // Back to fullscreen.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(w1.get(), GetDesktop()->children()[0]);
  EXPECT_EQ(GetFullscreenBounds(w1.get()).width(), w1->bounds().width());
  EXPECT_EQ(GetFullscreenBounds(w1.get()).height(), w1->bounds().height());
  WindowState* window_state = WindowState::Get(w1.get());

  ASSERT_TRUE(window_state->HasRestoreBounds());
  EXPECT_EQ("0,0 250x251", window_state->GetRestoreBoundsInScreen().ToString());
}

// Assertions around minimizing a single window.
TEST_F(WorkspaceControllerTest, MinimizeSingleWindow) {
  std::unique_ptr<Window> w1(CreateTestWindow());

  w1->Show();

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMinimized);
  EXPECT_FALSE(w1->layer()->IsVisible());
  EXPECT_TRUE(w1->layer()->GetTargetTransform().IsIdentity());

  // Show the window.
  w1->Show();
  EXPECT_TRUE(WindowState::Get(w1.get())->IsNormalStateType());
  EXPECT_TRUE(w1->layer()->IsVisible());
}

// Assertions around minimizing a fullscreen window.
TEST_F(WorkspaceControllerTest, MinimizeFullscreenWindow) {
  // Two windows, w1 normal, w2 fullscreen.
  std::unique_ptr<Window> w1(CreateTestWindow());
  std::unique_ptr<Window> w2(CreateTestWindow());
  w1->Show();
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kFullscreen);
  w2->Show();

  WindowState* w1_state = WindowState::Get(w1.get());
  WindowState* w2_state = WindowState::Get(w2.get());

  w2_state->Activate();

  // Minimize w2.
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMinimized);
  EXPECT_TRUE(w1->layer()->IsVisible());
  EXPECT_FALSE(w2->layer()->IsVisible());

  // Show the window, which should trigger unminimizing.
  w2->Show();
  w2_state->Activate();

  EXPECT_TRUE(w2_state->IsFullscreen());
  EXPECT_TRUE(w1->layer()->IsVisible());
  EXPECT_TRUE(w2->layer()->IsVisible());

  // Minimize the window, which should hide the window.
  EXPECT_TRUE(w2_state->IsActive());
  w2_state->Minimize();
  EXPECT_FALSE(w2_state->IsActive());
  EXPECT_FALSE(w2->layer()->IsVisible());
  EXPECT_TRUE(w1_state->IsActive());
  EXPECT_EQ(w2.get(), GetDesktop()->children()[0]);
  EXPECT_EQ(w1.get(), GetDesktop()->children()[1]);

  // Make the window normal.
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);
  // Setting back to normal doesn't change the activation.
  EXPECT_FALSE(w2_state->IsActive());
  EXPECT_TRUE(w1_state->IsActive());
  EXPECT_EQ(w2.get(), GetDesktop()->children()[0]);
  EXPECT_EQ(w1.get(), GetDesktop()->children()[1]);
  EXPECT_TRUE(w2->layer()->IsVisible());
}

// Verifies ShelfLayoutManager's visibility/auto-hide state is correctly
// updated.
TEST_F(WorkspaceControllerTest, ShelfStateUpdated) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point());
  generator.MoveMouseTo(0, 0);

  std::unique_ptr<Window> w1(CreateTestWindow());
  const gfx::Rect w1_bounds(0, 1, 101, 102);
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  const gfx::Rect touches_shelf_bounds(
      0, shelf_layout_manager()->GetIdealBounds().y() - 10, 101, 102);
  // Move |w1| to overlap the shelf.
  w1->SetBounds(touches_shelf_bounds);
  w1->Show();

  wm::ActivateWindow(w1.get());
  w1->SetBounds(w1_bounds);
  w1->Show();
  wm::ActivateWindow(w1.get());

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  // Maximize the window.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Restore.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());

  // Fullscreen.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  // Normal.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());
  w1->SetBounds(w1_bounds);

  // Maximize again.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Minimize.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMinimized);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  // Since the restore from minimize will restore to the pre-minimize
  // state (tested elsewhere), we abandon the current size and restore
  // rect and set them to the window.
  WindowState* window_state = WindowState::Get(w1.get());

  gfx::Rect restore = window_state->GetRestoreBoundsInScreen();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600).ToString(), w1->bounds().ToString());
  EXPECT_EQ("0,1 101x102", restore.ToString());
  window_state->ClearRestoreBounds();
  w1->SetBounds(restore);

  // Restore.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());

  // Create another window, maximized.
  std::unique_ptr<Window> w2(CreateTestWindow());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  w2->Show();
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());
  EXPECT_EQ(
      screen_util::GetMaximizedWindowBoundsInParent(w2->parent()).ToString(),
      w2->bounds().ToString());

  // Switch to w1.
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());
  EXPECT_EQ(
      screen_util::GetMaximizedWindowBoundsInParent(w2->parent()).ToString(),
      w2->bounds().ToString());

  // Switch to w2.
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());
  EXPECT_EQ(screen_util::GetMaximizedWindowBoundsInParent(w2.get()).ToString(),
            w2->bounds().ToString());
}

// Verifies going from maximized to minimized sets the right state for painting
// the background of the launcher.
TEST_F(WorkspaceControllerTest, MinimizeResetsVisibility) {
  // TODO(bruthig|xiyuan): Move SessionState setup into AshTestBase or
  // AshTestHelper.
  SessionInfo info;
  info.state = session_manager::SessionState::ACTIVE;
  Shell::Get()->session_controller()->SetSessionInfo(info);

  std::unique_ptr<Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            GetPrimaryShelf()->shelf_layout_manager()->shelf_background_type());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMinimized);
  EXPECT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetPrimaryShelf()->shelf_layout_manager()->shelf_background_type());
}

// Verifies window visibility during various workspace changes.
TEST_F(WorkspaceControllerTest, VisibilityTests) {
  std::unique_ptr<Window> w1(CreateTestWindow());
  w1->Show();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_EQ(1.0f, w1->layer()->GetCombinedOpacity());

  // Create another window, activate it and make it fullscreen.
  std::unique_ptr<Window> w2(CreateTestWindow());
  w2->Show();
  wm::ActivateWindow(w2.get());
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kFullscreen);
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_EQ(1.0f, w2->layer()->GetCombinedOpacity());
  EXPECT_TRUE(w1->IsVisible());

  // Switch to w1. |w1| should be visible on top of |w2|.
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_EQ(1.0f, w1->layer()->GetCombinedOpacity());
  EXPECT_TRUE(w2->IsVisible());

  // Switch back to |w2|.
  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_EQ(1.0f, w2->layer()->GetCombinedOpacity());
  EXPECT_TRUE(w1->IsVisible());

  // Restore |w2|, both windows should be visible.
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_EQ(1.0f, w1->layer()->GetCombinedOpacity());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_EQ(1.0f, w2->layer()->GetCombinedOpacity());

  // Make |w2| fullscreen again, then close it.
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kFullscreen);
  w2->Hide();
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_EQ(1.0f, w1->layer()->GetCombinedOpacity());
  EXPECT_TRUE(w1->IsVisible());

  // Create |w2| and maximize it.
  w2.reset(CreateTestWindow());
  w2->Show();
  wm::ActivateWindow(w2.get());
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_EQ(1.0f, w2->layer()->GetCombinedOpacity());
  EXPECT_TRUE(w1->IsVisible());

  // Close |w2|.
  w2.reset();
  EXPECT_EQ(1.0f, w1->layer()->GetCombinedOpacity());
  EXPECT_TRUE(w1->IsVisible());
}

// Verifies windows that are offscreen don't move when switching workspaces.
TEST_F(WorkspaceControllerTest, DontMoveOnSwitch) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point());
  generator.MoveMouseTo(0, 0);

  std::unique_ptr<Window> w1(CreateTestWindow());
  const gfx::Rect touches_shelf_bounds(
      0, shelf_layout_manager()->GetIdealBounds().y() - 10, 101, 102);
  // Move |w1| to overlap the shelf.
  w1->SetBounds(touches_shelf_bounds);
  w1->Show();
  wm::ActivateWindow(w1.get());

  // Create another window and maximize it.
  std::unique_ptr<Window> w2(CreateTestWindow());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  w2->Show();
  wm::ActivateWindow(w2.get());

  // Switch to w1.
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(touches_shelf_bounds.ToString(), w1->bounds().ToString());
}

// Verifies that windows that are completely offscreen move when switching
// workspaces.
TEST_F(WorkspaceControllerTest, MoveOnSwitch) {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root, gfx::Point());
  generator.MoveMouseTo(0, 0);

  std::unique_ptr<Window> w1(CreateTestWindow());
  const gfx::Rect w1_bounds(0, shelf_layout_manager()->GetIdealBounds().y(),
                            100, 200);
  // Move |w1| so that the top edge is the same as the top edge of the shelf.
  w1->SetBounds(w1_bounds);
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(w1_bounds.ToString(), w1->bounds().ToString());

  // Create another window and maximize it.
  std::unique_ptr<Window> w2(CreateTestWindow());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  w2->Show();
  wm::ActivateWindow(w2.get());

  // Increase the size of the WorkAreaInsets. This would make |w1| fall
  // completely out of the display work area.
  WorkAreaInsets* work_area_insets = WorkAreaInsets::ForWindow(root);
  gfx::Insets insets = work_area_insets->in_session_user_work_area_insets();
  insets = gfx::Insets::TLBR(0, 0, insets.bottom() + 30, 0);
  work_area_insets->UpdateWorkAreaInsetsForTest(root, gfx::Rect(), insets,
                                                insets);

  // Switch to w1. The window should have moved.
  wm::ActivateWindow(w1.get());
  EXPECT_NE(w1_bounds.ToString(), w1->bounds().ToString());
}

namespace {

// WindowDelegate used by DontCrashOnChangeAndActivate.
class DontCrashOnChangeAndActivateDelegate
    : public aura::test::TestWindowDelegate {
 public:
  DontCrashOnChangeAndActivateDelegate() = default;

  DontCrashOnChangeAndActivateDelegate(
      const DontCrashOnChangeAndActivateDelegate&) = delete;
  DontCrashOnChangeAndActivateDelegate& operator=(
      const DontCrashOnChangeAndActivateDelegate&) = delete;

  void set_window(aura::Window* window) { window_ = window; }

  // WindowDelegate overrides:
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {
    if (window_) {
      wm::ActivateWindow(window_);
      window_ = nullptr;
    }
  }

 private:
  raw_ptr<aura::Window> window_ = nullptr;
};

}  // namespace

// Exercises possible crash in W2. Here's the sequence:
// . minimize a maximized window.
// . remove the window (which happens when switching displays).
// . add the window back.
// . show the window and during the bounds change activate it.
TEST_F(WorkspaceControllerTest, DontCrashOnChangeAndActivate) {
  // Force the shelf
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);

  DontCrashOnChangeAndActivateDelegate delegate;
  std::unique_ptr<Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate, 1000, gfx::Rect(10, 11, 250, 251)));

  w1->Show();
  WindowState* w1_state = WindowState::Get(w1.get());
  w1_state->Activate();
  w1_state->Maximize();
  w1_state->Minimize();

  w1->parent()->RemoveChild(w1.get());

  // Do this so that when we Show() the window a resize occurs and we make the
  // window active.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  ParentWindowInPrimaryRootWindow(w1.get());
  delegate.set_window(w1.get());
  w1->Show();
}

// Verifies a window with a transient parent not managed by workspace works.
TEST_F(WorkspaceControllerTest, TransientParent) {
  std::unique_ptr<Window> w1(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->Show();

  // Normal window with no transient parent.
  std::unique_ptr<Window> w3(CreateTestWindow());
  w3->SetBounds(gfx::Rect(10, 11, 250, 251));
  w3->Show();
  wm::ActivateWindow(w3.get());

  // Window with a transient parent.
  std::unique_ptr<Window> w2(CreateTestWindowUnparented());
  ::wm::AddTransientChild(w1.get(), w2.get());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  ParentWindowInPrimaryRootWindow(w2.get());
  w2->Show();
  wm::ActivateWindow(w2.get());

  // The window with the transient parent should get added to the same parent as
  // the normal window.
  EXPECT_EQ(w3->parent(), w2->parent());
}

// Test the placement of newly created windows.
TEST_F(WorkspaceControllerTest, BasicAutoPlacingOnCreate) {
  UpdateDisplay("1600x1200");
  // Creating a popup handler here to make sure it does not interfere with the
  // existing windows.
  gfx::Rect source_browser_bounds(16, 32, 640, 320);
  std::unique_ptr<aura::Window> browser_window(
      CreateBrowserLikeWindow(source_browser_bounds));

  // Creating a popup to make sure it does not interfere with the positioning.
  std::unique_ptr<aura::Window> browser_popup(
      CreatePopupLikeWindow(gfx::Rect(16, 32, 128, 256)));

  browser_window->Show();
  browser_popup->Show();

  {  // With a shown window it's size should get returned.
    std::unique_ptr<aura::Window> new_browser_window(
        CreateBrowserLikeWindow(source_browser_bounds));
    // The position should be right flush.
    EXPECT_EQ("960,32 640x320", new_browser_window->bounds().ToString());
  }

  {  // With the window shown - but more on the right side then on the left
    // side (and partially out of the screen), it should default to the other
    // side and inside the screen.
    gfx::Rect new_bounds(gfx::Rect(1000, 600, 640, 320));
    browser_window->SetBounds(new_bounds);

    std::unique_ptr<aura::Window> new_browser_window(
        CreateBrowserLikeWindow(new_bounds));
    // The position should be left & bottom flush.
    EXPECT_EQ("0,600 640x320", new_browser_window->bounds().ToString());

    // If the other window was already beyond the point to get right flush
    // it will remain where it is.
    EXPECT_EQ("1000,600 640x320", browser_window->bounds().ToString());
  }

  {  // Make sure that popups do not get changed.
    std::unique_ptr<aura::Window> new_popup_window(
        CreatePopupLikeWindow(gfx::Rect(50, 100, 300, 150)));
    EXPECT_EQ("50,100 300x150", new_popup_window->bounds().ToString());
  }

  browser_window->Hide();
  {  // If a window is there but not shown the default should be centered.
    std::unique_ptr<aura::Window> new_browser_window(
        CreateBrowserLikeWindow(gfx::Rect(50, 100, 300, 150)));
    EXPECT_EQ("650,100 300x150", new_browser_window->bounds().ToString());
  }
}

// Test that adding a second window shifts both the first window and its
// transient child.
TEST_F(WorkspaceControllerTest, AutoPlacingMovesTransientChild) {
  // Create an auto-positioned window.
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  gfx::Rect desktop_area = window1->parent()->bounds();
  WindowState::Get(window1.get())->SetWindowPositionManaged(true);
  // Hide and then show |window1| to trigger auto-positioning logic.
  window1->Hide();
  window1->SetBounds(gfx::Rect(16, 32, 300, 300));
  window1->Show();

  // |window1| should be horizontally centered.
  int x_window1 = (desktop_area.width() - 300) / 2;
  EXPECT_EQ(base::NumberToString(x_window1) + ",32 300x300",
            window1->bounds().ToString());

  // Create a |child| window and make it a transient child of |window1|.
  std::unique_ptr<Window> child(CreateTestWindowUnparented());
  ::wm::AddTransientChild(window1.get(), child.get());
  const int x_child = x_window1 + 50;
  child->SetBounds(gfx::Rect(x_child, 20, 200, 200));
  ParentWindowInPrimaryRootWindow(child.get());
  child->Show();
  wm::ActivateWindow(child.get());

  // The |child| should be where it was created.
  EXPECT_EQ(base::NumberToString(x_child) + ",20 200x200",
            child->bounds().ToString());

  // Create and show a second window forcing the first window and its child to
  // move.
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  WindowState::Get(window2.get())->SetWindowPositionManaged(true);
  // Hide and then show |window2| to trigger auto-positioning logic.
  window2->Hide();
  window2->SetBounds(gfx::Rect(32, 48, 250, 250));
  window2->Show();

  // Check that both |window1| and |child| have moved left.
  EXPECT_EQ("0,32 300x300", window1->bounds().ToString());
  int x = x_child - x_window1;
  EXPECT_EQ(base::NumberToString(x) + ",20 200x200",
            child->bounds().ToString());
  // Check that |window2| has moved right.
  x = desktop_area.width() - window2->bounds().width();
  EXPECT_EQ(base::NumberToString(x) + ",48 250x250",
            window2->bounds().ToString());
}

// Test the basic auto placement of one and or two windows in a "simulated
// session" of sequential window operations.
TEST_F(WorkspaceControllerTest, BasicAutoPlacingOnShowHide) {
  // Test 1: In case there is no manageable window, no window should shift.

  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  gfx::Rect desktop_area = window1->parent()->bounds();

  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  // Trigger the auto window placement function by making it visible.
  // Note that the bounds are getting changed while it is invisible.
  window2->Hide();
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));
  window2->Show();

  // Check the initial position of the windows is unchanged.
  EXPECT_EQ("16,32 640x320", window1->bounds().ToString());
  EXPECT_EQ("32,48 256x512", window2->bounds().ToString());

  // Remove the second window and make sure that the first window
  // does NOT get centered.
  window2.reset();
  EXPECT_EQ("16,32 640x320", window1->bounds().ToString());

  WindowState* window1_state = WindowState::Get(window1.get());
  // Test 2: Set up two managed windows and check their auto positioning.
  window1_state->SetWindowPositionManaged(true);

  std::unique_ptr<aura::Window> window3(CreateTestWindowInShellWithId(2));
  WindowState::Get(window3.get())->SetWindowPositionManaged(true);
  // To avoid any auto window manager changes due to SetBounds, the window
  // gets first hidden and then shown again.
  window3->Hide();
  window3->SetBounds(gfx::Rect(32, 48, 256, 512));
  window3->Show();
  // |window1| should be flush left and |window3| flush right.
  EXPECT_EQ("0,32 640x320", window1->bounds().ToString());
  EXPECT_EQ(
      base::NumberToString(desktop_area.width() - window3->bounds().width()) +
          ",48 256x512",
      window3->bounds().ToString());

  // After removing |window3|, |window1| should be centered again.
  window3.reset();
  EXPECT_EQ(base::NumberToString(
                (desktop_area.width() - window1->bounds().width()) / 2) +
                ",32 640x320",
            window1->bounds().ToString());

  // Test 3: Set up a manageable and a non manageable window and check
  // positioning.
  std::unique_ptr<aura::Window> window4(CreateTestWindowInShellWithId(3));
  // To avoid any auto window manager changes due to SetBounds, the window
  // gets first hidden and then shown again.
  window1->Hide();
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  window4->SetBounds(gfx::Rect(32, 48, 256, 512));
  window1->Show();
  // |window1| should be centered and |window4| untouched.
  EXPECT_EQ(base::NumberToString(
                (desktop_area.width() - window1->bounds().width()) / 2) +
                ",32 640x320",
            window1->bounds().ToString());
  EXPECT_EQ("32,48 256x512", window4->bounds().ToString());

  // Test4: A single manageable window should get centered.
  window4.reset();
  window1_state->SetBoundsChangedByUser(false);
  // Trigger the auto window placement function by showing (and hiding) it.
  window1->Hide();
  window1->Show();
  // |window1| should be centered.
  EXPECT_EQ(base::NumberToString(
                (desktop_area.width() - window1->bounds().width()) / 2) +
                ",32 640x320",
            window1->bounds().ToString());
}

// Test the proper usage of user window movement interaction.
TEST_F(WorkspaceControllerTest, TestUserMovedWindowRepositioning) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  gfx::Rect desktop_area = window1->parent()->bounds();
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));
  window1->Hide();
  window2->Hide();
  WindowState* window1_state = WindowState::Get(window1.get());
  WindowState* window2_state = WindowState::Get(window2.get());

  window1_state->SetWindowPositionManaged(true);
  window2_state->SetWindowPositionManaged(true);
  EXPECT_FALSE(window1_state->bounds_changed_by_user());
  EXPECT_FALSE(window2_state->bounds_changed_by_user());

  // Check that the current location gets preserved if the user has
  // positioned it previously.
  window1_state->SetBoundsChangedByUser(true);
  window1->Show();
  EXPECT_EQ("16,32 640x320", window1->bounds().ToString());
  // Flag should be still set.
  EXPECT_TRUE(window1_state->bounds_changed_by_user());
  EXPECT_FALSE(window2_state->bounds_changed_by_user());

  // Turn on the second window and make sure that both windows are now
  // positionable again (user movement cleared).
  window2->Show();

  // |window1| should be flush left and |window2| flush right.
  EXPECT_EQ("0,32 640x320", window1->bounds().ToString());
  EXPECT_EQ(
      base::NumberToString(desktop_area.width() - window2->bounds().width()) +
          ",48 256x512",
      window2->bounds().ToString());
  // FLag should now be reset.
  EXPECT_FALSE(window1_state->bounds_changed_by_user());
  EXPECT_FALSE(window2_state->bounds_changed_by_user());

  // Going back to one shown window should keep the state.
  window1_state->SetBoundsChangedByUser(true);
  window2->Hide();
  EXPECT_EQ("0,32 640x320", window1->bounds().ToString());
  EXPECT_TRUE(window1_state->bounds_changed_by_user());
}

// Test if the single window will be restored at original position.
TEST_F(WorkspaceControllerTest, TestSingleWindowsRestoredBounds) {
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(110, 110, 100, 100)));
  std::unique_ptr<aura::Window> window3(
      CreateTestWindowInShellWithBounds(gfx::Rect(120, 120, 100, 100)));
  window1->Hide();
  window2->Hide();
  window3->Hide();
  WindowState::Get(window1.get())->SetWindowPositionManaged(true);
  WindowState::Get(window2.get())->SetWindowPositionManaged(true);
  WindowState::Get(window3.get())->SetWindowPositionManaged(true);

  window1->Show();
  wm::ActivateWindow(window1.get());
  window2->Show();
  wm::ActivateWindow(window2.get());
  window3->Show();
  wm::ActivateWindow(window3.get());
  EXPECT_EQ(0, window1->bounds().x());
  EXPECT_EQ(window2->GetRootWindow()->bounds().right(),
            window2->bounds().right());
  EXPECT_EQ(0, window3->bounds().x());

  window1->Hide();
  EXPECT_EQ(window2->GetRootWindow()->bounds().right(),
            window2->bounds().right());
  EXPECT_EQ(0, window3->bounds().x());

  // Being a single window will retore the original location.
  window3->Hide();
  wm::ActivateWindow(window2.get());
  EXPECT_EQ("110,110 100x100", window2->bounds().ToString());

  // Showing the 3rd will push the 2nd window left.
  window3->Show();
  wm::ActivateWindow(window3.get());
  EXPECT_EQ(0, window2->bounds().x());
  EXPECT_EQ(window3->GetRootWindow()->bounds().right(),
            window3->bounds().right());

  // Being a single window will retore the original location.
  window2->Hide();
  EXPECT_EQ("120,120 100x100", window3->bounds().ToString());
}

// Test that user placed windows go back to their user placement after the user
// closes all other windows.
TEST_F(WorkspaceControllerTest, TestUserHandledWindowRestore) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  gfx::Rect user_pos = gfx::Rect(16, 42, 640, 320);
  window1->SetBounds(user_pos);
  WindowState* window1_state = WindowState::Get(window1.get());

  window1_state->set_pre_auto_manage_window_bounds(user_pos);
  gfx::Rect desktop_area = window1->parent()->bounds();

  // Create a second window to let the auto manager kick in.
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));
  window1->Hide();
  window2->Hide();
  WindowState::Get(window1.get())->SetWindowPositionManaged(true);
  WindowState::Get(window2.get())->SetWindowPositionManaged(true);
  window1->Show();
  EXPECT_EQ(user_pos.ToString(), window1->bounds().ToString());
  window2->Show();

  // |window1| should be flush left and |window2| flush right.
  EXPECT_EQ("0," + base::NumberToString(user_pos.y()) + " 640x320",
            window1->bounds().ToString());
  EXPECT_EQ(
      base::NumberToString(desktop_area.width() - window2->bounds().width()) +
          ",48 256x512",
      window2->bounds().ToString());
  window2->Hide();

  // After the other window get hidden the window has to move back to the
  // previous position and the bounds should still be set and unchanged.
  EXPECT_EQ(user_pos.ToString(), window1->bounds().ToString());
  ASSERT_TRUE(window1_state->pre_auto_manage_window_bounds());
  EXPECT_EQ(user_pos.ToString(),
            window1_state->pre_auto_manage_window_bounds()->ToString());
}

// Solo window should be restored to the bounds where a user moved to.
TEST_F(WorkspaceControllerTest, TestRestoreToUserModifiedBounds) {
  UpdateDisplay("400x300");
  gfx::Rect default_bounds(10, 0, 100, 100);
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(default_bounds));
  WindowState* window1_state = WindowState::Get(window1.get());
  window1->Hide();
  window1_state->SetWindowPositionManaged(true);
  window1->Show();
  // First window is centered.
  EXPECT_EQ("150,0 100x100", window1->bounds().ToString());
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(default_bounds));
  WindowState* window2_state = WindowState::Get(window2.get());
  window2->Hide();
  window2_state->SetWindowPositionManaged(true);
  window2->Show();

  // Auto positioning pushes windows to each sides.
  EXPECT_EQ("0,0 100x100", window1->bounds().ToString());
  EXPECT_EQ("300,0 100x100", window2->bounds().ToString());

  window2->Hide();
  // Restores to the center.
  EXPECT_EQ("150,0 100x100", window1->bounds().ToString());

  // A user moved the window.
  std::unique_ptr<WindowResizer> resizer(
      CreateWindowResizer(window1.get(), gfx::PointF(), HTCAPTION,
                          ::wm::WINDOW_MOVE_SOURCE_MOUSE)
          .release());
  gfx::PointF location = resizer->GetInitialLocation();
  location.Offset(-50, 0);
  resizer->Drag(location, 0);
  resizer->CompleteDrag();

  window1_state->SetBoundsChangedByUser(true);
  window1->SetBounds(gfx::Rect(100, 0, 100, 100));

  window2->Show();
  EXPECT_EQ("0,0 100x100", window1->bounds().ToString());
  EXPECT_EQ("300,0 100x100", window2->bounds().ToString());

  // Window 1 should be restored to the user modified bounds.
  window2->Hide();
  EXPECT_EQ("100,0 100x100", window1->bounds().ToString());
}

// Test that a window from normal to minimize will repos the remaining.
TEST_F(WorkspaceControllerTest, ToMinimizeRepositionsRemaining) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  WindowState* window1_state = WindowState::Get(window1.get());
  window1_state->SetWindowPositionManaged(true);
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  gfx::Rect desktop_area = window1->parent()->bounds();

  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  WindowState* window2_state = WindowState::Get(window2.get());
  window2_state->SetWindowPositionManaged(true);
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));

  window1_state->Minimize();

  // |window2| should be centered now.
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window2_state->IsNormalStateType());
  EXPECT_EQ(base::NumberToString(
                (desktop_area.width() - window2->bounds().width()) / 2) +
                ",48 256x512",
            window2->bounds().ToString());

  window1_state->Restore();
  // |window1| should be flush right and |window3| flush left.
  EXPECT_EQ(
      base::NumberToString(desktop_area.width() - window1->bounds().width()) +
          ",32 640x320",
      window1->bounds().ToString());
  EXPECT_EQ("0,48 256x512", window2->bounds().ToString());
}

// Test that minimizing an initially maximized window will repos the remaining.
TEST_F(WorkspaceControllerTest, MaxToMinRepositionsRemaining) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  WindowState* window1_state = WindowState::Get(window1.get());
  window1_state->SetWindowPositionManaged(true);
  gfx::Rect desktop_area = window1->parent()->bounds();

  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  WindowState* window2_state = WindowState::Get(window2.get());
  window2_state->SetWindowPositionManaged(true);
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));

  window1_state->Maximize();
  window1_state->Minimize();

  // |window2| should be centered now.
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window2_state->IsNormalStateType());
  EXPECT_EQ(base::NumberToString(
                (desktop_area.width() - window2->bounds().width()) / 2) +
                ",48 256x512",
            window2->bounds().ToString());
}

// Test that nomral, maximize, minimizing will repos the remaining.
TEST_F(WorkspaceControllerTest, NormToMaxToMinRepositionsRemaining) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  WindowState* window1_state = WindowState::Get(window1.get());
  window1_state->SetWindowPositionManaged(true);
  gfx::Rect desktop_area = window1->parent()->bounds();

  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  WindowState* window2_state = WindowState::Get(window2.get());
  window2_state->SetWindowPositionManaged(true);
  window2->SetBounds(gfx::Rect(32, 40, 256, 512));

  // Trigger the auto window placement function by showing (and hiding) it.
  window1->Hide();
  window1->Show();

  // |window1| should be flush right and |window3| flush left.
  EXPECT_EQ(
      base::NumberToString(desktop_area.width() - window1->bounds().width()) +
          ",32 640x320",
      window1->bounds().ToString());
  EXPECT_EQ("0,40 256x512", window2->bounds().ToString());

  window1_state->Maximize();
  window1_state->Minimize();

  // |window2| should be centered now.
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window2_state->IsNormalStateType());
  EXPECT_EQ(base::NumberToString(
                (desktop_area.width() - window2->bounds().width()) / 2) +
                ",40 256x512",
            window2->bounds().ToString());
}

// Test that nomral, maximize, normal will repos the remaining.
TEST_F(WorkspaceControllerTest, NormToMaxToNormRepositionsRemaining) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  WindowState* window1_state = WindowState::Get(window1.get());
  window1_state->SetWindowPositionManaged(true);
  gfx::Rect desktop_area = window1->parent()->bounds();

  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  WindowState::Get(window2.get())->SetWindowPositionManaged(true);
  window2->SetBounds(gfx::Rect(32, 40, 256, 512));

  // Trigger the auto window placement function by showing (and hiding) it.
  window1->Hide();
  window1->Show();

  // |window1| should be flush right and |window3| flush left.
  EXPECT_EQ(
      base::NumberToString(desktop_area.width() - window1->bounds().width()) +
          ",32 640x320",
      window1->bounds().ToString());
  EXPECT_EQ("0,40 256x512", window2->bounds().ToString());

  window1_state->Maximize();
  window1_state->Restore();

  // |window1| should be flush right and |window2| flush left.
  EXPECT_EQ(
      base::NumberToString(desktop_area.width() - window1->bounds().width()) +
          ",32 640x320",
      window1->bounds().ToString());
  EXPECT_EQ("0,40 256x512", window2->bounds().ToString());
}

// Test that animations are triggered.
TEST_F(WorkspaceControllerTest, AnimatedNormToMaxToNormRepositionsRemaining) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->Hide();
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  gfx::Rect desktop_area = window1->parent()->bounds();
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  window2->Hide();
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));

  WindowState::Get(window1.get())->SetWindowPositionManaged(true);
  WindowState::Get(window2.get())->SetWindowPositionManaged(true);
  // Make sure nothing is animating.
  window1->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  window2->Show();

  // The second window should now animate.
  EXPECT_FALSE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  window2->layer()->GetAnimator()->StopAnimating();

  window1->Show();
  EXPECT_TRUE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());

  window1->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  // |window1| should be flush right and |window2| flush left.
  EXPECT_EQ(
      base::NumberToString(desktop_area.width() - window1->bounds().width()) +
          ",32 640x320",
      window1->bounds().ToString());
  EXPECT_EQ("0,48 256x512", window2->bounds().ToString());
}

// This tests simulates a browser and an app and verifies the ordering of the
// windows and layers doesn't get out of sync as various operations occur. Its
// really testing code in FocusController, but easier to simulate here. Just as
// with a real browser the browser here has a transient child window
// (corresponds to the status bubble).
TEST_F(WorkspaceControllerTest, VerifyLayerOrdering) {
  std::unique_ptr<Window> browser(aura::test::CreateTestWindowWithDelegate(
      nullptr, aura::client::WINDOW_TYPE_NORMAL, gfx::Rect(5, 6, 7, 8),
      nullptr));
  browser->SetName("browser");
  ParentWindowInPrimaryRootWindow(browser.get());
  browser->Show();
  wm::ActivateWindow(browser.get());

  // |status_bubble| is made a transient child of |browser| and as a result
  // owned by |browser|.
  aura::test::TestWindowDelegate* status_bubble_delegate =
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
  status_bubble_delegate->set_can_focus(false);
  Window* status_bubble = aura::test::CreateTestWindowWithDelegate(
      status_bubble_delegate, aura::client::WINDOW_TYPE_POPUP,
      gfx::Rect(5, 6, 7, 8), nullptr);
  ::wm::AddTransientChild(browser.get(), status_bubble);
  ParentWindowInPrimaryRootWindow(status_bubble);
  status_bubble->SetName("status_bubble");

  std::unique_ptr<Window> app(aura::test::CreateTestWindowWithDelegate(
      nullptr, aura::client::WINDOW_TYPE_NORMAL, gfx::Rect(5, 6, 7, 8),
      nullptr));
  app->SetName("app");
  ParentWindowInPrimaryRootWindow(app.get());

  aura::Window* parent = browser->parent();

  app->Show();
  wm::ActivateWindow(app.get());
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Minimize the app, focus should go the browser.
  app->SetProperty(aura::client::kShowStateKey,
                   ui::mojom::WindowShowState::kMinimized);
  EXPECT_TRUE(wm::IsActiveWindow(browser.get()));
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Minimize the browser (neither windows are focused).
  browser->SetProperty(aura::client::kShowStateKey,
                       ui::mojom::WindowShowState::kMinimized);
  EXPECT_FALSE(wm::IsActiveWindow(browser.get()));
  EXPECT_FALSE(wm::IsActiveWindow(app.get()));
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Show the browser (which should restore it).
  browser->Show();
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Activate the browser.
  wm::ActivateWindow(browser.get());
  EXPECT_TRUE(wm::IsActiveWindow(browser.get()));
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Restore the app. This differs from above code for |browser| as internally
  // the app code does this. Restoring this way or using Show() should not make
  // a difference.
  app->SetProperty(aura::client::kShowStateKey,
                   ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Activate the app.
  wm::ActivateWindow(app.get());
  EXPECT_TRUE(wm::IsActiveWindow(app.get()));
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));
}

// Test that minimizing and restoring a snapped window should restore to the
// snapped bounds. When a window is created and snapped, it must be a
// user-initiated operation, no need to do rearrangement for restoring this
// window (crbug.com/692175).
TEST_F(WorkspaceControllerTest, RestoreMinimizedSnappedWindow) {
  // Create an auto-positioned window.
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->SetWindowPositionManaged(true);
  window->SetBounds(gfx::Rect(10, 20, 100, 200));
  window->Show();

  // Left snap |window|.
  EXPECT_FALSE(window_state->bounds_changed_by_user());
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);
  const gfx::Rect work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(window->bounds().origin())
          .work_area();
  gfx::Rect snapped_bounds(work_area.x(), work_area.y(), work_area.width() / 2,
                           work_area.height());
  EXPECT_EQ(snapped_bounds, window->bounds());
  EXPECT_TRUE(window_state->bounds_changed_by_user());

  // Minimize and Restore |window|, the restored bounds should be equal to the
  // bounds of left snapped state.
  window_state->Minimize();
  window_state->Restore();
  EXPECT_EQ(snapped_bounds, window->bounds());
}

// Verifies that a new maximized window becomes visible after its activation
// is requested, even though it does not become activated because a system
// modal window is active.
TEST_F(WorkspaceControllerTest, SwitchFromModal) {
  std::unique_ptr<Window> modal_window(CreateTestWindowUnparented());
  modal_window->SetBounds(gfx::Rect(10, 11, 21, 22));
  modal_window->SetProperty(aura::client::kModalKey,
                            ui::mojom::ModalType::kSystem);
  ParentWindowInPrimaryRootWindow(modal_window.get());
  modal_window->Show();
  wm::ActivateWindow(modal_window.get());

  std::unique_ptr<Window> maximized_window(CreateTestWindow());
  maximized_window->SetProperty(aura::client::kShowStateKey,
                                ui::mojom::WindowShowState::kMaximized);
  maximized_window->Show();
  wm::ActivateWindow(maximized_window.get());
  EXPECT_TRUE(maximized_window->IsVisible());
}

// Verifies that when dragging a window autohidden shelf stays hidden during
// and after the drag.
TEST_F(WorkspaceControllerTest, DragWindowKeepsShelfAutohidden) {
  aura::test::TestWindowDelegate delegate;
  delegate.set_window_component(HTCAPTION);
  std::unique_ptr<Window> window(aura::test::CreateTestWindowWithDelegate(
      &delegate, aura::client::WINDOW_TYPE_NORMAL, gfx::Rect(5, 5, 100, 50),
      nullptr));
  ParentWindowInPrimaryRootWindow(window.get());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  const auto window_bounds_before_drag = window->GetBoundsInScreen();

  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      window_bounds_before_drag.CenterPoint());
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(10, 10);

  // Shelf should be hidden during and after the drag.
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  event_generator->ReleaseLeftButton();
  EXPECT_NE(window->GetBoundsInScreen(), window_bounds_before_drag);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Verifies that events are targeted properly just outside the window edges.
TEST_F(WorkspaceControllerTest, WindowEdgeHitTest) {
  aura::test::TestWindowDelegate d_first, d_second;
  std::unique_ptr<Window> first(aura::test::CreateTestWindowWithDelegate(
      &d_first, 123, gfx::Rect(20, 10, 100, 50), nullptr));
  ParentWindowInPrimaryRootWindow(first.get());
  first->Show();

  std::unique_ptr<Window> second(aura::test::CreateTestWindowWithDelegate(
      &d_second, 234, gfx::Rect(30, 40, 40, 10), nullptr));
  ParentWindowInPrimaryRootWindow(second.get());
  second->Show();

  aura::Window* root = first->GetRootWindow();
  ui::EventTargeter* targeter =
      root->GetHost()->dispatcher()->GetDefaultEventTargeter();

  // The windows overlap, and |second| is on top of |first|. Events targeted
  // slightly outside the edges of the |second| window should still be targeted
  // to |second| to allow resizing the windows easily.

  const int kNumPoints = 4;
  struct {
    const char* direction;
    gfx::Point location;
  } points[kNumPoints] = {
      {"left", gfx::Point(28, 45)},    // outside the left edge.
      {"top", gfx::Point(50, 38)},     // outside the top edge.
      {"right", gfx::Point(72, 45)},   // outside the right edge.
      {"bottom", gfx::Point(50, 52)},  // outside the bottom edge.
  };
  // Do two iterations, first without any transform on |second|, and the second
  // time after applying some transform on |second| so that it doesn't get
  // targeted.
  for (int times = 0; times < 2; ++times) {
    SCOPED_TRACE(times == 0 ? "Without transform" : "With transform");
    aura::Window* expected_target = times == 0 ? second.get() : first.get();
    for (int i = 0; i < kNumPoints; ++i) {
      SCOPED_TRACE(points[i].direction);
      const gfx::Point& location = points[i].location;
      ui::MouseEvent mouse(ui::EventType::kMouseMoved, location, location,
                           ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
      ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
      EXPECT_EQ(expected_target, target);

      ui::TouchEvent touch(ui::EventType::kTouchPressed, location,
                           ui::EventTimeForNow(),
                           ui::PointerDetails(ui::EventPointerType::kTouch, 0));
      target = targeter->FindTargetForEvent(root, &touch);
      EXPECT_EQ(expected_target, target);
    }
    // Apply a transform on |second|. After the transform is applied, the window
    // should no longer be targeted.
    gfx::Transform transform;
    transform.Translate(70, 40);
    second->SetTransform(transform);
  }
}

}  // namespace ash
