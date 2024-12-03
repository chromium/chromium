// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/test/fake_window_state.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/wm_event.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace window_util {

namespace {

std::string GetAdjustedBounds(const gfx::Rect& visible,
                              gfx::Rect to_be_adjusted) {
  AdjustBoundsToEnsureMinimumWindowVisibility(
      visible, /*client_controlled=*/false, &to_be_adjusted);
  return to_be_adjusted.ToString();
}

}  // namespace

using WindowUtilTest = AshTestBase;

TEST_F(WindowUtilTest, AdjustBoundsToEnsureMinimumVisibility) {
  constexpr gfx::Rect kVisibleBounds(0, 0, 100, 100);

  EXPECT_EQ("0,0 90x90",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(0, 0, 90, 90)));
  EXPECT_EQ("0,0 100x100",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(0, 0, 150, 150)));
  EXPECT_EQ("-50,0 100x100",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(-50, -50, 150, 150)));
  EXPECT_EQ("-55,10 100x100",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(-100, 10, 150, 150)));
  EXPECT_EQ("55,55 100x100",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(100, 100, 150, 150)));

  // For windows that have smaller dimensions than kMinimumOnScreenArea,
  // we should adjust bounds accordingly, leaving no white space.
  EXPECT_EQ("50,80 20x20",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(50, 80, 20, 20)));
  EXPECT_EQ("80,50 20x20",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(80, 50, 20, 20)));
  EXPECT_EQ("0,50 20x20",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(0, 50, 20, 20)));
  EXPECT_EQ("50,0 20x20",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(50, 0, 20, 20)));
  EXPECT_EQ("50,80 20x20",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(50, 100, 20, 20)));
  EXPECT_EQ("80,50 20x20",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(100, 50, 20, 20)));
  EXPECT_EQ("0,50 20x20",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(-10, 50, 20, 20)));
  EXPECT_EQ("50,0 20x20",
            GetAdjustedBounds(kVisibleBounds, gfx::Rect(50, -10, 20, 20)));

  constexpr gfx::Rect kVisibleBoundsRight(200, 50, 100, 100);

  EXPECT_EQ("210,60 90x90",
            GetAdjustedBounds(kVisibleBoundsRight, gfx::Rect(210, 60, 90, 90)));
  EXPECT_EQ("210,60 100x100", GetAdjustedBounds(kVisibleBoundsRight,
                                                gfx::Rect(210, 60, 150, 150)));
  EXPECT_EQ("145,50 100x100",
            GetAdjustedBounds(kVisibleBoundsRight, gfx::Rect(0, 0, 150, 150)));
  EXPECT_EQ("255,50 100x100", GetAdjustedBounds(kVisibleBoundsRight,
                                                gfx::Rect(300, 20, 150, 150)));
  EXPECT_EQ(
      "145,105 100x100",
      GetAdjustedBounds(kVisibleBoundsRight, gfx::Rect(-100, 150, 150, 150)));

  constexpr gfx::Rect kVisibleBoundsLeft(-200, -50, 100, 100);
  EXPECT_EQ("-190,-40 90x90", GetAdjustedBounds(kVisibleBoundsLeft,
                                                gfx::Rect(-190, -40, 90, 90)));
  EXPECT_EQ(
      "-190,-40 100x100",
      GetAdjustedBounds(kVisibleBoundsLeft, gfx::Rect(-190, -40, 150, 150)));
  EXPECT_EQ(
      "-250,-40 100x100",
      GetAdjustedBounds(kVisibleBoundsLeft, gfx::Rect(-250, -40, 150, 150)));
  EXPECT_EQ(
      "-255,-50 100x100",
      GetAdjustedBounds(kVisibleBoundsLeft, gfx::Rect(-400, -60, 150, 150)));
  EXPECT_EQ("-145,0 100x100",
            GetAdjustedBounds(kVisibleBoundsLeft, gfx::Rect(0, 0, 150, 150)));
}

TEST_F(WindowUtilTest, MoveWindowToDisplay) {
  UpdateDisplay("500x400, 600x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(12, 20, 100, 100)));
  display::Screen* screen = display::Screen::GetScreen();
  const int64_t original_display_id =
      screen->GetDisplayNearestWindow(window.get()).id();
  EXPECT_EQ(screen->GetPrimaryDisplay().id(), original_display_id);
  const int original_container_id = window->parent()->GetId();
  const aura::Window* original_root = window->GetRootWindow();

  ASSERT_EQ(2, screen->GetNumDisplays());
  const int64_t secondary_display_id = screen->GetAllDisplays()[1].id();
  EXPECT_NE(original_display_id, secondary_display_id);
  EXPECT_TRUE(MoveWindowToDisplay(window.get(), secondary_display_id));
  EXPECT_EQ(secondary_display_id,
            screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_EQ(original_container_id, window->parent()->GetId());
  EXPECT_NE(original_root, window->GetRootWindow());

  EXPECT_TRUE(MoveWindowToDisplay(window.get(), original_display_id));
  EXPECT_EQ(original_display_id,
            screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_EQ(original_container_id, window->parent()->GetId());
  EXPECT_EQ(original_root, window->GetRootWindow());
}

// Tests that locking and unlocking the screen does not alter the display of a
// window moved by MoveWindowToDisplay.
TEST_F(WindowUtilTest, MoveWindowToDisplayAndLockScreen) {
  UpdateDisplay("500x400, 600x400");
  auto window = CreateTestWindow(gfx::Rect(12, 20, 100, 100));
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(2, screen->GetNumDisplays());
  const int64_t primary_display_id = screen->GetAllDisplays()[0].id();
  const int64_t secondary_display_id = screen->GetAllDisplays()[1].id();
  ASSERT_EQ(primary_display_id,
            screen->GetDisplayNearestWindow(window.get()).id());

  EXPECT_TRUE(MoveWindowToDisplay(window.get(), secondary_display_id));
  EXPECT_EQ(secondary_display_id,
            screen->GetDisplayNearestWindow(window.get()).id());

  // Tests that after locking and unlocking the screen the window remains on the
  // secondary display.
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_EQ(secondary_display_id,
            screen->GetDisplayNearestWindow(window.get()).id());

  // Move the window to the primary display. Tests that after locking and
  // unlocking the screen the window remains on the secondary display.
  EXPECT_TRUE(MoveWindowToDisplay(window.get(), primary_display_id));
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_EQ(primary_display_id,
            screen->GetDisplayNearestWindow(window.get()).id());
}

TEST_F(WindowUtilTest, EnsureTransientRoots) {
  // Create two windows which have no transient children or parents. Test that
  // neither of them get removed when running EnsureTransientRoots.
  auto window1 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  std::vector<raw_ptr<aura::Window, VectorExperimental>> window_list = {
      window1.get(), window2.get()};
  EnsureTransientRoots(&window_list);
  ASSERT_EQ(2u, window_list.size());

  // Create two windows whose transient roots are |window1|. One is a direct
  // transient child and one is a transient descendant. Test that both get
  // removed when calling EnsureTransientRoots.
  auto descendant1 = CreateTestWindow();
  auto descendant2 = CreateTestWindow();
  ::wm::AddTransientChild(descendant1.get(), descendant2.get());
  ::wm::AddTransientChild(window1.get(), descendant1.get());
  window_list.push_back(descendant1.get());
  window_list.push_back(descendant2.get());
  EnsureTransientRoots(&window_list);
  ASSERT_EQ(2u, window_list.size());
  ASSERT_TRUE(base::Contains(window_list, window1.get()));
  ASSERT_TRUE(base::Contains(window_list, window2.get()));

  // Create a window which has a transient parent that is not in |window_list|.
  // Test that the window is replaced with its transient root when calling
  // EnsureTransientRoots.
  auto window3 = CreateTestWindow();
  auto descendant3 = CreateTestWindow();
  ::wm::AddTransientChild(window3.get(), descendant3.get());
  window_list.push_back(descendant3.get());
  EnsureTransientRoots(&window_list);
  EXPECT_EQ(3u, window_list.size());
  EXPECT_TRUE(base::Contains(window_list, window3.get()));
  EXPECT_FALSE(base::Contains(window_list, descendant3.get()));

  // Create two windows which have the same transient parent that is not in
  // |window_list|. Test that one of the windows is replaced with its transient
  // root and the other is removed from |window_list| when calling
  // EnsureTransientRoots.
  auto window4 = CreateTestWindow();
  auto descendant4 = CreateTestWindow();
  auto descendant5 = CreateTestWindow();
  ::wm::AddTransientChild(window4.get(), descendant4.get());
  ::wm::AddTransientChild(window4.get(), descendant5.get());
  window_list.push_back(descendant4.get());
  window_list.push_back(descendant5.get());
  EnsureTransientRoots(&window_list);
  EXPECT_EQ(4u, window_list.size());
  EXPECT_TRUE(base::Contains(window_list, window4.get()));
  EXPECT_FALSE(base::Contains(window_list, descendant4.get()));
  EXPECT_FALSE(base::Contains(window_list, descendant5.get()));
}

TEST_F(WindowUtilTest,
       MinimizeAndHideWithoutAnimationMinimizesArcWindowsBeforeHiding) {
  auto window = CreateTestWindow();
  auto* state = new FakeWindowState(chromeos::WindowStateType::kNormal);
  WindowState::Get(window.get())
      ->SetStateObject(std::unique_ptr<WindowState::State>(state));

  std::vector<raw_ptr<aura::Window, VectorExperimental>> windows = {
      window.get()};
  MinimizeAndHideWithoutAnimation(windows);

  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(state->was_visible_on_minimize());
}

TEST_F(WindowUtilTest, SortWindowsBottomToTop) {
  auto window1 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  auto window3 = CreateTestWindow();

  EXPECT_EQ(
      (std::vector<aura::Window*>{window1.get(), window2.get(), window3.get()}),
      SortWindowsBottomToTop({window3.get(), window1.get(), window2.get()}));

  EXPECT_EQ((std::vector<aura::Window*>{window2.get(), window3.get()}),
            SortWindowsBottomToTop({window3.get(), window2.get()}));

  EXPECT_EQ((std::vector<aura::Window*>{window1.get(), window2.get()}),
            SortWindowsBottomToTop({window2.get(), window1.get()}));

  EXPECT_EQ((std::vector<aura::Window*>{window2.get()}),
            SortWindowsBottomToTop({window2.get()}));

  // Add some children to window2:
  std::unique_ptr<aura::Window> window21 =
      std::make_unique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  window21->Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<aura::Window> window22 =
      std::make_unique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  window22->Init(ui::LAYER_NOT_DRAWN);

  window2->AddChild(window21.get());
  window2->AddChild(window22.get());

  EXPECT_EQ(
      (std::vector<aura::Window*>{window1.get(), window2.get(), window21.get(),
                                  window22.get(), window3.get()}),
      SortWindowsBottomToTop({window21.get(), window22.get(), window3.get(),
                              window1.get(), window2.get()}));
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
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(0, 0),
                         gfx::Point(0, 0), ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(child, targeter->FindTargetForEvent(root_target, &mouse));
  }

  // InteriorEventTargeter is now active and should pass an event at the edge to
  // its parent.
  WindowState::Get(window.get())->Restore();
  {
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(0, 0),
                         gfx::Point(0, 0), ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root_target, &mouse));
  }
}

TEST_F(WindowUtilTest, PinWindow) {
  auto window_state_delegate = std::make_unique<FakeWindowStateDelegate>();
  auto* window_state_delegate_ptr = window_state_delegate.get();
  EXPECT_EQ(window_state_delegate_ptr->toggle_locked_fullscreen_count(), 0);

  auto window = CreateTestWindow();
  WindowState* window_state = WindowState::Get(window.get());
  window_state->SetDelegate(std::move(window_state_delegate));
  window_util::PinWindow(window.get(), /* trusted */ false);
  EXPECT_TRUE(WindowState::Get(window.get())->IsPinned());
  EXPECT_FALSE(WindowState::Get(window.get())->IsTrustedPinned());
  EXPECT_EQ(window_state_delegate_ptr->toggle_locked_fullscreen_count(), 1);

  WindowState::Get(window.get())->Restore();

  EXPECT_FALSE(WindowState::Get(window.get())->IsPinned());
  EXPECT_FALSE(WindowState::Get(window.get())->IsTrustedPinned());
  EXPECT_EQ(window_state_delegate_ptr->toggle_locked_fullscreen_count(), 2);

  window_util::PinWindow(window.get(), /* trusted */ true);
  EXPECT_TRUE(WindowState::Get(window.get())->IsPinned());
  EXPECT_TRUE(WindowState::Get(window.get())->IsTrustedPinned());
  EXPECT_EQ(window_state_delegate_ptr->toggle_locked_fullscreen_count(), 3);
}

TEST_F(WindowUtilTest, PinWindow_TabletMode) {
  // Use tablet mode controller.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  auto window_state_delegate = std::make_unique<FakeWindowStateDelegate>();
  auto* window_state_delegate_ptr = window_state_delegate.get();
  EXPECT_EQ(window_state_delegate_ptr->toggle_locked_fullscreen_count(), 0);

  auto window = CreateTestWindow();
  WindowState* window_state = WindowState::Get(window.get());
  window_state->SetDelegate(std::move(window_state_delegate));
  window_util::PinWindow(window.get(), /* trusted */ false);
  EXPECT_TRUE(WindowState::Get(window.get())->IsPinned());
  EXPECT_FALSE(WindowState::Get(window.get())->IsTrustedPinned());
  EXPECT_EQ(window_state_delegate_ptr->toggle_locked_fullscreen_count(), 1);

  WindowState::Get(window.get())->Restore();

  EXPECT_FALSE(WindowState::Get(window.get())->IsPinned());
  EXPECT_FALSE(WindowState::Get(window.get())->IsTrustedPinned());
  EXPECT_EQ(window_state_delegate_ptr->toggle_locked_fullscreen_count(), 2);

  window_util::PinWindow(window.get(), /* trusted */ true);
  EXPECT_TRUE(WindowState::Get(window.get())->IsPinned());
  EXPECT_TRUE(WindowState::Get(window.get())->IsTrustedPinned());
  EXPECT_EQ(window_state_delegate_ptr->toggle_locked_fullscreen_count(), 3);
}

TEST_F(WindowUtilTest, ShouldRoundThumbnailWindow) {
  const float rounding = 30.f;
  auto backdrop_view = std::make_unique<views::View>();
  backdrop_view->SetPaintToLayer();
  backdrop_view->layer()->SetRoundedCornerRadius(
      {rounding, rounding, rounding, rounding});

  // Note that `SetPosition` does nothing since this view is floating. For this
  // test this is fine, but if we need to have a position, we need to attach
  // this view to a views tree.
  backdrop_view->SetBounds(0, 0, 300, 200);
  ASSERT_EQ(gfx::Rect(300, 200), backdrop_view->GetBoundsInScreen());

  // If the thumbnail covers the backdrop completely, it should be rounded as
  // well.
  EXPECT_TRUE(ShouldRoundThumbnailWindow(backdrop_view.get(),
                                         gfx::RectF(300.f, 200.f)));

  // If the thumbnail is completely within the backdrop's bounds including
  // rounding, it doesn't need to be rounded.
  EXPECT_FALSE(ShouldRoundThumbnailWindow(backdrop_view.get(),
                                          gfx::RectF(0.f, 30.f, 300.f, 140.f)));
  EXPECT_FALSE(ShouldRoundThumbnailWindow(backdrop_view.get(),
                                          gfx::RectF(30.f, 0.f, 240.f, 200.f)));

  // The thumbnail partially covers the part of the backdrop that will not get
  // drawn. We should round the thumbnail as well in this case, otherwise the
  // corner will be drawn over the rounding.
  EXPECT_TRUE(ShouldRoundThumbnailWindow(backdrop_view.get(),
                                         gfx::RectF(0.f, 15.f, 300.f, 170.f)));
  EXPECT_TRUE(ShouldRoundThumbnailWindow(backdrop_view.get(),
                                         gfx::RectF(15.f, 0.f, 270.f, 200.f)));
}

}  // namespace window_util
}  // namespace ash
