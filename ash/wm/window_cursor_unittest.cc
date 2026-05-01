// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/test/test_frame_view_ash.h"
#include "ash/wm/window_state.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class WindowCursorTest : public AshTestBase {
 public:
  WindowCursorTest() = default;

  WindowCursorTest(const WindowCursorTest&) = delete;
  WindowCursorTest& operator=(const WindowCursorTest&) = delete;

  ~WindowCursorTest() override = default;

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

  // AshTestBase override:
  void TearDown() override {
    window_ = nullptr;
    AshTestBase::TearDown();
  }

  // Returns the current cursor type.
  ui::mojom::CursorType GetCurrentCursorType() const {
    return Shell::Get()->cursor_manager()->GetCursor().type();
  }

  aura::Window* window() { return window_; }

 private:
  raw_ptr<aura::Window> window_;
};

// Test the cursor type based on the mouse's position.
TEST_F(WindowCursorTest, MouseHover) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());

  generator.MoveMouseTo(50, 50);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 0));
  EXPECT_EQ(ui::mojom::CursorType::kNorthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 50);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(200, 100);
  EXPECT_EQ(ui::mojom::CursorType::kSouthEastResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100);
  EXPECT_EQ(ui::mojom::CursorType::kSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 + chromeos::kResizeOutsideBoundsSize - 1);
  EXPECT_EQ(ui::mojom::CursorType::kSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 + chromeos::kResizeOutsideBoundsSize + 10);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 - chromeos::kResizeInsideBoundsSize);
  EXPECT_EQ(ui::mojom::CursorType::kSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 - chromeos::kResizeInsideBoundsSize - 10);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());
}

// For windows that are not resizable, checks for the correct cursor type for
// the cursor position.
TEST_F(WindowCursorTest, MouseHoverOverNonresizable) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());

  // Make the window nonresizable.
  auto* const widget = views::Widget::GetWidgetForNativeWindow(window());
  auto* widget_delegate = widget->widget_delegate();
  widget_delegate->SetCanResize(false);

  generator.MoveMouseTo(gfx::Point(50, 50));
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 0));
  EXPECT_EQ(ui::mojom::CursorType::kNorthSouthNoResize, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 50));
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(199, 99));
  EXPECT_EQ(ui::mojom::CursorType::kNorthWestSouthEastNoResize,
            GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 99));
  EXPECT_EQ(ui::mojom::CursorType::kNorthSouthNoResize, GetCurrentCursorType());

  generator.MoveMouseTo(
      gfx::Point(50, 100 + chromeos::kResizeOutsideBoundsSize - 1));
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(
      gfx::Point(50, 100 + chromeos::kResizeOutsideBoundsSize + 10));
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(
      gfx::Point(50, 100 - chromeos::kResizeInsideBoundsSize));
  EXPECT_EQ(ui::mojom::CursorType::kNorthSouthNoResize, GetCurrentCursorType());

  generator.MoveMouseTo(
      gfx::Point(50, 100 - chromeos::kResizeInsideBoundsSize - 10));
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());
}

TEST_F(WindowCursorTest, DefaultCursorOnBubbleWidgetCorners) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  // Create a dummy view for the bubble, adding it to the window.
  views::View* child_view = views::Widget::GetWidgetForNativeWindow(window())
                                ->GetRootView()
                                ->AddChildView(std::make_unique<views::View>());
  child_view->SetBounds(200, 200, 10, 10);

  // Create the bubble widget.
  auto delegate = std::make_unique<views::BubbleDialogDelegate>(
      /*anchor=*/child_view, views::BubbleBorder::Arrow::NONE);
  views::Widget* bubble = views::BubbleDialogDelegate::CreateBubble(
      delegate.get(), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
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

  bubble->CloseNow();
}

TEST_F(WindowCursorTest, NoResizeShadowOnNonToplevelWindow) {
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
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 0));
  EXPECT_EQ(ui::mojom::CursorType::kNorthResize, GetCurrentCursorType());
}

// Test that the cursor stays the same as long as a user is resizing a window.
TEST_F(WindowCursorTest, MouseDrag) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());

  generator.MoveMouseTo(200, 50);
  generator.PressLeftButton();
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());

  generator.MoveMouseTo(210, 50);
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());

  generator.ReleaseLeftButton();
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());
}

TEST_F(WindowCursorTest, MaximizeRestore) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(WindowState::Get(window())->IsNormalStateType());

  generator.MoveMouseTo(200, 50);
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());
  generator.MoveMouseTo(200 - chromeos::kResizeInsideBoundsSize, 50);
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());

  WindowState::Get(window())->Maximize();
  gfx::Rect bounds(window()->GetBoundsInRootWindow());
  gfx::Point right_center(bounds.right() - 1,
                          (bounds.y() + bounds.bottom()) / 2);
  generator.MoveMouseTo(right_center);
  EXPECT_EQ(ui::mojom::CursorType::kNull, GetCurrentCursorType());

  WindowState::Get(window())->Restore();
  generator.MoveMouseTo(200, 50);
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());
  generator.MoveMouseTo(200 - chromeos::kResizeInsideBoundsSize, 50);
  EXPECT_EQ(ui::mojom::CursorType::kEastResize, GetCurrentCursorType());
}

}  // namespace ash
