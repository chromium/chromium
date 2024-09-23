// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_preview_view.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_preview_view_test_api.h"
#include "ash/wm/window_state.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using WindowPreviewViewTest = AshTestBase;

// Creates and returns a widget whose type is the given |type|, which is added
// as a transient child of the given |parent_widget|.
std::unique_ptr<views::Widget> CreateTransientChild(
    views::Widget* parent_widget,
    views::Widget::InitParams::Type type) {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params{
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, type};
  params.bounds = gfx::Rect{40, 50};
  params.context = params.parent = parent_widget->GetNativeWindow();
  params.init_properties_container.SetProperty(chromeos::kAppTypeKey,
                                               chromeos::AppType::ARC_APP);
  widget->Init(std::move(params));
  widget->Show();
  return widget;
}

// Test that if we have two widgets whos windows are linked together by
// transience, WindowPreviewView's internal collection will contain both those
// two windows.
TEST_F(WindowPreviewViewTest, Basic) {
  auto widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  wm::AddTransientChild(widget1->GetNativeWindow(), widget2->GetNativeWindow());
  auto preview_view =
      std::make_unique<WindowPreviewView>(widget1->GetNativeWindow());
  WindowPreviewViewTestApi test_api(preview_view.get());
  EXPECT_EQ(2u, test_api.GetMirrorViews().size());
  EXPECT_TRUE(test_api.GetMirrorViews().contains(widget1->GetNativeWindow()));
  EXPECT_TRUE(test_api.GetMirrorViews().contains(widget2->GetNativeWindow()));
}

// Tests that the preview view preferred size is the same aspect ratio as the
// window's non client view.
TEST_F(WindowPreviewViewTest, AspectRatio) {
  // Default frame header is 32dp, so we expect a window of size 300, 300 to
  // have a preview of 1:1 ratio.
  auto window = CreateAppWindow(gfx::Rect(300, 332));
  auto preview_view = std::make_unique<WindowPreviewView>(window.get());

  const gfx::SizeF preferred_size(preview_view->GetPreferredSize());
  EXPECT_EQ(1.f, preferred_size.width() / preferred_size.height());
}

// Tests that WindowPreviewView behaves as expected when we add or remove
// transient children.
TEST_F(WindowPreviewViewTest, TransientChildAddedAndRemoved) {
  auto widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto widget3 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  ::wm::AddTransientChild(widget1->GetNativeWindow(),
                          widget2->GetNativeWindow());
  auto preview_view =
      std::make_unique<WindowPreviewView>(widget1->GetNativeWindow());
  WindowPreviewViewTestApi test_api(preview_view.get());
  ASSERT_EQ(2u, test_api.GetMirrorViews().size());

  ::wm::AddTransientChild(widget1->GetNativeWindow(),
                          widget3->GetNativeWindow());
  EXPECT_EQ(3u, test_api.GetMirrorViews().size());

  ::wm::RemoveTransientChild(widget1->GetNativeWindow(),
                             widget3->GetNativeWindow());
  EXPECT_EQ(2u, test_api.GetMirrorViews().size());
}

// Tests that init'ing a Widget with a native window as a transient child before
// it is parented to a parent window doesn't cause a crash while the
// WindowPreviewView is observing transient windows additions.
// https://crbug.com/1003544.
TEST_F(WindowPreviewViewTest, NoCrashWithTransientChildWithNoWindowState) {
  auto widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  auto transient_child1 = CreateTransientChild(
      widget1.get(), views::Widget::InitParams::TYPE_WINDOW);

  EXPECT_EQ(widget1->GetNativeWindow(),
            wm::GetTransientParent(transient_child1->GetNativeWindow()));

  auto preview_view =
      std::make_unique<WindowPreviewView>(widget1->GetNativeWindow());
  WindowPreviewViewTestApi test_api(preview_view.get());
  ASSERT_EQ(2u, test_api.GetMirrorViews().size());

  // The popup and bubble transient child should be ignored (they result in a
  // native window type WINDOW_TYPE_POPUP), while the TYPE_WINDOW one will be
  // added as a transient child before it is parented to a container. This
  // should not cause a crash.
  auto transient_child2 = CreateTransientChild(
      widget1.get(), views::Widget::InitParams::TYPE_POPUP);
  auto transient_child3 = CreateTransientChild(
      widget1.get(), views::Widget::InitParams::TYPE_BUBBLE);
  auto transient_child4 = CreateTransientChild(
      widget1.get(), views::Widget::InitParams::TYPE_WINDOW);

  EXPECT_EQ(widget1->GetNativeWindow(),
            wm::GetTransientParent(transient_child2->GetNativeWindow()));
  EXPECT_EQ(widget1->GetNativeWindow(),
            wm::GetTransientParent(transient_child3->GetNativeWindow()));
  EXPECT_EQ(widget1->GetNativeWindow(),
            wm::GetTransientParent(transient_child4->GetNativeWindow()));
  EXPECT_EQ(3u, test_api.GetMirrorViews().size());

  transient_child3.reset();
  EXPECT_EQ(3u, test_api.GetMirrorViews().size());
  transient_child4.reset();
  EXPECT_EQ(2u, test_api.GetMirrorViews().size());
}

// Tests that if cycling stops before a transient popup child is destroyed
// doesn't introduce a crash. https://crbug.com/1014543.
TEST_F(WindowPreviewViewTest,
       NoCrashWhenWindowCyclingIsCanceledWithATransientPopup) {
  auto widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  auto preview_view =
      std::make_unique<WindowPreviewView>(widget1->GetNativeWindow());
  WindowPreviewViewTestApi test_api(preview_view.get());
  ASSERT_EQ(1u, test_api.GetMirrorViews().size());

  auto transient_popup = CreateTransientChild(
      widget1.get(), views::Widget::InitParams::TYPE_POPUP);
  ASSERT_EQ(1u, test_api.GetMirrorViews().size());

  // Simulate canceling window cycling now, there should be no crashes.
  preview_view.reset();
}

TEST_F(WindowPreviewViewTest, LayoutChildWithinParentBounds) {
  UpdateDisplay("1000x900");

  // Create two widgets linked transiently. The child window is within the
  // bounds of the parent window.
  auto widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  WindowState::Get(widget1->GetNativeWindow())
      ->SetBoundsDirectForTesting(gfx::Rect(0, -20, 100, 120));
  widget1->GetNativeWindow()->SetProperty(aura::client::kTopViewInset, 20);
  widget2->GetNativeWindow()->SetBounds(gfx::Rect(20, 20, 50, 50));
  widget2->GetNativeWindow()->SetProperty(aura::client::kTopViewInset, 10);
  ::wm::AddTransientChild(widget1->GetNativeWindow(),
                          widget2->GetNativeWindow());

  // The top inset is excluded from GetUnionRect() calculations.
  auto preview_view =
      std::make_unique<WindowPreviewView>(widget1->GetNativeWindow());
  WindowPreviewViewTestApi test_api(preview_view.get());
  EXPECT_EQ(gfx::RectF(100.f, 100.f), test_api.GetUnionRect());

  // Test that the ratio between the two windows is maintained.
  preview_view->SetBoundsRect(gfx::Rect(500, 500));
  EXPECT_EQ(gfx::Rect(500, 500),
            test_api.GetMirrorViewForWidget(widget1.get())->bounds());
  // The original rect (20, 20, 50, 50) should now be (100, 100, 250, 250).
  // However, the top inset of 10 is now 50, and is excluded from calculations.
  EXPECT_EQ(gfx::Rect(100, 150, 250, 200),
            test_api.GetMirrorViewForWidget(widget2.get())->bounds());
}

// Test that WindowPreviewView layouts the transient tree correctly when each
// transient child is outside the bounds of its transient parent.
TEST_F(WindowPreviewViewTest, LayoutChildOutsideParentBounds) {
  UpdateDisplay("1000x900");

  // Create two widgets linked transiently. The child window is outside of the
  // bounds of the parent window.
  auto widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  WindowState::Get(widget1->GetNativeWindow())
      ->SetBoundsDirectForTesting(gfx::Rect(0, -20, 200, 220));
  widget1->GetNativeWindow()->SetProperty(aura::client::kTopViewInset, 20);
  widget2->GetNativeWindow()->SetBounds(gfx::Rect(300, 300, 100, 100));
  widget2->GetNativeWindow()->SetProperty(aura::client::kTopViewInset, 20);
  ::wm::AddTransientChild(widget1->GetNativeWindow(),
                          widget2->GetNativeWindow());

  // Get the union rect of the two windows. The top inset is excluded from
  // calculations.
  auto preview_view =
      std::make_unique<WindowPreviewView>(widget1->GetNativeWindow());
  WindowPreviewViewTestApi test_api(preview_view.get());
  EXPECT_EQ(gfx::RectF(400.f, 400.f), test_api.GetUnionRect());

  // Test that the ratio between the two windows, relative to the smallest
  // rectangle which encompasses them both (0, 0, 400, 400) is maintained. The
  // first window is positioned at the origin, the second window is positioned
  // such that it is 25% larger and the bottom right corner is aligned with
  // |preview_view|'s bottom right corner.
  preview_view->SetBoundsRect(gfx::Rect(500, 500));
  EXPECT_EQ(gfx::Rect(250, 250),
            test_api.GetMirrorViewForWidget(widget1.get())->bounds());
  EXPECT_EQ(gfx::Rect(375, 400, 125, 100),
            test_api.GetMirrorViewForWidget(widget2.get())->bounds());
}

}  // namespace
}  // namespace ash
