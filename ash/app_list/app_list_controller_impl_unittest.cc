// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include <set>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/test/apps_grid_view_test_api.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/ime/test_ime_controller_client.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/views/controls/textfield/textfield_test_api.h"

namespace ash {

namespace {

void PressHomeButton() {
  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      AppListShowSource::kShelfButton, base::TimeTicks());
}

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

AppListView* GetAppListView() {
  return Shell::Get()->app_list_controller()->presenter()->GetView();
}

ContentsView* GetContentsView() {
  return GetAppListView()->app_list_main_view()->contents_view();
}

views::View* GetExpandArrowView() {
  return GetContentsView()->expand_arrow_view();
}

bool GetExpandArrowViewVisibility() {
  return GetExpandArrowView()->GetVisible();
}

SearchBoxView* GetSearchBoxView() {
  return GetContentsView()->GetSearchBoxView();
}

aura::Window* GetVirtualKeyboardWindow() {
  return Shell::Get()
      ->keyboard_controller()
      ->keyboard_ui_controller()
      ->GetKeyboardWindow();
}

AppsGridView* GetAppsGridView() {
  return GetContentsView()->apps_container_view()->apps_grid_view();
}

void ShowAppListNow() {
  Shell::Get()->app_list_controller()->presenter()->Show(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      base::TimeTicks::Now());
}

void DismissAppListNow() {
  Shell::Get()->app_list_controller()->presenter()->Dismiss(
      base::TimeTicks::Now());
}

aura::Window* GetAppListViewNativeWindow() {
  return GetAppListView()->GetWidget()->GetNativeView();
}

void SetSearchText(AppListControllerImpl* controller, const std::string& text) {
  controller->GetSearchModel()->search_box()->Update(base::ASCIIToUTF16(text),
                                                     false);
}

}  // namespace

class AppListControllerImplTest : public AshTestBase {
 public:
  AppListControllerImplTest() = default;
  ~AppListControllerImplTest() override = default;

  std::unique_ptr<aura::Window> CreateTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

  void PopulateItem(int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<AppListItem> item(
          new AppListItem("app_id" + base::UTF16ToUTF8(base::FormatNumber(i))));
      Shell::Get()->app_list_controller()->GetModel()->AddItem(std::move(item));
    }
  }

  bool IsAppListBoundsAnimationRunning() {
    AppListView* app_list_view = GetAppListTestHelper()->GetAppListView();
    ui::Layer* widget_layer =
        app_list_view ? app_list_view->GetWidget()->GetLayer() : nullptr;
    return widget_layer && !widget_layer->GetAnimator()->is_animating();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerImplTest);
};

// Tests that the AppList hides when shelf alignment changes. This necessary
// because the AppList is shown with certain assumptions based on shelf
// orientation.
TEST_F(AppListControllerImplTest, AppListHiddenWhenShelfAlignmentChanges) {
  Shelf* const shelf = AshTestBase::GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kBottom);

  const std::vector<ShelfAlignment> alignments(
      {ShelfAlignment::kLeft, ShelfAlignment::kRight, ShelfAlignment::kBottom});
  for (ShelfAlignment alignment : alignments) {
    ShowAppListNow();
    EXPECT_TRUE(Shell::Get()
                    ->app_list_controller()
                    ->presenter()
                    ->IsVisibleDeprecated());
    shelf->SetAlignment(alignment);
    EXPECT_EQ(AppListViewState::kClosed, GetAppListView()->app_list_state());
  }
}

// In clamshell mode, when the AppListView's bottom is on the display edge
// and app list state is HALF, the rounded corners should be hidden
// (https://crbug.com/942084).
TEST_F(AppListControllerImplTest, HideRoundingCorners) {
  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);

  // Show the app list view and click on the search box with mouse. So the
  // VirtualKeyboard is shown.
  ShowAppListNow();
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);

  // Wait until the virtual keyboard shows on the screen.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Test the following things:
  // (1) AppListView is at the top of the screen.
  // (2) AppListView's state is HALF.
  // (3) AppListBackgroundShield is translated to hide the rounded corners.
  aura::Window* native_window = GetAppListView()->GetWidget()->GetNativeView();
  gfx::Rect app_list_screen_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(0, app_list_screen_bounds.y());
  EXPECT_EQ(AppListViewState::kHalf, GetAppListView()->app_list_state());
  gfx::Transform expected_transform;
  expected_transform.Translate(0, -(ShelfConfig::Get()->shelf_size() / 2));
  EXPECT_EQ(
      expected_transform,
      GetAppListView()->GetAppListBackgroundShieldForTest()->GetTransform());

  // Set the search box inactive and wait until the virtual keyboard is hidden.
  GetSearchBoxView()->SetSearchBoxActive(false, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Test that the rounded corners should show again.
  expected_transform = gfx::Transform();
  EXPECT_EQ(
      expected_transform,
      GetAppListView()->GetAppListBackgroundShieldForTest()->GetTransform());
}

// Verify that when the emoji panel shows and AppListView is in Peeking state,
// AppListView's rounded corners should be hidden (see https://crbug.com/950468)
TEST_F(AppListControllerImplTest, HideRoundingCornersWhenEmojiShows) {
  // Set IME client. Otherwise the emoji panel is unable to show.
  ImeController* ime_controller = Shell::Get()->ime_controller();
  TestImeControllerClient client;
  ime_controller->SetClient(&client);

  // Show the app list view and right-click on the search box with mouse. So the
  // text field's context menu shows.
  ShowAppListNow();
  SearchBoxView* search_box_view =
      GetAppListView()->app_list_main_view()->search_box_view();
  gfx::Point center_point = search_box_view->GetBoundsInScreen().CenterPoint();
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(center_point);
  event_generator->ClickRightButton();

  // Expect that the first item in the context menu should be "Emoji". Show the
  // emoji panel.
  auto text_field_api =
      std::make_unique<views::TextfieldTestApi>(search_box_view->search_box());
  ASSERT_EQ("Emoji",
            base::UTF16ToUTF8(
                text_field_api->context_menu_contents()->GetLabelAt(0)));
  text_field_api->context_menu_contents()->ActivatedAt(0);

  // Wait for enough time. Then expect that AppListView is pushed up.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(gfx::Point(0, 0), GetAppListView()->GetBoundsInScreen().origin());

  // AppListBackgroundShield is translated to hide the rounded corners.
  gfx::Transform expected_transform;
  expected_transform.Translate(0, -(ShelfConfig::Get()->shelf_size() / 2));
  EXPECT_EQ(
      expected_transform,
      GetAppListView()->GetAppListBackgroundShieldForTest()->GetTransform());
}

// Verifies that the dragged item has the correct focusable siblings after drag
// (https://crbug.com/990071).
TEST_F(AppListControllerImplTest, CheckTabOrderAfterDragIconToShelf) {
  // Adds three items to AppsGridView.
  PopulateItem(3);

  // Shows the app list in fullscreen.
  ShowAppListNow();
  ASSERT_EQ(AppListViewState::kPeeking, GetAppListView()->app_list_state());
  GetEventGenerator()->GestureTapAt(
      GetExpandArrowView()->GetBoundsInScreen().CenterPoint());
  ASSERT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  test::AppsGridViewTestApi apps_grid_view_test_api(GetAppsGridView());
  const AppListItemView* item1 =
      apps_grid_view_test_api.GetViewAtIndex(GridIndex(0, 0));
  AppListItemView* item2 =
      apps_grid_view_test_api.GetViewAtIndex(GridIndex(0, 1));
  const AppListItemView* item3 =
      apps_grid_view_test_api.GetViewAtIndex(GridIndex(0, 2));

  // Verifies that AppListItemView has the correct focusable siblings before
  // drag.
  ASSERT_EQ(item1, item2->GetPreviousFocusableView());
  ASSERT_EQ(item3, item2->GetNextFocusableView());

  // Pins |item2| by dragging it to ShelfView.
  ShelfView* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  ASSERT_EQ(0, shelf_view->view_model()->view_size());
  GetEventGenerator()->MoveMouseTo(item2->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();
  item2->FireMouseDragTimerForTest();
  GetEventGenerator()->MoveMouseTo(
      shelf_view->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(GetAppsGridView()->FireDragToShelfTimerForTest());
  GetEventGenerator()->ReleaseLeftButton();
  ASSERT_EQ(1, shelf_view->view_model()->view_size());

  // Verifies that the dragged item has the correct previous/next focusable
  // view after drag.
  EXPECT_EQ(item1, item2->GetPreviousFocusableView());
  EXPECT_EQ(item3, item2->GetNextFocusableView());
}

TEST_F(AppListControllerImplTest, PageResetByTimerInTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  PopulateItem(30);

  ShowAppListNow();

  AppsGridView* apps_grid_view = GetAppsGridView();
  apps_grid_view->pagination_model()->SelectPage(1, false /* animate */);

  DismissAppListNow();

  // When timer is not skipped the selected page should not change when app list
  // is closed.
  EXPECT_EQ(1, apps_grid_view->pagination_model()->selected_page());

  // Skip the page reset timer to simulate timer exipration.
  GetAppListView()->SetSkipPageResetTimerForTesting(true);

  ShowAppListNow();
  EXPECT_EQ(1, apps_grid_view->pagination_model()->selected_page());
  DismissAppListNow();

  // Once the app list is closed, the page should be reset when the timer is
  // skipped.
  EXPECT_EQ(0, apps_grid_view->pagination_model()->selected_page());
}

// Verifies that in clamshell mode the bounds of AppListView are correct when
// the AppListView is in PEEKING state and the virtual keyboard is enabled (see
// https://crbug.com/944233).
TEST_F(AppListControllerImplTest, CheckAppListViewBoundsWhenVKeyboardEnabled) {
  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);

  // Show the AppListView and click on the search box with mouse. So the
  // VirtualKeyboard is shown. Wait until the virtual keyboard shows.
  ShowAppListNow();
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Hide the AppListView. Wait until the virtual keyboard is hidden as well.
  DismissAppListNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Show the AppListView again. Check the following things:
  // (1) Virtual keyboard does not show.
  // (2) AppListView is in PEEKING state.
  // (3) AppListView's bounds are the same as the preferred bounds for
  // the PEEKING state.
  ShowAppListNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AppListViewState::kPeeking, GetAppListView()->app_list_state());
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());
  EXPECT_EQ(GetAppListView()->GetPreferredWidgetBoundsForState(
                AppListViewState::kPeeking),
            GetAppListViewNativeWindow()->bounds());
}

// Verifies that the the virtual keyboard does not get shown if the search box
// is activated by user typing when the app list in the peeking state.
TEST_F(AppListControllerImplTest, VirtualKeyboardNotShownWhenUserStartsTyping) {
  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);

  // Show the AppListView, then simulate a key press - verify that the virtual
  // keyboard is not shown.
  ShowAppListNow();
  EXPECT_EQ(AppListViewState::kPeeking, GetAppListView()->app_list_state());
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  event_generator->ReleaseKey(ui::KeyboardCode::VKEY_0, 0);
  EXPECT_EQ(AppListViewState::kHalf, GetAppListView()->app_list_state());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetVirtualKeyboardWindow()->IsVisible());

  // The keyboard should get shown if the user taps on the search box.
  event_generator->GestureTapAt(
      GetAppListView()->search_box_view()->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(keyboard::WaitUntilShown());

  DismissAppListNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());
}

// Verifies that in clamshell mode the AppListView bounds remain in the
// fullscreen size while the virtual keyboard is shown, even if the app list
// view state changes.
TEST_F(AppListControllerImplTest,
       AppListViewBoundsRemainFullScreenWhenVKeyboardEnabled) {
  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);

  // Show the AppListView in fullscreen state and click on the search box with
  // the mouse. So the VirtualKeyboard is shown. Wait until the virtual keyboard
  // shows.
  ShowAppListNow();
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AppListViewState::kHalf, GetAppListView()->app_list_state());
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  EXPECT_EQ(0, GetAppListView()->GetBoundsInScreen().y());

  // Simulate half state getting set again, and but verify the app list bounds
  // remain at the top of the screen.
  GetAppListView()->SetState(AppListViewState::kHalf);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AppListViewState::kHalf, GetAppListView()->app_list_state());

  EXPECT_EQ(0, GetAppListView()->GetBoundsInScreen().y());

  // Close the virtual keyboard. Wait until it is hidden.
  Shell::Get()->keyboard_controller()->HideKeyboard(HideReason::kUser);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Verify the app list bounds have been updated to match kHalf state.
  EXPECT_EQ(AppListViewState::kHalf, GetAppListView()->app_list_state());
  const gfx::Rect shelf_bounds =
      AshTestBase::GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(shelf_bounds.bottom() - 545 /*half app list height*/,
            GetAppListView()->GetBoundsInScreen().y());
}

// Verifies that in tablet mode, the AppListView has correct bounds when the
// virtual keyboard is dismissed (see https://crbug.com/944133).
TEST_F(AppListControllerImplTest, CheckAppListViewBoundsWhenDismissVKeyboard) {
  Shell::Get()->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);

  // Show the AppListView and click on the search box with mouse so the
  // VirtualKeyboard is shown. Wait until the virtual keyboard shows.
  ShowAppListNow();
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Turn on the tablet mode. The virtual keyboard should still show.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Close the virtual keyboard. Wait until it is hidden.
  Shell::Get()->keyboard_controller()->HideKeyboard(HideReason::kUser);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Check the following things:
  // (1) AppListView's state is FULLSCREEN_SEARCH
  // (2) AppListView's bounds are the same as the preferred bounds for
  // the FULLSCREEN_SEARCH state.
  EXPECT_EQ(AppListViewState::kFullscreenSearch,
            GetAppListView()->app_list_state());
  EXPECT_EQ(GetAppListView()->GetPreferredWidgetBoundsForState(
                AppListViewState::kFullscreenSearch),
            GetAppListViewNativeWindow()->bounds());
}

#if defined(ADDRESS_SANITIZER)
#define MAYBE_CloseNotificationWithAppListShown \
  DISABLED_CloseNotificationWithAppListShown
#else
#define MAYBE_CloseNotificationWithAppListShown \
  CloseNotificationWithAppListShown
#endif

// Verifies that closing notification by gesture should not dismiss the AppList.
// (see https://crbug.com/948344)
// TODO(crbug.com/1120501): Test is flaky on ASAN builds.
TEST_F(AppListControllerImplTest, MAYBE_CloseNotificationWithAppListShown) {
  ShowAppListNow();

  // Add one notification.
  ASSERT_EQ(
      0u, message_center::MessageCenter::Get()->GetPopupNotifications().size());
  const std::string notification_id("id");
  const std::string notification_title("title");
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_BASE_FORMAT, notification_id,
          base::UTF8ToUTF16(notification_title),
          base::UTF8ToUTF16("test message"), gfx::Image(),
          base::string16() /* display_source */, GURL(),
          message_center::NotifierId(), message_center::RichNotificationData(),
          new message_center::NotificationDelegate()));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(
      1u, message_center::MessageCenter::Get()->GetPopupNotifications().size());

  // Calculate the drag start point and end point.
  SystemTrayTestApi test_api;
  message_center::MessagePopupView* popup_view =
      test_api.GetPopupViewForNotificationID(notification_id);
  ASSERT_TRUE(popup_view);
  gfx::Rect bounds_in_screen = popup_view->GetBoundsInScreen();
  const gfx::Point drag_start = bounds_in_screen.left_center();
  const gfx::Point drag_end = bounds_in_screen.right_center();

  // Swipe away notification by gesture. Verifies that AppListView still shows.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureScrollSequence(
      drag_start, drag_end, base::TimeDelta::FromMicroseconds(500), 10);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetAppListView());
  EXPECT_EQ(
      0u, message_center::MessageCenter::Get()->GetPopupNotifications().size());
}

// Verifiy that when showing the launcher, the virtual keyboard dismissed before
// will not show automatically due to the feature called "transient blur" (see
// https://crbug.com/1057320).
TEST_F(AppListControllerImplTest,
       TransientBlurIsNotTriggeredWhenShowingLauncher) {
  // Enable animation.
  ui::ScopedAnimationDurationScaleMode non_zero_duration(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Enable virtual keyboard.
  KeyboardController* const keyboard_controller =
      Shell::Get()->keyboard_controller();
  keyboard_controller->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kCommandLineEnabled);

  // Create |window1| which contains a textfield as child view.
  std::unique_ptr<aura::Window> window1 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  auto* widget = views::Widget::GetWidgetForNativeView(window1.get());
  std::unique_ptr<views::Textfield> text_field =
      std::make_unique<views::Textfield>();

  // Note that the bounds of |text_field| cannot be too small. Otherwise, it
  // may not receive the gesture event.
  text_field->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  const auto* text_field_p = text_field.get();
  widget->GetRootView()->AddChildView(std::move(text_field));
  wm::ActivateWindow(window1.get());
  widget->Show();

  // Create |window2|.
  std::unique_ptr<aura::Window> window2 =
      AshTestBase::CreateTestWindow(gfx::Rect(200, 0, 200, 200));
  window2->Show();

  // Tap at the textfield in |window1|. The virtual keyboard should be visible.
  const gfx::Point tap_point = text_field_p->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->GestureTapAt(tap_point);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Tap at the center of |window2| to hide the virtual keyboard.
  GetEventGenerator()->GestureTapAt(window2->GetBoundsInScreen().CenterPoint());
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(keyboard::WaitUntilHidden());

  // Press the home button to show the launcher. Wait for the animation of
  // launcher to finish. Note that the launcher does not exist before toggling
  // the home button.
  PressHomeButton();
  const base::TimeDelta delta = base::TimeDelta::FromMilliseconds(200);
  do {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delta);
    run_loop.Run();
  } while (IsAppListBoundsAnimationRunning());

  // Expect that the virtual keyboard is invisible when the launcher shows.
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
}

// Tests that full screen apps list opens when user touches on or near the
// expand view arrow. (see https://crbug.com/906858)
TEST_F(AppListControllerImplTest,
       EnterFullScreenModeAfterTappingNearExpandArrow) {
  // The bounds for the tap target of the expand arrow button, taken from
  // expand_arrow_view.cc |kTapTargetWidth| and |kTapTargetHeight|.
  constexpr int tapping_width = 156;
  constexpr int tapping_height = 72;

  ShowAppListNow();
  ASSERT_EQ(AppListViewState::kPeeking, GetAppListView()->app_list_state());

  // Get in screen bounds of arrow
  gfx::Rect expand_arrow = GetAppListView()
                               ->app_list_main_view()
                               ->contents_view()
                               ->expand_arrow_view()
                               ->GetBoundsInScreen();
  const int horizontal_padding = (tapping_width - expand_arrow.width()) / 2;
  const int vertical_padding = (tapping_height - expand_arrow.height()) / 2;
  expand_arrow.Inset(-horizontal_padding, -vertical_padding);

  // Tap expand arrow icon and check that full screen apps view is entered.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureTapAt(expand_arrow.CenterPoint());
  ASSERT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  // Hide the AppListView. Wait until animation is finished
  DismissAppListNow();
  base::RunLoop().RunUntilIdle();

  // Re-enter peeking mode and test that tapping on one of the bounds of the
  // tap target for the expand arrow icon still brings up full app list
  // view.
  ShowAppListNow();
  ASSERT_EQ(AppListViewState::kPeeking, GetAppListView()->app_list_state());

  event_generator->GestureTapAt(gfx::Point(expand_arrow.top_right().x() - 1,
                                           expand_arrow.top_right().y() + 1));

  ASSERT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());
}

// Regression test for https://crbug.com/1073548
// Verifies that app list shown from overview after toggling tablet mode can be
// closed.
TEST_F(AppListControllerImplTest,
       CloseAppListShownFromOverviewAfterTabletExit) {
  // Move to tablet mode and back.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);

  std::unique_ptr<aura::Window> w(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  OverviewController* const overview_controller =
      Shell::Get()->overview_controller();
  overview_controller->StartOverview();

  // Press home button - verify overview exits and the app list is shown.
  PressHomeButton();

  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(AppListViewState::kPeeking, GetAppListView()->app_list_state());
  GetAppListTestHelper()->CheckVisibility(true);
  ASSERT_TRUE(GetAppListView()->GetWidget());
  EXPECT_TRUE(GetAppListView()->GetWidget()->GetNativeWindow()->IsVisible());

  // Pressing home button again should close the app list.
  PressHomeButton();

  EXPECT_EQ(AppListViewState::kClosed, GetAppListView()->app_list_state());
  GetAppListTestHelper()->CheckVisibility(false);
  ASSERT_TRUE(GetAppListView()->GetWidget());
  EXPECT_FALSE(GetAppListView()->GetWidget()->GetNativeWindow()->IsVisible());
}

// Tests that swapping out an AppListModel (simulating a profile swap with
// multiprofile enabled) drops all references to previous folders (see
// https://crbug.com/1130901).
TEST_F(AppListControllerImplTest, SimulateProfileSwapNoCrashOnDestruct) {
  // Add a folder, whose AppListItemList the AppListModel will observe.
  const std::string folder_id("folder_1");
  auto folder = std::make_unique<AppListFolderItem>(folder_id);
  AppListModel* model = Shell::Get()->app_list_controller()->GetModel();
  model->AddItem(std::move(folder));

  for (int i = 0; i < 2; ++i) {
    auto item = std::make_unique<AppListItem>(base::StringPrintf("app_%d", i));
    model->AddItemToFolder(std::move(item), folder_id);
  }

  // Set a new model, simulating profile switching in multi-profile mode. This
  // should cleanly drop the reference to the folder added earlier.
  Shell::Get()->app_list_controller()->SetModelData(
      /*profile_id=*/12, /*apps=*/{}, /*is_search_engine_google=*/false);

  // Test that there is no crash on ~AppListModel() when the test finishes.
}

class AppListControllerImplTestWithNotificationBadging
    : public AppListControllerImplTest {
 public:
  AppListControllerImplTestWithNotificationBadging() {
    scoped_features_.InitWithFeatures({::features::kNotificationIndicator}, {});
  }
  AppListControllerImplTestWithNotificationBadging(
      const AppListControllerImplTestWithNotificationBadging& other) = delete;
  AppListControllerImplTestWithNotificationBadging& operator=(
      const AppListControllerImplTestWithNotificationBadging& other) = delete;
  ~AppListControllerImplTestWithNotificationBadging() override = default;

  void UpdateAppHasBadge(const std::string& app_id, bool app_has_badge) {
    AppListControllerImpl* controller = Shell::Get()->app_list_controller();
    AccountId account_id = AccountId::FromUserEmail("test@gmail.com");

    apps::mojom::App test_app;
    test_app.app_id = app_id;
    if (app_has_badge)
      test_app.has_badge = apps::mojom::OptionalBool::kTrue;
    else
      test_app.has_badge = apps::mojom::OptionalBool::kFalse;

    apps::AppUpdate test_update(nullptr, &test_app /* delta */, account_id);
    static_cast<apps::AppRegistryCache::Observer*>(controller)
        ->OnAppUpdate(test_update);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Tests that when an app has an update to its notification badge, the change
// gets propagated to the corresponding AppListItemView.
TEST_F(AppListControllerImplTestWithNotificationBadging,
       NotificationBadgeUpdateTest) {
  PopulateItem(1);
  ShowAppListNow();

  test::AppsGridViewTestApi apps_grid_view_test_api(GetAppsGridView());
  const AppListItemView* item_view =
      apps_grid_view_test_api.GetViewAtIndex(GridIndex(0, 0));
  ASSERT_TRUE(item_view);

  const std::string app_id = item_view->item()->id();

  EXPECT_FALSE(item_view->IsNotificationIndicatorShownForTest());

  UpdateAppHasBadge(app_id, /*has_badge=*/true);
  EXPECT_TRUE(item_view->IsNotificationIndicatorShownForTest());

  UpdateAppHasBadge(app_id, /*has_badge=*/false);
  EXPECT_FALSE(item_view->IsNotificationIndicatorShownForTest());
}

class AppListControllerImplTestWithoutHotseat
    : public AppListControllerImplTest {
 public:
  AppListControllerImplTestWithoutHotseat() = default;
  ~AppListControllerImplTestWithoutHotseat() override = default;
  // AshTestBase:
  void SetUp() override {
    // The feature verified by this test is only enabled if drag from shelf to
    // home or overview (which is controlled by hotseat flag) is disabled.
    scoped_features_.InitWithFeatures({}, {chromeos::features::kShelfHotseat});
    AppListControllerImplTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
  DISALLOW_COPY_AND_ASSIGN(AppListControllerImplTestWithoutHotseat);
};

// Hide the expand arrow view in tablet mode when there is no activatable window
// (see https://crbug.com/923089).
TEST_F(AppListControllerImplTestWithoutHotseat,
       UpdateExpandArrowViewVisibility) {
  // Turn on the tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());

  // No activatable windows. So hide the expand arrow view.
  EXPECT_FALSE(GetExpandArrowViewVisibility());

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());

  // Activate w1 then press home launcher button. Expand arrow view should show
  // because w1 still exists.
  wm::ActivateWindow(w1.get());
  Shell::Get()->home_screen_controller()->GoHome(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(WindowStateType::kMinimized,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_TRUE(GetExpandArrowViewVisibility());

  // Activate w2 then close w1. w2 still exists so expand arrow view shows.
  wm::ActivateWindow(w2.get());
  w1.reset();
  EXPECT_TRUE(GetExpandArrowViewVisibility());

  // No activatable windows. Hide the expand arrow view.
  w2.reset();
  EXPECT_FALSE(GetExpandArrowViewVisibility());
}

class HotseatAppListControllerImplTest : public base::test::WithFeatureOverride,
                                         public AppListControllerImplTest {
 public:
  HotseatAppListControllerImplTest()
      : WithFeatureOverride(chromeos::features::kShelfHotseat) {}
  ~HotseatAppListControllerImplTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
  DISALLOW_COPY_AND_ASSIGN(HotseatAppListControllerImplTest);
};

// Tests with both hotseat disabled and enabled.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(HotseatAppListControllerImplTest);

// Verifies that the pinned app should still show after canceling the drag from
// AppsGridView to Shelf (https://crbug.com/1021768).
TEST_P(HotseatAppListControllerImplTest, DragItemFromAppsGridView) {
  // Turn on the tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());

  Shelf* const shelf = GetPrimaryShelf();

  // Add icons with the same app id to Shelf and AppsGridView respectively.
  ShelfViewTestAPI shelf_view_test_api(shelf->GetShelfViewForTesting());
  std::string app_id = shelf_view_test_api.AddItem(TYPE_PINNED_APP).app_id;
  Shell::Get()->app_list_controller()->GetModel()->AddItem(
      std::make_unique<AppListItem>(app_id));

  AppsGridView* apps_grid_view = GetAppsGridView();
  AppListItemView* app_list_item_view =
      test::AppsGridViewTestApi(apps_grid_view).GetViewAtIndex(GridIndex(0, 0));
  views::View* shelf_icon_view =
      shelf->GetShelfViewForTesting()->view_model()->view_at(0);

  // Drag the app icon from AppsGridView to Shelf. Then move the icon back to
  // AppsGridView before drag ends.
  GetEventGenerator()->MoveMouseTo(
      app_list_item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();
  app_list_item_view->FireMouseDragTimerForTest();
  GetEventGenerator()->MoveMouseTo(
      shelf_icon_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->MoveMouseTo(
      apps_grid_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ReleaseLeftButton();

  // The icon's opacity updates at the end of animation.
  shelf_view_test_api.RunMessageLoopUntilAnimationsDone();

  // The icon is pinned before drag starts. So the shelf icon should show in
  // spite that drag is canceled.
  EXPECT_TRUE(shelf_icon_view->GetVisible());
  EXPECT_EQ(1.0f, shelf_icon_view->layer()->opacity());
}

// Tests for HomeScreenDelegate::GetInitialAppListItemScreenBoundsForWindow
// implemtenation.
TEST_P(HotseatAppListControllerImplTest, GetItemBoundsForWindow) {
  // Populate app list model with 25 items, of which items at indices in
  // |folders| are folders containing a single item.
  const std::set<int> folders = {5, 23};
  AppListModel* model = Shell::Get()->app_list_controller()->GetModel();
  for (int i = 0; i < 25; ++i) {
    if (folders.count(i)) {
      const std::string folder_id = base::StringPrintf("fake_folder_%d", i);
      auto folder = std::make_unique<AppListFolderItem>(folder_id);
      model->AddItem(std::move(folder));
      auto item = std::make_unique<AppListItem>(
          base::StringPrintf("fake_id_in_folder_%d", i));
      model->AddItemToFolder(std::move(item), folder_id);
    } else {
      auto item =
          std::make_unique<AppListItem>(base::StringPrintf("fake_id_%d", i));
      model->AddItem(std::move(item));
    }
  }
  // Enable tablet mode - this should also show the app list.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  AppsGridView* apps_grid_view = GetAppListView()
                                     ->app_list_main_view()
                                     ->contents_view()
                                     ->apps_container_view()
                                     ->apps_grid_view();
  auto apps_grid_test_api =
      std::make_unique<test::AppsGridViewTestApi>(apps_grid_view);

  const struct {
    // The kAppIDKey property value for the test window.
    std::string window_app_id;
    // If set the grid position of the app list item whose screen bounds should
    // be returned by GetInitialAppListItemScreenBoundsForWindow().
    // If nullopt, GetInitialAppListItemScreenBoundsForWindow() is expected to
    // return the apps grid center rect.
    base::Optional<GridIndex> grid_position;
  } kTestCases[] = {{"fake_id_0", GridIndex(0, 0)},
                    {"fake_id_2", GridIndex(0, 2)},
                    {"fake_id_in_folder_5", base::nullopt},
                    {"fake_id_15", GridIndex(0, 15)},
                    {"fake_id_in_folder_23", base::nullopt},
                    {"non_existent", base::nullopt},
                    {"", base::nullopt},
                    {"fake_id_22", base::nullopt}};

  // Tests the case app ID property is not set on the window.
  std::unique_ptr<aura::Window> window_without_app_id(CreateTestWindow());

  HomeScreenDelegate* const home_screen_delegate =
      Shell::Get()->home_screen_controller()->delegate();
  // NOTE: Calculate the apps grid bounds after test window is shown, as showing
  // the window can change the app list layout (due to the change in the shelf
  // height).
  const gfx::Rect apps_grid_bounds = apps_grid_view->GetBoundsInScreen();
  const gfx::Rect apps_grid_center =
      gfx::Rect(apps_grid_bounds.CenterPoint(), gfx::Size(1, 1));

  EXPECT_EQ(apps_grid_center,
            home_screen_delegate->GetInitialAppListItemScreenBoundsForWindow(
                window_without_app_id.get()));

  // Run tests cases, both for when the first and the second apps grid page is
  // selected - the returned bounds should be the same in both cases, as
  // GetInitialAppListItemScreenBoundsForWindow() always returns bounds state
  // for the first page.
  for (int selected_page = 0; selected_page < 2; ++selected_page) {
    GetAppListView()->GetAppsPaginationModel()->SelectPage(selected_page,
                                                           false);
    for (const auto& test_case : kTestCases) {
      SCOPED_TRACE(::testing::Message()
                   << "Test case: {" << test_case.window_app_id << ", "
                   << (test_case.grid_position.has_value()
                           ? test_case.grid_position->ToString()
                           : "null")
                   << "} with selected page " << selected_page);

      std::unique_ptr<aura::Window> window(CreateTestWindow());
      window->SetProperty(kAppIDKey, new std::string(test_case.window_app_id));

      const gfx::Rect item_bounds =
          home_screen_delegate->GetInitialAppListItemScreenBoundsForWindow(
              window.get());
      if (!test_case.grid_position.has_value()) {
        EXPECT_EQ(apps_grid_center, item_bounds);
      } else {
        const int kItemsPerRow = 5;
        gfx::Rect expected_bounds =
            apps_grid_test_api->GetItemTileRectOnCurrentPageAt(
                test_case.grid_position->slot / kItemsPerRow,
                test_case.grid_position->slot % kItemsPerRow);
        expected_bounds.Offset(apps_grid_bounds.x(), apps_grid_bounds.y());
        EXPECT_EQ(expected_bounds, item_bounds);
      }
    }
  }
}

// Verifies that apps grid and hotseat bounds do not overlap when switching from
// side shelf app list to tablet mode.
TEST_P(HotseatAppListControllerImplTest,
       NoOverlapWithHotseatOnSwitchFromSideShelf) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  Shelf* const shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kRight);
  ShowAppListNow();
  ASSERT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  gfx::Rect apps_grid_view_bounds = GetAppsGridView()->GetBoundsInScreen();
  EXPECT_FALSE(apps_grid_view_bounds.Intersects(
      shelf->shelf_widget()->GetWindowBoundsInScreen()));
  EXPECT_FALSE(apps_grid_view_bounds.Intersects(
      shelf->hotseat_widget()->GetWindowBoundsInScreen()));

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  apps_grid_view_bounds = GetAppsGridView()->GetBoundsInScreen();
  EXPECT_FALSE(apps_grid_view_bounds.Intersects(
      shelf->shelf_widget()->GetWindowBoundsInScreen()));
  EXPECT_FALSE(apps_grid_view_bounds.Intersects(
      shelf->hotseat_widget()->GetWindowBoundsInScreen()));
}

// The test parameter indicates whether the shelf should auto-hide. In either
// case the animation behaviors should be the same.
class AppListAnimationTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  AppListAnimationTest() = default;
  ~AppListAnimationTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    Shelf* const shelf = AshTestBase::GetPrimaryShelf();
    shelf->SetAlignment(ShelfAlignment::kBottom);

    if (GetParam()) {
      shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
    }

    // The shelf should be shown at this point despite auto hide behavior, given
    // that no windows are shown.
    shown_shelf_bounds_ = shelf->shelf_widget()->GetWindowBoundsInScreen();
  }

  int GetAppListCurrentTop() {
    gfx::Point app_list_top =
        GetAppListView()->GetBoundsInScreen().top_center();
    GetAppListView()->GetWidget()->GetLayer()->transform().TransformPoint(
        &app_list_top);
    return app_list_top.y();
  }

  int GetAppListTargetTop() {
    gfx::Point app_list_top =
        GetAppListView()->GetBoundsInScreen().top_center();
    GetAppListView()
        ->GetWidget()
        ->GetLayer()
        ->GetTargetTransform()
        .TransformPoint(&app_list_top);
    return app_list_top.y();
  }

  int shown_shelf_top() const { return shown_shelf_bounds_.y(); }

  // The offset that should be animated between kPeeking and kClosed app list
  // view states - the vertical distance between shelf top (in shown state) and
  // the app list top in peeking state.
  int PeekingHeightOffset() const {
    return shown_shelf_bounds_.y() - PeekingHeightTop();
  }

  // The app list view y coordinate in peeking state.
  int PeekingHeightTop() const {
    return shown_shelf_bounds_.bottom() -
           AppListConfig::instance().peeking_app_list_height();
  }

 private:
  // Set during setup.
  gfx::Rect shown_shelf_bounds_;

  DISALLOW_COPY_AND_ASSIGN(AppListAnimationTest);
};

INSTANTIATE_TEST_SUITE_P(AutoHideShelf, AppListAnimationTest, testing::Bool());

// Tests app list animation to peeking state.
TEST_P(AppListAnimationTest, AppListShowPeekingAnimation) {
  // Set the normal transition duration so tests can easily determine intended
  // animation length, and calculate expected app list position at different
  // animation step points. Also, prevents the app list view to snapping to the
  // final position.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  ShowAppListNow();

  // Verify that the app list view's top matches the shown shelf top as the show
  // animation starts.
  EXPECT_EQ(shown_shelf_top(), GetAppListCurrentTop());
  EXPECT_EQ(PeekingHeightTop(), GetAppListTargetTop());
}

// Tests app list animation from peeking to closed state.
TEST_P(AppListAnimationTest, AppListCloseFromPeekingAnimation) {
  ShowAppListNow();

  // Set the normal transition duration so tests can easily determine intended
  // animation length, and calculate expected app list position at different
  // animation step points. Also, prevents the app list view to snapping to the
  // final position.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Dismiss app list, initial app list position should be at peeking height.
  const int offset_to_animate = PeekingHeightOffset();
  DismissAppListNow();
  EXPECT_EQ(shown_shelf_top() - offset_to_animate, GetAppListCurrentTop());
  EXPECT_EQ(shown_shelf_top(), GetAppListTargetTop());
}

// Tests app list close animation when app list gets dismissed while animating
// to peeking state.
TEST_P(AppListAnimationTest, AppListDismissWhileShowingPeeking) {
  // Set the normal transition duration so tests can easily determine intended
  // animation length, and calculate expected app list position at different
  // animation step points. Also, prevents the app list view to snapping to the
  // final position.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  ShowAppListNow();

  // Verify that the app list view's top matches the shown shelf top as the show
  // animation starts.
  EXPECT_EQ(shown_shelf_top(), GetAppListCurrentTop());
  EXPECT_EQ(PeekingHeightTop(), GetAppListTargetTop());

  // Start dismissing app list. Verify the new animation starts at the same
  // point the show animation ended.
  DismissAppListNow();

  EXPECT_EQ(shown_shelf_top(), GetAppListTargetTop());
}

// Tests app list animation when show is requested while app list close
// animation is in progress.
TEST_P(AppListAnimationTest, AppListShowPeekingWhileClosing) {
  // Show app list while animations are still instantanious.
  ShowAppListNow();

  // Set the normal transition duration so tests can easily determine intended
  // animation length, and calculate expected app list position at different
  // animation step points. Also, prevents the app list view to snapping to the
  // final position.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  int offset_to_animate = PeekingHeightOffset();
  DismissAppListNow();

  // Verify that the app list view's top initially matches the peeking height.
  EXPECT_EQ(shown_shelf_top() - offset_to_animate, GetAppListCurrentTop());
  EXPECT_EQ(shown_shelf_top(), GetAppListTargetTop());

  // Start showing the app list. Verify the new animation starts at the same
  // point the show animation ended.
  ShowAppListNow();

  EXPECT_EQ(PeekingHeightTop(), GetAppListTargetTop());
}

// Tests that how search box opacity is animated when the app list is shown and
// closed.
TEST_P(AppListAnimationTest, SearchBoxOpacityDuringShowAndClose) {
  // Set a transition duration that prevents the app list view from snapping to
  // the final position.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ShowAppListNow();

  SearchBoxView* const search_box = GetSearchBoxView();

  // The search box opacity should start  at 0, and animate to 1.
  EXPECT_EQ(0.0f, search_box->layer()->opacity());
  EXPECT_EQ(1.0f, search_box->layer()->GetTargetOpacity());

  // If the app list is closed while the animation is still in progress, the
  // search box opacity should animate from the current opacity.
  DismissAppListNow();

  EXPECT_EQ(0.0f, search_box->layer()->opacity());
  EXPECT_EQ(0.0f, search_box->layer()->GetTargetOpacity());

  search_box->layer()->GetAnimator()->StopAnimating();

  // When show again, verify the app list animates from 0 opacity again.
  ShowAppListNow();

  EXPECT_EQ(0.0f, search_box->layer()->opacity());
  EXPECT_EQ(1.0f, search_box->layer()->GetTargetOpacity());

  search_box->layer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(1.0f, search_box->layer()->opacity());

  // Search box opacity animates from the current (full opacity) when closed
  // from shown state.
  DismissAppListNow();

  EXPECT_EQ(1.0f, search_box->layer()->opacity());
  EXPECT_EQ(0.0f, search_box->layer()->GetTargetOpacity());

  // If the app list is show again during close animation, the search box
  // opacity should animate from the current value.
  ShowAppListNow();

  EXPECT_EQ(1.0f, search_box->layer()->opacity());
  EXPECT_EQ(1.0f, search_box->layer()->GetTargetOpacity());
}

class AppListControllerImplMetricsTest : public AshTestBase {
 public:
  AppListControllerImplMetricsTest() = default;
  ~AppListControllerImplMetricsTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = Shell::Get()->app_list_controller();
    PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(true);
  }

  void TearDown() override {
    PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    AshTestBase::TearDown();
  }

  AppListControllerImpl* controller_;
  const base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerImplMetricsTest);
};

TEST_F(AppListControllerImplMetricsTest, LogSingleResultListClick) {
  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     0);
  SetSearchText(controller_, "");
  controller_->LogResultLaunchHistogram(SearchResultLaunchLocation::kResultList,
                                        4);
  histogram_tester_.ExpectUniqueSample(kAppListResultLaunchIndexAndQueryLength,
                                       4, 1);
}

TEST_F(AppListControllerImplMetricsTest, LogOneClickInEveryBucket) {
  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     0);
  for (int query_length = 0; query_length < 11; ++query_length) {
    const std::string query(query_length, 'a');
    for (int click_index = 0; click_index < 7; ++click_index) {
      SetSearchText(controller_, query);
      controller_->LogResultLaunchHistogram(
          SearchResultLaunchLocation::kResultList, click_index);
    }
  }

  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     77);
  for (int query_length = 0; query_length < 11; ++query_length) {
    for (int click_index = 0; click_index < 7; ++click_index) {
      histogram_tester_.ExpectBucketCount(
          kAppListResultLaunchIndexAndQueryLength,
          7 * query_length + click_index, 1);
    }
  }
}

// One edge case may do harm to the presentation metrics reporter for tablet
// mode: the user may keep pressing on launcher while exiting the tablet mode by
// rotating the lid. In this situation, OnHomeLauncherDragEnd is not triggered.
// It is handled correctly now because the AppListView is always closed after
// exiting the tablet mode. But it still has potential risk to break in future.
// Write this test case for precaution (https://crbug.com/947105).
TEST_F(AppListControllerImplMetricsTest,
       PresentationMetricsForTabletNotRecordedInClamshell) {
  // Wait until the construction of TabletModeController finishes.
  base::RunLoop().RunUntilIdle();

  // Turn on the tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());

  // Create a window then press the home launcher button. Expect that |w| is
  // hidden.
  std::unique_ptr<aura::Window> w(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  Shell::Get()->home_screen_controller()->GoHome(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(w->IsVisible());
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  gfx::Point start =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen().top_right();
  base::TimeTicks timestamp = base::TimeTicks::Now();

  // Emulate to drag the launcher downward.
  // Send SCROLL_START event. Check the presentation metrics values.
  ui::GestureEvent start_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 1));
  GetAppListView()->OnGestureEvent(&start_event);

  // Turn off the tablet mode before scrolling is finished.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(IsTabletMode());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AppListViewState::kClosed, GetAppListView()->app_list_state());
  GetAppListTestHelper()->CheckVisibility(false);

  // Check metrics initial values.
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 0);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // Emulate to drag launcher from shelf. Then verifies the following things:
  // (1) Metrics values for tablet mode are not recorded.
  // (2) Metrics values for clamshell mode are recorded correctly.
  gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  shelf_bounds.Intersect(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  gfx::Point shelf_center = shelf_bounds.CenterPoint();
  gfx::Point target_point =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(shelf_center, target_point,
                                   base::TimeDelta::FromMicroseconds(500), 1);
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 0);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // AppListView::UpdateYPositionAndOpacity is triggered by
  // ShelfLayoutManager::StartGestureDrag and
  // ShelfLayoutManager::UpdateGestureDrag. Note that scrolling step of event
  // generator is 1. So the expected value is 2.
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 2);

  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode", 1);
}

class AppListControllerImplMetricsTestWithoutHotseat
    : public AppListControllerImplMetricsTest {
 public:
  AppListControllerImplMetricsTestWithoutHotseat() = default;
  ~AppListControllerImplMetricsTestWithoutHotseat() override = default;
  void SetUp() override {
    // The feature verified by this test is only enabled if drag from shelf to
    // home or overview (which is controlled by hotseat flag) is disabled.
    scoped_features_.InitWithFeatures({}, {chromeos::features::kShelfHotseat});
    AppListControllerImplMetricsTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
  DISALLOW_COPY_AND_ASSIGN(AppListControllerImplMetricsTestWithoutHotseat);
};

// Verifies that the PresentationTimeRecorder works correctly for the home
// launcher gesture drag in tablet mode (https://crbug.com/947105).
TEST_F(AppListControllerImplMetricsTestWithoutHotseat,
       PresentationTimeRecordedForDragInTabletMode) {
  // Turn on the tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(IsTabletMode());

  // Create a window then press the home launcher button. Expect that |w| is
  // hidden.
  std::unique_ptr<aura::Window> w(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  Shell::Get()->home_screen_controller()->GoHome(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(w->IsVisible());
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  int delta_y = 1;
  gfx::Point start =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen().top_right();
  base::TimeTicks timestamp = base::TimeTicks::Now();

  // Emulate to drag the launcher downward.
  // Send SCROLL_START event. Check the presentation metrics values.
  ui::GestureEvent start_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, delta_y));
  GetAppListView()->OnGestureEvent(&start_event);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 0);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // Send SCROLL_UPDATE event. Check the presentation metrics values.
  timestamp += base::TimeDelta::FromMilliseconds(25);
  delta_y += 20;
  start.Offset(0, 1);
  ui::GestureEvent update_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, delta_y));
  GetAppListView()->OnGestureEvent(&update_event);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 1);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // Send SCROLL_END event. Check the presentation metrics values.
  timestamp += base::TimeDelta::FromMilliseconds(25);
  start.Offset(0, 1);
  ui::GestureEvent end_event =
      ui::GestureEvent(start.x(), start.y() + delta_y, ui::EF_NONE, timestamp,
                       ui::GestureEventDetails(ui::ET_GESTURE_END));
  GetAppListView()->OnGestureEvent(&end_event);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 1);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 1);

  // After the gesture scroll event ends, the window shows.
  EXPECT_TRUE(w->IsVisible());
  ASSERT_TRUE(IsTabletMode());
}

}  // namespace ash
