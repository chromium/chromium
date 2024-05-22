// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"

#include "ash/constants/ash_switches.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/vector_icons/vector_icons.h"

namespace ash {

using ::chromeos::FrameCaptionButtonContainerView;

class FrameCaptionButtonContainerViewTest : public AshTestBase {
 public:
  enum MaximizeAllowed { MAXIMIZE_ALLOWED, MAXIMIZE_DISALLOWED };

  enum MinimizeAllowed { MINIMIZE_ALLOWED, MINIMIZE_DISALLOWED };

  enum CloseButtonVisible { CLOSE_BUTTON_VISIBLE, CLOSE_BUTTON_NOT_VISIBLE };

  FrameCaptionButtonContainerViewTest() = default;

  FrameCaptionButtonContainerViewTest(
      const FrameCaptionButtonContainerViewTest&) = delete;
  FrameCaptionButtonContainerViewTest& operator=(
      const FrameCaptionButtonContainerViewTest&) = delete;

  ~FrameCaptionButtonContainerViewTest() override = default;

  // Creates a widget which allows maximizing based on |maximize_allowed|.
  // The caller takes ownership of the returned widget.
  [[nodiscard]] views::Widget* CreateTestWidget(
      MaximizeAllowed maximize_allowed,
      MinimizeAllowed minimize_allowed,
      CloseButtonVisible close_button_visible) {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    auto delegate = std::make_unique<views::WidgetDelegateView>();
    delegate->SetCanMaximize(maximize_allowed == MAXIMIZE_ALLOWED);
    delegate->SetCanMinimize(minimize_allowed == MINIMIZE_ALLOWED);
    delegate->SetCanResize(true);
    delegate->SetShowCloseButton(close_button_visible == CLOSE_BUTTON_VISIBLE);
    params.delegate = delegate.release();
    params.bounds = gfx::Rect(10, 10, 100, 100);
    params.context = GetContext();
    widget->Init(std::move(params));
    return widget;
  }

  // Sets arbitrary images for the icons and assign the default caption button
  // size to the buttons in |container|.
  void InitContainer(FrameCaptionButtonContainerView* container) {
    container->SetButtonSize(views::GetCaptionButtonLayoutSize(
        views::CaptionButtonLayoutSize::kNonBrowserCaption));
    for (int icon = 0; icon < views::CAPTION_BUTTON_ICON_COUNT; ++icon) {
      container->SetButtonImage(static_cast<views::CaptionButtonIcon>(icon),
                                views::kWindowControlCloseIcon);
    }
    container->SizeToPreferredSize();
  }

  // Tests that |leftmost| and |rightmost| are at |container|'s edges.
  bool CheckButtonsAtEdges(FrameCaptionButtonContainerView* container,
                           const views::FrameCaptionButton& leftmost,
                           const views::FrameCaptionButton& rightmost) {
    gfx::Rect expected(container->GetPreferredSize());

    gfx::Rect container_size(container->GetPreferredSize());
    if (leftmost.y() == rightmost.y() &&
        leftmost.height() == rightmost.height() &&
        leftmost.x() == expected.x() && leftmost.y() == expected.y() &&
        leftmost.height() == expected.height() &&
        rightmost.bounds().right() == expected.right()) {
      return true;
    }

    LOG(ERROR) << "Buttons " << leftmost.bounds().ToString() << " "
               << rightmost.bounds().ToString() << " not at edges of "
               << expected.ToString();
    return false;
  }

  void ClickSizeButton(FrameCaptionButtonContainerView::TestApi* testApi) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(
        testApi->size_button()->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();
    base::RunLoop().RunUntilIdle();
  }
};

// Test how the allowed actions affect which caption buttons are visible.
TEST_F(FrameCaptionButtonContainerViewTest, ButtonVisibility) {
  // All the buttons should be visible when minimizing and maximizing are
  // allowed.
  FrameCaptionButtonContainerView container1(CreateTestWidget(
      MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED, CLOSE_BUTTON_VISIBLE));
  InitContainer(&container1);
  views::test::RunScheduledLayout(&container1);
  FrameCaptionButtonContainerView::TestApi t1(&container1);
  EXPECT_TRUE(t1.minimize_button()->GetVisible());
  EXPECT_TRUE(t1.size_button()->GetVisible());
  EXPECT_TRUE(t1.close_button()->GetVisible());
  EXPECT_TRUE(CheckButtonsAtEdges(&container1, *t1.minimize_button(),
                                  *t1.close_button()));

  // The minimize button should be visible when minimizing is allowed but
  // maximizing is disallowed.
  FrameCaptionButtonContainerView container2(CreateTestWidget(
      MAXIMIZE_DISALLOWED, MINIMIZE_ALLOWED, CLOSE_BUTTON_VISIBLE));
  InitContainer(&container2);
  views::test::RunScheduledLayout(&container2);
  FrameCaptionButtonContainerView::TestApi t2(&container2);
  EXPECT_TRUE(t2.minimize_button()->GetVisible());
  EXPECT_FALSE(t2.size_button()->GetVisible());
  EXPECT_TRUE(t2.close_button()->GetVisible());
  EXPECT_TRUE(CheckButtonsAtEdges(&container2, *t2.minimize_button(),
                                  *t2.close_button()));

  // Neither the minimize button nor the size button should be visible when
  // neither minimizing nor maximizing are allowed.
  FrameCaptionButtonContainerView container3(CreateTestWidget(
      MAXIMIZE_DISALLOWED, MINIMIZE_DISALLOWED, CLOSE_BUTTON_VISIBLE));
  InitContainer(&container3);
  views::test::RunScheduledLayout(&container3);
  FrameCaptionButtonContainerView::TestApi t3(&container3);
  EXPECT_FALSE(t3.minimize_button()->GetVisible());
  EXPECT_FALSE(t3.size_button()->GetVisible());
  EXPECT_TRUE(t3.close_button()->GetVisible());
  EXPECT_TRUE(
      CheckButtonsAtEdges(&container3, *t3.close_button(), *t3.close_button()));
}

// Tests that the layout animations trigered by button visibility result in the
// correct placement of the buttons.
TEST_F(FrameCaptionButtonContainerViewTest,
       TestUpdateSizeButtonVisibilityAnimation) {
  FrameCaptionButtonContainerView container(CreateTestWidget(
      MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED, CLOSE_BUTTON_VISIBLE));

  // Add an extra button to the left of the size button to verify that it is
  // repositioned similarly to the minimize button. This simulates the PWA menu
  // button being added to the left of the minimize button.
  views::View* extra_button = new views::StaticSizedView(gfx::Size(32, 32));
  container.AddChildViewAt(extra_button, 0);

  InitContainer(&container);
  views::test::RunScheduledLayout(&container);

  FrameCaptionButtonContainerView::TestApi test(&container);
  gfx::Rect initial_extra_button_bounds = extra_button->bounds();
  gfx::Rect initial_minimize_button_bounds = test.minimize_button()->bounds();
  gfx::Rect initial_size_button_bounds = test.size_button()->bounds();
  gfx::Rect initial_close_button_bounds = test.close_button()->bounds();
  gfx::Rect initial_container_bounds = container.bounds();

  ASSERT_EQ(initial_minimize_button_bounds.x(),
            initial_extra_button_bounds.right());
  ASSERT_EQ(initial_size_button_bounds.x(),
            initial_minimize_button_bounds.right());
  ASSERT_EQ(initial_close_button_bounds.x(),
            initial_size_button_bounds.right());

  // Size and minimize buttons are hidden in tablet mode and the other buttons
  // should shift accordingly.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  container.UpdateCaptionButtonState(/*animate=*/false);
  test.EndAnimations();
  // Parent needs to layout in response to size change.
  views::test::RunScheduledLayout(&container);

  EXPECT_TRUE(extra_button->GetVisible());
  EXPECT_FALSE(test.minimize_button()->GetVisible());
  EXPECT_FALSE(test.size_button()->GetVisible());
  EXPECT_TRUE(test.close_button()->GetVisible());
  gfx::Rect extra_button_bounds = extra_button->bounds();
  gfx::Rect close_button_bounds = test.close_button()->bounds();
  EXPECT_EQ(close_button_bounds.x(), extra_button_bounds.right());
  EXPECT_EQ(initial_close_button_bounds.size(), close_button_bounds.size());
  EXPECT_EQ(initial_container_bounds.width() -
                initial_size_button_bounds.width() -
                initial_minimize_button_bounds.width(),
            container.GetPreferredSize().width());

  // Button positions should be the same when leaving tablet mode.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  container.UpdateCaptionButtonState(/*animate=*/false);
  // Calling code needs to layout in response to size change.
  views::test::RunScheduledLayout(&container);
  test.EndAnimations();
  EXPECT_TRUE(test.minimize_button()->GetVisible());
  EXPECT_TRUE(test.size_button()->GetVisible());
  EXPECT_TRUE(test.close_button()->GetVisible());
  EXPECT_EQ(initial_extra_button_bounds, extra_button->bounds());
  EXPECT_EQ(initial_minimize_button_bounds, test.minimize_button()->bounds());
  EXPECT_EQ(initial_size_button_bounds, test.size_button()->bounds());
  EXPECT_EQ(initial_close_button_bounds, test.close_button()->bounds());
  EXPECT_EQ(container.GetPreferredSize().width(),
            initial_container_bounds.width());
}

// Test that the close button is visible when
// |ShouldShowCloseButton()| returns true.
TEST_F(FrameCaptionButtonContainerViewTest, ShouldShowCloseButtonTrue) {
  FrameCaptionButtonContainerView container(CreateTestWidget(
      MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED, CLOSE_BUTTON_VISIBLE));
  InitContainer(&container);
  views::test::RunScheduledLayout(&container);
  FrameCaptionButtonContainerView::TestApi testApi(&container);
  EXPECT_TRUE(testApi.close_button()->GetVisible());
  EXPECT_TRUE(testApi.close_button()->GetEnabled());
}

// Test that the close button is disabled and has correct Tooltip when
// `is_close_button_enabled` is `false`.
TEST_F(FrameCaptionButtonContainerViewTest, CloseButtonIsDisabled) {
  FrameCaptionButtonContainerView container(
      CreateTestWidget(MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED,
                       CLOSE_BUTTON_VISIBLE),
      false /*=is_close_button_enabled*/);
  InitContainer(&container);
  views::test::RunScheduledLayout(&container);
  FrameCaptionButtonContainerView::TestApi testApi(&container);
  EXPECT_TRUE(testApi.close_button()->GetVisible());
  EXPECT_FALSE(testApi.close_button()->GetEnabled());
  EXPECT_EQ(testApi.close_button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_APP_CLOSE_BUTTON_DISABLED_BY_ADMIN));
}

// Test that the close button is enabled and has correct Tooltip when
// `is_close_button_enabled` is `true`.
TEST_F(FrameCaptionButtonContainerViewTest, CloseButtonIsEnabled) {
  FrameCaptionButtonContainerView container(
      CreateTestWidget(MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED,
                       CLOSE_BUTTON_VISIBLE),
      true /*=is_close_button_enabled*/);
  InitContainer(&container);
  views::test::RunScheduledLayout(&container);
  FrameCaptionButtonContainerView::TestApi testApi(&container);
  EXPECT_TRUE(testApi.close_button()->GetVisible());
  EXPECT_TRUE(testApi.close_button()->GetEnabled());
  EXPECT_EQ(testApi.close_button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
}

// Test that the close button enablement is changed.
TEST_F(FrameCaptionButtonContainerViewTest, CloseButtonChanged) {
  FrameCaptionButtonContainerView container(
      CreateTestWidget(MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED,
                       CLOSE_BUTTON_VISIBLE),
      true /*=is_close_button_enabled*/);
  InitContainer(&container);
  views::test::RunScheduledLayout(&container);
  FrameCaptionButtonContainerView::TestApi testApi(&container);
  EXPECT_TRUE(testApi.close_button()->GetVisible());
  EXPECT_TRUE(testApi.close_button()->GetEnabled());
  EXPECT_EQ(testApi.close_button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));

  container.SetCloseButtonEnabled(false);
  EXPECT_TRUE(testApi.close_button()->GetVisible());
  EXPECT_FALSE(testApi.close_button()->GetEnabled());
  EXPECT_EQ(testApi.close_button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_APP_CLOSE_BUTTON_DISABLED_BY_ADMIN));
}

// Test that the close button is not visible when
// |ShouldShowCloseButton()| returns false.
TEST_F(FrameCaptionButtonContainerViewTest, ShouldShowCloseButtonFalse) {
  FrameCaptionButtonContainerView container(CreateTestWidget(
      MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED, CLOSE_BUTTON_NOT_VISIBLE));
  InitContainer(&container);
  views::test::RunScheduledLayout(&container);
  FrameCaptionButtonContainerView::TestApi testApi(&container);
  EXPECT_FALSE(testApi.close_button()->GetVisible());
  EXPECT_TRUE(testApi.close_button()->GetEnabled());
}

// Test that overriding size button behavior works properly.
TEST_F(FrameCaptionButtonContainerViewTest, TestSizeButtonBehaviorOverride) {
  auto* widget = CreateTestWidget(MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED,
                                  CLOSE_BUTTON_VISIBLE);
  widget->Show();

  auto* window_state = WindowState::Get(widget->GetNativeWindow());

  FrameCaptionButtonContainerView container(widget);
  InitContainer(&container);
  widget->GetContentsView()->AddChildView(&container);
  views::test::RunScheduledLayout(&container);
  FrameCaptionButtonContainerView::TestApi testApi(&container);

  EXPECT_TRUE(window_state->IsNormalStateType());

  // Test that the size button works without override.
  ClickSizeButton(&testApi);
  EXPECT_TRUE(window_state->IsMaximized());
  ClickSizeButton(&testApi);
  EXPECT_TRUE(window_state->IsNormalStateType());

  // Test that the size button behavior is overridden when override callback
  // returning true is set.
  bool called = false;
  container.SetOnSizeButtonPressedCallback(
      base::BindLambdaForTesting([&called]() {
        called = true;
        return true;
      }));
  ClickSizeButton(&testApi);
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(called);

  // Test that the override callback is removable.
  called = false;
  container.ClearOnSizeButtonPressedCallback();
  ClickSizeButton(&testApi);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_FALSE(called);
  ClickSizeButton(&testApi);
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_FALSE(called);

  // Test that the size button behavior fall back to the default one when
  // override callback returns false.
  called = false;
  container.SetOnSizeButtonPressedCallback(
      base::BindLambdaForTesting([&called]() {
        called = true;
        return false;
      }));
  ClickSizeButton(&testApi);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_TRUE(called);
  ClickSizeButton(&testApi);
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(called);
}

TEST_F(FrameCaptionButtonContainerViewTest, ResizeButtonRestoreBehavior) {
  auto* widget = CreateTestWidget(MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED,
                                  CLOSE_BUTTON_VISIBLE);
  widget->Show();

  auto* window_state = WindowState::Get(widget->GetNativeWindow());

  FrameCaptionButtonContainerView container(widget);
  InitContainer(&container);
  widget->GetContentsView()->AddChildView(&container);
  views::test::RunScheduledLayout(&container);
  FrameCaptionButtonContainerView::TestApi testApi(&container);

  // Test using size button to restore the maximized window to its normal window
  // state.
  ClickSizeButton(&testApi);
  EXPECT_TRUE(window_state->IsMaximized());
  ClickSizeButton(&testApi);
  EXPECT_TRUE(window_state->IsNormalStateType());

  // Snap the window.
  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_event);
  // Check the window is now snapped.
  EXPECT_TRUE(window_state->IsSnapped());
  ClickSizeButton(&testApi);
  EXPECT_TRUE(window_state->IsMaximized());
  ClickSizeButton(&testApi);
  // Check instead of returning back to normal window state, the window should
  // return back to Snapped window state.
  EXPECT_TRUE(window_state->IsSnapped());
}

TEST_F(FrameCaptionButtonContainerViewTest, TabletSizeButtonVisibility) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  // Create a window in tablet mode. It should be maximized and the size button
  // should be hidden.
  auto window = CreateAppWindow();
  auto* window_state = WindowState::Get(window.get());
  ASSERT_TRUE(window_state->IsMaximized());

  auto* frame = NonClientFrameViewAsh::Get(window.get());
  DCHECK(frame);
  FrameCaptionButtonContainerView* container =
      frame->GetHeaderView()->caption_button_container();
  FrameCaptionButtonContainerView::TestApi test_api(container);

  auto* size_button = test_api.size_button();
  EXPECT_FALSE(size_button->GetVisible());

  // Float the window. Test that the size button is visible.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(window_state->IsFloated());
  EXPECT_TRUE(size_button->GetVisible());
}

// Test how the allowed actions affect the visibility of the float button.
TEST_F(FrameCaptionButtonContainerViewTest, FloatButtonVisibility) {
  // The float button should not be visible when minimizing and maximizing are
  // allowed.
  auto* widget1 = CreateTestWidget(MAXIMIZE_ALLOWED, MINIMIZE_ALLOWED,
                                   CLOSE_BUTTON_VISIBLE);
  widget1->GetNativeWindow()->SetProperty(chromeos::kAppTypeKey,
                                          chromeos::AppType::ARC_APP);
  FrameCaptionButtonContainerView container1(widget1);
  InitContainer(&container1);
  views::test::RunScheduledLayout(&container1);
  FrameCaptionButtonContainerView::TestApi t1(&container1);
  EXPECT_TRUE(t1.minimize_button()->GetVisible());
  EXPECT_TRUE(t1.size_button()->GetVisible());
  EXPECT_TRUE(t1.close_button()->GetVisible());
  EXPECT_FALSE(t1.float_button()->GetVisible());
  EXPECT_TRUE(CheckButtonsAtEdges(&container1, *t1.minimize_button(),
                                  *t1.close_button()));

  // The float button should be visible when minimizing is allowed but
  // maximizing (resizing) is disallowed.
  auto* widget2 = CreateTestWidget(MAXIMIZE_DISALLOWED, MINIMIZE_ALLOWED,
                                   CLOSE_BUTTON_VISIBLE);
  widget2->GetNativeWindow()->SetProperty(chromeos::kAppTypeKey,
                                          chromeos::AppType::ARC_APP);
  FrameCaptionButtonContainerView container2(widget2);
  InitContainer(&container2);
  views::test::RunScheduledLayout(&container2);
  FrameCaptionButtonContainerView::TestApi t2(&container2);
  EXPECT_TRUE(t2.minimize_button()->GetVisible());
  EXPECT_FALSE(t2.size_button()->GetVisible());
  EXPECT_TRUE(t2.close_button()->GetVisible());
  EXPECT_TRUE(t2.float_button()->GetVisible());
  EXPECT_TRUE(CheckButtonsAtEdges(&container2, *t2.minimize_button(),
                                  *t2.close_button()));
}

TEST_F(FrameCaptionButtonContainerViewTest, TestFloatButtonBehavior) {
  auto* widget = CreateTestWidget(MAXIMIZE_DISALLOWED, MINIMIZE_ALLOWED,
                                  CLOSE_BUTTON_VISIBLE);
  auto* window = widget->GetNativeWindow();
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
  widget->Show();

  FrameCaptionButtonContainerView container(widget);
  InitContainer(&container);
  widget->GetContentsView()->AddChildView(&container);
  views::test::RunScheduledLayout(&container);
  FrameCaptionButtonContainerView::TestApi test_api(&container);

  LeftClickOn(test_api.float_button());
  auto* window_state = WindowState::Get(window);
  // Check if window is floated.
  EXPECT_TRUE(window_state->IsFloated());
  EXPECT_EQ(window->GetProperty(chromeos::kWindowStateTypeKey),
            chromeos::WindowStateType::kFloated);

  LeftClickOn(test_api.float_button());
  // Check if window is unfloated.
  EXPECT_FALSE(window_state->IsFloated());
  EXPECT_EQ(window->GetProperty(chromeos::kWindowStateTypeKey),
            chromeos::WindowStateType::kNormal);
}

}  // namespace ash
