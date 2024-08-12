// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/test/test_non_client_frame_view_ash.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/frame/multitask_menu/multitask_button.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/cursor_manager.h"

using chromeos::kResizeInsideBoundsSize;
using chromeos::kResizeOutsideBoundsSize;

namespace ash {

// The test tests that the mouse cursor is changed and that the resize shadows
// are shown when the mouse is hovered over the window edge.
class ResizeShadowAndCursorTest : public AshTestBase {
 public:
  ResizeShadowAndCursorTest() = default;

  ResizeShadowAndCursorTest(const ResizeShadowAndCursorTest&) = delete;
  ResizeShadowAndCursorTest& operator=(const ResizeShadowAndCursorTest&) =
      delete;

  ~ResizeShadowAndCursorTest() override = default;

  // AshTestBase override:
  void SetUp() override {
    AshTestBase::SetUp();

    views::Widget* widget = views::Widget::CreateWindowWithContext(
        new TestWidgetDelegateAsh(), GetContext(), gfx::Rect(0, 0, 200, 100));
    widget->Show();
    window_ = widget->GetNativeView();

    // Add a child window to |window_| in order to properly test that the resize
    // handles and the resize shadows are shown when the mouse is
    // ash::kResizeInsideBoundsSize inside of |window_|'s edges.
    aura::Window* child = TestWindowBuilder()
                              .SetColorWindowDelegate(SK_ColorWHITE)
                              .SetBounds(gfx::Rect(0, 10, 200, 90))
                              .Build()
                              .release();
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

  // Returns true if there is a resize shadow with a given type. Default type is
  // unlock for compatibility.
  void VerifyResizeShadow(bool visible) const {
    if (visible)
      EXPECT_TRUE(GetShadow());
    if (GetShadow()) {
      const ui::Layer* shadow_layer = GetShadow()->GetLayerForTest();
      EXPECT_EQ(visible, shadow_layer->GetTargetVisibility());
      ASSERT_TRUE(window_->layer());
      EXPECT_EQ(window_->layer()->parent(), shadow_layer->parent());
      const auto& layers = shadow_layer->parent()->children();
      // We don't care about the layer order if it's invisible.
      if (visible) {
        // Make sure the shadow layer is stacked directly beneath the window
        // layer.
        EXPECT_EQ(*(base::ranges::find(layers, shadow_layer) + 1),
                  window_->layer());
      }
    }
  }

  // Returns the current cursor type.
  ui::mojom::CursorType GetCurrentCursorType() const {
    return Shell::Get()->cursor_manager()->GetCursor().type();
  }

  // Called for each step of a scroll sequence initiated at the bottom right
  // corner of |window_|. Tests whether the resize shadow is shown.
  void ProcessBottomRightResizeGesture(ui::EventType type,
                                       const gfx::Vector2dF& delta) {
    if (type == ui::EventType::kGestureScrollEnd) {
      // After gesture scroll ends, there should be no resize shadow.
      VerifyResizeShadow(false);
    } else {
      VerifyResizeShadow(true);
      EXPECT_EQ(HTBOTTOMRIGHT, ResizeShadowHitTest());
    }
  }

  aura::Window* window() { return window_; }

 private:
  raw_ptr<aura::Window, DanglingUntriaged> window_;
};

// Test whether the resize shadows are visible and the cursor type based on the
// mouse's position.
TEST_F(ResizeShadowAndCursorTest, MouseHover) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());

  generator.MoveMouseTo(50, 50);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 0));
  VerifyResizeShadow(true);
  EXPECT_EQ(HTTOP, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kNorthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 50);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(200, 100);
  VerifyResizeShadow(true);
  EXPECT_EQ(HTBOTTOMRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kSouthEastResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100);
  VerifyResizeShadow(true);
  EXPECT_EQ(HTBOTTOM, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 + kResizeOutsideBoundsSize - 1);
  VerifyResizeShadow(true);
  EXPECT_EQ(HTBOTTOM, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 + kResizeOutsideBoundsSize + 10);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 - kResizeInsideBoundsSize);
  VerifyResizeShadow(true);
  EXPECT_EQ(HTBOTTOM, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 - kResizeInsideBoundsSize - 10);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());
}

// For windows that are not resizable, checks that there is no resize shadow,
// and for the correct cursor type for the cursor position.
TEST_F(ResizeShadowAndCursorTest, MouseHoverOverNonresizable) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());

  // Make the window nonresizable.
  auto* const widget = views::Widget::GetWidgetForNativeWindow(window());
  auto* widget_delegate = widget->widget_delegate();
  widget_delegate->SetCanResize(false);

  generator.MoveMouseTo(gfx::Point(50, 50));
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 0));
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNorthSouthNoResize, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 50));
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(199, 99));
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNorthWestSouthEastNoResize,
            GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 99));
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNorthSouthNoResize, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 100 + kResizeOutsideBoundsSize - 1));
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 100 + kResizeOutsideBoundsSize + 10));
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 100 - kResizeInsideBoundsSize));
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNorthSouthNoResize, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 100 - kResizeInsideBoundsSize - 10));
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());
}

TEST_F(ResizeShadowAndCursorTest, DefaultCursorOnBubbleWidgetCorners) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  // Create a dummy view for the bubble, adding it to the window.
  views::View* child_view = new views::View();
  child_view->SetBounds(200, 200, 10, 10);
  views::Widget::GetWidgetForNativeWindow(window())
      ->GetRootView()
      ->AddChildView(child_view);

  // Create the bubble widget.
  views::Widget* bubble(views::BubbleDialogDelegateView::CreateBubble(
      new views::BubbleDialogDelegateView(child_view,
                                          views::BubbleBorder::NONE)));
  bubble->Show();

  // Get the screen rectangle for the bubble frame
  const gfx::Rect bounds = bubble->GetNativeView()->GetBoundsInScreen();
  EXPECT_THAT(
      bounds,
      ::testing::AllOf(::testing::Property(&gfx::Rect::x, ::testing::Gt(0)),
                       ::testing::Property(&gfx::Rect::y, ::testing::Gt(0))));

  // The cursor at the frame corners should be the default cursor.
  generator.MoveMouseTo(bounds.origin());
  EXPECT_THAT(GetCurrentCursorType(),
              ::testing::Eq(ui::mojom::CursorType::kNull));

  generator.MoveMouseTo(bounds.top_right());
  EXPECT_THAT(GetCurrentCursorType(),
              ::testing::Eq(ui::mojom::CursorType::kNull));

  generator.MoveMouseTo(bounds.bottom_left());
  EXPECT_THAT(GetCurrentCursorType(),
              ::testing::Eq(ui::mojom::CursorType::kNull));

  generator.MoveMouseTo(bounds.bottom_right());
  EXPECT_THAT(GetCurrentCursorType(),
              ::testing::Eq(ui::mojom::CursorType::kNull));
}

TEST_F(ResizeShadowAndCursorTest, NoResizeShadowOnNonToplevelWindow) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());

  auto* embedded = views::Widget::CreateWindowWithContext(
      new TestWidgetDelegateAsh(), GetContext(), gfx::Rect(0, 0, 100, 100));
  embedded->Show();
  window()->AddChild(embedded->GetNativeWindow());

  embedded->GetNativeWindow()->SetName("BBB");
  window()->SetName("AAAA");
  embedded->SetBounds(gfx::Rect(10, 10, 100, 100));

  generator.MoveMouseTo(50, 11);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  EXPECT_FALSE(
      Shell::Get()->resize_shadow_controller()->GetShadowForWindowForTest(
          embedded->GetNativeWindow()));

  generator.MoveMouseTo(gfx::Point(50, 0));
  VerifyResizeShadow(true);
  EXPECT_EQ(HTTOP, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kNorthResize, GetCurrentCursorType());

  EXPECT_FALSE(
      Shell::Get()->resize_shadow_controller()->GetShadowForWindowForTest(
          embedded->GetNativeWindow()));
}

// Test that the resize shadows stay visible and that the cursor stays the same
// as long as a user is resizing a window.
TEST_F(ResizeShadowAndCursorTest, MouseDrag) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());
  gfx::Size initial_size(window()->bounds().size());

  generator.MoveMouseTo(200, 50);
  generator.PressLeftButton();
  VerifyResizeShadow(true);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());

  generator.MoveMouseTo(210, 50);
  VerifyResizeShadow(true);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());

  generator.ReleaseLeftButton();
  VerifyResizeShadow(true);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());

  gfx::Size new_size(window()->bounds().size());
  EXPECT_NE(new_size.ToString(), initial_size.ToString());
}

// Test that the resize shadows stay visible while resizing a window via touch.
TEST_F(ResizeShadowAndCursorTest, Touch) {
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  int start_x = 200 + kResizeOutsideBoundsSize - 1;
  int start_y = 100 + kResizeOutsideBoundsSize - 1;
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(start_x, start_y), gfx::Point(start_x + 50, start_y + 50),
      base::Milliseconds(200), 3,
      base::BindRepeating(
          &ResizeShadowAndCursorTest::ProcessBottomRightResizeGesture,
          base::Unretained(this)));
}

// Test that the resize shadows are not visible and that the default cursor is
// used when the window is maximized.
TEST_F(ResizeShadowAndCursorTest, MaximizeRestore) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());

  generator.MoveMouseTo(200, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());
  generator.MoveMouseTo(200 - kResizeInsideBoundsSize, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());

  WindowState::Get(window())->Maximize();
  gfx::Rect bounds(window()->GetBoundsInRootWindow());
  gfx::Point right_center(bounds.right() - 1,
                          (bounds.y() + bounds.bottom()) / 2);
  generator.MoveMouseTo(right_center);
  VerifyResizeShadow(false);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  WindowState::Get(window())->Restore();
  generator.MoveMouseTo(200, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());
  generator.MoveMouseTo(200 - kResizeInsideBoundsSize, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());
}

// Verifies that the shadow hides when a window is minimized. Regression test
// for crbug.com/752583
TEST_F(ResizeShadowAndCursorTest, Minimize) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());

  generator.MoveMouseTo(200, 50);
  VerifyResizeShadow(true);

  WindowState::Get(window())->Minimize();
  VerifyResizeShadow(false);

  WindowState::Get(window())->Restore();
  VerifyResizeShadow(false);
}

// Verifies that the lock style shadow gets updated when the window's bounds
// changed.
TEST_F(ResizeShadowAndCursorTest, LockShadowBounds) {
  window()->SetProperty(kResizeShadowTypeKey, ResizeShadowType::kLock);
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());
  // Set window's bounds
  const gfx::Rect kOldBounds(20, 30, 400, 300);
  window()->SetBounds(kOldBounds);
  auto* resize_shadow = GetShadow();
  ASSERT_TRUE(resize_shadow);
  VerifyResizeShadow(true);
  auto* layer = resize_shadow->GetLayerForTest();
  constexpr int kVisualThickness = 6;
  EXPECT_EQ(gfx::Rect(kOldBounds.width() + kVisualThickness * 2,
                      kOldBounds.height() + kVisualThickness * 2)
                .ToString(),
            gfx::Rect(layer->GetTargetBounds().size()).ToString());

  // Change the window's bounds, the shadow's should be updated too.
  gfx::Rect kNewBounds(50, 60, 500, 400);
  window()->SetBounds(kNewBounds);
  EXPECT_EQ(gfx::Rect(kNewBounds.width() + kVisualThickness * 2,
                      kNewBounds.height() + kVisualThickness * 2)
                .ToString(),
            gfx::Rect(layer->GetTargetBounds().size()).ToString());
}

// Tests that shadow gets updated according to the window's visibility.
TEST_F(ResizeShadowAndCursorTest, ShowHideLockShadow) {
  ASSERT_FALSE(GetShadow());
  window()->SetProperty(kResizeShadowTypeKey, ResizeShadowType::kLock);

  // Test shown window.
  window()->Show();
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());
  ASSERT_TRUE(GetShadow());
  VerifyResizeShadow(true);
  Shell::Get()->resize_shadow_controller()->HideShadow(window());
  VerifyResizeShadow(false);
  Shell::Get()->resize_shadow_controller()->TryShowAllShadows();
  VerifyResizeShadow(true);
  Shell::Get()->resize_shadow_controller()->HideAllShadows();
  VerifyResizeShadow(false);

  // Test hidden window.
  window()->Hide();
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());
  VerifyResizeShadow(false);
  Shell::Get()->resize_shadow_controller()->HideShadow(window());
  VerifyResizeShadow(false);
  Shell::Get()->resize_shadow_controller()->TryShowAllShadows();
  VerifyResizeShadow(false);
  Shell::Get()->resize_shadow_controller()->HideAllShadows();
  VerifyResizeShadow(false);
}

// Tests that shadow gets updated when the window's visibility changed.
TEST_F(ResizeShadowAndCursorTest, WindowVisibilityChange) {
  ASSERT_FALSE(GetShadow());

  window()->SetProperty(kResizeShadowTypeKey, ResizeShadowType::kLock);
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());
  ASSERT_TRUE(GetShadow());
  window()->Show();
  VerifyResizeShadow(true);
  window()->Hide();
  VerifyResizeShadow(false);
  window()->Show();
  VerifyResizeShadow(true);
}

// Tests that shadow type gets updated according to the window's property.
TEST_F(ResizeShadowAndCursorTest, ResizeShadowTypeChange) {
  ASSERT_FALSE(GetShadow());

  window()->SetProperty(kResizeShadowTypeKey, ResizeShadowType::kLock);
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());
  ASSERT_TRUE(GetShadow());
  ASSERT_EQ(GetShadow()->GetResizeShadowTypeForTest(), ResizeShadowType::kLock);
  Shell::Get()->resize_shadow_controller()->HideShadow(window());

  window()->SetProperty(kResizeShadowTypeKey, ResizeShadowType::kUnlock);
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());
  ASSERT_EQ(GetShadow()->GetResizeShadowTypeForTest(),
            ResizeShadowType::kUnlock);
  Shell::Get()->resize_shadow_controller()->HideShadow(window());
}

// Tests that resize shadow matches window rounded corners.
TEST_F(ResizeShadowAndCursorTest, ResizeShadowMatchesWindowRoundness) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {chromeos::features::kRoundedWindows,
       chromeos::features::kFeatureManagementRoundedWindows},
      /*disabled_features=*/{});

  ASSERT_FALSE(GetShadow());
  WindowState* window_state = WindowState::Get(window());
  ASSERT_TRUE(window_state->IsNormalStateType());

  window()->SetProperty(kResizeShadowTypeKey, ResizeShadowType::kLock);
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());

  // For normal window state, top-level windows have rounded window.
  EXPECT_TRUE(GetShadow()->is_for_rounded_window());
  VerifyResizeShadow(true);

  // Window in snapped state does not have rounded corners, therefore the resize
  // shadow should adjust accordingly.
  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_event);

  ASSERT_TRUE(window_state->IsSnapped());
  EXPECT_FALSE(GetShadow()->is_for_rounded_window());
  VerifyResizeShadow(true);

  window_state->Restore();

  ASSERT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(GetShadow()->is_for_rounded_window());
  VerifyResizeShadow(true);

  // Ensure that shadow variant is correct after restoring from a state that has
  // invisible resize shadow.
  window_state->Maximize();
  VerifyResizeShadow(false);

  window_state->Restore();
  ASSERT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(GetShadow()->is_for_rounded_window());
}

// Tests that shadow gets updated when the window's state changed.
TEST_F(ResizeShadowAndCursorTest, WindowStateChange) {
  ASSERT_FALSE(GetShadow());
  auto* const window_state = WindowState::Get(window());
  ASSERT_TRUE(window_state->IsNormalStateType());

  window()->SetProperty(kResizeShadowTypeKey, ResizeShadowType::kLock);
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());
  VerifyResizeShadow(true);
  window_state->Maximize();
  VerifyResizeShadow(false);
  window_state->Restore();
  VerifyResizeShadow(true);
  window_state->Minimize();
  VerifyResizeShadow(false);
  window_state->Unminimize();
  VerifyResizeShadow(true);
}

// Tests that shadow gets hidden and restored according to the oveview mode
// state.
TEST_F(ResizeShadowAndCursorTest, OverviewModeChange) {
  ASSERT_FALSE(GetShadow());

  window()->SetProperty(kResizeShadowTypeKey, ResizeShadowType::kLock);

  // Requests ShowShadow() before entering overview.
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());
  ASSERT_TRUE(GetShadow());
  window()->Show();
  VerifyResizeShadow(true);
  EnterOverview();
  VerifyResizeShadow(false);
  ExitOverview();
  VerifyResizeShadow(true);
  Shell::Get()->resize_shadow_controller()->HideAllShadows();

  // Requests ShowShadow() after entering overview.
  EnterOverview();
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());
  VerifyResizeShadow(false);
  ExitOverview();
  VerifyResizeShadow(true);
}

// Tests that the code does not break when we unparent a window with a shadow
// and then try to rearrange the hierarchy. (b/257306979)
TEST_F(ResizeShadowAndCursorTest, ShadowCanExistInUnparentedWindow) {
  ASSERT_FALSE(GetShadow());
  window()->SetProperty(kResizeShadowTypeKey, ResizeShadowType::kLock);
  auto* controller = Shell::Get()->resize_shadow_controller();
  controller->ShowShadow(window());
  ASSERT_TRUE(GetShadow());
  window()->Show();
  auto* parent = window()->parent();
  parent->RemoveChild(window());

  // Previously this would break the code because a hierarchy change on an
  // unparented window with a shadow would try to use the window layer's null
  // parent layer to reparent the shadow.
  ASSERT_TRUE(parent);
  parent->AddChild(window());
}

// Tests that the resize shadow will observe a new color provider source when
// the window is reparented to other root windows.
TEST_F(ResizeShadowAndCursorTest, NoCrashOnRootWindowChange) {
  // Create a resize shadow on primary root window.
  Shell::Get()->resize_shadow_controller()->ShowShadow(window());
  // The color provider source of primary root window should be observed by
  // resize shadow.
  EXPECT_TRUE(Shell::GetPrimaryRootWindowController()
                  ->color_provider_source()
                  ->observers_for_testing()
                  .HasObserver(GetShadow()));

  // Add an secondary display.
  display_manager()->AddRemoveDisplay();
  aura::Window* secondary_root = nullptr;
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    if (root != Shell::GetPrimaryRootWindow()) {
      secondary_root = root;
      break;
    }
  }
  EXPECT_TRUE(!!secondary_root);

  // Move the window to secondary display and the resize shadow should observe
  // the color provider source of secondary root window.
  Shell::GetContainer(secondary_root, desks_util::GetActiveDeskContainerId())
      ->AddChild(window());
  EXPECT_TRUE(RootWindowController::ForWindow(secondary_root)
                  ->color_provider_source()
                  ->observers_for_testing()
                  .HasObserver(GetShadow()));

  // Remove secondary root window. The window will be reparent to primary
  // display, the resize shadow will observe the color provider source of
  // primary root window, and there should be no crash.
  display_manager()->AddRemoveDisplay();
  EXPECT_TRUE(Shell::GetPrimaryRootWindowController()
                  ->color_provider_source()
                  ->observers_for_testing()
                  .HasObserver(GetShadow()));
}

// Tests if the resize shadow is beneath window when float the window by
// multi-task menu.
TEST_F(ResizeShadowAndCursorTest, KeepShadowBeneathFloatWindow) {
  // Create a resizable test window whose size should be larger than the normal
  // floating window size.
  auto test_window =
      CreateAppWindow(/*bounds_in_screen=*/gfx::Rect(10, 10, 800, 600));

  // Create a resize shadow for the native window.
  auto* resize_shadow_controller = Shell::Get()->resize_shadow_controller();
  resize_shadow_controller->ShowShadow(test_window.get());
  auto* resize_shadow =
      resize_shadow_controller->GetShadowForWindowForTest(test_window.get());
  EXPECT_TRUE(resize_shadow);

  // Open multi-task menu by hovering on the resize button.
  chromeos::MultitaskMenu* multitask_menu =
      ShowAndWaitMultitaskMenuForWindow(test_window.get());
  ASSERT_TRUE(multitask_menu);

  // Click on the floating window option.
  ui::ScopedAnimationDurationScaleMode zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  chromeos::MultitaskMenuView* menu_view =
      multitask_menu->multitask_menu_view();
  LeftClickOn(chromeos::MultitaskMenuViewTestApi(menu_view).GetFloatButton());
  EXPECT_TRUE(WindowState::Get(test_window.get())->IsFloated());

  // Check if the resize shadow layer is beneath the window layer. We check
  // their positions in their parent layer's children container. The highest
  // index is the topmost.
  auto* shadow_layer = resize_shadow->GetLayerForTest();
  auto parent_children = shadow_layer->parent()->children();
  auto* window_layer = test_window->layer();

  auto shadow_iter = base::ranges::find(parent_children, shadow_layer);
  auto window_iter = base::ranges::find(parent_children, window_layer);
  EXPECT_LT(std::distance(parent_children.begin(), shadow_iter),
            std::distance(parent_children.begin(), window_iter));
}

}  // namespace ash
