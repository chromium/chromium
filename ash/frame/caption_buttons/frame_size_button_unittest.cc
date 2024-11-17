// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/caption_buttons/frame_size_button.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/frame/snap_controller_impl.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_test_util.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/multitask_menu/multitask_button.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view_test_api.h"
#include "chromeos/ui/frame/multitask_menu/split_button_view.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/vector_icons/vector_icons.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

using ::chromeos::FrameCaptionButtonContainerView;
using ::chromeos::FrameSizeButton;
using ::chromeos::MultitaskButton;
using ::chromeos::MultitaskMenu;
using ::chromeos::MultitaskMenuEntryType;
using ::chromeos::MultitaskMenuViewTestApi;
using ::chromeos::SplitButtonView;
using ::chromeos::WindowStateType;

constexpr char kMultitaskMenuBubbleWidgetName[] = "MultitaskMenuBubbleWidget";

class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  explicit TestWidgetDelegate(bool resizable) {
    SetCanMaximize(true);
    SetCanMinimize(true);
    SetCanResize(resizable);
  }

  TestWidgetDelegate(const TestWidgetDelegate&) = delete;
  TestWidgetDelegate& operator=(const TestWidgetDelegate&) = delete;

  ~TestWidgetDelegate() override = default;

  FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

 private:
  // Overridden from views::View:
  void Layout(PassKey) override {
    // Right align the caption button container.
    gfx::Size preferred_size = caption_button_container_->GetPreferredSize();
    caption_button_container_->SetBounds(width() - preferred_size.width(), 0,
                                         preferred_size.width(),
                                         preferred_size.height());
  }

  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override {
    if (details.is_add && details.child == this) {
      caption_button_container_ =
          new FrameCaptionButtonContainerView(GetWidget());

      // Set images for the button icons and assign the default caption button
      // size.
      caption_button_container_->SetButtonSize(
          views::GetCaptionButtonLayoutSize(
              views::CaptionButtonLayoutSize::kNonBrowserCaption));
      caption_button_container_->SetButtonImage(
          views::CAPTION_BUTTON_ICON_MINIMIZE,
          views::kWindowControlMinimizeIcon);
      caption_button_container_->SetButtonImage(views::CAPTION_BUTTON_ICON_MENU,
                                                chromeos::kFloatWindowIcon);
      caption_button_container_->SetButtonImage(
          views::CAPTION_BUTTON_ICON_CLOSE, views::kWindowControlCloseIcon);
      caption_button_container_->SetButtonImage(
          views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
          chromeos::kWindowControlLeftSnappedIcon);
      caption_button_container_->SetButtonImage(
          views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
          chromeos::kWindowControlRightSnappedIcon);
      caption_button_container()->SetButtonImage(
          views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
          views::kWindowControlMaximizeIcon);

      AddChildView(caption_button_container_.get());
    }
  }

  // Not owned.
  raw_ptr<FrameCaptionButtonContainerView> caption_button_container_;
};

class FrameSizeButtonTest : public AshTestBase {
 public:
  FrameSizeButtonTest() = default;
  explicit FrameSizeButtonTest(bool resizable) : resizable_(resizable) {}

  FrameSizeButtonTest(const FrameSizeButtonTest&) = delete;
  FrameSizeButtonTest& operator=(const FrameSizeButtonTest&) = delete;

  ~FrameSizeButtonTest() override = default;

  // Returns the center point of |view| in screen coordinates.
  gfx::Point CenterPointInScreen(views::View* view) {
    return view->GetBoundsInScreen().CenterPoint();
  }

  // Returns true if the window has |state_type|.
  bool HasStateType(WindowStateType state_type) const {
    return window_state()->GetStateType() == state_type;
  }

  // Returns true if all three buttons are in the normal state.
  bool AllButtonsInNormalState() const {
    return minimize_button_->GetState() == views::Button::STATE_NORMAL &&
           size_button_->GetState() == views::Button::STATE_NORMAL &&
           close_button_->GetState() == views::Button::STATE_NORMAL;
  }

  // Creates a widget with |delegate|. The returned widget takes ownership of
  // |delegate|.
  views::Widget* CreateWidget(views::WidgetDelegate* delegate) {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.delegate = delegate;
    params.bounds = gfx::Rect(10, 10, 100, 100);
    params.context = GetContext();
    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }

  // AshTestBase overrides:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_delegate_ = new TestWidgetDelegate(resizable_);
    widget_ = CreateWidget(widget_delegate_);
    widget_->GetNativeWindow()->SetProperty(chromeos::kAppTypeKey,
                                            chromeos::AppType::BROWSER);
    window_state_ = WindowState::Get(widget_->GetNativeWindow());

    FrameCaptionButtonContainerView::TestApi test(
        widget_delegate_->caption_button_container());

    minimize_button_ = test.minimize_button();
    size_button_ = test.size_button();
    static_cast<FrameSizeButton*>(size_button_)
        ->set_long_tap_delay_for_testing(base::Milliseconds(0));
    close_button_ = test.close_button();
  }

  WindowState* window_state() { return window_state_; }
  const WindowState* window_state() const { return window_state_; }
  views::Widget* GetWidget() { return widget_; }

  views::FrameCaptionButton* minimize_button() { return minimize_button_; }
  views::FrameCaptionButton* size_button() { return size_button_; }
  views::FrameCaptionButton* close_button() { return close_button_; }
  TestWidgetDelegate* widget_delegate() { return widget_delegate_; }

 private:
  // Not owned.
  raw_ptr<WindowState, DanglingUntriaged> window_state_;
  raw_ptr<views::Widget, DanglingUntriaged> widget_;
  raw_ptr<views::FrameCaptionButton, DanglingUntriaged> minimize_button_;
  raw_ptr<views::FrameCaptionButton, DanglingUntriaged> size_button_;
  raw_ptr<views::FrameCaptionButton, DanglingUntriaged> close_button_;
  raw_ptr<TestWidgetDelegate, DanglingUntriaged> widget_delegate_;
  bool resizable_ = true;
};

}  // namespace

// Tests that pressing the left mouse button or tapping down on the size button
// puts the button into the pressed state.
TEST_F(FrameSizeButtonTest, PressedState) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());

  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressTouchId(3);
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  generator->ReleaseTouchId(3);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());
}

// Tests that clicking on the size button toggles between the maximized and
// normal state.
TEST_F(FrameSizeButtonTest, ClickSizeButtonTogglesMaximize) {
  EXPECT_FALSE(window_state()->IsMaximized());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_state()->IsMaximized());

  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_state()->IsMaximized());

  generator->GestureTapAt(CenterPointInScreen(size_button()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_state()->IsMaximized());

  generator->GestureTapAt(CenterPointInScreen(size_button()));
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kSecondarySnapped));

  // Snap left.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));

  // 2) Test with scroll gestures.
  // Snap right.
  generator->GestureScrollSequence(CenterPointInScreen(size_button()),
                                   CenterPointInScreen(close_button()),
                                   base::Milliseconds(100), 3);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kSecondarySnapped));

  // Snap left.
  generator->GestureScrollSequence(CenterPointInScreen(size_button()),
                                   CenterPointInScreen(minimize_button()),
                                   base::Milliseconds(100), 3);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));

  // 3) Test with tap gestures.
  const float touch_default_radius =
      ui::GestureConfiguration::GetInstance()->default_radius();
  ui::GestureConfiguration::GetInstance()->set_default_radius(0);
  // Snap right.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressMoveAndReleaseTouchTo(CenterPointInScreen(close_button()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kSecondarySnapped));
  // Snap left.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressMoveAndReleaseTouchTo(CenterPointInScreen(minimize_button()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));
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
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));
}

// Test that right clicking the size button has no effect.
TEST_F(FrameSizeButtonTest, RightMouseButton) {
  EXPECT_TRUE(window_state()->IsNormalStateType());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressRightButton();
  generator->ReleaseRightButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_state()->IsNormalStateType());
}

// Test that during the waiting to snap mode, if the window's state is changed,
// or the window is put in overview, we should cancel the waiting to snap mode.
TEST_F(FrameSizeButtonTest, CancelSnapTest) {
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());

  // Press on the size button and drag toward to close buton to enter waiting-
  // for-snap mode.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  generator->MoveMouseTo(CenterPointInScreen(close_button()));
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_TRUE(
      static_cast<FrameSizeButton*>(size_button())->in_snap_mode_for_testing());
  // Maximize the window.
  window_state()->Maximize();
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());
  EXPECT_FALSE(
      static_cast<FrameSizeButton*>(size_button())->in_snap_mode_for_testing());
  generator->ReleaseLeftButton();

  // Test that if window is put in overview, the waiting-to-snap is canceled.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  generator->MoveMouseTo(CenterPointInScreen(close_button()));
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_TRUE(
      static_cast<FrameSizeButton*>(size_button())->in_snap_mode_for_testing());
  window_state()->window()->SetProperty(chromeos::kIsShowingInOverviewKey,
                                        true);
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());
  EXPECT_FALSE(
      static_cast<FrameSizeButton*>(size_button())->in_snap_mode_for_testing());
  generator->ReleaseLeftButton();
}

// Test that upon releasing the mouse button after having pressed the size
// button
// - The state of all the caption buttons is reset.
// - The icon displayed by all of the caption buttons is reset.
TEST_F(FrameSizeButtonTest, ResetButtonsAfterClick) {
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_TRUE(chromeos::kWindowControlLeftSnappedIcon.name ==
              minimize_button()->icon_definition_for_test()->name);
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());
  EXPECT_TRUE(chromeos::kWindowControlRightSnappedIcon.name ==
              close_button()->icon_definition_for_test()->name);

  // Dragging the mouse over the minimize button should hover the minimize
  // button and the minimize and close button icons should stay changed.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  // Release the mouse, snapping the window left.
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));

  // None of the buttons should stay pressed and the buttons should have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());

  // Repeat test but release button where it does not affect the window's state
  // because the code path is different.
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  generator->MoveMouseTo(work_area_bounds_in_screen.bottom_left());

  // None of the buttons should be pressed because we are really far away from
  // any of the caption buttons. The minimize and close button icons should
  // be changed because the mouse is pressed.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  // Release the mouse. The window should stay snapped left.
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));

  // The buttons should stay unpressed and the buttons should now have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
}

// Test that the size button is pressed whenever the snap left/right buttons
// are hovered.
TEST_F(FrameSizeButtonTest, SizeButtonPressedWhenSnapButtonHovered) {
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  // Dragging the mouse over the minimize button (snap left button) should hover
  // the minimize button and keep the size button pressed.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());

  // Moving the mouse far away from the caption buttons and then moving it over
  // the close button (snap right button) should hover the close button and
  // keep the size button pressed.
  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  generator->MoveMouseTo(work_area_bounds_in_screen.bottom_left());
  EXPECT_TRUE(AllButtonsInNormalState());
  generator->MoveMouseTo(CenterPointInScreen(close_button()));
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_HOVERED, close_button()->GetState());
}

class FrameSizeButtonTestRTL : public FrameSizeButtonTest {
 public:
  FrameSizeButtonTestRTL() = default;

  FrameSizeButtonTestRTL(const FrameSizeButtonTestRTL&) = delete;
  FrameSizeButtonTestRTL& operator=(const FrameSizeButtonTestRTL&) = delete;

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
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());

  // Pressing the size button should swap the icons of the minimize and close
  // buttons to icons for snapping right and for snapping left respectively.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            close_button()->GetIcon());

  // Dragging over to the minimize button should press it.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());

  // Releasing should snap the window right.
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kSecondarySnapped));

  // None of the buttons should stay pressed and the buttons should have their
  // regular icons.
  EXPECT_TRUE(AllButtonsInNormalState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
}

namespace {

class FrameSizeButtonNonResizableTest : public FrameSizeButtonTest {
 public:
  FrameSizeButtonNonResizableTest() : FrameSizeButtonTest(false) {}
  FrameSizeButtonNonResizableTest(const FrameSizeButtonNonResizableTest&) =
      delete;
  FrameSizeButtonNonResizableTest& operator=(
      const FrameSizeButtonNonResizableTest&) = delete;
  ~FrameSizeButtonNonResizableTest() override = default;
};

}  // namespace

TEST_F(FrameSizeButtonNonResizableTest, NoSnap) {
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());

  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
}

// FrameSizeButtonPortraitDisplayTest is used to test functionalities to snap
// top and bottom in portrait layout, affecting snap icons.
using FrameSizeButtonPortraitDisplayTest = FrameSizeButtonTest;

// Test that upon pressed the size button should show left and right arrows for
// horizontal snap and upward and downward arrow for vertical snap.
TEST_F(FrameSizeButtonPortraitDisplayTest, SnapButtons) {
  UpdateDisplay("600x800");
  FrameCaptionButtonContainerView* container =
      widget_delegate()->caption_button_container();
  views::Widget* widget = widget_delegate()->GetWidget();
  chromeos::DefaultFrameHeader frame_header(
      widget, widget->non_client_view()->frame_view(), container);
  frame_header.LayoutHeader();

  EXPECT_EQ(views::CAPTION_BUTTON_ICON_MINIMIZE, minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_CLOSE, close_button()->GetIcon());
  EXPECT_TRUE(AllButtonsInNormalState());

  // Pressing the size button should result in the size button being pressed and
  // the minimize and close button icons changing.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  EXPECT_EQ(views::Button::STATE_NORMAL, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  const gfx::VectorIcon* left_icon = &chromeos::kWindowControlTopSnappedIcon;
  const gfx::VectorIcon* right_icon =
      &chromeos::kWindowControlBottomSnappedIcon;

  EXPECT_TRUE(left_icon->name ==
              minimize_button()->icon_definition_for_test()->name);
  EXPECT_TRUE(right_icon->name ==
              close_button()->icon_definition_for_test()->name);

  // Dragging the mouse over the minimize button should hover the minimize
  // button (snap top/left). The minimize and close button icons should stay
  // changed.
  generator->MoveMouseTo(CenterPointInScreen(minimize_button()));
  EXPECT_EQ(views::Button::STATE_HOVERED, minimize_button()->GetState());
  EXPECT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, close_button()->GetState());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
            minimize_button()->GetIcon());
  EXPECT_EQ(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
            close_button()->GetIcon());

  // Release the mouse, snapping the window to the primary position.
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasStateType(WindowStateType::kPrimarySnapped));
}

class MultitaskMenuTest : public FrameSizeButtonTest {
 public:
  MultitaskMenuTest() = default;
  MultitaskMenuTest(const MultitaskMenuTest&) = delete;
  MultitaskMenuTest& operator=(const MultitaskMenuTest&) = delete;
  ~MultitaskMenuTest() override = default;

  MultitaskMenu* GetMultitaskMenu() {
    views::Widget* widget = static_cast<FrameSizeButton*>(size_button())
                                ->multitask_menu_widget_for_testing();
    return widget ? static_cast<MultitaskMenu*>(
                        widget->widget_delegate()->AsDialogDelegate())
                  : nullptr;
  }

  void ShowMultitaskMenu(MultitaskMenuEntryType entry_type =
                             MultitaskMenuEntryType::kFrameSizeButtonHover) {
    ShowAndWaitMultitaskMenuForWindow(
        static_cast<FrameSizeButton*>(size_button()), entry_type);
  }

  aura::Window* window() { return window_state()->window(); }
  wm::ActivationClient* activation_client() {
    return wm::GetActivationClient(window()->GetRootWindow());
  }
};

// Test float button functionality.
TEST_F(MultitaskMenuTest, TestMultitaskMenuFloatFunctionality) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(window_state()->IsNormalStateType());
  ui::test::EventGenerator* generator = GetEventGenerator();
  window_state()->Deactivate();
  ASSERT_NE(activation_client()->GetActiveWindow(), window());
  ShowMultitaskMenu();
  EXPECT_NE(activation_client()->GetActiveWindow(), window());
  generator->MoveMouseTo(CenterPointInScreen(
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetFloatButton()));
  generator->ClickLeftButton();
  EXPECT_TRUE(window_state()->IsFloated());
  histogram_tester.ExpectBucketCount(
      chromeos::GetActionTypeHistogramName(),
      chromeos::MultitaskMenuActionType::kFloatButton, 1);
  EXPECT_EQ(activation_client()->GetActiveWindow(), window());
}

// Test Half Button Functionality.
TEST_F(MultitaskMenuTest, TestMultitaskMenuHalfFunctionality) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(window_state()->IsNormalStateType());
  window_state()->Deactivate();
  ASSERT_NE(activation_client()->GetActiveWindow(), window());
  ShowMultitaskMenu();
  EXPECT_NE(activation_client()->GetActiveWindow(), window());
  LeftClickOn(
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetHalfButton()
          ->GetLeftTopButton());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  histogram_tester.ExpectBucketCount(
      chromeos::GetActionTypeHistogramName(),
      chromeos::MultitaskMenuActionType::kHalfSplitButton, 1);
  EXPECT_EQ(activation_client()->GetActiveWindow(), window());
}

// Tests that clicking the left side of the half button works as intended for
// RTL setups.
TEST_F(MultitaskMenuTest, HalfButtonRTL) {
  UpdateDisplay("800x600");

  base::i18n::SetRTLForTesting(true);

  ShowMultitaskMenu();
  LeftClickOn(
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetHalfButton()
          ->GetLeftTopButton());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(gfx::Rect(400, 552), GetWidget()->GetWindowBoundsInScreen());

  // Overview may start due to faster split screen when the window is snapped.
  // Escape overview if it is active, otherwise the key event will be handled in
  // `OverviewSession` to exit overview, see `OverviewSession::OnKeyEvent()` for
  // more details. Pressing the Alt key below won't reverse the multi-task menu.
  if (IsInOverviewSession()) {
    PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  }

  // Reverse the menu. Test that the left button still snaps to primary.
  ShowMultitaskMenu();
  PressAndReleaseKey(ui::VKEY_MENU, ui::EF_ALT_DOWN);
  ASSERT_TRUE(
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetIsReversed());
  LeftClickOn(GetMultitaskMenu()
                  ->multitask_menu_view()
                  ->partial_button()
                  ->GetLeftTopButton());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(gfx::Rect(266, 552), GetWidget()->GetWindowBoundsInScreen());
}

// Tests that if the display is in secondary layout, pressing the physically
// left side button should snap it to the correct side.
TEST_F(MultitaskMenuTest, HalfButtonSecondaryLayout) {
  // Rotate the display 180 degrees so its layout is not primary.
  const int64_t display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::test::ScopedSetInternalDisplayId set_internal(
      Shell::Get()->display_manager(), display_id);
  ScreenOrientationControllerTestApi(
      Shell::Get()->screen_orientation_controller())
      .SetDisplayRotation(display::Display::ROTATE_180,
                          display::Display::RotationSource::ACTIVE);

  ShowMultitaskMenu(MultitaskMenuEntryType::kAccel);

  // Click on the left side of the half button. It should be in secondary
  // snapped state, because in this orientation secondary snapped is actually
  // physically on the left side.
  GetEventGenerator()->MoveMouseToInHost(
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetHalfButton()
          ->GetBoundsInScreen()
          .left_center());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
}

// Test Partial Split Button Functionality.
TEST_F(MultitaskMenuTest, TestMultitaskMenuPartialSplit) {
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(window_state()->IsNormalStateType());
  window_state()->Deactivate();
  ASSERT_NE(activation_client()->GetActiveWindow(), window());
  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  // Verify the metrics initial states.
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(
                chromeos::kPartialSplitOneThirdUserAction),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                chromeos::kPartialSplitTwoThirdsUserAction),
            0);

  // Snap to primary with 0.67f screen ratio.
  ShowMultitaskMenu();
  EXPECT_NE(activation_client()->GetActiveWindow(), window());
  LeftClickOn(GetMultitaskMenu()
                  ->multitask_menu_view()
                  ->partial_button()
                  ->GetLeftTopButton());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(window_state()->window()->bounds().width(),
            std::round(work_area_bounds_in_screen.width() *
                       chromeos::kTwoThirdSnapRatio));
  EXPECT_EQ(user_action_tester.GetActionCount(
                chromeos::kPartialSplitTwoThirdsUserAction),
            1);
  histogram_tester.ExpectBucketCount(
      chromeos::GetActionTypeHistogramName(),
      chromeos::MultitaskMenuActionType::kPartialSplitButton, 1);
  EXPECT_EQ(activation_client()->GetActiveWindow(), window());

  // Snap to secondary with 0.33f screen ratio.
  ShowMultitaskMenu();
  LeftClickOn(GetMultitaskMenu()
                  ->multitask_menu_view()
                  ->partial_button()
                  ->GetRightBottomButton());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  EXPECT_EQ(window_state()->window()->bounds().width(),
            std::round(work_area_bounds_in_screen.width() *
                       chromeos::kOneThirdSnapRatio));
  EXPECT_EQ(user_action_tester.GetActionCount(
                chromeos::kPartialSplitOneThirdUserAction),
            1);
  histogram_tester.ExpectBucketCount(
      chromeos::GetActionTypeHistogramName(),
      chromeos::MultitaskMenuActionType::kPartialSplitButton, 2);
}

// Verify that selecting the 2/3 partial split option from the window layout
// menu correctly updates the snap ratio to 2/3 and snaps the target window to
// occupy two-thirds of the available space. Regression test for
// http://b/356537586.
TEST_F(MultitaskMenuTest, PartialSplitInNonPrimaryDisplay) {
  // Update display to be in non-primary landscape mode.
  UpdateDisplay("800x600/u");

  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(1U, displays.size());
  ASSERT_EQ(chromeos::OrientationType::kLandscapeSecondary,
            chromeos::GetDisplayCurrentOrientation(displays[0]));

  ShowMultitaskMenu(MultitaskMenuEntryType::kAccel);

  const gfx::Point two_thirds_partial_button_center =
      GetMultitaskMenu()
          ->multitask_menu_view()
          ->partial_button()
          ->GetLeftTopButton()
          ->GetBoundsInScreen()
          .CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToInHost(two_thirds_partial_button_center);

  // Verify that the target window snaps at expected snap position with the
  // correct snap ratio applied.
  event_generator->ClickLeftButton();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  EXPECT_THAT(window_state()->snap_ratio(),
              testing::Optional(chromeos::kTwoThirdSnapRatio));
  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  EXPECT_NEAR(work_area_bounds_in_screen.width() * chromeos::kTwoThirdSnapRatio,
              window_state()->window()->bounds().width(), 1);
}

// Test Full Button Functionality.
TEST_F(MultitaskMenuTest, TestMultitaskMenuFullFunctionality) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(window_state()->IsNormalStateType());
  window_state()->Deactivate();
  ASSERT_NE(activation_client()->GetActiveWindow(), window());
  ShowMultitaskMenu();
  EXPECT_NE(activation_client()->GetActiveWindow(), window());
  LeftClickOn(
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetFullButton());
  EXPECT_TRUE(window_state()->IsFullscreen());
  histogram_tester.ExpectBucketCount(
      chromeos::GetActionTypeHistogramName(),
      chromeos::MultitaskMenuActionType::kFullscreenButton, 1);
  EXPECT_EQ(activation_client()->GetActiveWindow(), window());
}

TEST_F(MultitaskMenuTest, MultitaskMenuClosesOnTabletMode) {
  ShowMultitaskMenu();
  ASSERT_TRUE(GetMultitaskMenu());

  TabletMode::Get()->SetEnabledForTest(true);

  // Closing the widget is done on a post task.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetMultitaskMenu());
}

// Verifies that long touch on the size button shows the multitask menu.
// Regression test for https://crbug.com/1367376.
TEST_F(MultitaskMenuTest, LongTouchShowsMultitaskMenu) {
  ASSERT_TRUE(size_button());
  base::HistogramTester histogram_tester;

  // Touch until the multitask bubble shows up. This would time out if long
  // touch was not working.
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      std::string(kMultitaskMenuBubbleWidgetName));
  GetEventGenerator()->PressTouch(
      size_button()->GetBoundsInScreen().CenterPoint());
  views::Widget* bubble_widget = waiter.WaitIfNeededAndGet();
  EXPECT_TRUE(bubble_widget);

  histogram_tester.ExpectBucketCount(
      chromeos::GetEntryTypeHistogramName(),
      MultitaskMenuEntryType::kFrameSizeButtonLongTouch, 1);
}

// Verifies that metrics are recorded properly for clamshell entry points.
TEST_F(MultitaskMenuTest, EntryTypeHistogram) {
  base::HistogramTester histogram_tester;

  // Check that mouse hover increments the correct bucket.
  GetEventGenerator()->MoveMouseTo(CenterPointInScreen(size_button()));
  histogram_tester.ExpectBucketCount(
      chromeos::GetEntryTypeHistogramName(),
      MultitaskMenuEntryType::kFrameSizeButtonHover, 1);

  // Check that long press increments the correct bucket.
  GetEventGenerator()->MoveMouseTo(CenterPointInScreen(size_button()));
  GetEventGenerator()->PressLeftButton();
  histogram_tester.ExpectBucketCount(
      chromeos::GetEntryTypeHistogramName(),
      MultitaskMenuEntryType::kFrameSizeButtonLongPress, 1);

  // Check that the accelerator increments the correct bucket.
  // Create an active window for the toggle menu to work.
  auto window = CreateTestWindow();
  PressAndReleaseKey(ui::VKEY_Z, ui::EF_COMMAND_DOWN);
  histogram_tester.ExpectBucketCount(chromeos::GetEntryTypeHistogramName(),
                                     MultitaskMenuEntryType::kAccel, 1);

  // Check total counts for each histogram to ensure calls aren't counted in
  // multiple buckets.
  histogram_tester.ExpectTotalCount(chromeos::GetEntryTypeHistogramName(), 3);
}

// Tests that we do not create a new widget when hovering on the size button
// when the multitask menu is already opened.
TEST_F(MultitaskMenuTest, HoverWhenMenuAlreadyShown) {
  ShowMultitaskMenu();
  MultitaskMenu* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);

  // Ensure the mouse is not already on the size button then move it back on.
  // The menu should be the same one opened beforehand.
  GetEventGenerator()->MoveMouseTo(
      size_button()->GetBoundsInScreen().bottom_right() + gfx::Vector2d(2, 2));
  GetEventGenerator()->MoveMouseTo(CenterPointInScreen(size_button()));
  EXPECT_EQ(multitask_menu, GetMultitaskMenu());
}

TEST_F(MultitaskMenuTest, CloseOnClickOutside) {
  // Snap the window to half so we can click outside the window bounds.
  ShowMultitaskMenu();
  LeftClickOn(
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetHalfButton()
          ->GetLeftTopButton());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  ShowMultitaskMenu();
  MultitaskMenu* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);

  // Click on a point outside the menu on the screen below.
  gfx::Point offset_point(multitask_menu->multitask_menu_view()
                              ->GetBoundsInScreen()
                              .bottom_right());
  offset_point.Offset(10, 10);
  DCHECK(!GetWidget()->GetWindowBoundsInScreen().Contains(offset_point));
  GetEventGenerator()->MoveMouseTo(offset_point);
  GetEventGenerator()->ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(GetMultitaskMenu());
}

// Tests that moving the mouse outside the menu will close the menu, if opened
// via hovering on the frame size button.
TEST_F(MultitaskMenuTest, MoveMouseOutsideMenu) {
  chromeos::MultitaskMenuView::SetSkipMouseOutDelayForTesting(true);

  // Simulate opening the menu by moving the mouse to the frame size button and
  // opening the menu.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      size_button()->GetBoundsInScreen().CenterPoint());
  ShowMultitaskMenu();

  MultitaskMenu* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  event_generator->MoveMouseTo(
      multitask_menu->GetBoundsInScreen().CenterPoint());
  // Widget is closed with a post task.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetMultitaskMenu());

  event_generator->MoveMouseTo(gfx::Point(1, 1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetMultitaskMenu());

  // Open the menu using the accelerator.
  event_generator->MoveMouseTo(
      size_button()->GetBoundsInScreen().CenterPoint());
  ShowMultitaskMenu(MultitaskMenuEntryType::kAccel);

  // Test that the menu remains open if we move outside when using the
  // accelerator.
  event_generator->MoveMouseTo(gfx::Point(1, 1));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetMultitaskMenu());
}

// Tests that window accelerators, e.g. minimize, still work when the multitask
// menu is open.
TEST_F(MultitaskMenuTest, MinimizeWhenMenuShown) {
  ShowMultitaskMenu();

  PressAndReleaseKey(ui::VKEY_OEM_MINUS, ui::EF_ALT_DOWN);
  ASSERT_TRUE(window_state()->IsMinimized());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetMultitaskMenu());

  PressAndReleaseKey(ui::VKEY_OEM_MINUS, ui::EF_ALT_DOWN);
  ASSERT_FALSE(window_state()->IsMinimized());
  EXPECT_TRUE(GetWidget()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that the partial button is horizontally flipped when the `Alt` key is
// pressed while the menu is shown.
TEST_F(MultitaskMenuTest, ReversePartialButton) {
  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  // Reverse the menu. Test that the left button snaps to 1/3.
  ShowMultitaskMenu();
  PressAndReleaseKey(ui::VKEY_MENU, ui::EF_ALT_DOWN);
  ASSERT_TRUE(
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetIsReversed());
  LeftClickOn(GetMultitaskMenu()
                  ->multitask_menu_view()
                  ->partial_button()
                  ->GetLeftTopButton());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(std::floor(work_area_bounds_in_screen.width() *
                       chromeos::kOneThirdSnapRatio),
            GetWidget()->GetWindowBoundsInScreen().width());

  // Overview may start due to faster split screen when the window is
  // snapped. Escape overview if it is active, otherwise the key event will be
  // handled in `OverviewSession` to exit overview, see
  // `OverviewSession::OnKeyEvent()` for more details. Pressing the Alt key
  // below won't reverse the multi-task menu.
  if (IsInOverviewSession()) {
    PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  }

  // Reverse the menu. Test that the right button snaps to 2/3.
  ShowMultitaskMenu();
  PressAndReleaseKey(ui::VKEY_MENU, ui::EF_ALT_DOWN);
  ASSERT_TRUE(
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetIsReversed());
  LeftClickOn(GetMultitaskMenu()
                  ->multitask_menu_view()
                  ->partial_button()
                  ->GetRightBottomButton());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  EXPECT_EQ(std::ceil(work_area_bounds_in_screen.width() *
                      chromeos::kTwoThirdSnapRatio),
            GetWidget()->GetWindowBoundsInScreen().width());
}

// Tests that the float button is horizontally flipped when the `Alt` key is
// pressed while the menu is shown.
TEST_F(MultitaskMenuTest, ReverseFloatButton) {
  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect original_bounds = window_state()->window()->bounds();

  // Reverse the menu and press the float button. Test that the window is
  // floated and is roughly on the left edge (there is some padding).
  ShowMultitaskMenu();
  PressAndReleaseKey(ui::VKEY_MENU, ui::EF_ALT_DOWN);
  MultitaskMenuViewTestApi test_api(GetMultitaskMenu()->multitask_menu_view());
  ASSERT_TRUE(test_api.GetIsReversed());
  LeftClickOn(test_api.GetFloatButton());
  EXPECT_EQ(WindowStateType::kFloated, window_state()->GetStateType());
  EXPECT_EQ(
      work_area_bounds_in_screen.x() + chromeos::wm::kFloatedWindowPaddingDp,
      GetWidget()->GetWindowBoundsInScreen().x());

  // Tests that the float button unfloats if reversed and the window is already
  // floated.
  ShowMultitaskMenu();
  PressAndReleaseKey(ui::VKEY_MENU, ui::EF_ALT_DOWN);
  MultitaskMenuViewTestApi test_api2(GetMultitaskMenu()->multitask_menu_view());
  ASSERT_TRUE(test_api2.GetIsReversed());
  LeftClickOn(test_api2.GetFloatButton());
  EXPECT_EQ(WindowStateType::kNormal, window_state()->GetStateType());
  EXPECT_EQ(original_bounds, window_state()->window()->bounds());
}

// Tests that pressing on the size button and then dragging and releasing on a
// multitask menu button will trigger it.
TEST_F(MultitaskMenuTest, PressOnSizeButtonReleaseOnMultitaskMenu) {
  // First assert that all four buttons are visible with the display size and
  // window we are given. We'll need them later and users who add buttons later
  // will need to update this test.
  ShowMultitaskMenu();
  ASSERT_EQ(4u, GetMultitaskMenu()->multitask_menu_view()->children().size());

  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Functions to be used for both touch and mouse testing.
  auto move_to_center = [event_generator](const views::View* view, bool touch) {
    const gfx::Point& screen_location = view->GetBoundsInScreen().CenterPoint();
    touch ? event_generator->MoveTouch(screen_location)
          : event_generator->MoveMouseTo(screen_location);
  };
  auto press = [event_generator](bool touch) {
    touch ? event_generator->PressTouch() : event_generator->PressLeftButton();
  };
  auto release = [event_generator](bool touch) {
    touch ? event_generator->ReleaseTouch()
          : event_generator->ReleaseLeftButton();
  };

  // Test pressing on the size button, dragging to snap left and float buttons,
  // for both mouse and touch.
  for (bool touch : {false, true}) {
    SCOPED_TRACE(
        base::StringPrintf("Testing state: %s", touch ? "touch" : "mouse"));

    // Move to the size button and long press to show the multitask menu.
    move_to_center(size_button(), touch);
    {
      views::NamedWidgetShownWaiter waiter(
          views::test::AnyWidgetTestPasskey{},
          std::string(kMultitaskMenuBubbleWidgetName));
      press(touch);
      waiter.WaitIfNeededAndGet();
    }

    // Without releasing, drag to the left button and release. Test that we are
    // in snapped state.
    MultitaskMenu* multitask_menu = GetMultitaskMenu();
    ASSERT_TRUE(multitask_menu);
    views::Button* left_half_button =
        MultitaskMenuViewTestApi(multitask_menu->multitask_menu_view())
            .GetHalfButton()
            ->GetLeftTopButton();
    move_to_center(left_half_button, touch);
    EXPECT_EQ(views::Button::STATE_HOVERED, left_half_button->GetState());
    release(touch);
    EXPECT_TRUE(window_state()->IsSnapped());

    // Move back to the size button and long press again to show the multitask
    // menu.
    move_to_center(size_button(), touch);
    {
      views::NamedWidgetShownWaiter waiter(
          views::test::AnyWidgetTestPasskey{},
          std::string(kMultitaskMenuBubbleWidgetName));
      press(touch);
      waiter.WaitIfNeededAndGet();
    }

    // Without releasing, drag to the float button and release. Test that we are
    // in float state.
    multitask_menu = GetMultitaskMenu();
    ASSERT_TRUE(multitask_menu);
    views::Button* float_button =
        MultitaskMenuViewTestApi(multitask_menu->multitask_menu_view())
            .GetFloatButton();
    move_to_center(float_button, touch);
    EXPECT_EQ(views::Button::STATE_HOVERED, float_button->GetState());
    release(touch);
    EXPECT_TRUE(window_state()->IsFloated());
  }
}

// Tests that if the window is right snapped, and we try to fullscreen the
// window via touch-dragging the multitask menu, the window is properly
// fullscreened. Regression test for http://b/304437185.
TEST_F(MultitaskMenuTest, FullscreenFromTouchMultitaskMenu) {
  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
  window_state()->OnWMEvent(&snap_secondary);
  ASSERT_TRUE(window_state()->IsSnapped());

  // Long press on the size button until the multitask menu is shown.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      std::string(kMultitaskMenuBubbleWidgetName));
  event_generator->PressTouch(size_button()->GetBoundsInScreen().CenterPoint());
  waiter.WaitIfNeededAndGet();

  // Without releasing, drag to the full button and release. Test that we are
  // in fullscreen state.
  MultitaskMenu* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  event_generator->MoveTouch(
      MultitaskMenuViewTestApi(multitask_menu->multitask_menu_view())
          .GetFullButton()
          ->GetBoundsInScreen()
          .CenterPoint());
  event_generator->ReleaseTouch();
  EXPECT_TRUE(window_state()->IsFullscreen());
}

// Tests that focus traversal with the tab and arrow keys works as expected.
TEST_F(MultitaskMenuTest, TabAndArrowKeyTraversal) {
  // First assert that all four buttons are visible with the display size and
  // window we are given.
  ShowMultitaskMenu();
  MultitaskMenu* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  ASSERT_EQ(4u, multitask_menu->multitask_menu_view()->children().size());

  // Nothing is focused initially.
  views::FocusManager* focus_manager =
      multitask_menu->GetWidget()->GetFocusManager();
  EXPECT_FALSE(focus_manager->GetFocusedView());

  // Press tab. The left button of the half button is focused.
  views::Button* left_half_button =
      MultitaskMenuViewTestApi(multitask_menu->multitask_menu_view())
          .GetHalfButton()
          ->GetLeftTopButton();
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(left_half_button, focus_manager->GetFocusedView());

  // Press shift+tab. The last button (float button) is focused.
  views::Button* float_button =
      MultitaskMenuViewTestApi(multitask_menu->multitask_menu_view())
          .GetFloatButton();
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(float_button, focus_manager->GetFocusedView());

  // Press right and left arrow keys. They should work like tab and shift+tab,
  // respectively.
  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_EQ(left_half_button, focus_manager->GetFocusedView());
  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_EQ(float_button, focus_manager->GetFocusedView());

  // Pressing up/down keys do not affect focus.
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(float_button, focus_manager->GetFocusedView());
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(float_button, focus_manager->GetFocusedView());
}

// Tests that the menu always fits the display work area.
TEST_F(MultitaskMenuTest, AdjustedMenuBounds) {
  // Position the window so that the size button is slightly offscreen.
  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  GetWidget()->SetBounds(
      gfx::Rect(gfx::Point(work_area_bounds_in_screen.right() - 50, 0),
                GetWidget()->GetWindowBoundsInScreen().size()));
  ASSERT_FALSE(
      work_area_bounds_in_screen.Contains(size_button()->GetBoundsInScreen()));

  // Hover over the visible part of the size button. Test that the menu fits
  // onscreen.
  ShowMultitaskMenu();
  EXPECT_TRUE(work_area_bounds_in_screen.Contains(
      GetMultitaskMenu()->GetBoundsInScreen()));
}

using SnapGroupFrameSizeButtonTest = MultitaskMenuTest;

// Tests that long press caption button to show snap phantom bounds are updated.
TEST_F(SnapGroupFrameSizeButtonTest, SnapCaptionButton) {
  EXPECT_EQ(views::Button::STATE_NORMAL, size_button()->GetState());

  // Create an opposite snapped window with non-default snap ratio.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  const WindowSnapWMEvent snap_primary(
      WM_EVENT_SNAP_PRIMARY, chromeos::kTwoThirdSnapRatio,
      WindowSnapActionSource::kSnapByWindowLayoutMenu);
  WindowState::Get(w1.get())->OnWMEvent(&snap_primary);

  // Press on the size button and drag toward the close button to show the snap
  // phantom bounds.
  wm::ActivateWindow(GetWidget()->GetNativeWindow());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(CenterPointInScreen(size_button()));
  generator->PressLeftButton();
  generator->MoveMouseTo(CenterPointInScreen(close_button()));
  ASSERT_EQ(views::Button::STATE_PRESSED, size_button()->GetState());
  ASSERT_TRUE(
      static_cast<FrameSizeButton*>(size_button())->in_snap_mode_for_testing());
  auto* snap_controller =
      static_cast<SnapControllerImpl*>(chromeos::SnapController::Get());
  ASSERT_TRUE(snap_controller);

  // Test the phantom bounds reflect the opposite snapped `w1`.
  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect expected_bounds(work_area);
  expected_bounds.Subtract(w1->GetBoundsInScreen());
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      snap_controller->phantom_window_controller_for_testing()
          ->GetTargetWindowBounds(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));
}

// Tests that when a snap group with a partially occluding window is re-snapped
// via the layout menu, we do not start partial overview. See http://b/348068768
// for context.
TEST_F(SnapGroupFrameSizeButtonTest, ReSnapViaWindowLayoutMenu) {
  UpdateDisplay("800x600");

  // Create a snap group with `window`, whose frame contains the multitask menu,
  // and an `opposite` snapped window.
  aura::Window* window = window_state()->window();
  std::unique_ptr<aura::Window> opposite(CreateAppWindow());
  const WindowSnapWMEvent snap_primary(
      WM_EVENT_SNAP_PRIMARY, chromeos::kDefaultSnapRatio,
      WindowSnapActionSource::kSnapByWindowLayoutMenu);
  window_state()->OnWMEvent(&snap_primary);

  const WindowSnapWMEvent snap_secondary(
      WM_EVENT_SNAP_SECONDARY, chromeos::kDefaultSnapRatio,
      WindowSnapActionSource::kSnapByWindowLayoutMenu);
  WindowState::Get(opposite.get())->OnWMEvent(&snap_secondary);
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(
      snap_group_controller->AreWindowsInSnapGroup(window, opposite.get()));

  // Create a partially occluding window on top of `opposite`.
  std::unique_ptr<aura::Window> occlude(
      CreateAppWindow(gfx::Rect(410, 10, 200, 200)));
  ASSERT_TRUE(
      opposite->GetBoundsInScreen().Contains(occlude->GetBoundsInScreen()));

  // Hover to show the multitask menu on `window`.
  ShowMultitaskMenu();
  MultitaskMenu* multitask_menu = GetMultitaskMenu();
  views::Button* left_half_button =
      MultitaskMenuViewTestApi(multitask_menu->multitask_menu_view())
          .GetHalfButton()
          ->GetLeftTopButton();

  // Click on the snap button to re-snap `window`. Test we don't start overview
  // and recall the windows to the front.
  LeftClickOn(left_half_button);
  ASSERT_FALSE(IsInOverviewSession());
  EXPECT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(
      window, opposite.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(occlude.get(), window));
  EXPECT_TRUE(window_util::IsStackedBelow(occlude.get(), opposite.get()));
}

// Tests that re-snapping to the opposite side via the window layout menu starts
// partial overview. Regression test for http://b/349892870.
TEST_F(SnapGroupFrameSizeButtonTest, ReSnapToOppositeSide) {
  UpdateDisplay("800x600");

  // Create a snap group with `window`, whose frame contains the multitask menu,
  // and `window2`.
  aura::Window* window = window_state()->window();
  SnapOneTestWindow(window, chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  SnapOneTestWindow(window2.get(), chromeos::WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio);
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(
      snap_group_controller->AreWindowsInSnapGroup(window, window2.get()));

  // Snap `window` to the right via the layout menu.
  ShowMultitaskMenu();
  views::Button* right_half_button =
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetHalfButton()
          ->GetRightBottomButton();
  LeftClickOn(right_half_button);
  VerifySplitViewOverviewSession(window);
  EXPECT_TRUE(GetOverviewSession()->IsWindowInOverview(window2.get()));

  // Snap `window` to the left via the layout menu.
  ShowMultitaskMenu();
  views::Button* left_half_button =
      MultitaskMenuViewTestApi(GetMultitaskMenu()->multitask_menu_view())
          .GetHalfButton()
          ->GetLeftTopButton();
  LeftClickOn(left_half_button);
  VerifySplitViewOverviewSession(window);
  EXPECT_TRUE(GetOverviewSession()->IsWindowInOverview(window2.get()));
}

}  // namespace ash
