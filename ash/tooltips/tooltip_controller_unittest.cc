// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/corewm/tooltip_controller_test_helper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/tooltip_client.h"

using views::corewm::TooltipController;
using views::corewm::test::TooltipTestView;
using views::corewm::test::TooltipControllerTestHelper;

// The tests in this file exercise bits of TooltipController that are hard to
// test outside of ash. Meaning these tests require the shell and related things
// to be installed.

namespace ash {

namespace {

views::Widget* CreateNewWidgetWithBoundsOn(int display,
                                           const gfx::Rect& bounds) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent =
      Shell::Get()->GetContainer(Shell::GetAllRootWindows().at(display),
                                 desks_util::GetActiveDeskContainerId());
  params.bounds = bounds;
  widget->Init(std::move(params));
  widget->Show();
  return widget;
}

views::Widget* CreateNewWidgetOn(int display) {
  return CreateNewWidgetWithBoundsOn(display, gfx::Rect());
}

void AddViewToWidgetAndResize(views::Widget* widget, views::View* view) {
  if (!widget->GetContentsView()) {
    views::View* contents_view = new views::View;
    widget->SetContentsView(contents_view);
  }

  views::View* contents_view = widget->GetContentsView();
  contents_view->AddChildView(view);
  view->SetBounds(contents_view->width(), 0, 100, 100);
  gfx::Rect contents_view_bounds = contents_view->bounds();
  contents_view_bounds.Union(view->bounds());
  contents_view->SetBoundsRect(contents_view_bounds);
  widget->SetBounds(gfx::Rect(widget->GetWindowBoundsInScreen().origin(),
                              contents_view_bounds.size()));
}

TooltipController* GetController() {
  return static_cast<TooltipController*>(
      ::wm::GetTooltipClient(Shell::GetPrimaryRootWindow()));
}

}  // namespace

class TooltipControllerTest : public AshTestBase {
 public:
  TooltipControllerTest() = default;
  ~TooltipControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    helper_.reset(new TooltipControllerTestHelper(GetController()));
  }

 protected:
  std::unique_ptr<TooltipControllerTestHelper> helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TooltipControllerTest);
};

TEST_F(TooltipControllerTest, NonNullTooltipClient) {
  EXPECT_TRUE(::wm::GetTooltipClient(Shell::GetPrimaryRootWindow()) != NULL);
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

TEST_F(TooltipControllerTest, HideTooltipWhenCursorHidden) {
  std::unique_ptr<views::Widget> widget(CreateNewWidgetOn(0));
  TooltipTestView* view = new TooltipTestView;
  AddViewToWidgetAndResize(widget.get(), view);
  view->set_tooltip_text(base::ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseRelativeTo(widget->GetNativeView(),
                                view->bounds().CenterPoint());
  base::string16 expected_tooltip = base::ASCIIToUTF16("Tooltip Text");

  // Mouse event triggers tooltip update so it becomes visible.
  EXPECT_TRUE(helper_->IsTooltipVisible());

  // Disable mouse event which hides the cursor and check again.
  ash::Shell::Get()->cursor_manager()->DisableMouseEvents();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ash::Shell::Get()->cursor_manager()->IsCursorVisible());
  helper_->UpdateIfRequired();
  EXPECT_FALSE(helper_->IsTooltipVisible());

  // Enable mouse event which shows the cursor and re-check.
  ash::Shell::Get()->cursor_manager()->EnableMouseEvents();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ash::Shell::Get()->cursor_manager()->IsCursorVisible());
  helper_->UpdateIfRequired();
  EXPECT_TRUE(helper_->IsTooltipVisible());
}

TEST_F(TooltipControllerTest, TooltipsOnMultiDisplayShouldNotCrash) {
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::unique_ptr<views::Widget> widget1(
      CreateNewWidgetWithBoundsOn(0, gfx::Rect(10, 10, 100, 100)));
  TooltipTestView* view1 = new TooltipTestView;
  AddViewToWidgetAndResize(widget1.get(), view1);
  view1->set_tooltip_text(base::ASCIIToUTF16("Tooltip Text for view 1"));
  EXPECT_EQ(widget1->GetNativeView()->GetRootWindow(), root_windows[0]);

  std::unique_ptr<views::Widget> widget2(
      CreateNewWidgetWithBoundsOn(1, gfx::Rect(1200, 10, 100, 100)));
  TooltipTestView* view2 = new TooltipTestView;
  AddViewToWidgetAndResize(widget2.get(), view2);
  view2->set_tooltip_text(base::ASCIIToUTF16("Tooltip Text for view 2"));
  EXPECT_EQ(widget2->GetNativeView()->GetRootWindow(), root_windows[1]);

  // Show tooltip on second display.
  ui::test::EventGenerator generator(root_windows[1]);
  generator.MoveMouseRelativeTo(widget2->GetNativeView(),
                                view2->bounds().CenterPoint());
  EXPECT_TRUE(helper_->IsTooltipVisible());

  // Get rid of secondary display. This destroy's the tooltip's aura window. If
  // we have handled this case, we will not crash in the following statement.
  UpdateDisplay("1000x600");
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_EQ(widget2->GetNativeView()->GetRootWindow(), root_windows[0]);

  // The tooltip should create a new aura window for itself, so we should still
  // be able to show tooltips on the primary display.
  ui::test::EventGenerator generator1(root_windows[0]);
  generator1.MoveMouseRelativeTo(widget1->GetNativeView(),
                                 view1->bounds().CenterPoint());
  EXPECT_TRUE(helper_->IsTooltipVisible());
}

}  // namespace ash
