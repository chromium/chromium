// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/cursor_manager_test_api.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_state.h"
#include "base/bind.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// views::WidgetDelegate which uses ash::NonClientFrameViewAsh.
class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() = default;
  ~TestWidgetDelegate() override = default;

  // views::WidgetDelegateView overrides:
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    return new NonClientFrameViewAsh(widget);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

// The test tests that the mouse cursor is changed and that the resize shadows
// are shown when the mouse is hovered over the window edge.
class ResizeShadowAndCursorTest : public AshTestBase {
 public:
  ResizeShadowAndCursorTest() = default;
  ~ResizeShadowAndCursorTest() override = default;

  // AshTestBase override:
  void SetUp() override {
    AshTestBase::SetUp();

    views::Widget* widget(views::Widget::CreateWindowWithContextAndBounds(
        new TestWidgetDelegate(), CurrentContext(), gfx::Rect(0, 0, 200, 100)));
    widget->Show();
    window_ = widget->GetNativeView();

    // Add a child window to |window_| in order to properly test that the resize
    // handles and the resize shadows are shown when the mouse is
    // ash::kResizeInsideBoundsSize inside of |window_|'s edges.
    aura::Window* child =
        CreateTestWindowInShell(SK_ColorWHITE, 0, gfx::Rect(0, 10, 200, 90));
    window_->AddChild(child);
  }

  const ResizeShadow* GetShadow() const {
    return Shell::Get()->resize_shadow_controller()->GetShadowForWindowForTest(
        window_);
  }

  // Returns the hit test code if there is a resize shadow. Returns HTNOWHERE if
  // there is no resize shadow.
  int ResizeShadowHitTest() const {
    auto* resize_shadow = GetShadow();
    return resize_shadow ? resize_shadow->GetLastHitTestForTest() : HTNOWHERE;
  }

  // Returns true if there is a resize shadow.
  void VerifyResizeShadow(bool visible) const {
    if (visible)
      EXPECT_TRUE(GetShadow());
    if (GetShadow()) {
      const ui::Layer* shadow_layer = GetShadow()->GetLayerForTest();
      EXPECT_EQ(visible, shadow_layer->GetTargetVisibility());
      ASSERT_TRUE(window_->layer());
      EXPECT_EQ(window_->layer()->parent(), shadow_layer->parent());
      const auto& layers = shadow_layer->parent()->children();
      // Make sure the shadow layer is stacked directly beneath the window
      // layer.
      EXPECT_EQ(*(std::find(layers.begin(), layers.end(), shadow_layer) + 1),
                window_->layer());
    }
  }

  // Returns the current cursor type.
  ui::CursorType GetCurrentCursorType() const {
    CursorManagerTestApi test_api(Shell::Get()->cursor_manager());
    return test_api.GetCurrentCursor().native_type();
  }

  // Called for each step of a scroll sequence initiated at the bottom right
  // corner of |window_|. Tests whether the resize shadow is shown.
  void ProcessBottomRightResizeGesture(ui::EventType type,
                                       const gfx::Vector2dF& delta) {
    if (type == ui::ET_GESTURE_SCROLL_END) {
      // After gesture scroll ends, there should be no resize shadow.
      VerifyResizeShadow(false);
    } else {
      VerifyResizeShadow(true);
      EXPECT_EQ(HTBOTTOMRIGHT, ResizeShadowHitTest());
    }
  }

  aura::Window* window() { return window_; }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ResizeShadowAndCursorTest);
};

// Test whether the resize shadows are visible and the cursor type based on the
// mouse's position.
TEST_F(ResizeShadowAndCursorTest, MouseHover) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(ash::WindowState::Get(window())->IsNormalStateType());

  generator.MoveMouseTo(50, 50);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 0));
  VerifyResizeShadow(true);
  EXPECT_EQ(HTTOP, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kNorthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 50);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(200, 100);
  VerifyResizeShadow(true);
  EXPECT_EQ(HTBOTTOMRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kSouthEastResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100);
  VerifyResizeShadow(true);
  EXPECT_EQ(HTBOTTOM, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 + ash::kResizeOutsideBoundsSize - 1);
  VerifyResizeShadow(true);
  EXPECT_EQ(HTBOTTOM, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 + ash::kResizeOutsideBoundsSize + 10);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 - ash::kResizeInsideBoundsSize);
  VerifyResizeShadow(true);
  EXPECT_EQ(HTBOTTOM, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 - ash::kResizeInsideBoundsSize - 10);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::CursorType::kNull, GetCurrentCursorType());
}

// Test that the resize shadows stay visible and that the cursor stays the same
// as long as a user is resizing a window.
TEST_F(ResizeShadowAndCursorTest, MouseDrag) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(ash::WindowState::Get(window())->IsNormalStateType());
  gfx::Size initial_size(window()->bounds().size());

  generator.MoveMouseTo(200, 50);
  generator.PressLeftButton();
  VerifyResizeShadow(true);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kEastResize, GetCurrentCursorType());

  generator.MoveMouseTo(210, 50);
  VerifyResizeShadow(true);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kEastResize, GetCurrentCursorType());

  generator.ReleaseLeftButton();
  VerifyResizeShadow(true);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kEastResize, GetCurrentCursorType());

  gfx::Size new_size(window()->bounds().size());
  EXPECT_NE(new_size.ToString(), initial_size.ToString());
}

// Test that the resize shadows stay visible while resizing a window via touch.
TEST_F(ResizeShadowAndCursorTest, Touch) {
  ASSERT_TRUE(ash::WindowState::Get(window())->IsNormalStateType());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  int start_x = 200 + ash::kResizeOutsideBoundsSize - 1;
  int start_y = 100 + ash::kResizeOutsideBoundsSize - 1;
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(start_x, start_y), gfx::Point(start_x + 50, start_y + 50),
      base::TimeDelta::FromMilliseconds(200), 3,
      base::BindRepeating(
          &ResizeShadowAndCursorTest::ProcessBottomRightResizeGesture,
          base::Unretained(this)));
}

// Test that the resize shadows are not visible and that the default cursor is
// used when the window is maximized.
TEST_F(ResizeShadowAndCursorTest, MaximizeRestore) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(ash::WindowState::Get(window())->IsNormalStateType());

  generator.MoveMouseTo(200, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kEastResize, GetCurrentCursorType());
  generator.MoveMouseTo(200 - ash::kResizeInsideBoundsSize, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kEastResize, GetCurrentCursorType());

  ash::WindowState::Get(window())->Maximize();
  gfx::Rect bounds(window()->GetBoundsInRootWindow());
  gfx::Point right_center(bounds.right() - 1,
                          (bounds.y() + bounds.bottom()) / 2);
  generator.MoveMouseTo(right_center);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::CursorType::kNull, GetCurrentCursorType());

  ash::WindowState::Get(window())->Restore();
  generator.MoveMouseTo(200, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kEastResize, GetCurrentCursorType());
  generator.MoveMouseTo(200 - ash::kResizeInsideBoundsSize, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::CursorType::kEastResize, GetCurrentCursorType());
}

// Verifies that the shadow hides when a window is minimized. Regression test
// for crbug.com/752583
TEST_F(ResizeShadowAndCursorTest, Minimize) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(ash::WindowState::Get(window())->IsNormalStateType());

  generator.MoveMouseTo(200, 50);
  VerifyResizeShadow(true);

  ash::WindowState::Get(window())->Minimize();
  VerifyResizeShadow(false);

  ash::WindowState::Get(window())->Restore();
  VerifyResizeShadow(false);
}

}  // namespace ash
