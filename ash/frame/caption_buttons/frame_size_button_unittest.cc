// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/caption_buttons/frame_size_button.h"

#include "ash/frame/ash_frame_caption_controller.h"
#include "ash/public/cpp/ash_layout_constants.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/i18n/rtl.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  explicit TestWidgetDelegate(bool resizable) : resizable_(resizable) {}
  ~TestWidgetDelegate() override = default;

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override { return resizable_; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }

  ash::FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

 private:
  // Overridden from views::View:
  void Layout() override {
    caption_button_container_->Layout();

    // Right align the caption button container.
    gfx::Size preferred_size = caption_button_container_->GetPreferredSize();
    caption_button_container_->SetBounds(width() - preferred_size.width(), 0,
                                         preferred_size.width(),
                                         preferred_size.height());
  }

  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    if (details.is_add && details.child == this) {
      caption_button_container_ = new FrameCaptionButtonContainerView(
          GetWidget(), &caption_controller_);

      // Set arbitrary images for the button icons and assign the default
      // caption button size.
      caption_button_container_->SetButtonSize(
          GetAshLayoutSize(AshLayoutSize::kNonBrowserCaption));
      for (int icon = 0; icon < CAPTION_BUTTON_ICON_COUNT; ++icon) {
        caption_button_container_->SetButtonImage(
            static_cast<CaptionButtonIcon>(icon), kWindowControlCloseIcon);
      }

      AddChildView(caption_button_container_);
    }
  }

  // Not owned.
  ash::FrameCaptionButtonContainerView* caption_button_container_;
  AshFrameCaptionController caption_controller_;
  bool resizable_;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

class FrameSizeButtonTest : public AshTestBase {
 public:
  FrameSizeButtonTest() = default;
  explicit FrameSizeButtonTest(bool resizable) : resizable_(resizable) {}
  ~FrameSizeButtonTest() override = default;

  // Returns the center point of |view| in screen coordinates.
  gfx::Point CenterPointInScreen(views::View* view) {
    return view->GetBoundsInScreen().CenterPoint();
  }

  // Returns true if the window has |state_type|.
  bool HasStateType(mojom::WindowStateType state_type) const {
    return window_state()->GetStateType() == state_type;
  }

  // Returns true if all three buttons are in the normal state.
  bool AllButtonsInNormalState() const {
    return minimize_button_->state() == views::Button::STATE_NORMAL &&
           size_button_->state() == views::Button::STATE_NORMAL &&
           close_button_->state() == views::Button::STATE_NORMAL;
  }

  // Creates a widget with |delegate|. The returned widget takes ownership of
  // |delegate|.
  views::Widget* CreateWidget(views::WidgetDelegate* delegate) {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.delegate = delegate;
    params.bounds = gfx::Rect(10, 10, 100, 100);
    params.context = CurrentContext();
    widget->Init(params);
    widget->Show();

    return widget;
  }

  // AshTestBase overrides:
  void SetUp() override {
    AshTestBase::SetUp();

    TestWidgetDelegate* delegate = new TestWidgetDelegate(resizable_);
    window_state_ =
        ash::wm::GetWindowState(CreateWidget(delegate)->GetNativeWindow());

    FrameCaptionButtonContainerView::TestApi test(
        delegate->caption_button_container());

    minimize_button_ = test.minimize_button();
    size_button_ = test.size_button();
    static_cast<FrameSizeButton*>(size_button_)
        ->set_delay_to_set_buttons_to_snap_mode(0);
    close_button_ = test.close_button();
  }

  ash::wm::WindowState* window_state() { return window_state_; }
  const ash::wm::WindowState* window_state() const { return window_state_; }

  FrameCaptionButton* minimize_button() { return minimize_button_; }
  FrameCaptionButton* size_button() { return size_button_; }
  FrameCaptionButton* close_button() { return close_button_; }

 private:
  // Not owned.
  ash::wm::WindowState* window_state_;
  FrameCaptionButton* minimize_button_;
  FrameCaptionButton* size_button_;
  FrameCaptionButton* close_button_;
  bool resizable_ = true;

  DISALLOW_COPY_AND_ASSIGN(FrameSizeButtonTest);
};

}  // namespace

// Tests that pressing the left mouse button or tapping down on the size button
// puts the button into the pressed state.
TEST_F(FrameSizeButtonTest, PressedState) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  generator->ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->state());

  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressTouchId(3);
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  generator->ReleaseTouchId(3);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->state());
}

// Tests that clicking on the size button toggles between the maximized and
// normal state.
TEST_F(FrameSizeButtonTest, ClickSizeButtonTogglesMaximize) {
  EXPECT_FALSE(window_state()->IsMaximized());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->ClickLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(window_state()->IsMaximized());

  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->ClickLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(window_state()->IsMaximized());

  generator->GestureTapAt(CenterPointInScreen(size_button()));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(window_state()->IsMaximized());

  generator->GestureTapAt(CenterPointInScreen(size_button()));
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(window_state()->IsMaximized());
}

// Test that clicking + dragging to a button adjacent to the size button snaps
// the window left or right.
TEST_F(FrameSizeButtonTest, ButtonDrag) {
  EXPECT_TRUE(window_state()->IsNormalStateType());

  // 1) Test by dragging the mouse.
  // Snap right.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  generator->MoveMouseTo(CenterPointInScreen(close_button()));
  generator->ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(HasStateType(mojom::WindowStateType::RIGHT_SNAPPED));

  // Snap left.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  generator->ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(HasStateType(mojom::WindowStateType::LEFT_SNAPPED));

  // 2) Test with scroll gestures.
  // Snap right.
  generator->GestureScrollSequence(CenterPointInScreen(size_button()),
                                   CenterPointInScreen(close_button()),
                                   base::TimeDelta::FromMilliseconds(100), 3);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(HasStateType(mojom::WindowStateType::RIGHT_SNAPPED));

  // Snap left.
  generator->GestureScrollSequence(CenterPointInScreen(size_button()),
                                   CenterPointInScreen(minimize_button()),
                                   base::TimeDelta::FromMilliseconds(100), 3);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(HasStateType(mojom::WindowStateType::LEFT_SNAPPED));

  // 3) Test with tap gestures.
  const float touch_default_radius =
      ui::GestureConfiguration::GetInstance()->default_radius();
  ui::GestureConfiguration::GetInstance()->set_default_radius(0);
  // Snap right.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressMoveAndReleaseTouchTo(CenterPointInScreen(close_button()));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(HasStateType(mojom::WindowStateType::RIGHT_SNAPPED));
  // Snap left.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressMoveAndReleaseTouchTo(CenterPointInScreen(minimize_button()));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(HasStateType(mojom::WindowStateType::LEFT_SNAPPED));
  ui::GestureConfiguration::GetInstance()->set_default_radius(
      touch_default_radius);
}

// Test that clicking, dragging, and overshooting the minimize button a bit
// horizontally still snaps the window left.
TEST_F(FrameSizeButtonTest, SnapLeftOvershootMinimize) {
  EXPECT_TRUE(window_state()->IsNormalStateType());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));

  generator->PressLeftButton();
  // Move to the minimize button.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  // Overshoot the minimize button.
  generator->MoveMouseBy(-minimize_button()->width(), 0);
  generator->ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(HasStateType(mojom::WindowStateType::LEFT_SNAPPED));
}

// Test that right clicking the size button has no effect.
TEST_F(FrameSizeButtonTest, RightMouseButton) {
  EXPECT_TRUE(window_state()->IsNormalStateType());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressRightButton();
  generator->ReleaseRightButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(window_state()->IsNormalStateType());
}

// Test that upon releasing the mouse button after having pressed the size
// button
// - The state of all the caption buttons is reset.
// - The icon displayed by all of the caption buttons is reset.
TEST_F(FrameSizeButtonTest, ResetButtonsAfterClick) {
  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());
  EXPECT_EQ(CAPTION_BUTTON_ICON_LEFT_SNAPPED, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_RIGHT_SNAPPED, close_button()->icon());

  // Dragging the mouse over the minimize button should hover the minimize
  // button and the minimize and close button icons should stay changed.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());
  EXPECT_EQ(CAPTION_BUTTON_ICON_LEFT_SNAPPED, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_RIGHT_SNAPPED, close_button()->icon());

  // Release the mouse, snapping the window left.
  generator->ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(HasStateType(mojom::WindowStateType::LEFT_SNAPPED));

  // None of the buttons should stay pressed and the buttons should have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());

  // Repeat test but release button where it does not affect the window's state
  // because the code path is different.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());
  EXPECT_EQ(CAPTION_BUTTON_ICON_LEFT_SNAPPED, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_RIGHT_SNAPPED, close_button()->icon());

  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  generator->MoveMouseTo(work_area_bounds_in_screen.bottom_left());

  // None of the buttons should be pressed because we are really far away from
  // any of the caption buttons. The minimize and close button icons should
  // be changed because the mouse is pressed.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(CAPTION_BUTTON_ICON_LEFT_SNAPPED, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_RIGHT_SNAPPED, close_button()->icon());

  // Release the mouse. The window should stay snapped left.
  generator->ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(HasStateType(mojom::WindowStateType::LEFT_SNAPPED));

  // The buttons should stay unpressed and the buttons should now have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());
}

// Test that the size button is pressed whenever the snap left/right buttons
// are hovered.
TEST_F(FrameSizeButtonTest, SizeButtonPressedWhenSnapButtonHovered) {
  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());
  EXPECT_EQ(CAPTION_BUTTON_ICON_LEFT_SNAPPED, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_RIGHT_SNAPPED, close_button()->icon());

  // Dragging the mouse over the minimize button (snap left button) should hover
  // the minimize button and keep the size button pressed.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());

  // Moving the mouse far away from the caption buttons and then moving it over
  // the close button (snap right button) should hover the close button and
  // keep the size button pressed.
  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  generator->MoveMouseTo(work_area_bounds_in_screen.bottom_left());
  EXPECT_TRUE(AllButtonsInNormalState());
  generator->MoveMouseTo(CenterPointInScreen(close_button()));
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_HOVERED, close_button()->state());
}

class FrameSizeButtonTestRTL : public FrameSizeButtonTest {
 public:
  FrameSizeButtonTestRTL() = default;
  ~FrameSizeButtonTestRTL() override = default;

  void SetUp() override {
    original_locale_ = base::i18n::GetConfiguredLocale();
    base::i18n::SetICUDefaultLocale("he");

    FrameSizeButtonTest::SetUp();
  }

  void TearDown() override {
    FrameSizeButtonTest::TearDown();
    base::i18n::SetICUDefaultLocale(original_locale_);
  }

 private:
  std::string original_locale_;

  DISALLOW_COPY_AND_ASSIGN(FrameSizeButtonTestRTL);
};

// Test that clicking + dragging to a button adjacent to the size button presses
// the correct button and snaps the window to the correct side.
TEST_F(FrameSizeButtonTestRTL, ButtonDrag) {
  // In RTL the close button should be left of the size button and the minimize
  // button should be right of the size button.
  ASSERT_LT(close_button()->GetBoundsInScreen().x(),
            size_button()->GetBoundsInScreen().x());
  ASSERT_LT(size_button()->GetBoundsInScreen().x(),
            minimize_button()->GetBoundsInScreen().x());

  // Test initial state.
  EXPECT_TRUE(window_state()->IsNormalStateType());
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());

  // Pressing the size button should swap the icons of the minimize and close
  // buttons to icons for snapping right and for snapping left respectively.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());
  EXPECT_EQ(CAPTION_BUTTON_ICON_RIGHT_SNAPPED, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_LEFT_SNAPPED, close_button()->icon());

  // Dragging over to the minimize button should press it.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());

  // Releasing should snap the window right.
  generator->ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(HasStateType(mojom::WindowStateType::RIGHT_SNAPPED));

  // None of the buttons should stay pressed and the buttons should have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());
}

namespace {

class FrameSizeButtonNonResizableTest : public FrameSizeButtonTest {
 public:
  FrameSizeButtonNonResizableTest() : FrameSizeButtonTest(false) {}
  ~FrameSizeButtonNonResizableTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameSizeButtonNonResizableTest);
};

}  // namespace

TEST_F(FrameSizeButtonNonResizableTest, NoSnap) {
  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->state());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->state());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->state());

  EXPECT_EQ(CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->icon());
  EXPECT_EQ(CAPTION_BUTTON_ICON_CLOSE, close_button()->icon());
}

}  // namespace ash
