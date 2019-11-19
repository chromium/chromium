// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace window_util {

namespace {

std::string GetAdjustedBounds(const gfx::Rect& visible,
                              gfx::Rect to_be_adjusted) {
  AdjustBoundsToEnsureMinimumWindowVisibility(visible, &to_be_adjusted);
  return to_be_adjusted.ToString();
}

class FakeWindowState : public WindowState::State {
 public:
  explicit FakeWindowState() = default;
  ~FakeWindowState() override = default;

  // WindowState::State overrides:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override {
    if (event->type() == WM_EVENT_MINIMIZE)
      was_visible_on_minimize_ = window_state->window()->IsVisible();
  }
  WindowStateType GetType() const override { return WindowStateType::kNormal; }
  void AttachState(WindowState* window_state,
                   WindowState::State* previous_state) override {}
  void DetachState(WindowState* window_state) override {}

  bool was_visible_on_minimize() { return was_visible_on_minimize_; }

 private:
  bool was_visible_on_minimize_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeWindowState);
};

}  // namespace

using WindowUtilTest = AshTestBase;

TEST_F(WindowUtilTest, CenterWindow) {
  UpdateDisplay("500x400, 600x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(12, 20, 100, 100)));

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->bounds_changed_by_user());

  CenterWindow(window.get());
  // Centring window is considered as a user's action.
  EXPECT_TRUE(window_state->bounds_changed_by_user());
  if (chromeos::switches::ShouldShowShelfHotseat()) {
    EXPECT_EQ("200,126 100x100", window->bounds().ToString());
    EXPECT_EQ("200,126 100x100", window->GetBoundsInScreen().ToString());
  } else {
    EXPECT_EQ("200,122 100x100", window->bounds().ToString());
    EXPECT_EQ("200,122 100x100", window->GetBoundsInScreen().ToString());
  }
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100), GetSecondaryDisplay());
  CenterWindow(window.get());
  if (chromeos::switches::ShouldShowShelfHotseat()) {
    EXPECT_EQ("250,126 100x100", window->bounds().ToString());
    EXPECT_EQ("750,126 100x100", window->GetBoundsInScreen().ToString());
  } else {
    EXPECT_EQ("250,122 100x100", window->bounds().ToString());
    EXPECT_EQ("750,122 100x100", window->GetBoundsInScreen().ToString());
  }
}

TEST_F(WindowUtilTest, AdjustBoundsToEnsureMinimumVisibility) {
  const gfx::Rect visible_bounds(0, 0, 100, 100);

  EXPECT_EQ("0,0 90x90",
            GetAdjustedBounds(visible_bounds, gfx::Rect(0, 0, 90, 90)));
  EXPECT_EQ("0,0 100x100",
            GetAdjustedBounds(visible_bounds, gfx::Rect(0, 0, 150, 150)));
  EXPECT_EQ("-50,0 100x100",
            GetAdjustedBounds(visible_bounds, gfx::Rect(-50, -50, 150, 150)));
  EXPECT_EQ("-75,10 100x100",
            GetAdjustedBounds(visible_bounds, gfx::Rect(-100, 10, 150, 150)));
  EXPECT_EQ("75,75 100x100",
            GetAdjustedBounds(visible_bounds, gfx::Rect(100, 100, 150, 150)));

  // For windows that have smaller dimensions than kMinimumOnScreenArea,
  // we should adjust bounds accordingly, leaving no white space.
  EXPECT_EQ("50,80 20x20",
            GetAdjustedBounds(visible_bounds, gfx::Rect(50, 80, 20, 20)));
  EXPECT_EQ("80,50 20x20",
            GetAdjustedBounds(visible_bounds, gfx::Rect(80, 50, 20, 20)));
  EXPECT_EQ("0,50 20x20",
            GetAdjustedBounds(visible_bounds, gfx::Rect(0, 50, 20, 20)));
  EXPECT_EQ("50,0 20x20",
            GetAdjustedBounds(visible_bounds, gfx::Rect(50, 0, 20, 20)));
  EXPECT_EQ("50,80 20x20",
            GetAdjustedBounds(visible_bounds, gfx::Rect(50, 100, 20, 20)));
  EXPECT_EQ("80,50 20x20",
            GetAdjustedBounds(visible_bounds, gfx::Rect(100, 50, 20, 20)));
  EXPECT_EQ("0,50 20x20",
            GetAdjustedBounds(visible_bounds, gfx::Rect(-10, 50, 20, 20)));
  EXPECT_EQ("50,0 20x20",
            GetAdjustedBounds(visible_bounds, gfx::Rect(50, -10, 20, 20)));

  const gfx::Rect visible_bounds_right(200, 50, 100, 100);

  EXPECT_EQ("210,60 90x90", GetAdjustedBounds(visible_bounds_right,
                                              gfx::Rect(210, 60, 90, 90)));
  EXPECT_EQ("210,60 100x100", GetAdjustedBounds(visible_bounds_right,
                                                gfx::Rect(210, 60, 150, 150)));
  EXPECT_EQ("125,50 100x100",
            GetAdjustedBounds(visible_bounds_right, gfx::Rect(0, 0, 150, 150)));
  EXPECT_EQ("275,50 100x100", GetAdjustedBounds(visible_bounds_right,
                                                gfx::Rect(300, 20, 150, 150)));
  EXPECT_EQ(
      "125,125 100x100",
      GetAdjustedBounds(visible_bounds_right, gfx::Rect(-100, 150, 150, 150)));

  const gfx::Rect visible_bounds_left(-200, -50, 100, 100);
  EXPECT_EQ("-190,-40 90x90", GetAdjustedBounds(visible_bounds_left,
                                                gfx::Rect(-190, -40, 90, 90)));
  EXPECT_EQ(
      "-190,-40 100x100",
      GetAdjustedBounds(visible_bounds_left, gfx::Rect(-190, -40, 150, 150)));
  EXPECT_EQ(
      "-250,-40 100x100",
      GetAdjustedBounds(visible_bounds_left, gfx::Rect(-250, -40, 150, 150)));
  EXPECT_EQ(
      "-275,-50 100x100",
      GetAdjustedBounds(visible_bounds_left, gfx::Rect(-400, -60, 150, 150)));
  EXPECT_EQ("-125,0 100x100",
            GetAdjustedBounds(visible_bounds_left, gfx::Rect(0, 0, 150, 150)));
}

TEST_F(WindowUtilTest, MoveWindowToDisplay) {
  UpdateDisplay("500x400, 600x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(12, 20, 100, 100)));
  display::Screen* screen = display::Screen::GetScreen();
  const int64_t original_display_id =
      screen->GetDisplayNearestWindow(window.get()).id();
  EXPECT_EQ(screen->GetPrimaryDisplay().id(), original_display_id);
  const int original_container_id = window->parent()->id();
  const aura::Window* original_root = window->GetRootWindow();

  EXPECT_FALSE(MoveWindowToDisplay(window.get(), display::kInvalidDisplayId));
  EXPECT_EQ(original_display_id,
            screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_FALSE(MoveWindowToDisplay(window.get(), original_display_id));
  EXPECT_EQ(original_display_id,
            screen->GetDisplayNearestWindow(window.get()).id());

  ASSERT_EQ(2, screen->GetNumDisplays());
  const int64_t secondary_display_id = screen->GetAllDisplays()[1].id();
  EXPECT_NE(original_display_id, secondary_display_id);
  EXPECT_TRUE(MoveWindowToDisplay(window.get(), secondary_display_id));
  EXPECT_EQ(secondary_display_id,
            screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_EQ(original_container_id, window->parent()->id());
  EXPECT_NE(original_root, window->GetRootWindow());

  EXPECT_TRUE(MoveWindowToDisplay(window.get(), original_display_id));
  EXPECT_EQ(original_display_id,
            screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_EQ(original_container_id, window->parent()->id());
  EXPECT_EQ(original_root, window->GetRootWindow());
}

TEST_F(WindowUtilTest, RemoveTransientDescendants) {
  // Create two windows which have no transient children or parents. Test that
  // neither of them get removed when running RemoveTransientDescendants.
  auto window1 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  std::vector<aura::Window*> window_list = {window1.get(), window2.get()};
  RemoveTransientDescendants(&window_list);
  ASSERT_EQ(2u, window_list.size());

  // Create two windows whose transient roots are |window1|. One is a direct
  // transient child and one is a transient descendant. Test that both get
  // removed when calling RemoveTransientDescendants.
  auto descendant1 = CreateTestWindow();
  auto descendant2 = CreateTestWindow();
  ::wm::AddTransientChild(descendant1.get(), descendant2.get());
  ::wm::AddTransientChild(window1.get(), descendant1.get());
  window_list.push_back(descendant1.get());
  window_list.push_back(descendant2.get());
  RemoveTransientDescendants(&window_list);
  ASSERT_EQ(2u, window_list.size());
  ASSERT_TRUE(base::Contains(window_list, window1.get()));
  ASSERT_TRUE(base::Contains(window_list, window2.get()));

  // Create a window which has a transient parent that is not in |window_list|.
  // Test that the window is not removed when calling
  // RemoveTransientDescendants.
  auto window3 = CreateTestWindow();
  auto descendant3 = CreateTestWindow();
  ::wm::AddTransientChild(window3.get(), descendant3.get());
  window_list.push_back(descendant3.get());
  RemoveTransientDescendants(&window_list);
  EXPECT_EQ(3u, window_list.size());
}

TEST_F(WindowUtilTest,
       HideAndMaybeMinimizeWithoutAnimationMinimizesArcWindowsBeforeHiding) {
  auto window = CreateTestWindow();
  auto* state = new FakeWindowState();
  WindowState::Get(window.get())
      ->SetStateObject(std::unique_ptr<WindowState::State>(state));

  std::vector<aura::Window*> windows = {window.get()};
  HideAndMaybeMinimizeWithoutAnimation(windows, /*minimize=*/true);

  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(state->was_visible_on_minimize());
}

TEST_F(WindowUtilTest, InteriorTargeter) {
  auto window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));

  WindowState::Get(window.get())->Maximize();
  InstallResizeHandleWindowTargeterForWindow(window.get());

  auto* child = aura::test::CreateTestWindowWithDelegateAndType(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(),
      aura::client::WINDOW_TYPE_UNKNOWN, 1, gfx::Rect(window->bounds().size()),
      window.get(),
      /*show_on_creation=*/true);

  ui::EventTarget* root_target = window->GetRootWindow();
  auto* targeter = root_target->GetEventTargeter();
  {
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, gfx::Point(0, 0), gfx::Point(0, 0),
                         ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(child, targeter->FindTargetForEvent(root_target, &mouse));
  }

  // InteriorEventTargeter is now active and should pass an event at the edge to
  // its parent.
  WindowState::Get(window.get())->Restore();
  {
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, gfx::Point(0, 0), gfx::Point(0, 0),
                         ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root_target, &mouse));
  }
}

}  // namespace window_util
}  // namespace ash
