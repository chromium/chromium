// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"

#include <memory>

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/widget/widget.h"

namespace ash {

using aura::Window;

namespace {

StatusAreaWidgetDelegate* GetStatusAreaWidgetDelegate(views::Widget* widget) {
  return static_cast<StatusAreaWidgetDelegate*>(widget->GetContentsView());
}

class PanedWidgetDelegate : public views::WidgetDelegate {
 public:
  PanedWidgetDelegate(views::Widget* widget) : widget_(widget) {}

  void SetAccessiblePanes(
      const std::vector<raw_ptr<views::View, VectorExperimental>>& panes) {
    accessible_panes_ = panes;
  }

  // views::WidgetDelegate:
  void GetAccessiblePanes(std::vector<views::View*>* panes) override {
    base::ranges::copy(accessible_panes_, std::back_inserter(*panes));
  }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }

 private:
  raw_ptr<views::Widget, DanglingUntriaged> widget_;
  std::vector<raw_ptr<views::View, VectorExperimental>> accessible_panes_;
};

}  // namespace

class FocusCyclerTest : public AshTestBase {
 public:
  FocusCyclerTest() = default;

  FocusCyclerTest(const FocusCyclerTest&) = delete;
  FocusCyclerTest& operator=(const FocusCyclerTest&) = delete;

  void SetUp() override {
    AshTestBase::SetUp();

    focus_cycler_ = std::make_unique<FocusCycler>();
  }

  void TearDown() override {
    GetStatusAreaWidgetDelegate(GetPrimaryStatusAreaWidget())
        ->SetFocusCyclerForTesting(nullptr);

    GetPrimaryShelf()->shelf_widget()->SetFocusCycler(nullptr);

    focus_cycler_.reset();

    AshTestBase::TearDown();
  }

 protected:
  // Setup the system tray focus cycler.
  void SetUpTrayFocusCycle() {
    views::Widget* system_tray_widget = GetPrimaryStatusAreaWidget();
    ASSERT_TRUE(system_tray_widget);
    focus_cycler_->AddWidget(system_tray_widget);
    GetStatusAreaWidgetDelegate(system_tray_widget)
        ->SetFocusCyclerForTesting(focus_cycler());
  }

  views::Widget* GetPrimaryStatusAreaWidget() {
    return GetPrimaryShelf()->GetStatusAreaWidget();
  }

  FocusCycler* focus_cycler() { return focus_cycler_.get(); }

  void InstallFocusCycleOnShelf() {
    // Add the shelf.
    GetPrimaryShelf()->hotseat_widget()->SetFocusCycler(focus_cycler());
  }

 private:
  std::unique_ptr<FocusCycler> focus_cycler_;
};

TEST_F(FocusCyclerTest, CycleFocusBrowserOnly) {
  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle the window
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusForward) {
  SetUpTrayFocusCycle();

  InstallFocusCycleOnShelf();

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusBackward) {
  SetUpTrayFocusCycle();

  InstallFocusCycleOnShelf();

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->IsActive());

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusForwardBackward) {
  SetUpTrayFocusCycle();

  InstallFocusCycleOnShelf();

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->IsActive());

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusNoBrowser) {
  SetUpTrayFocusCycle();

  InstallFocusCycleOnShelf();

  // Add the shelf and focus it.
  focus_cycler()->FocusWidget(GetPrimaryShelf()->hotseat_widget());

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->IsActive());

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->IsActive());

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());
}

// Tests that focus cycles from the active browser to the status area and back.
TEST_F(FocusCyclerTest, Shelf_CycleFocusForward) {
  SetUpTrayFocusCycle();
  InstallFocusCycleOnShelf();
  GetPrimaryShelf()->hotseat_widget()->Hide();

  // Create two test windows.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());
}

TEST_F(FocusCyclerTest, Shelf_CycleFocusBackwardInvisible) {
  SetUpTrayFocusCycle();
  InstallFocusCycleOnShelf();
  GetPrimaryShelf()->hotseat_widget()->Hide();

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusThroughWindowWithPanes) {
  SetUpTrayFocusCycle();

  InstallFocusCycleOnShelf();

  std::unique_ptr<PanedWidgetDelegate> test_widget_delegate;
  std::unique_ptr<views::Widget> browser_widget(new views::Widget);
  test_widget_delegate =
      std::make_unique<PanedWidgetDelegate>(browser_widget.get());
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.delegate = test_widget_delegate.get();
  widget_params.context = GetContext();
  browser_widget->Init(std::move(widget_params));
  browser_widget->Show();

  aura::Window* browser_window = browser_widget->GetNativeView();

  views::View* root_view = browser_widget->GetRootView();

  // pane1 contains view1 and view2, pane2 contains view3 and view4.
  views::AccessiblePaneView* pane1 = new views::AccessiblePaneView();
  root_view->AddChildView(pane1);

  views::View* view1 = new views::View;
  view1->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  pane1->AddChildView(view1);

  views::View* view2 = new views::View;
  view2->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  pane1->AddChildView(view2);

  views::AccessiblePaneView* pane2 = new views::AccessiblePaneView();
  root_view->AddChildView(pane2);

  views::View* view3 = new views::View;
  view3->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  pane2->AddChildView(view3);

  views::View* view4 = new views::View;
  view4->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  pane2->AddChildView(view4);

  std::vector<raw_ptr<views::View, VectorExperimental>> panes;
  panes.push_back(pane1);
  panes.push_back(pane2);

  test_widget_delegate->SetAccessiblePanes(panes);

  views::FocusManager* focus_manager = browser_widget->GetFocusManager();

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->IsActive());

  // Cycle focus to the first pane in the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view1);

  // Cycle focus to the second pane in the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view3);

  // Cycle focus back to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Reverse direction - back to the second pane in the browser.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view3);

  // Back to the first pane in the browser.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view1);

  // Back to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->IsActive());

  // Back to the status area.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Pressing "Escape" while on the status area should
  // deactivate it, and activate the browser window.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view1);

  // Similarly, pressing "Escape" while on the shelf should do the same thing.
  // Focus the navigation widget directly because the shelf has no apps here.
  GetPrimaryShelf()->shelf_focus_cycler()->FocusNavigation(false /* last */);
  EXPECT_TRUE(GetPrimaryShelf()->navigation_widget()->IsActive());
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view1);
}

TEST_F(FocusCyclerTest, CycleFocusThroughWindowWithPanes_MoveOntoNext) {
  SetUpTrayFocusCycle();

  InstallFocusCycleOnShelf();

  std::unique_ptr<views::Widget> browser_widget =
      std::make_unique<views::Widget>();
  std::unique_ptr<PanedWidgetDelegate> test_widget_delegate =
      std::make_unique<PanedWidgetDelegate>(browser_widget.get());
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.delegate = test_widget_delegate.get();

  widget_params.context = GetContext();
  browser_widget->Init(std::move(widget_params));
  browser_widget->Show();

  aura::Window* browser_window = browser_widget->GetNativeView();

  views::View* root_view = browser_widget->GetRootView();

  // pane1 contains view1 and view2, pane2 contains view3 and view4.
  views::AccessiblePaneView* pane1 = new views::AccessiblePaneView();
  root_view->AddChildView(pane1);

  views::View* view1 = new views::View();
  view1->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  pane1->AddChildView(view1);

  views::AccessiblePaneView* pane2 = new views::AccessiblePaneView();
  root_view->AddChildView(pane2);

  views::View* view2 = new views::View();
  view2->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  pane2->AddChildView(view2);

  test_widget_delegate->SetAccessiblePanes({pane1, pane2});

  views::FocusManager* focus_manager = browser_widget->GetFocusManager();

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->IsActive());

  // Cycle focus to the first pane in the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view1);

  // Cycle focus back to the status area by asking the focus_cycler to move
  // onto the next widget. This should skip the next accessible pane.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD, true);
  EXPECT_FALSE(wm::IsActiveWindow(browser_window));
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Manually deallocate to ensure delegate outlives widget.
  browser_widget.release();
  test_widget_delegate.release();
}

// Test that when the shelf widget & status area widget are removed, they should
// also be removed from focus cycler.
TEST_F(FocusCyclerTest, RemoveWidgetOnDisplayRemoved) {
  // Two displays are added, so two shelf widgets and two status area widgets
  // are added to focus cycler.
  UpdateDisplay("800x700, 600x500");
  // Remove one display. Its shelf widget and status area widget should also be
  // removed from focus cycler.
  UpdateDisplay("800x700");

  // Create a single test window.
  std::unique_ptr<Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());
  EXPECT_TRUE(wm::IsActiveWindow(window.get()));

  // Cycle focus to the navigation widget.
  Shell::Get()->focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryShelf()->navigation_widget()->IsActive());

  // Cycle focus to the hotseat widget.
  Shell::Get()->focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->IsActive());

  // Cycle focus to the status area.
  Shell::Get()->focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(GetPrimaryStatusAreaWidget()->IsActive());

  // Cycle focus should go back to the browser.
  Shell::Get()->focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window.get()));
}

}  // namespace ash
