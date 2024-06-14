// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/scoped_display_for_new_windows.h"

namespace ash {

using ScreenAshTest = AshTestBase;

// Tests that ScreenAsh::GetWindowAtScreenPoint() returns the correct window on
// the correct display.
TEST_F(ScreenAshTest, TestGetWindowAtScreenPoint) {
  UpdateDisplay("300x200,500x400");

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> win1(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 200, 200)));

  std::unique_ptr<aura::Window> win2(CreateTestWindowInShellWithDelegate(
      &delegate, 1, gfx::Rect(300, 200, 100, 100)));

  ASSERT_NE(win1->GetRootWindow(), win2->GetRootWindow());

  EXPECT_EQ(win1.get(), display::Screen::GetScreen()->GetWindowAtScreenPoint(
                            gfx::Point(50, 60)));
  EXPECT_EQ(win2.get(), display::Screen::GetScreen()->GetWindowAtScreenPoint(
                            gfx::Point(350, 260)));
}

TEST_F(ScreenAshTest, GetDisplayForNewWindows) {
  UpdateDisplay("300x200,500x400");
  display::Screen* screen = display::Screen::GetScreen();
  const std::vector<display::Display> displays = screen->GetAllDisplays();
  ASSERT_EQ(2u, displays.size());

  // The display for new windows defaults to primary display.
  EXPECT_EQ(displays[0].id(), screen->GetDisplayForNewWindows().id());

  // The display for new windows is updated when the root window for new windows
  // changes.
  display::ScopedDisplayForNewWindows scoped_display(
      Shell::GetAllRootWindows()[1]);
  EXPECT_EQ(displays[1].id(), screen->GetDisplayForNewWindows().id());
}

namespace {

// Simulates an observer that tries to get the primary display when notified of
// displays addition or removal when switching to or from the Unified Desktop
// mode.
class TestDisplayRemoveObserver : public display::DisplayObserver {
 public:
  TestDisplayRemoveObserver() = default;

  TestDisplayRemoveObserver(const TestDisplayRemoveObserver&) = delete;
  TestDisplayRemoveObserver& operator=(const TestDisplayRemoveObserver&) =
      delete;

  ~TestDisplayRemoveObserver() override = default;

  int added_displays() const { return added_displays_; }
  int removed_displays() const { return removed_displays_; }

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    TestPrimaryDisplay();
    ++added_displays_;
  }

  void OnDisplaysRemoved(const display::Displays& removed_displays) override {
    TestPrimaryDisplay();
    removed_displays_ += removed_displays.size();
  }

 private:
  void TestPrimaryDisplay() const {
    auto display = display::Screen::GetScreen()->GetPrimaryDisplay();
    DCHECK_NE(display.id(), display::kInvalidDisplayId);
  }

  int added_displays_ = 0;
  int removed_displays_ = 0;
};

}  // namespace

// Switching to Unified Desktop removes all current displays (including primary
// display) and replaces them with the unified display. The display manager
// notifies observers of display removals before display additions. At this
// point if an observer tries to get the primary display, it could lead to a
// crash because all displays have been removed. This test makes sure doesn't
// happen anymore. https://crbug.com/866714.
TEST_F(ScreenAshTest, TestNoCrashesOnGettingPrimaryDisplayOnDisplayRemoved) {
  UpdateDisplay("400x500,300x200");

  TestDisplayRemoveObserver observer;
  display_manager()->AddDisplayObserver(&observer);

  // Enter Unified Mode.
  display_manager()->SetUnifiedDesktopEnabled(true);
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  EXPECT_EQ(observer.added_displays(), 1);
  EXPECT_EQ(observer.removed_displays(), 2);

  // Exit Unified Mode, there shouldn't be any crashes either.
  display_manager()->SetUnifiedDesktopEnabled(false);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  EXPECT_EQ(observer.added_displays(), 3);
  EXPECT_EQ(observer.removed_displays(), 3);

  display_manager()->RemoveDisplayObserver(&observer);
}

}  // namespace ash
