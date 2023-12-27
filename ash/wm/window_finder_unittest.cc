// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_finder.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_state.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

using WindowFinderTest = AshTestBase;

TEST_F(WindowFinderTest, RealTopmostCanBeNullptr) {
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::set<aura::Window*> ignore;

  EXPECT_EQ(window1.get(), GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore));
}

TEST_F(WindowFinderTest, ToplevelCanBeNotDrawn) {
  aura::test::TestWindowDelegate delegate;
  auto window = std::make_unique<aura::Window>(&delegate,
                                               aura::client::WINDOW_TYPE_POPUP);
  window->Init(ui::LAYER_NOT_DRAWN);
  gfx::Rect bounds(0, 0, 100, 100);
  window->SetBounds(bounds);
  auto* parent = GetDefaultParentForWindow(
      window.get(), Shell::GetPrimaryRootWindow(), bounds);
  parent->AddChild(window.get());
  window->Show();

  std::set<aura::Window*> ignore;
  EXPECT_EQ(window.get(), GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore));
}

TEST_F(WindowFinderTest, MultipleDisplays) {
  UpdateDisplay("300x200,400x300");

  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(300, 0, 100, 100));
  ASSERT_NE(window1->GetRootWindow(), window2->GetRootWindow());

  std::set<aura::Window*> ignore;
  EXPECT_EQ(window1.get(), GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore));
  EXPECT_EQ(window2.get(),
            GetTopmostWindowAtPoint(gfx::Point(310, 10), ignore));
  EXPECT_EQ(nullptr, GetTopmostWindowAtPoint(gfx::Point(10, 210), ignore));
}

TEST_F(WindowFinderTest, WindowTargeterWithHitTestRects) {
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));

  std::set<aura::Window*> ignore;

  EXPECT_EQ(window2.get(), GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore));

  auto targeter = std::make_unique<aura::WindowTargeter>();
  targeter->SetInsets(gfx::Insets::TLBR(0, 50, 0, 0));
  window2->SetEventTargeter(std::move(targeter));

  EXPECT_EQ(window1.get(), GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore));
  EXPECT_EQ(window2.get(), GetTopmostWindowAtPoint(gfx::Point(60, 10), ignore));
}

// Tests that when overview is active, GetTopmostWindowAtPoint() will return
// the window in overview that contains the specified screen point, even though
// it might be a minimized window.
TEST_F(WindowFinderTest, TopmostWindowWithOverviewActive) {
  UpdateDisplay("500x400");
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Get |window1| and |window2|'s transformed bounds in overview.
  OverviewGrid* grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          window1->GetRootWindow());
  gfx::Rect bounds1 = gfx::ToEnclosedRect(
      grid->GetOverviewItemContaining(window1.get())->target_bounds());
  gfx::Rect bounds2 = gfx::ToEnclosedRect(
      grid->GetOverviewItemContaining(window2.get())->target_bounds());

  std::set<aura::Window*> ignore;
  EXPECT_EQ(window1.get(),
            GetTopmostWindowAtPoint(bounds1.CenterPoint(), ignore));
  EXPECT_EQ(window2.get(),
            GetTopmostWindowAtPoint(bounds2.CenterPoint(), ignore));

  WindowState::Get(window1.get())->Minimize();
  EXPECT_EQ(window1.get(),
            GetTopmostWindowAtPoint(bounds1.CenterPoint(), ignore));
}

namespace {

// Defines an observer that tries to get the top-most window while the window it
// observes is being destroyed. This is to verify that the destroying window
// cannot be found and returned as the top-most one.
class WindowDestroyingObserver : public aura::WindowObserver {
 public:
  WindowDestroyingObserver(const gfx::Point& screen_point, aura::Window* window)
      : screen_point_(screen_point), window_being_observed_(window) {
    window_being_observed_->AddObserver(this);
  }

  aura::Window* top_most_window_while_destroying() const {
    return top_most_window_while_destroying_;
  }

  ~WindowDestroyingObserver() override { StopObserving(); }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    StopObserving();
    top_most_window_while_destroying_ =
        GetTopmostWindowAtPoint(screen_point_, /*ignore=*/{});
  }

 private:
  void StopObserving() {
    if (window_being_observed_) {
      window_being_observed_->RemoveObserver(this);
      window_being_observed_ = nullptr;
    }
  }

  // The point in screen coordinates at which we'll get the top-most window when
  // `window_being_observed_` is destroying.
  const gfx::Point screen_point_;

  raw_ptr<aura::Window> window_being_observed_;

  // This is the window we find as the top-most window while
  // `window_being_observed_` is being destroyed.
  raw_ptr<aura::Window> top_most_window_while_destroying_ = nullptr;
};

}  // namespace

TEST_F(WindowFinderTest, WindowBeingDestroyedCannotBeReturned) {
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  auto* window_ptr = window.get();
  WindowDestroyingObserver observer{window->GetBoundsInScreen().CenterPoint(),
                                    window_ptr};
  window.reset();
  EXPECT_NE(window_ptr, observer.top_most_window_while_destroying());
}

}  // namespace ash
