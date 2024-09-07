// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_contents_view.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "ash/child_accounts/parent_access_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/fake_login_detachable_base_model.h"
#include "ash/login/ui/kiosk_app_default_message.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/lock_screen_media_view.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_camera_timeout_view.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/login/ui/login_keyboard_test_base.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_public_account_user_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/scrollable_users_list_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/reauth_reason.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Mock;

namespace ash {

namespace {

constexpr char kNbPasswordAttemptsUntilSuccessHistogramName[] =
    "Ash.OSAuth.Login.NbPasswordAttempts.UntilSuccess";
constexpr char kNbPasswordAttemptsUntilFailureHistogramName[] =
    "Ash.OSAuth.Login.NbPasswordAttempts.UntilFailure";

void PressAndReleasePowerButton() {
  base::SimpleTestTickClock tick_clock;
  auto dispatch_power_button_event_after_delay =
      [&](const base::TimeDelta& delta, bool down) {
        tick_clock.Advance(delta + base::Milliseconds(1));
        Shell::Get()->power_button_controller()->OnPowerButtonEvent(
            down, tick_clock.NowTicks());
        base::RunLoop().RunUntilIdle();
      };

  // Press and release the power button to force backlights off.
  dispatch_power_button_event_after_delay(
      PowerButtonController::kIgnorePowerButtonAfterResumeDelay, true /*down*/);
  dispatch_power_button_event_after_delay(
      PowerButtonController::kIgnoreRepeatedButtonUpDelay, false /*down*/);
}

// Returns sample AuthDisabledData to be used in tests, if the details are not
// important.
AuthDisabledData GetTestDisabledAuthData() {
  return AuthDisabledData(ash::AuthDisabledReason::kTimeWindowLimit,
                          base::Time::Now() + base::Hours(8), base::Hours(1),
                          true /*disable_lock_screen_media*/);
}

}  // namespace

using LockContentsViewKeyboardUnitTest = LoginKeyboardTestBase;

class LockContentsViewUnitTest : public LoginTestBase {
 public:
  LockContentsViewUnitTest() {
    set_start_session(true);
    AuthEventsRecorder::Get()->OnAuthenticationSurfaceChange(
        AuthEventsRecorder::AuthenticationSurface::kLogin);
  }
  LockContentsViewUnitTest(LockContentsViewUnitTest&) = delete;
  LockContentsViewUnitTest& operator=(LockContentsViewUnitTest&) = delete;
  ~LockContentsViewUnitTest() override = default;

  // Change the active LoginBigUserView by sending a mouse click event.
  void MakeAuthViewActive(LoginBigUserView* view) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    // Send event to swap users.
    LoginUserView* user_view = view->GetUserView();
    generator->MoveMouseTo(user_view->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();
  }

  void VerifyUpdatingSmartLockStateSetsAuthMethod(
      LoginBigUserView* view,
      SmartLockState smart_lock_state,
      bool should_have_auth_method) {
    ASSERT_TRUE(view);
    LoginAuthUserView* auth_user_view = view->auth_user();
    ASSERT_TRUE(auth_user_view);
    LoginAuthUserView::TestApi test_api(auth_user_view);
    AccountId account_id = view->GetCurrentUser().basic_user_info.account_id;

    DataDispatcher()->SetSmartLockState(account_id, smart_lock_state);
    EXPECT_EQ(should_have_auth_method,
              test_api.HasAuthMethod(LoginAuthUserView::AUTH_SMART_LOCK));
  }
};

class LockContentsMediaViewUnitTest : public LockContentsViewUnitTest {
 public:
  LockContentsMediaViewUnitTest() {
    set_start_session(true);
    AuthEventsRecorder::Get()->OnAuthenticationSurfaceChange(
        AuthEventsRecorder::AuthenticationSurface::kLogin);
  }
  LockContentsMediaViewUnitTest(LockContentsMediaViewUnitTest&) = delete;
  LockContentsMediaViewUnitTest& operator=(LockContentsMediaViewUnitTest&) =
      delete;
  ~LockContentsMediaViewUnitTest() override = default;

  void SimulateMediaSessionChanged(
      LockContentsViewTestApi& lock_contents,
      media_session::mojom::MediaPlaybackState playback_state) {
    // Create media session information.
    media_session::mojom::MediaSessionInfoPtr session_info(
        media_session::mojom::MediaSessionInfo::New());
    session_info->playback_state = playback_state;
    session_info->is_controllable = true;

    // Simulate media session and media session information change.
    lock_contents.media_view()->MediaSessionChanged(
        base::UnguessableToken::Create());
    lock_contents.media_view()->MediaSessionInfoChanged(
        std::move(session_info));
  }

  bool IsMediaViewDrawn(LockContentsViewTestApi& lock_contents) {
    return lock_contents.media_view()->IsDrawn();
  }
};

TEST_F(LockContentsViewUnitTest, DisplayMode) {
  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Verify user list and secondary auth are not shown for one user.
  LockContentsViewTestApi lock_contents(contents);
  EXPECT_EQ(nullptr, lock_contents.users_list());
  EXPECT_FALSE(lock_contents.opt_secondary_big_view());

  // Verify user list is not shown for two users, but secondary auth is.
  SetUserCount(2);
  EXPECT_EQ(nullptr, lock_contents.users_list());
  EXPECT_TRUE(lock_contents.opt_secondary_big_view());

  // Verify user names and pod style is set correctly for 3-25 users. This also
  // sanity checks that LockContentsView can respond to a multiple user change
  // events fired from the data dispatcher, which is needed for the debug UI.
  for (size_t user_count = 3; user_count < 25; ++user_count) {
    SetUserCount(user_count);
    ScrollableUsersListView::TestApi users_list(lock_contents.users_list());
    EXPECT_EQ(user_count - 1, users_list.user_views().size());

    // 1 extra user gets large style.
    LoginDisplayStyle expected_style = LoginDisplayStyle::kLarge;
    // 2-6 extra users get small style.
    if (user_count >= 3) {
      expected_style = LoginDisplayStyle::kSmall;
    }
    // 7+ users get get extra small style.
    if (user_count >= 7) {
      expected_style = LoginDisplayStyle::kExtraSmall;
    }

    for (size_t i = 0; i < users_list.user_views().size(); ++i) {
      LoginUserView::TestApi user_test_api(users_list.user_views()[i]);
      EXPECT_EQ(expected_style, user_test_api.display_style());

      const LoginUserInfo& user = users()[i + 1];
      EXPECT_EQ(base::UTF8ToUTF16(user.basic_user_info.display_name),
                user_test_api.displayed_name());
    }
  }
}

// Verifies that the single user view is centered.
TEST_F(LockContentsViewUnitTest, SingleUserCentered) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LockContentsViewTestApi test_api(contents);
  LoginBigUserView* auth_view = test_api.primary_big_view();
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  int expected_margin =
      (widget_bounds.width() - auth_view->GetPreferredSize().width()) / 2;
  gfx::Rect auth_bounds = auth_view->GetBoundsInScreen();

  EXPECT_NE(0, expected_margin);
  EXPECT_EQ(expected_margin, auth_bounds.x());
  EXPECT_EQ(expected_margin,
            widget_bounds.width() - (auth_bounds.x() + auth_bounds.width()));
}

// Verifies that the single user view is centered when lock screen notes are
// enabled.
TEST_F(LockContentsViewUnitTest, SingleUserCenteredNoteActionEnabled) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LockContentsViewTestApi test_api(contents);
  LoginBigUserView* auth_view = test_api.primary_big_view();
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  int expected_margin =
      (widget_bounds.width() - auth_view->GetPreferredSize().width()) / 2;
  gfx::Rect auth_bounds = auth_view->GetBoundsInScreen();

  EXPECT_NE(0, expected_margin);
  EXPECT_EQ(expected_margin, auth_bounds.x());
  EXPECT_EQ(expected_margin,
            widget_bounds.width() - (auth_bounds.x() + auth_bounds.width()));
}

// Verifies that any top-level spacing views go down to width zero in small
// screen sizes.
TEST_F(LockContentsViewUnitTest, LayoutInSmallScreenSize) {
  // Build lock screen.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  LockContentsViewTestApi lock_contents(contents);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  display::test::DisplayManagerTestApi display_manager_test_api(
      display_manager());

  auto get_left_view = [&]() -> views::View* {
    return lock_contents.primary_big_view();
  };
  auto get_right_view = [&]() -> views::View* {
    if (lock_contents.opt_secondary_big_view()) {
      return lock_contents.opt_secondary_big_view();
    }
    return lock_contents.users_list();
  };

  for (int i = 2; i < 10; ++i) {
    SetUserCount(i);
    views::View* left_view = get_left_view();
    views::View* right_view = get_right_view();

    // Determine the full-sized widths when there is plenty of spacing available
    display_manager_test_api.UpdateDisplay("2000x1000");
    int left_width = left_view->width();
    int right_width = right_view->width();

    int left_x = left_view->x();
    int right_x = right_view->x();

    // Resize to the minimum width that will fit both the left and right views
    int display_width = left_width + right_width;
    display_manager_test_api.UpdateDisplay(base::NumberToString(display_width) +
                                           "x400");

    // Verify the views moved, ie, a layout was performed
    EXPECT_NE(left_view->x(), left_x);
    EXPECT_NE(right_view->x(), right_x);

    // Left and right views still have their full widths
    EXPECT_EQ(left_width, left_view->width());
    EXPECT_EQ(right_width, right_view->width());

    // Left edge of |left_view| should be at start of the screen.
    EXPECT_EQ(left_view->GetBoundsInScreen().x(), 0);
    // Left edge of |right_view| should immediately follow |left_view| with no
    // gap.
    EXPECT_EQ(left_view->GetBoundsInScreen().right(),
              right_view->GetBoundsInScreen().x());
    // Right edge of |right_view| should be at the end of the screen.
    EXPECT_EQ(right_view->GetBoundsInScreen().right(), display_width);
  }
}

// Verifies that layout dynamically updates after a rotation by checking the
// distance between the auth user and the user list in landscape and portrait
// mode.
TEST_F(LockContentsViewUnitTest, AutoLayoutAfterRotation) {
  // Build lock screen with three users.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi lock_contents(contents);
  SetUserCount(3);

  // Returns the distance between the auth user view and the user view.
  auto calculate_distance = [&]() {
    if (lock_contents.opt_secondary_big_view()) {
      return lock_contents.opt_secondary_big_view()->GetBoundsInScreen().x() -
             lock_contents.primary_big_view()->GetBoundsInScreen().x();
    }
    ScrollableUsersListView::TestApi users_list(lock_contents.users_list());
    return users_list.user_views()[0]->GetBoundsInScreen().x() -
           lock_contents.primary_big_view()->GetBoundsInScreen().x();
  };

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());
  for (int i = 2; i < 10; ++i) {
    SetUserCount(i);
    SCOPED_TRACE(testing::Message() << "User count: " << i);

    // Start at 0 degrees (landscape).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_0,
        display::Display::RotationSource::ACTIVE);
    widget->LayoutRootViewIfNecessary();
    int distance_0deg = calculate_distance();
    EXPECT_NE(distance_0deg, 0);

    // Rotate the display to 90 degrees (portrait).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_90,
        display::Display::RotationSource::ACTIVE);
    widget->LayoutRootViewIfNecessary();
    int distance_90deg = calculate_distance();
    EXPECT_LT(distance_90deg, distance_0deg);

    // Rotate the display back to 0 degrees (landscape).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_0,
        display::Display::RotationSource::ACTIVE);
    widget->LayoutRootViewIfNecessary();
    int distance_0deg_2 = calculate_distance();
    EXPECT_EQ(distance_0deg_2, distance_0deg);
    EXPECT_NE(distance_0deg_2, distance_90deg);
  }
}

TEST_F(LockContentsViewUnitTest, AutoLayoutExtraSmallUsersListAfterRotation) {
  // Build lock screen with extra small layout (> 6 users).
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  SetUserCount(9);
  ScrollableUsersListView* users_list =
      LockContentsViewTestApi(contents).users_list();

  // Users list in extra small layout should adjust its height to parent.
  EXPECT_EQ(contents->height(), users_list->height());

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());

  // Start at 0 degrees (landscape).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(contents->height(), users_list->height());

  // Rotate the display to 90 degrees (portrait).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(contents->height(), users_list->height());

  // Rotate the display back to 0 degrees (landscape).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(contents->height(), users_list->height());
}

TEST_F(LockContentsViewUnitTest, AutoLayoutSmallUsersListAfterRotation) {
  // Build lock screen with small layout (3-6 users).
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  SetUserCount(4);
  ScrollableUsersListView* users_list =
      LockContentsViewTestApi(contents).users_list();

  // Calculate top spacing between users list and lock screen contents.
  auto top_margin = [&]() {
    return users_list->GetBoundsInScreen().y() -
           contents->GetBoundsInScreen().y();
  };

  // Calculate bottom spacing between users list and lock screen contents.
  auto bottom_margin = [&]() {
    return contents->GetBoundsInScreen().bottom() -
           users_list->GetBoundsInScreen().bottom();
  };

  // Users list in small layout should adjust its height to content and be
  // vertical centered in parent.
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());

  // Start at 0 degrees (landscape).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());

  // Rotate the display to 90 degrees (portrait).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());

  // Rotate the display back to 0 degrees (landscape).
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());
}

TEST_F(LockContentsViewKeyboardUnitTest,
       AutoLayoutExtraSmallUsersListForKeyboard) {
  // Build lock screen with extra small layout (> 6 users).
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  ASSERT_NE(nullptr, contents);
  SetUserCount(9);

  // Users list in extra small layout should adjust its height to parent.
  ScrollableUsersListView* users_list =
      LockContentsViewTestApi(contents).users_list();
  EXPECT_EQ(contents->height(), users_list->height());

  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  gfx::Rect keyboard_bounds = GetKeyboardBoundsInScreen();
  EXPECT_FALSE(users_list->GetBoundsInScreen().Intersects(keyboard_bounds));
  EXPECT_EQ(contents->height(), users_list->height());

  ASSERT_NO_FATAL_FAILURE(HideKeyboard());
  EXPECT_EQ(contents->height(), users_list->height());
}

TEST_F(LockContentsViewKeyboardUnitTest, AutoLayoutSmallUsersListForKeyboard) {
  // Build lock screen with small layout (3-6 users).
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  ASSERT_NE(nullptr, contents);
  SetUserCount(4);
  ScrollableUsersListView* users_list =
      LockContentsViewTestApi(contents).users_list();

  // Calculate top spacing between users list and lock screen contents.
  auto top_margin = [&]() {
    return users_list->GetBoundsInScreen().y() -
           contents->GetBoundsInScreen().y();
  };

  // Calculate bottom spacing between users list and lock screen contents.
  auto bottom_margin = [&]() {
    return contents->GetBoundsInScreen().bottom() -
           users_list->GetBoundsInScreen().bottom();
  };

  // Users list in small layout should adjust its height to content and be
  // vertical centered in parent.
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());

  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  gfx::Rect keyboard_bounds = GetKeyboardBoundsInScreen();
  EXPECT_FALSE(users_list->GetBoundsInScreen().Intersects(keyboard_bounds));
  EXPECT_EQ(top_margin(), bottom_margin());

  ASSERT_NO_FATAL_FAILURE(HideKeyboard());
  EXPECT_EQ(top_margin(), bottom_margin());
  EXPECT_EQ(users_list->height(), users_list->contents()->height());
}

TEST_F(LockContentsViewKeyboardUnitTest, ShowPinPadForPassword) {
  SetUserCount(1);
  users()[0].show_pin_pad_for_password = true;
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockContentsView* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  ASSERT_NE(nullptr, contents);
  DataDispatcher()->SetUserList(users());
  LoginAuthUserView* login_auth_user_view =
      LockContentsViewTestApi(contents).primary_big_view()->auth_user();
  LoginAuthUserView::TestApi primary_user(login_auth_user_view);
  LoginPinView* pin_view(primary_user.pin_view());
  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  EXPECT_FALSE(pin_view->GetVisible());
  ASSERT_NO_FATAL_FAILURE(HideKeyboard());
  EXPECT_TRUE(pin_view->GetVisible());
}

// Ensures that when swapping between two users, only auth method display swaps.
TEST_F(LockContentsViewUnitTest, SwapAuthUsersInTwoUserLayout) {
  // Build lock screen with two users.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  LockContentsViewTestApi test_api(contents);
  SetUserCount(2);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Capture user info to validate it did not change during the swap.
  AccountId primary_user =
      test_api.primary_big_view()->GetCurrentUser().basic_user_info.account_id;
  AccountId secondary_user = test_api.opt_secondary_big_view()
                                 ->GetCurrentUser()
                                 .basic_user_info.account_id;
  EXPECT_NE(primary_user, secondary_user);

  // Primary user starts with auth. Secondary user does not have any auth.
  EXPECT_TRUE(test_api.primary_big_view()->IsAuthEnabled());
  EXPECT_FALSE(test_api.opt_secondary_big_view()->IsAuthEnabled());
  ASSERT_NE(nullptr, test_api.opt_secondary_big_view()->auth_user());

  // Send event to swap users.
  ui::test::EventGenerator* generator = GetEventGenerator();
  LoginAuthUserView::TestApi secondary_test_api(
      test_api.opt_secondary_big_view()->auth_user());
  generator->MoveMouseTo(
      secondary_test_api.user_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // User info is not swapped.
  EXPECT_EQ(
      primary_user,
      test_api.primary_big_view()->GetCurrentUser().basic_user_info.account_id);
  EXPECT_EQ(secondary_user, test_api.opt_secondary_big_view()
                                ->GetCurrentUser()
                                .basic_user_info.account_id);

  // Active auth user (ie, which user is showing password) is swapped.
  EXPECT_FALSE(test_api.primary_big_view()->IsAuthEnabled());
  EXPECT_TRUE(test_api.opt_secondary_big_view()->IsAuthEnabled());
}

// Ensures that when swapping from a user list, the entire user info is swapped.
TEST_F(LockContentsViewUnitTest, SwapUserListToPrimaryAuthUser) {
  // Build lock screen with five users.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi lock_contents(contents);
  SetUserCount(5);
  ScrollableUsersListView::TestApi users_list(lock_contents.users_list());
  EXPECT_EQ(users().size() - 1, users_list.user_views().size());

  LoginBigUserView* auth_view = lock_contents.primary_big_view();

  for (const LoginUserView* const list_user_view : users_list.user_views()) {
    // Capture user info to validate it did not change during the swap.
    AccountId auth_id = auth_view->GetCurrentUser().basic_user_info.account_id;
    AccountId list_user_id =
        list_user_view->current_user().basic_user_info.account_id;
    EXPECT_NE(auth_id, list_user_id);

    // Send event to swap users.
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(list_user_view->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();

    // User info is swapped.
    EXPECT_EQ(list_user_id,
              auth_view->GetCurrentUser().basic_user_info.account_id);
    EXPECT_EQ(auth_id,
              list_user_view->current_user().basic_user_info.account_id);

    // Validate that every user is still unique.
    std::unordered_set<std::string> emails;
    for (const LoginUserView* const view : users_list.user_views()) {
      std::string email =
          view->current_user().basic_user_info.account_id.GetUserEmail();
      EXPECT_TRUE(emails.insert(email).second);
    }
  }
}

// Test goes through different lock screen note state changes and tests that
// the note action visibility is updated accordingly.
TEST_F(LockContentsViewUnitTest, NoteActionButtonVisibilityChanges) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents));

  LockContentsViewTestApi test_api(contents);
  views::View* note_action_button = test_api.note_action();

  // In kAvailable state, the note action button should be visible.
  EXPECT_TRUE(note_action_button->GetVisible());

  // In kLaunching state, the note action button should not be visible.
  DataDispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kLaunching);
  EXPECT_FALSE(note_action_button->GetVisible());

  // In kActive state, the note action button should not be visible.
  DataDispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kActive);
  EXPECT_FALSE(note_action_button->GetVisible());

  // When moved back to kAvailable state, the note action button should become
  // visible again.
  DataDispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_TRUE(note_action_button->GetVisible());

  // In kNotAvailable state, the note action button should not be visible.
  DataDispatcher()->SetLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(note_action_button->GetVisible());
}

// Verifies note action view bounds.
TEST_F(LockContentsViewUnitTest, NoteActionButtonBounds) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LockContentsViewTestApi test_api(contents);

  // The note action button should not be visible if the note action is not
  // available.
  EXPECT_FALSE(test_api.note_action()->GetVisible());

  // When the note action becomes available, the note action button should be
  // shown.
  DataDispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_TRUE(test_api.note_action()->GetVisible());

  // Verify the bounds of the note action button are as expected.
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  gfx::Size note_action_size = test_api.note_action()->GetPreferredSize();
  EXPECT_EQ(gfx::Rect(widget_bounds.top_right() -
                          gfx::Vector2d(note_action_size.width(), 0),
                      note_action_size),
            test_api.note_action()->GetBoundsInScreen());

  // If the note action is disabled again, the note action button should be
  // hidden.
  DataDispatcher()->SetLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(test_api.note_action()->GetVisible());
}

// Verifies the note action view bounds when note action is available at lock
// contents view creation.
TEST_F(LockContentsViewUnitTest, NoteActionButtonBoundsInitiallyAvailable) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  LockContentsViewTestApi test_api(contents);

  // Verify the note action button is visible and positioned in the top right
  // corner of the screen.
  EXPECT_TRUE(test_api.note_action()->GetVisible());
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  gfx::Size note_action_size = test_api.note_action()->GetPreferredSize();
  EXPECT_EQ(gfx::Rect(widget_bounds.top_right() -
                          gfx::Vector2d(note_action_size.width(), 0),
                      note_action_size),
            test_api.note_action()->GetBoundsInScreen());

  // If the note action is disabled, the note action button should be hidden.
  DataDispatcher()->SetLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(test_api.note_action()->GetVisible());
}

// Verifies the system info view bounds interaction with the note-taking button.
TEST_F(LockContentsViewUnitTest, SystemInfoViewBounds) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  LockContentsViewTestApi test_api(contents);
  // Verify that the system info view is hidden by default.
  EXPECT_FALSE(test_api.system_info()->GetVisible());

  // Verify that the system info view becomes visible and it doesn't block the
  // note action button.
  DataDispatcher()->SetSystemInfo(
      true /*show*/, false /*enforced*/, "Best version ever", "Asset ID: 6666",
      "Bluetooth adapter", false /*adb_sideloading_enabled*/);
  EXPECT_TRUE(test_api.system_info()->GetVisible());
  EXPECT_FALSE(test_api.bottom_status_indicator()->GetVisible());
  EXPECT_TRUE(test_api.note_action()->GetVisible());
  gfx::Size note_action_size = test_api.note_action()->GetPreferredSize();
  EXPECT_GE(widget_bounds.right() -
                test_api.system_info()->GetBoundsInScreen().right(),
            note_action_size.width());

  // Verify that if the note action is disabled, the system info view moves to
  // the right to fill the empty space.
  DataDispatcher()->SetLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(test_api.note_action()->GetVisible());
  EXPECT_LT(widget_bounds.right() -
                test_api.system_info()->GetBoundsInScreen().right(),
            note_action_size.width());

  // Verify that bottom status indicator is invisible if neither adb sideloading
  // is enabled nor the device is enrolled.
  EXPECT_FALSE(test_api.bottom_status_indicator()->GetVisible());
}

// Alt-V toggles display of system information.
TEST_F(LockContentsViewUnitTest, AltVTogglesHiddenSystemInfo) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi test_api(contents);
  // Verify that the system info view is hidden by default.
  EXPECT_FALSE(test_api.system_info()->GetVisible());

  // Verify that the system info view does not become visible when given data
  // but show is false.
  DataDispatcher()->SetSystemInfo(
      false /*show*/, false /*enforced*/, "Best version ever", "Asset ID: 6666",
      "Bluetooth adapter", false /*adb_sideloading_enabled*/);
  EXPECT_FALSE(test_api.system_info()->GetVisible());

  // Alt-V shows hidden system info.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_V, ui::EF_ALT_DOWN);
  EXPECT_TRUE(test_api.system_info()->GetVisible());
  // System info is not empty, ie, it is actually being displayed.
  EXPECT_FALSE(test_api.system_info()->bounds().IsEmpty());

  // Alt-V again hides the system info.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_V, ui::EF_ALT_DOWN);
  EXPECT_FALSE(test_api.system_info()->GetVisible());
}

// Updating existing system info and setting show_=true later will
// reveal hidden system info.
TEST_F(LockContentsViewUnitTest, ShowRevealsHiddenSystemInfo) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi test_api(contents);

  auto set_system_info = [&](bool show, bool enforced) {
    DataDispatcher()->SetSystemInfo(show, enforced, "Best version ever",
                                    "Asset ID: 6666", "Bluetooth adapter",
                                    false /*adb_sideloading_enabled*/);
  };

  // Start with hidden system info.
  set_system_info(false, false);
  EXPECT_FALSE(test_api.system_info()->GetVisible());

  // Update system info but request it be shown.
  set_system_info(true, false);
  EXPECT_TRUE(test_api.system_info()->GetVisible());

  // Trying to hide system info from mojom call doesn't do anything.
  set_system_info(false, false);
  EXPECT_TRUE(test_api.system_info()->GetVisible());

  // Trying to hide system info from mojom call with enforced=true. It should
  // work.
  set_system_info(false, true);
  EXPECT_FALSE(test_api.system_info()->GetVisible());

  // System info will be shown again when enforced is reset to false
  // because the view remembers user wants to show it if possible.
  set_system_info(false, false);
  EXPECT_TRUE(test_api.system_info()->GetVisible());
}

// Show bottom status indicator if ADB sideloading is enabled.
TEST_F(LockContentsViewUnitTest, ShowStatusIndicatorIfAdbSideloadingEnabled) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi test_api(contents);

  // If the system starts with ADB sideloading enabled, bottom_status_indicator
  // should be visible.
  DataDispatcher()->SetSystemInfo(
      false /*show*/, false /*enforced*/, "Best version ever", "Asset ID: 6666",
      "Bluetooth adapter", true /*adb_sideloading_enabled*/);
  EXPECT_TRUE(test_api.bottom_status_indicator()->GetVisible());

  // bottom_status_indicator should always be visible when displaying ADB
  // sideloading warning.
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::EXTENSION_LOGIN);
  EXPECT_TRUE(test_api.bottom_status_indicator()->GetVisible());
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(test_api.bottom_status_indicator()->GetVisible());
}

// Show bottom status indicator if device is enrolled
TEST_F(LockContentsViewUnitTest, ShowStatusIndicatorIfEnrolledDevice) {
  // If the device is enrolled, bottom_status_indicator should be visible.
  Shell::Get()->system_tray_model()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"BestCompanyEver", ManagementDeviceMode::kNone});

  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi test_api(contents);

  EXPECT_TRUE(test_api.bottom_status_indicator()->GetVisible());

  // bottom_status_indicator should not be visible when displaying enterprise
  // domain and extension UI is visible.
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::EXTENSION_LOGIN);
  EXPECT_FALSE(test_api.bottom_status_indicator()->GetVisible());
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(test_api.bottom_status_indicator()->GetVisible());
}

// Show bottom status indicator if device is enrolled
TEST_F(LockContentsViewUnitTest, ShowManagementBubbleOnClickIfEnrolledDevice) {
  // If the device is enrolled, bottom_status_indicator should be visible.
  Shell::Get()->system_tray_model()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"BestCompanyEver", ManagementDeviceMode::kNone});

  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi test_api(contents);

  EXPECT_TRUE(test_api.bottom_status_indicator()->GetVisible());
  EXPECT_FALSE(test_api.management_bubble()->GetVisible());

  // Make the management bubble appear on click.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      test_api.bottom_status_indicator()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_TRUE(test_api.management_bubble()->GetVisible());

  // Click somewhere else to make the management bubble disappear.
  generator->MoveMouseTo(test_api.primary_big_view()
                             ->GetUserView()
                             ->GetBoundsInScreen()
                             .CenterPoint());
  generator->ClickLeftButton();
  EXPECT_FALSE(test_api.management_bubble()->GetVisible());
}

// Do not show the management bubble on click if ADB sideloading is enabled and
// device is enrolled.
TEST_F(LockContentsViewUnitTest, DoNotShowManagementBubbleOnClickIfAdb) {
  // If the device is enrolled, bottom_status_indicator should be visible.
  Shell::Get()->system_tray_model()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"BestCompanyEver", ManagementDeviceMode::kNone});

  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi test_api(contents);

  // Before system info are set, bottom status indicator status should be set to
  // managed device.
  EXPECT_EQ(test_api.bottom_status_indicator_status(),
            BottomIndicatorState::kManagedDevice);

  // If the system starts with ADB sideloading enabled and the device is
  // enrolled, the bottom status indicator should show the ADB warning.
  DataDispatcher()->SetSystemInfo(
      false /*show*/, false /*enforced*/, "Best version ever", "Asset ID: 6666",
      "Bluetooth adapter", true /*adb_sideloading_enabled*/);
  EXPECT_TRUE(test_api.bottom_status_indicator()->GetVisible());
  EXPECT_EQ(test_api.bottom_status_indicator_status(),
            BottomIndicatorState::kAdbSideLoadingEnabled);

  // The management bubble should not appear on click.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      test_api.bottom_status_indicator()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_FALSE(test_api.management_bubble()->GetVisible());
}

TEST_F(LockContentsViewUnitTest, ShowErrorBubbleOnAuthFailure) {
  // Build lock screen with a single user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents));

  LockContentsViewTestApi test_api(contents);

  // Password submit runs mojo.
  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                           users()[0].basic_user_info.account_id, _, false, _));

  // Submit password.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_api.auth_error_bubble()->GetVisible());
}

TEST_F(LockContentsViewUnitTest, AuthErrorLockscreenLearnMoreButton) {
  // Build lock screen with a single user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  SetWidget(CreateWidgetWithContent(contents));

  LockContentsViewTestApi test_api(contents);

  // Password submit runs mojo.
  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                           users()[0].basic_user_info.account_id, _, false, _));

  // AuthErrorButton should not be visible yet.
  EXPECT_FALSE(test_api.auth_error_bubble()->GetVisible());

  // Submit password.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();

  // Auth Error button should be visible as an incorrect password was given.
  EXPECT_TRUE(test_api.auth_error_bubble()->GetVisible());

  // Find button in auth_error_bubble children.
  views::View* learn_more_button = nullptr;
  for (views::View* child :
       test_api.auth_error_bubble()->GetContent()->children()) {
    if (views::Button::AsButton(child)) {
      // The bubble should only have one button ("learn more") on the lock
      // screen.
      EXPECT_FALSE(learn_more_button);
      learn_more_button = child;
    }
  }

  EXPECT_TRUE(learn_more_button);

  // Expect ShowAccountAccessHelp() to be called due to button click.
  EXPECT_CALL(*client, ShowAccountAccessHelpApp(widget()->GetNativeWindow()))
      .Times(1);

  // Move mouse to AuthError's ShowAccountAccessHelp button and click it.
  // Should result in ShowAccountAccessHelpApp().
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(learn_more_button->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // The error bubble should go away after button press.
  EXPECT_FALSE(test_api.auth_error_bubble()->GetVisible());
}

TEST_F(LockContentsViewUnitTest, AuthErrorLoginScreenRecoverUserButton) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);

  SetWidget(CreateWidgetWithContent(contents));

  LockContentsViewTestApi test_api(contents);

  // Password submit runs mojo.
  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                           users()[0].basic_user_info.account_id, _, false, _));

  // AuthErrorButton should not be visible yet.
  EXPECT_FALSE(test_api.auth_error_bubble()->GetVisible());

  // Submit password.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();

  // Auth Error button should be visible as an incorrect password was given.
  EXPECT_TRUE(test_api.auth_error_bubble()->GetVisible());

  // There should be two buttons in the error bubble: The "learn more" button
  // and the "recover user" button, in that order.
  std::vector<views::Button*> buttons;
  for (views::View* child :
       test_api.auth_error_bubble()->GetContent()->children()) {
    if (views::Button* button = views::Button::AsButton(child)) {
      buttons.push_back(button);
    }
  }
  EXPECT_EQ(2u, buttons.size());
  views::Button* recover_user_button = buttons[1];

  // Expect the ShowGaiaSignin to be called due to button click.
  EXPECT_CALL(*client, StartUserRecovery(users()[0].basic_user_info.account_id))
      .Times(1);

  // Move mouse to the "Recover user" button and click it.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      recover_user_button->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // The error bubble should be hidden because of the button press.
  EXPECT_FALSE(test_api.auth_error_bubble()->GetVisible());

  std::optional<int> reauth_reason =
      user_manager::KnownUser(Shell::Get()->local_state())
          .FindReauthReason(users()[0].basic_user_info.account_id);
  EXPECT_EQ(reauth_reason, static_cast<int>(ReauthReason::kForgotPassword));
}

// Gaia is never shown on lock, no mater how many times auth fails.
TEST_F(LockContentsViewUnitTest, GaiaNeverShownOnLockAfterFailedAuth) {
  // Build lock screen with a single user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents));

  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);

  auto submit_password = [&]() {
    PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    base::RunLoop().RunUntilIdle();
  };

  // ShowGaiaSignin is never triggered.
  EXPECT_CALL(*client, ShowGaiaSignin(_)).Times(0);
  for (int i = 0; i < LockContentsView::kLoginAttemptsBeforeGaiaDialog + 1;
       ++i) {
    submit_password();
  }
}

// Gaia should not be shown after first failed login attempt for a user, even if
// there are many failed login attempts made by other users on the same device.
TEST_F(LockContentsViewUnitTest, GaiaNeverShownAfterFirstFailedLoginAttempt) {
  // Build lock screen with two users.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(2);
  SetWidget(CreateWidgetWithContent(contents));

  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);

  auto submit_password = [&]() {
    PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    base::RunLoop().RunUntilIdle();
  };

  // ShowGaiaSignin is never triggered.
  EXPECT_CALL(*client, ShowGaiaSignin(_)).Times(0);
  for (int i = 0; i < LockContentsView::kLoginAttemptsBeforeGaiaDialog - 1;
       ++i) {
    submit_password();
  }
  Mock::VerifyAndClearExpectations(client.get());

  // Simulate a button click on the secondary UserView.
  LoginAuthUserView::TestApi secondary_user(
      LockContentsViewTestApi(contents).opt_secondary_big_view()->auth_user());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      secondary_user.user_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_TRUE(LoginPasswordView::TestApi(secondary_user.password_view())
                  .textfield()
                  ->HasFocus());

  // Verify ShowGaiaSignin is not triggered for other users.
  EXPECT_CALL(*client, ShowGaiaSignin(_)).Times(0);
  submit_password();
  Mock::VerifyAndClearExpectations(client.get());
}

// Gaia screen is not shown for failed login attempt using pin.
TEST_F(LockContentsViewUnitTest, GaiaNeverShownAfterFailedPinAuth) {
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();

  // Add user who can use pin authentication.
  const std::string email = "user@domain.com";
  AddUserByEmail(email);
  contents->OnPinEnabledForUserChanged(AccountId::FromUserEmail(email), true,
                                       /*available_at*/ std::nullopt);

  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);

  LoginBigUserView* big_view =
      LockContentsViewTestApi(contents).primary_big_view();
  LoginPinView* pin_view =
      LoginAuthUserView::TestApi(big_view->auth_user()).pin_view();
  LoginPinView::TestApi pin_pad_api{pin_view};
  ui::test::EventGenerator* generator = GetEventGenerator();

  auto submit_pin = [&]() {
    pin_pad_api.ClickOnDigit(1);
    generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  };

  EXPECT_TRUE(pin_view->GetVisible());

  // ShowGaiaSignin is never triggered.
  EXPECT_CALL(*client, ShowGaiaSignin(_)).Times(0);
  for (int i = 0; i < LockContentsView::kLoginAttemptsBeforeGaiaDialog + 1;
       ++i) {
    submit_pin();
  }

  Mock::VerifyAndClearExpectations(client.get());
}

TEST_F(LockContentsViewUnitTest, ErrorBubbleOnUntrustedDetachableBase) {
  auto fake_detachable_base_model =
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher());
  FakeLoginDetachableBaseModel* detachable_base_model =
      fake_detachable_base_model.get();

  // Build lock screen with 2 users.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(), std::move(fake_detachable_base_model));
  SetUserCount(2);

  const AccountId& kFirstUserAccountId = users()[0].basic_user_info.account_id;
  const AccountId& kSecondUserAccountId = users()[1].basic_user_info.account_id;

  // Initialize the detachable base state, so the user 1 has previously used
  // detachable base.
  detachable_base_model->InitLastUsedBases({{kFirstUserAccountId, "1234"}});
  detachable_base_model->SetPairingStatus(
      DetachableBasePairingStatus::kAuthenticated, "1234");
  SetWidget(CreateWidgetWithContent(contents));

  LockContentsViewTestApi test_api(contents);
  ui::test::EventGenerator* generator = GetEventGenerator();

  EXPECT_FALSE(test_api.detachable_base_error_bubble()->GetVisible());

  // Change detachable base to a base different than the one previously used by
  // the user - verify that a detachable base error bubble is shown.
  detachable_base_model->SetPairingStatus(
      DetachableBasePairingStatus::kAuthenticated, "5678");
  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());

  // Verify that the bubble is not hidden if the user starts typing.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_B);
  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());

  // Switching to the user that doesn't have previously used detachable base
  // (and should thus not be warned about the detachable base missmatch) should
  // hide the login bubble.
  LoginAuthUserView::TestApi secondary_test_api(
      test_api.opt_secondary_big_view()->auth_user());
  generator->MoveMouseTo(
      secondary_test_api.user_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  EXPECT_FALSE(test_api.detachable_base_error_bubble()->GetVisible());

  // The error should be shown again when switching back to the primary user.
  LoginAuthUserView::TestApi primary_test_api(
      test_api.primary_big_view()->auth_user());
  generator->MoveMouseTo(
      primary_test_api.user_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());
  EXPECT_FALSE(primary_test_api.password_view()->HasFocus());

  EXPECT_EQ("1234",
            detachable_base_model->GetLastUsedBase(kFirstUserAccountId));
  EXPECT_EQ("", detachable_base_model->GetLastUsedBase(kSecondUserAccountId));

  // The current detachable base should be set as the last used one by the user
  // after they authenticate - test for this.
  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(true);
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(kFirstUserAccountId,
                                                          _, false, _));

  // Submit password.
  primary_test_api.password_view()->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("5678",
            detachable_base_model->GetLastUsedBase(kFirstUserAccountId));
  EXPECT_EQ("", detachable_base_model->GetLastUsedBase(kSecondUserAccountId));
}

TEST_F(LockContentsViewUnitTest, ErrorBubbleForUnauthenticatedDetachableBase) {
  auto fake_detachable_base_model =
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher());
  FakeLoginDetachableBaseModel* detachable_base_model =
      fake_detachable_base_model.get();

  // Build lock screen with 2 users.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(), std::move(fake_detachable_base_model));
  SetUserCount(2);

  const AccountId& kFirstUserAccountId = users()[0].basic_user_info.account_id;
  const AccountId& kSecondUserAccountId = users()[1].basic_user_info.account_id;

  detachable_base_model->InitLastUsedBases({{kSecondUserAccountId, "5678"}});

  SetWidget(CreateWidgetWithContent(contents));

  LockContentsViewTestApi test_api(contents);
  ui::test::EventGenerator* generator = GetEventGenerator();

  EXPECT_FALSE(test_api.detachable_base_error_bubble()->GetVisible());

  // Show notification if unauthenticated base is attached.
  detachable_base_model->SetPairingStatus(
      DetachableBasePairingStatus::kNotAuthenticated, "");
  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());

  // Verify that the bubble is not hidden if the user starts typing.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_B);
  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());

  // Switching to another user should not hide the error bubble.
  LoginAuthUserView::TestApi secondary_test_api(
      test_api.opt_secondary_big_view()->auth_user());
  generator->MoveMouseTo(
      secondary_test_api.user_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());
  EXPECT_FALSE(secondary_test_api.password_view()->HasFocus());

  // The last trusted detachable used by the user should not be overriden by
  // user authentication.
  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(true);
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(kSecondUserAccountId,
                                                          _, false, _));

  // Submit password.
  secondary_test_api.password_view()->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("", detachable_base_model->GetLastUsedBase(kFirstUserAccountId));
  EXPECT_EQ("5678",
            detachable_base_model->GetLastUsedBase(kSecondUserAccountId));
}
TEST_F(LockContentsViewUnitTest,
       RemovingAttachedBaseHidesDetachableBaseNotification) {
  auto fake_detachable_base_model =
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher());
  FakeLoginDetachableBaseModel* detachable_base_model =
      fake_detachable_base_model.get();

  // Build lock screen with 2 users.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(), std::move(fake_detachable_base_model));
  SetUserCount(1);

  const AccountId& kUserAccountId = users()[0].basic_user_info.account_id;

  // Initialize the detachable base state, as if the user has previously used
  // detachable base.
  detachable_base_model->InitLastUsedBases({{kUserAccountId, "1234"}});
  detachable_base_model->SetPairingStatus(
      DetachableBasePairingStatus::kAuthenticated, "1234");

  SetWidget(CreateWidgetWithContent(contents));

  LockContentsViewTestApi test_api(contents);

  // Change detachable base to a base different than the one previously used by
  // the user - verify that a detachable base error bubble is shown.
  detachable_base_model->SetPairingStatus(
      DetachableBasePairingStatus::kAuthenticated, "5678");
  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());

  // The notification should be hidden if the base gets detached.
  detachable_base_model->SetPairingStatus(DetachableBasePairingStatus::kNone,
                                          "");
  EXPECT_FALSE(test_api.detachable_base_error_bubble()->GetVisible());
}

TEST_F(LockContentsViewUnitTest, DetachableBaseErrorClearsAuthError) {
  auto fake_detachable_base_model =
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher());
  FakeLoginDetachableBaseModel* detachable_base_model =
      fake_detachable_base_model.get();

  // Build lock screen with a single user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(), std::move(fake_detachable_base_model));
  SetUserCount(1);

  const AccountId& kUserAccountId = users()[0].basic_user_info.account_id;

  // Initialize the detachable base state, as if the user has previously used
  // detachable base.
  detachable_base_model->InitLastUsedBases({{kUserAccountId, "1234"}});
  detachable_base_model->SetPairingStatus(
      DetachableBasePairingStatus::kAuthenticated, "1234");

  SetWidget(CreateWidgetWithContent(contents));

  LockContentsViewTestApi test_api(contents);

  EXPECT_FALSE(test_api.detachable_base_error_bubble()->GetVisible());

  // Attempt and fail user auth - an auth error is expected to be shown.
  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);
  EXPECT_CALL(*client,
              AuthenticateUserWithPasswordOrPin_(kUserAccountId, _, false, _));

  // Submit password.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_api.auth_error_bubble()->GetVisible());
  EXPECT_FALSE(test_api.detachable_base_error_bubble()->GetVisible());

  // Change detachable base to a base different than the one previously used by
  // the user - verify that a detachable base error bubble is shown, and the
  // auth error is hidden.
  detachable_base_model->SetPairingStatus(
      DetachableBasePairingStatus::kAuthenticated, "5678");

  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());
  EXPECT_FALSE(test_api.auth_error_bubble()->GetVisible());
}

TEST_F(LockContentsViewUnitTest, AuthErrorDoesNotRemoveDetachableBaseError) {
  auto fake_detachable_base_model =
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher());
  FakeLoginDetachableBaseModel* detachable_base_model =
      fake_detachable_base_model.get();

  // Build lock screen with a single user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(), std::move(fake_detachable_base_model));
  SetUserCount(1);

  const AccountId& kUserAccountId = users()[0].basic_user_info.account_id;

  // Initialize the detachable base state, as if the user has previously used
  // detachable base.
  detachable_base_model->InitLastUsedBases({{kUserAccountId, "1234"}});
  SetWidget(CreateWidgetWithContent(contents));

  detachable_base_model->SetPairingStatus(
      DetachableBasePairingStatus::kAuthenticated, "1234");

  LockContentsViewTestApi test_api(contents);

  EXPECT_FALSE(test_api.detachable_base_error_bubble()->GetVisible());

  // Change detachable base to a base different than the one previously used by
  // the user - verify that a detachable base error bubble is shown, and the
  // auth error is hidden.
  detachable_base_model->SetPairingStatus(
      DetachableBasePairingStatus::kAuthenticated, "5678");

  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());

  // Attempt and fail user auth - an auth error is expected to be shown.
  // Detachable base error should not be hidden.
  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);
  EXPECT_CALL(*client,
              AuthenticateUserWithPasswordOrPin_(kUserAccountId, _, false, _));

  // Submit password.
  LoginAuthUserView::TestApi(test_api.primary_big_view()->auth_user())
      .password_view()
      ->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_api.auth_error_bubble()->GetVisible());
  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());

  // User action, like pressing a key, should not close the detachable base
  // error bubble.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);

  EXPECT_TRUE(test_api.detachable_base_error_bubble()->GetVisible());
}

TEST_F(LockContentsViewKeyboardUnitTest, SwitchPinAndVirtualKeyboard) {
  ASSERT_NO_FATAL_FAILURE(ShowLockScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  ASSERT_NE(nullptr, contents);

  // Add user who can use pin authentication.
  const std::string email = "user@domain.com";
  AddUserByEmail(email);
  // When the user gets added, the password textfield is shown by default and
  // automatically gets focused, resulting in the virtual keyboard being shown
  // if enabled.
  ASSERT_NO_FATAL_FAILURE(HideKeyboard());
  contents->OnPinEnabledForUserChanged(AccountId::FromUserEmail(email), true,
                                       /*available_at*/ std::nullopt);
  LoginBigUserView* big_view =
      LockContentsViewTestApi(contents).primary_big_view();
  ASSERT_NE(nullptr, big_view);
  ASSERT_NE(nullptr, big_view->auth_user());

  // Pin keyboard should only be visible when there is no virtual keyboard
  // shown.
  LoginPinView* pin_view =
      LoginAuthUserView::TestApi(big_view->auth_user()).pin_view();
  EXPECT_TRUE(pin_view->GetVisible());

  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  EXPECT_FALSE(pin_view->GetVisible());

  ASSERT_NO_FATAL_FAILURE(HideKeyboard());
  EXPECT_TRUE(pin_view->GetVisible());
}

TEST_F(LockContentsViewKeyboardUnitTest,
       RotationWithKeyboardDoesNotCoverInput) {
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  ASSERT_NE(nullptr, contents);

  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          contents->GetWidget()->GetNativeWindow());

  for (int user_count = 1; user_count < 10; user_count++) {
    SetUserCount(user_count);
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_0,
        display::Display::RotationSource::ACTIVE);

    ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
    const int height_when_keyboard_shown = contents->height();
    ASSERT_NO_FATAL_FAILURE(HideKeyboard());
    const int height_when_keyboard_hidden = contents->height();
    EXPECT_LT(height_when_keyboard_shown, height_when_keyboard_hidden);

    ASSERT_NO_FATAL_FAILURE(ShowKeyboard());

    EXPECT_EQ(height_when_keyboard_shown, contents->height());
    // Rotate the display to 90 degrees (portrait).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_90,
        display::Display::RotationSource::ACTIVE);

    // Rotate the display back to 0 degrees (landscape).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_0,
        display::Display::RotationSource::ACTIVE);
    EXPECT_EQ(height_when_keyboard_shown, contents->height());

    ASSERT_NO_FATAL_FAILURE(HideKeyboard());

    EXPECT_EQ(height_when_keyboard_hidden, contents->height());
    // Rotate the display to 90 degrees (portrait).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_90,
        display::Display::RotationSource::ACTIVE);

    // Rotate the display back to 0 degrees (landscape).
    display_manager()->SetDisplayRotation(
        display.id(), display::Display::ROTATE_0,
        display::Display::RotationSource::ACTIVE);

    EXPECT_EQ(height_when_keyboard_hidden, contents->height());
  }
}

// Verifies that swapping auth users while the virtual keyboard is active
// focuses the other user's password field.
TEST_F(LockContentsViewKeyboardUnitTest, SwitchUserWhileKeyboardShown) {
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  ASSERT_NE(nullptr, contents);

  SetUserCount(2);

  LoginAuthUserView::TestApi primary_user(
      LockContentsViewTestApi(contents).primary_big_view()->auth_user());
  LoginAuthUserView::TestApi secondary_user(
      LockContentsViewTestApi(contents).opt_secondary_big_view()->auth_user());

  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  EXPECT_TRUE(LoginPasswordView::TestApi(primary_user.password_view())
                  .textfield()
                  ->HasFocus());

  // Simulate a button click on the secondary UserView.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      secondary_user.user_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  EXPECT_TRUE(LoginPasswordView::TestApi(secondary_user.password_view())
                  .textfield()
                  ->HasFocus());
  EXPECT_FALSE(LoginPasswordView::TestApi(primary_user.password_view())
                   .textfield()
                   ->HasFocus());

  // Simulate a button click on the primary UserView.
  generator->MoveMouseTo(
      primary_user.user_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  EXPECT_TRUE(LoginPasswordView::TestApi(primary_user.password_view())
                  .textfield()
                  ->HasFocus());
  EXPECT_FALSE(LoginPasswordView::TestApi(secondary_user.password_view())
                   .textfield()
                   ->HasFocus());
}

TEST_F(LockContentsViewKeyboardUnitTest, PinSubmitWithVirtualKeyboardShown) {
  ASSERT_NO_FATAL_FAILURE(ShowLockScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();

  // Add user who can use pin authentication.
  const std::string email = "user@domain.com";
  AddUserByEmail(email);
  // When the user gets added, the password textfield is shown by default and
  // automatically gets focused, resulting in the virtual keyboard being shown
  // if enabled.
  ASSERT_NO_FATAL_FAILURE(HideKeyboard());
  contents->OnPinEnabledForUserChanged(AccountId::FromUserEmail(email), true,
                                       /*available_at*/ std::nullopt);
  LoginBigUserView* big_view =
      LockContentsViewTestApi(contents).primary_big_view();

  // Require that AuthenticateUser is called with authenticated_by_pin set to
  // true.
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                           _, "1111", true /*authenticated_by_pin*/, _));

  LoginPinView* pin_view =
      LoginAuthUserView::TestApi(big_view->auth_user()).pin_view();
  // Hide the PIN keyboard when the virtual keyboard is shown.
  EXPECT_TRUE(pin_view->GetVisible());
  ASSERT_NO_FATAL_FAILURE(ShowKeyboard());
  EXPECT_FALSE(pin_view->GetVisible());

  // Submit a password.
  LoginAuthUserView::TestApi(big_view->auth_user())
      .password_view()
      ->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_1, 0);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_1, 0);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_1, 0);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_1, 0);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN, 0);
  base::RunLoop().RunUntilIdle();
}

// Verify that swapping works in two user layout between one regular auth user
// and one public account user.
TEST_F(LockContentsViewUnitTest, SwapAuthAndPublicAccountUserInTwoUserLayout) {
  // Build lock screen with two users: one public account user and one regular
  // user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  AddPublicAccountUsers(1);
  AddUsers(1);

  LockContentsViewTestApi test_api(contents);

  // Capture user info to validate it did not change during the swap.
  AccountId primary_user =
      test_api.primary_big_view()->GetCurrentUser().basic_user_info.account_id;
  AccountId secondary_user = test_api.opt_secondary_big_view()
                                 ->GetCurrentUser()
                                 .basic_user_info.account_id;
  EXPECT_NE(primary_user, secondary_user);

  // Primary user starts with auth. Secondary user does not have any auth.
  EXPECT_TRUE(test_api.primary_big_view()->IsAuthEnabled());
  EXPECT_FALSE(test_api.opt_secondary_big_view()->IsAuthEnabled());

  // Verify the LoginBigUserView has built the child view correctly.
  ASSERT_TRUE(test_api.primary_big_view()->public_account());
  ASSERT_FALSE(test_api.primary_big_view()->auth_user());
  ASSERT_FALSE(test_api.opt_secondary_big_view()->public_account());
  ASSERT_TRUE(test_api.opt_secondary_big_view()->auth_user());

  // Send event to swap users.
  ui::test::EventGenerator* generator = GetEventGenerator();
  LoginAuthUserView::TestApi secondary_test_api(
      test_api.opt_secondary_big_view()->auth_user());
  generator->MoveMouseTo(
      secondary_test_api.user_view()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // User info is not swapped.
  EXPECT_EQ(
      primary_user,
      test_api.primary_big_view()->GetCurrentUser().basic_user_info.account_id);
  EXPECT_EQ(secondary_user, test_api.opt_secondary_big_view()
                                ->GetCurrentUser()
                                .basic_user_info.account_id);

  // Child view of LoginBigUserView stays the same.
  ASSERT_TRUE(test_api.primary_big_view()->public_account());
  ASSERT_FALSE(test_api.primary_big_view()->auth_user());
  ASSERT_FALSE(test_api.opt_secondary_big_view()->public_account());
  ASSERT_TRUE(test_api.opt_secondary_big_view()->auth_user());

  // Active auth (ie, which user is showing password) is swapped.
  EXPECT_FALSE(test_api.primary_big_view()->IsAuthEnabled());
  EXPECT_TRUE(test_api.opt_secondary_big_view()->IsAuthEnabled());
}

// Ensures that when swapping from a user list, the entire user info is swapped
// and the primary big user will rebuild its child view when necessary.
TEST_F(LockContentsViewUnitTest, SwapUserListToPrimaryBigUser) {
  // Build lock screen with 4 users: two public account users and two regular
  // users.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  AddPublicAccountUsers(2);
  AddUsers(2);

  LockContentsViewTestApi contents_test_api(contents);
  ScrollableUsersListView::TestApi users_list(contents_test_api.users_list());
  EXPECT_EQ(users().size() - 1, users_list.user_views().size());

  LoginBigUserView* primary_big_view = contents_test_api.primary_big_view();

  // Verify that primary_big_view is public account user.
  ASSERT_TRUE(primary_big_view->public_account());
  ASSERT_FALSE(primary_big_view->auth_user());

  const LoginUserView* user_view0 = users_list.user_views()[0];
  const LoginUserView* user_view1 = users_list.user_views()[1];
  const LoginUserView* user_view2 = users_list.user_views()[2];

  // Clicks on |view| to make it swap with the primary big user.
  auto click_view = [&](const LoginUserView* view) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    generator->ClickLeftButton();
  };

  auto is_public_account = [](const LoginUserView* view) -> bool {
    return view->current_user().basic_user_info.type ==
           user_manager::UserType::kPublicAccount;
  };

  // Case 1: Swap user_view0 (public account user) with primary big user (public
  // account user).
  EXPECT_TRUE(is_public_account(user_view0));
  AccountId primary_id =
      primary_big_view->GetCurrentUser().basic_user_info.account_id;
  AccountId list_user_id =
      user_view0->current_user().basic_user_info.account_id;
  EXPECT_NE(primary_id, list_user_id);

  // Send event to swap users.
  click_view(user_view0);

  // User info is swapped.
  EXPECT_EQ(list_user_id,
            primary_big_view->GetCurrentUser().basic_user_info.account_id);
  EXPECT_EQ(primary_id, user_view0->current_user().basic_user_info.account_id);

  // Child view of primary big user stays the same.
  ASSERT_TRUE(primary_big_view->public_account());
  ASSERT_FALSE(primary_big_view->auth_user());
  // user_view0 is still public account user.
  EXPECT_TRUE(is_public_account(user_view0));

  // Case 2: Swap user_view1 (auth user) with primary big user (public account
  // user).
  EXPECT_FALSE(is_public_account(user_view1));
  primary_id = primary_big_view->GetCurrentUser().basic_user_info.account_id;
  list_user_id = user_view1->current_user().basic_user_info.account_id;
  EXPECT_NE(primary_id, list_user_id);

  // Send event to swap users.
  click_view(user_view1);

  // User info is swapped.
  EXPECT_EQ(list_user_id,
            primary_big_view->GetCurrentUser().basic_user_info.account_id);
  EXPECT_EQ(primary_id, user_view1->current_user().basic_user_info.account_id);

  // Primary big user becomes auth user and its child view is rebuilt.
  ASSERT_FALSE(primary_big_view->public_account());
  ASSERT_TRUE(primary_big_view->auth_user());
  // user_view1 becomes public account user.
  EXPECT_TRUE(is_public_account(user_view1));

  // Case 3: Swap user_view2 (auth user) with primary big user (auth user).
  EXPECT_FALSE(is_public_account(user_view2));
  primary_id = primary_big_view->GetCurrentUser().basic_user_info.account_id;
  list_user_id = user_view2->current_user().basic_user_info.account_id;
  EXPECT_NE(primary_id, list_user_id);

  // Send event to swap users.
  click_view(user_view2);

  // User info is swapped.
  EXPECT_EQ(list_user_id,
            primary_big_view->GetCurrentUser().basic_user_info.account_id);
  EXPECT_EQ(primary_id, user_view2->current_user().basic_user_info.account_id);

  // Child view of primary big user stays the same.
  ASSERT_FALSE(primary_big_view->public_account());
  ASSERT_TRUE(primary_big_view->auth_user());
  // user_view2 is still auth user.
  EXPECT_FALSE(is_public_account(user_view2));

  // Case 4: Swap user_view0 (public account user) with with primary big user
  // (auth user).
  EXPECT_TRUE(is_public_account(user_view0));
  primary_id = primary_big_view->GetCurrentUser().basic_user_info.account_id;
  list_user_id = user_view0->current_user().basic_user_info.account_id;
  EXPECT_NE(primary_id, list_user_id);

  // Send event to swap users.
  click_view(user_view0);

  // User info is swapped.
  EXPECT_EQ(list_user_id,
            primary_big_view->GetCurrentUser().basic_user_info.account_id);
  EXPECT_EQ(primary_id, user_view0->current_user().basic_user_info.account_id);

  // Primary big user becomes public account user and its child view is rebuilt.
  ASSERT_TRUE(primary_big_view->public_account());
  ASSERT_FALSE(primary_big_view->auth_user());
  // user_view0 becomes auth user.
  EXPECT_FALSE(is_public_account(user_view0));
}

// Validates that swapping between two auth users also changes password focus.
TEST_F(LockContentsViewUnitTest, AuthUserSwapFocusesPassword) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  AddUsers(2);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  auto do_test = [&](AuthTarget auth_target) {
    SCOPED_TRACE(AuthTargetToString(auth_target));

    LoginAuthUserView::TestApi test_api =
        MakeLoginAuthTestApi(contents, auth_target);
    LoginPasswordView* password = test_api.password_view();

    // Focus user, validate password did not get focused, then activate the
    // user, which shows and focuses the password.
    test_api.user_view()->RequestFocus();
    EXPECT_FALSE(HasFocusInAnyChildView(password));
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    EXPECT_TRUE(HasFocusInAnyChildView(password));
  };

  // do_test requires that the auth target is not active, so do secondary before
  // primary.
  do_test(AuthTarget::kSecondary);
  do_test(AuthTarget::kPrimary);
}

// Validates that tapping on an auth user will refocus the password.
TEST_F(LockContentsViewUnitTest, TapOnAuthUserFocusesPassword) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  auto do_test = [&](AuthTarget auth_target) {
    SCOPED_TRACE(testing::Message()
                 << "users=" << users().size()
                 << ", auth_target=" << AuthTargetToString(auth_target));

    LoginAuthUserView::TestApi auth_user_test_api =
        MakeLoginAuthTestApi(contents, auth_target);
    LoginPasswordView* password = auth_user_test_api.password_view();

    // Activate |auth_target|.
    auth_user_test_api.user_view()->RequestFocus();
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    // Move focus off of |auth_target|'s password.
    ASSERT_TRUE(HasFocusInAnyChildView(password));
    PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
    EXPECT_FALSE(HasFocusInAnyChildView(password));

    // Click the user view, verify the password was focused.
    GetEventGenerator()->MoveMouseTo(
        auth_user_test_api.user_view()->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickLeftButton();
    EXPECT_TRUE(HasFocusInAnyChildView(password));
  };

  SetUserCount(1);
  do_test(AuthTarget::kPrimary);

  SetUserCount(2);
  do_test(AuthTarget::kPrimary);
  do_test(AuthTarget::kSecondary);

  SetUserCount(3);
  do_test(AuthTarget::kPrimary);
}

// Validates that swapping between users in user lists maintains password focus.
TEST_F(LockContentsViewUnitTest, UserListUserSwapFocusesPassword) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi contents_test_api(contents);
  AddUsers(3);

  LoginPasswordView* password_view =
      LoginAuthUserView::TestApi(
          contents_test_api.primary_big_view()->auth_user())
          .password_view();
  LoginUserView* user_view = contents_test_api.users_list()->user_view_at(0);

  // Focus the user view, verify the password does not have focus, activate the
  // user view, verify the password now has focus.
  user_view->RequestFocus();
  EXPECT_FALSE(HasFocusInAnyChildView(password_view));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_TRUE(HasFocusInAnyChildView(password_view));
}

TEST_F(LockContentsViewUnitTest, BadDetachableBaseUnfocusesPasswordView) {
  auto fake_detachable_base_model =
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher());
  FakeLoginDetachableBaseModel* detachable_base_model =
      fake_detachable_base_model.get();
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(), std::move(fake_detachable_base_model));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  SetUserCount(3);

  LockContentsViewTestApi test_api(contents);
  LoginBigUserView* primary_view = test_api.primary_big_view();
  LoginPasswordView* primary_password_view =
      LoginAuthUserView::TestApi(primary_view->auth_user()).password_view();

  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_password_view));

  detachable_base_model->SetPairingStatus(
      DetachableBasePairingStatus::kNotAuthenticated, "");
  EXPECT_FALSE(
      login_views_utils::HasFocusInAnyChildView(primary_password_view));

  // Swapping to another user should still not focus password view.
  LoginUserView* first_list_user = test_api.users_list()->user_view_at(0);
  first_list_user->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  EXPECT_FALSE(
      login_views_utils::HasFocusInAnyChildView(primary_password_view));
}

TEST_F(LockContentsViewUnitTest, ExpandedPublicSessionView) {
  // Build lock screen with 3 users: one public account user and two regular
  // users.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  LockContentsViewTestApi lock_contents(contents);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  AddPublicAccountUsers(1);
  AddUsers(2);

  views::View* main_view = lock_contents.main_view();
  LoginExpandedPublicAccountView* expanded_view = lock_contents.expanded_view();
  EXPECT_TRUE(main_view->GetVisible());
  EXPECT_FALSE(expanded_view->GetVisible());

  LoginBigUserView* primary_big_view = lock_contents.primary_big_view();
  AccountId primary_id =
      primary_big_view->GetCurrentUser().basic_user_info.account_id;

  // Open the expanded public session view.
  ui::test::EventGenerator* generator = GetEventGenerator();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);

  EXPECT_FALSE(main_view->GetVisible());
  EXPECT_TRUE(expanded_view->GetVisible());
  EXPECT_EQ(expanded_view->current_user().basic_user_info.account_id,
            primary_id);

  // Expect LanuchPublicSession mojo call when the submit button is clicked.
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client, LaunchPublicSession(primary_id, _, _));

  // Click on the submit button.
  LoginExpandedPublicAccountView::TestApi expanded_view_api(expanded_view);
  generator->MoveMouseTo(
      expanded_view_api.submit_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  base::RunLoop().RunUntilIdle();
}

TEST_F(LockContentsViewUnitTest, OnAuthEnabledForUserChanged) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents));

  const AccountId& kFirstUserAccountId = users()[0].basic_user_info.account_id;
  LockContentsViewTestApi contents_test_api(contents);
  LoginAuthUserView::TestApi auth_test_api(
      contents_test_api.primary_big_view()->auth_user());
  LoginPasswordView* password_view = auth_test_api.password_view();
  LoginPinView* pin_view = auth_test_api.pin_view();
  views::View* disabled_auth_message = auth_test_api.disabled_auth_message();

  // The password field is shown by default.
  EXPECT_TRUE(password_view->GetVisible());
  EXPECT_FALSE(pin_view->GetVisible());
  EXPECT_FALSE(disabled_auth_message->GetVisible());
  // Setting auth disabled will hide the password field and show the message.
  DataDispatcher()->DisableAuthForUser(kFirstUserAccountId,
                                       GetTestDisabledAuthData());
  EXPECT_FALSE(password_view->GetVisible());
  EXPECT_FALSE(pin_view->GetVisible());
  EXPECT_TRUE(disabled_auth_message->GetVisible());
  // Setting auth enabled will hide the message and show the password field.
  DataDispatcher()->EnableAuthForUser(kFirstUserAccountId);
  EXPECT_TRUE(password_view->GetVisible());
  EXPECT_FALSE(pin_view->GetVisible());
  EXPECT_FALSE(disabled_auth_message->GetVisible());

  // Set auth disabled again.
  DataDispatcher()->DisableAuthForUser(kFirstUserAccountId,
                                       GetTestDisabledAuthData());
  EXPECT_FALSE(password_view->GetVisible());
  EXPECT_FALSE(pin_view->GetVisible());
  EXPECT_TRUE(disabled_auth_message->GetVisible());
  // Enable PIN. There's no UI change because auth is currently disabled.
  DataDispatcher()->SetPinEnabledForUser(kFirstUserAccountId, true,
                                         /*available_at*/ std::nullopt);
  EXPECT_FALSE(password_view->GetVisible());
  EXPECT_FALSE(pin_view->GetVisible());
  EXPECT_TRUE(disabled_auth_message->GetVisible());
  // Set auth enabled again. Both password field and PIN keyboard are shown.
  DataDispatcher()->EnableAuthForUser(kFirstUserAccountId);
  EXPECT_TRUE(password_view->GetVisible());
  EXPECT_TRUE(pin_view->GetVisible());
  EXPECT_FALSE(disabled_auth_message->GetVisible());
}

TEST_F(LockContentsViewUnitTest, ShowReasonOnAuthDisabled) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents));

  const AccountId& kFirstUserAccountId = users()[0].basic_user_info.account_id;
  LockContentsViewTestApi contents_test_api(contents);
  LoginAuthUserView::TestApi auth_test_api(
      contents_test_api.primary_big_view()->auth_user());
  LoginPasswordView* password_view = auth_test_api.password_view();
  LoginPinView* pin_view = auth_test_api.pin_view();
  views::View* disabled_auth_message = auth_test_api.disabled_auth_message();

  // The password field is shown by default.
  EXPECT_TRUE(password_view->GetVisible());
  EXPECT_FALSE(pin_view->GetVisible());
  EXPECT_FALSE(disabled_auth_message->GetVisible());
  // Setting auth disabled due to time window limit.
  DataDispatcher()->DisableAuthForUser(
      kFirstUserAccountId,
      AuthDisabledData(ash::AuthDisabledReason::kTimeWindowLimit,
                       base::Time::Now().LocalMidnight() + base::Hours(8),
                       base::Hours(1), true /*disable_lock_screen_media*/));
  EXPECT_FALSE(password_view->GetVisible());
  EXPECT_FALSE(pin_view->GetVisible());
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_COME_BACK_MESSAGE,
                                       u"8:00\u202fAM"),
            auth_test_api.GetDisabledAuthMessageContent());
  // Setting auth disabled due to time usage limit.
  DataDispatcher()->DisableAuthForUser(
      kFirstUserAccountId,
      AuthDisabledData(ash::AuthDisabledReason::kTimeUsageLimit,
                       base::Time::Now(), base::Minutes(30),
                       true /*disable_lock_screen_media*/));
  EXPECT_FALSE(password_view->GetVisible());
  EXPECT_FALSE(pin_view->GetVisible());
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_SCREEN_TIME_USED_MESSAGE,
                                       u"30 minutes"),
            auth_test_api.GetDisabledAuthMessageContent());
  // Setting auth disabled due to time limit override.
  DataDispatcher()->DisableAuthForUser(
      kFirstUserAccountId,
      AuthDisabledData(ash::AuthDisabledReason::kTimeLimitOverride,
                       base::Time::Now(), base::Minutes(30),
                       true /*disable_lock_screen_media*/));
  EXPECT_FALSE(password_view->GetVisible());
  EXPECT_FALSE(pin_view->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_LOGIN_MANUAL_LOCK_MESSAGE),
            auth_test_api.GetDisabledAuthMessageContent());
}

TEST_F(LockContentsViewUnitTest,
       ToggleNoteActionVisibilityOnAuthEnabledChanged) {
  auto* tray_action = Shell::Get()->tray_action();
  TestTrayActionClient action_client;
  tray_action->SetClient(action_client.CreateRemoteAndBind(),
                         mojom::TrayActionState::kAvailable);
  auto* contents = new LockContentsView(
      Shell::Get()->tray_action()->GetLockScreenNoteState(),
      LockScreen::ScreenType::kLock, DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents));

  const AccountId& kFirstUserAccountId = users()[0].basic_user_info.account_id;
  LockContentsViewTestApi contents_test_api(contents);
  views::View* note_action_button = contents_test_api.note_action();

  EXPECT_TRUE(note_action_button->GetVisible());
  // Setting auth disabled hides the note action button.
  DataDispatcher()->DisableAuthForUser(kFirstUserAccountId,
                                       GetTestDisabledAuthData());
  EXPECT_FALSE(note_action_button->GetVisible());
  // Setting auth enabled shows the note action button.
  DataDispatcher()->EnableAuthForUser(kFirstUserAccountId);
  EXPECT_TRUE(note_action_button->GetVisible());

  // Set auth disabled again.
  DataDispatcher()->DisableAuthForUser(kFirstUserAccountId,
                                       GetTestDisabledAuthData());
  EXPECT_FALSE(note_action_button->GetVisible());
  // Set the lock screen note state to |kNotAvailable| while the note action
  // button is hidden.
  tray_action->UpdateLockScreenNoteState(mojom::TrayActionState::kNotAvailable);
  DataDispatcher()->EnableAuthForUser(kFirstUserAccountId);
  // The note action button remains hidden after setting auth enabled.
  EXPECT_FALSE(note_action_button->GetVisible());
}

TEST_F(LockContentsViewUnitTest, DisabledAuthMessageFocusBehavior) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents));

  const AccountId& kFirstUserAccountId = users()[0].basic_user_info.account_id;
  LockContentsViewTestApi contents_test_api(contents);
  LoginAuthUserView::TestApi auth_test_api(
      contents_test_api.primary_big_view()->auth_user());
  views::View* disabled_auth_message = auth_test_api.disabled_auth_message();
  LoginUserView* user_view = auth_test_api.user_view();

  // The message is visible after disabling auth and it receives initial focus.
  DataDispatcher()->DisableAuthForUser(kFirstUserAccountId,
                                       GetTestDisabledAuthData());
  EXPECT_TRUE(disabled_auth_message->GetVisible());
  EXPECT_TRUE(HasFocusInAnyChildView(disabled_auth_message));
  // Tabbing from the message will move focus to the user view.
  ASSERT_TRUE(TabThroughView(GetEventGenerator(), disabled_auth_message,
                             false /*reverse*/));
  EXPECT_TRUE(HasFocusInAnyChildView(user_view));
  // Shift-tabbing from the user view will move focus back to the message.
  ASSERT_TRUE(TabThroughView(GetEventGenerator(), user_view, true /*reverse*/));
  EXPECT_TRUE(HasFocusInAnyChildView(disabled_auth_message));
  // Additional shift-tabbing will eventually move focus to the status area.
  ASSERT_TRUE(TabThroughView(GetEventGenerator(), disabled_auth_message,
                             true /*reverse*/));
  views::View* status_area =
      RootWindowController::ForWindow(contents->GetWidget()->GetNativeWindow())
          ->GetStatusAreaWidget()
          ->GetContentsView();
  EXPECT_TRUE(HasFocusInAnyChildView(status_area));
}

// Tests that the media view do not show on lock screen when auth is disabled
// after media session changes to playing.
TEST_F(LockContentsMediaViewUnitTest, DisableAuthAfterMediaSessionChanged) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  const AccountId& kFirstUserAccountId = users()[0].basic_user_info.account_id;
  LockContentsViewTestApi lock_contents(contents);

  // Simulate playing media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);
  EXPECT_TRUE(IsMediaViewDrawn(lock_contents));

  // Disable auth and media.
  DataDispatcher()->DisableAuthForUser(
      kFirstUserAccountId,
      AuthDisabledData(AuthDisabledReason::kTimeWindowLimit,
                       base::Time::Now() + base::Hours(8), base::Hours(1),
                       true /*disable_lock_screen_media*/));
  EXPECT_FALSE(IsMediaViewDrawn(lock_contents));
}

// Tests that media controls do not show on lock screen when auth is disabled
// before media session changes to playing.
TEST_F(LockContentsMediaViewUnitTest, DisableAuthBeforeMediaSessionChanged) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  const AccountId& kFirstUserAccountId = users()[0].basic_user_info.account_id;
  LockContentsViewTestApi lock_contents(contents);

  // Disable auth and media.
  DataDispatcher()->DisableAuthForUser(
      kFirstUserAccountId,
      AuthDisabledData(AuthDisabledReason::kTimeWindowLimit,
                       base::Time::Now() + base::Hours(8), base::Hours(1),
                       true /*disable_lock_screen_media*/));
  EXPECT_FALSE(IsMediaViewDrawn(lock_contents));

  // Simulate playing media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);
  EXPECT_FALSE(IsMediaViewDrawn(lock_contents));
}

TEST_F(LockContentsMediaViewUnitTest, DisableAuthAllowMediaControls) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  const AccountId& kFirstUserAccountId = users()[0].basic_user_info.account_id;
  LockContentsViewTestApi lock_contents(contents);

  // Simulate playing media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);
  EXPECT_TRUE(IsMediaViewDrawn(lock_contents));

  // Disable auth, but allow media.
  DataDispatcher()->DisableAuthForUser(
      kFirstUserAccountId,
      AuthDisabledData(AuthDisabledReason::kTimeWindowLimit,
                       base::Time::Now() + base::Hours(8), base::Hours(1),
                       false /*disable_lock_screen_media*/));
  EXPECT_TRUE(IsMediaViewDrawn(lock_contents));
}

// Tests parent access dialog showing/hiding and focus behavior.
TEST_F(LockContentsViewUnitTest, ParentAccessDialog) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  AddChildUsers(1);
  SetWidget(CreateWidgetWithContent(contents));

  LoginBigUserView* primary_view =
      LockContentsViewTestApi(contents).primary_big_view();
  LoginAuthUserView::TestApi auth_user =
      LoginAuthUserView::TestApi(primary_view->auth_user());

  EXPECT_TRUE(primary_view->auth_user());
  EXPECT_FALSE(PinRequestWidget::Get());
  EXPECT_TRUE(LoginPasswordView::TestApi(auth_user.password_view())
                  .textfield()
                  ->HasFocus());

  contents->ShowParentAccessDialog();

  EXPECT_TRUE(primary_view->auth_user());
  EXPECT_FALSE(LoginPasswordView::TestApi(auth_user.password_view())
                   .textfield()
                   ->HasFocus());

  PinRequestWidget::Get()->Close(false /* validation success */);

  EXPECT_TRUE(primary_view->auth_user());
  EXPECT_TRUE(LoginPasswordView::TestApi(auth_user.password_view())
                  .textfield()
                  ->HasFocus());
}

// Tests parent access shelf button is showing and hiding.
TEST_F(LockContentsViewUnitTest, ParentAccessButton) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  AddChildUsers(1);
  const AccountId child_id = users()[0].basic_user_info.account_id;
  SetWidget(CreateWidgetWithContent(contents));

  // Simulate initial state - user auth disabled and button shown.
  DataDispatcher()->DisableAuthForUser(child_id, GetTestDisabledAuthData());
  Shell::Get()->login_screen_controller()->ShowParentAccessButton(true);
  EXPECT_TRUE(LoginScreenTestApi::IsParentAccessButtonShown());

  // Validation failed - show the button.
  contents->ShowParentAccessDialog();
  EXPECT_FALSE(LoginScreenTestApi::IsParentAccessButtonShown());
  PinRequestWidget::Get()->Close(false /* validation success */);
  EXPECT_TRUE(LoginScreenTestApi::IsParentAccessButtonShown());

  // Validation succeeded - hide the button.
  contents->ShowParentAccessDialog();
  EXPECT_FALSE(LoginScreenTestApi::IsParentAccessButtonShown());
  PinRequestWidget::Get()->Close(true /* validation success */);
  EXPECT_FALSE(LoginScreenTestApi::IsParentAccessButtonShown());

  // Validation failed but user auth got enabled - hide button.
  // (Device got unlocked when parent access dialog was shown)
  contents->ShowParentAccessDialog();
  EXPECT_FALSE(LoginScreenTestApi::IsParentAccessButtonShown());
  DataDispatcher()->EnableAuthForUser(child_id);
  PinRequestWidget::Get()->Close(false /* validation success */);
  EXPECT_FALSE(LoginScreenTestApi::IsParentAccessButtonShown());
}

using LockContentsViewPowerManagerUnitTest = LockContentsViewUnitTest;

// Ensures that a PowerManagerClient::Observer is added on LockScreen::Show()
// and removed on LockScreen::Destroy().
TEST_F(LockContentsViewPowerManagerUnitTest,
       LockScreenManagesPowerManagerObserver) {
  ASSERT_NO_FATAL_FAILURE(ShowLockScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  EXPECT_TRUE(chromeos::PowerManagerClient::Get()->HasObserver(contents));

  LockScreen::Get()->Destroy();
  // Wait for LockScreen to be fully destroyed
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chromeos::PowerManagerClient::Get()->HasObserver(contents));
}

// Verifies that the password box for the active user is cleared if a suspend
// event is received.
TEST_F(LockContentsViewUnitTest, PasswordClearedOnSuspend) {
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  AddUsers(1);

  LockScreen::TestApi lock_screen = LockScreen::TestApi(LockScreen::Get());
  LockContentsView* contents = lock_screen.contents_view();
  LoginPasswordView* password_view = LockContentsViewTestApi(contents)
                                         .primary_big_view()
                                         ->auth_user()
                                         ->password_view();
  views::Textfield* textfield =
      LoginPasswordView::TestApi(password_view).textfield();

  textfield->SetText(u"some_password");
  // Suspend clears password.
  EXPECT_FALSE(textfield->GetText().empty());
  contents->SuspendImminent(power_manager::SuspendImminent_Reason_LID_CLOSED);
  EXPECT_TRUE(textfield->GetText().empty());
}

TEST_F(LockContentsViewUnitTest, ArrowNavSingleUser) {
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  SetUserCount(1);
  LockContentsView* lock_contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();

  LoginBigUserView* primary_big_view =
      LockContentsViewTestApi(lock_contents).primary_big_view();
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_big_view));

  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_big_view));

  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_big_view));
}

TEST_F(LockContentsViewUnitTest, ArrowNavTwoUsers) {
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  AddUsers(1);
  AddPublicAccountUsers(1);
  LockContentsViewTestApi lock_contents = LockContentsViewTestApi(
      LockScreen::TestApi(LockScreen::Get()).contents_view());

  LoginPasswordView* primary_password_view =
      LoginAuthUserView::TestApi(lock_contents.primary_big_view()->auth_user())
          .password_view();
  LoginBigUserView* secondary_user_view =
      lock_contents.opt_secondary_big_view();

  ASSERT_NE(nullptr, secondary_user_view);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_password_view));

  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(secondary_user_view));

  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_password_view));

  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(secondary_user_view));

  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_password_view));
}

TEST_F(LockContentsViewUnitTest, ArrowNavThreeUsers) {
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  SetUserCount(3);
  LockContentsViewTestApi lock_contents = LockContentsViewTestApi(
      LockScreen::TestApi(LockScreen::Get()).contents_view());

  LoginPasswordView* primary_password_view =
      LoginAuthUserView::TestApi(lock_contents.primary_big_view()->auth_user())
          .password_view();
  LoginUserView* first_list_user = lock_contents.users_list()->user_view_at(0);
  LoginUserView* second_list_user = lock_contents.users_list()->user_view_at(1);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_password_view));

  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(first_list_user));

  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(second_list_user));

  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_password_view));

  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(second_list_user));

  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(first_list_user));

  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_password_view));
}

TEST_F(LockContentsViewUnitTest, UserSwapFocusesBigView) {
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  SetUserCount(3);
  LockContentsViewTestApi lock_contents = LockContentsViewTestApi(
      LockScreen::TestApi(LockScreen::Get()).contents_view());

  LoginPasswordView* primary_password_view =
      LoginAuthUserView::TestApi(lock_contents.primary_big_view()->auth_user())
          .password_view();
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_password_view));

  lock_contents.users_list()->user_view_at(0)->RequestFocus();
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(primary_password_view));
}

TEST_F(LockContentsViewUnitTest, PowerwashShortcutSendsMojoCall) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents));

  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client,
              HandleAccelerator(ash::LoginAcceleratorAction::kShowResetScreen));

  PressAndReleaseKey(ui::KeyboardCode::VKEY_R,
                     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  base::RunLoop().RunUntilIdle();
}

TEST_F(LockContentsViewUnitTest, UsersChangedRetainsExistingState) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(2);
  SetWidget(CreateWidgetWithContent(contents));

  LockContentsViewTestApi test_api(contents);

  AccountId primary_user =
      test_api.primary_big_view()->GetCurrentUser().basic_user_info.account_id;
  DataDispatcher()->SetPinEnabledForUser(primary_user, true,
                                         /*available_at*/ std::nullopt);

  // This user should be identical to the user we enabled PIN for.
  SetUserCount(1);

  EXPECT_TRUE(
      LoginAuthUserView::TestApi(test_api.primary_big_view()->auth_user())
          .pin_view()
          ->GetVisible());
}

TEST_F(LockContentsViewUnitTest, ShowHideWarningBannerBubble) {
  // Build lock screen with a single user.
  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(lock));

  const AccountId& kUserAccountId = users()[0].basic_user_info.account_id;

  LockContentsViewTestApi test_api(lock);

  // Creating lock screen does not show warning banner bubble.
  EXPECT_FALSE(test_api.warning_banner_bubble()->GetVisible());

  // Verifies that a warning banner is shown by giving a non-empty message.
  DataDispatcher()->UpdateWarningMessage(u"foo");
  EXPECT_TRUE(test_api.warning_banner_bubble()->GetVisible());

  // Verifies that a warning banner is hidden by HideWarningBanner().
  DataDispatcher()->UpdateWarningMessage({});
  EXPECT_FALSE(test_api.warning_banner_bubble()->GetVisible());

  // Shows a warning banner again.
  DataDispatcher()->UpdateWarningMessage(u"foo");
  EXPECT_TRUE(test_api.warning_banner_bubble()->GetVisible());

  // Attempt and fail user auth - an auth error is expected to be shown.
  // The warning banner should not be hidden.
  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);
  EXPECT_CALL(*client,
              AuthenticateUserWithPasswordOrPin_(kUserAccountId, _, false, _));

  // Submit password.
  LoginAuthUserView::TestApi(test_api.primary_big_view()->auth_user())
      .password_view()
      ->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_api.auth_error_bubble()->GetVisible());
  EXPECT_TRUE(test_api.warning_banner_bubble()->GetVisible());
}

TEST_F(LockContentsViewUnitTest, RemoveUserFocusMovesBackToPrimaryUser) {
  // Build lock screen with one public account and one normal user.
  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  AddPublicAccountUsers(1);
  AddUsers(1);
  users()[1].can_remove = true;
  DataDispatcher()->SetUserList(users());
  SetWidget(CreateWidgetWithContent(lock));
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client, RemoveUser(users()[1].basic_user_info.account_id))
      .Times(1)
      .WillOnce(Invoke(this, &LoginTestBase::RemoveUser));

  LockContentsViewTestApi test_api(lock);
  LoginAuthUserView::TestApi secondary_test_api(
      test_api.opt_secondary_big_view()->auth_user());
  LoginUserView::TestApi user_test_api(secondary_test_api.user_view());

  // Remove the user. First, focus the dropdown to raise the bubble.
  user_test_api.dropdown()->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();
  // Focus the remove user bubble, tap twice to remove the user.
  secondary_test_api.remove_account_dialog()->RequestFocus();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();

  // Secondary user was removed.
  EXPECT_EQ(nullptr, test_api.opt_secondary_big_view());
  // Primary user has focus.
  EXPECT_TRUE(HasFocusInAnyChildView(test_api.primary_big_view()));
}

// Verifies that setting fingerprint state keeps the backlights forced off. A
// fingerprint state change is not a user action, excluding too many
// authentication attempts, which will trigger the auth attempt flow.
TEST_F(LockContentsViewUnitTest,
       BacklightRemainsForcedOffAfterFingerprintStateChange) {
  // Enter tablet mode so the power button events force the backlight off.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Show lock screen with one normal user.
  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  AddUsers(1);
  SetWidget(CreateWidgetWithContent(lock));

  // Force the backlights off.
  PressAndReleasePowerButton();
  EXPECT_TRUE(
      Shell::Get()->backlights_forced_off_setter()->backlights_forced_off());

  // Change fingerprint state; backlights remain forced off.
  DataDispatcher()->SetFingerprintState(
      users()[0].basic_user_info.account_id,
      FingerprintState::DISABLED_FROM_ATTEMPTS);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      Shell::Get()->backlights_forced_off_setter()->backlights_forced_off());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
}

// Verifies that a fingerprint authentication attempt makes sure the backlights
// are not forced off.
TEST_F(LockContentsViewUnitTest,
       BacklightIsNotForcedOffAfterFingerprintAuthenticationAttempt) {
  // Enter tablet mode so the power button events force the backlight off.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Show lock screen with one normal user.
  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  AddUsers(1);
  SetWidget(CreateWidgetWithContent(lock));

  // Force the backlights off.
  PressAndReleasePowerButton();
  EXPECT_TRUE(
      Shell::Get()->backlights_forced_off_setter()->backlights_forced_off());

  // Validate a fingerprint authentication attempt resets backlights being
  // forced off.
  DataDispatcher()->NotifyFingerprintAuthResult(
      users()[0].basic_user_info.account_id, false /*successful*/);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      Shell::Get()->backlights_forced_off_setter()->backlights_forced_off());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
}

TEST_F(LockContentsViewUnitTest, RightAndLeftAcceleratorsWithNoUser) {
  // Show lock screen but do *not* initialize any users.
  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(lock));

  // Nothing to validate except that there is no crash.
  lock->AcceleratorPressed(ui::Accelerator(ui::VKEY_RIGHT, 0));
  lock->AcceleratorPressed(ui::Accelerator(ui::VKEY_LEFT, 0));
}

TEST_F(LockContentsViewUnitTest, OnFocusLeavingSystemTrayWithNoUsers) {
  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(lock));

  // Check that there is always a focusable view after transitioning focus.
  lock->OnFocusLeavingSystemTray(false /* reverse */);
  EXPECT_TRUE(lock->GetFocusManager()->GetFocusedView());
  lock->OnFocusLeavingSystemTray(true /* reverse */);
  EXPECT_TRUE(lock->GetFocusManager()->GetFocusedView());
}

TEST_F(LockContentsViewUnitTest, OnFocusLeavingSystemTrayWithOobeDialogOpen) {
  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(lock));

  // FocusOobeDialog called when OOBE dialog visible.
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client, FocusOobeDialog()).Times(1);

  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::GAIA_SIGNIN);
  lock->OnFocusLeavingSystemTray(false /* reverse */);
}

TEST_F(LockContentsViewUnitTest, OnFocusLeavingSystemTrayWithOobeDialogClosed) {
  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(lock));

  // FocusOobeDialog not called when OOBE dialog not visible.
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client, FocusOobeDialog()).Times(0);

  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::HIDDEN);
  lock->OnFocusLeavingSystemTray(false /* reverse */);
}

TEST_F(LockContentsViewUnitTest, LoginNotReactingOnEventsWithOobeDialogShown) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(contents));
  SetUserCount(3);

  LockContentsViewTestApi lock_contents(contents);
  ScrollableUsersListView::TestApi users_list(lock_contents.users_list());
  const auto* const list_user_view = users_list.user_views()[0].get();
  LoginBigUserView* auth_view = lock_contents.primary_big_view();

  AccountId auth_view_user =
      auth_view->GetCurrentUser().basic_user_info.account_id;
  AccountId list_user =
      list_user_view->current_user().basic_user_info.account_id;

  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::GAIA_SIGNIN);

  // Send event to swap users.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(list_user_view->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // User info is not swapped.
  EXPECT_EQ(auth_view_user,
            auth_view->GetCurrentUser().basic_user_info.account_id);
  EXPECT_EQ(list_user,
            list_user_view->current_user().basic_user_info.account_id);

  // Hide OOBE dialog.
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::HIDDEN);

  // Attempt swap again.
  generator->MoveMouseTo(list_user_view->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // User info should be now swapped.
  EXPECT_EQ(list_user, auth_view->GetCurrentUser().basic_user_info.account_id);
  EXPECT_EQ(auth_view_user,
            list_user_view->current_user().basic_user_info.account_id);
}

TEST_F(LockContentsMediaViewUnitTest,
       LockScreenMediaControlsShownIfMediaPlaying) {
  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi lock_contents(contents);

  // Simulate playing media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);

  // Verify media controls are shown.
  EXPECT_TRUE(IsMediaViewDrawn(lock_contents));
}

TEST_F(LockContentsMediaViewUnitTest, LockScreenMediaControlsHiddenAfterDelay) {
  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi lock_contents(contents);

  // Test timer
  auto mock_timer_unique = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* mock_timer = mock_timer_unique.get();
  lock_contents.media_view()->SetSwitchMediaDelayTimerForTesting(
      std::move(mock_timer_unique));

  // Simulate playing media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);

  // Simulate media session stopping and delay.
  lock_contents.media_view()->MediaSessionChanged(std::nullopt);

  mock_timer->Fire();
  base::RunLoop().RunUntilIdle();

  // Simulate playing media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);

  // Verify media controls are hidden.
  EXPECT_FALSE(IsMediaViewDrawn(lock_contents));
}

TEST_F(LockContentsMediaViewUnitTest,
       MediaControlsHiddenIfScreenLockedWhileMediaPaused) {
  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi lock_contents(contents);

  // Simulate paused media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPaused);

  // Verify media controls are hidden.
  EXPECT_FALSE(IsMediaViewDrawn(lock_contents));
}

TEST_F(LockContentsMediaViewUnitTest, KeepMediaControlsShownWithinDelay) {
  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi lock_contents(contents);

  // Simulate playing media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);

  // Simulate media session stopping.
  lock_contents.media_view()->MediaSessionChanged(std::nullopt);

  // Simulate new media session starting within timer delay.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);

  // Verify media controls are shown.
  EXPECT_TRUE(IsMediaViewDrawn(lock_contents));
}

TEST_F(LockContentsMediaViewUnitTest, LockScreenMediaControlsHiddenNoMedia) {
  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi lock_contents(contents);

  // Simulate no media session on lock screen.
  lock_contents.media_view()->MediaSessionInfoChanged(nullptr);

  // Verify media controls are hidden.
  EXPECT_FALSE(IsMediaViewDrawn(lock_contents));
}

TEST_F(LockContentsMediaViewUnitTest,
       ShowMediaControlsIfPausedAndAlreadyShowing) {
  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi lock_contents(contents);

  // Simulate playing media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);

  // Simulate media session paused.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPaused);

  // Verify media controls are shown.
  EXPECT_TRUE(IsMediaViewDrawn(lock_contents));
}

TEST_F(LockContentsMediaViewUnitTest,
       LockScreenMediaControlsHiddenIfPreferenceDisabled) {
  // Disable user preference for media controls.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetBoolean(prefs::kLockScreenMediaControlsEnabled, false);

  // Build lock screen with 1 user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi lock_contents(contents);

  // Simulate active and playing media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);

  // Verify media controls are hidden.
  EXPECT_FALSE(IsMediaViewDrawn(lock_contents));
}

TEST_F(LockContentsMediaViewUnitTest, MediaControlsHiddenOnLoginScreen) {
  // Build login screen with 1 user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi lock_contents(contents);

  // Simulate active and playing media session.
  SimulateMediaSessionChanged(
      lock_contents, media_session::mojom::MediaPlaybackState::kPlaying);

  // Verify media controls are hidden on login screen for one user.
  EXPECT_FALSE(IsMediaViewDrawn(lock_contents));

  SetUserCount(5);

  // Verify that media view isn't created for non low-density layouts.
  EXPECT_EQ(nullptr, lock_contents.media_view());
}

TEST_F(LockContentsViewUnitTest, NoNavigationOrHotseatOnLockScreen) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  LockContentsView* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  ShelfWidget* shelf_widget =
      Shelf::ForWindow(widget->GetNativeWindow())->shelf_widget();
  EXPECT_FALSE(shelf_widget->navigation_widget()->IsVisible())
      << "The navigation widget should not appear on the lock screen.";
  EXPECT_FALSE(shelf_widget->hotseat_widget()->IsVisible())
      << "The hotseat widget should not appear on the lock screen.";
}

TEST_F(LockContentsViewUnitTest, NoUsersToShow) {
  // Build lock screen with 0 users.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi test_api(contents);
  DataDispatcher()->SetUserList(users());

  // Verify that primary big view is null.
  EXPECT_THAT(test_api.primary_big_view(), IsNull());
  // Verify that the main view has no children.
  EXPECT_TRUE(test_api.main_view()->children().empty());
}

TEST_F(LockContentsViewUnitTest, ToggleGaiaOnUsersChanged) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LockContentsViewTestApi test_api(contents);
  auto client = std::make_unique<MockLoginScreenClient>();
  // Expect Gaia to show when there is no users.
  EXPECT_CALL(*client, ShowGaiaSignin(_)).Times(1);
  AddUsers(0);
  Mock::VerifyAndClearExpectations(client.get());
}

TEST_F(LockContentsViewUnitTest, UpdatingSmartLockStateSetsAuthMethod) {
  // Build login screen with 1 user.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  AddUsers(1);
  SetWidget(CreateWidgetWithContent(contents));
  LoginBigUserView* big_view =
      LockContentsViewTestApi(contents).primary_big_view();

  std::pair<SmartLockState, bool> state_and_is_auth_method_expected[] = {
      {SmartLockState::kDisabled, false},
      {SmartLockState::kInactive, false},
      {SmartLockState::kBluetoothDisabled, true},
      {SmartLockState::kPhoneNotLockable, true},
      {SmartLockState::kPhoneNotFound, true},
      {SmartLockState::kConnectingToPhone, true},
      {SmartLockState::kPhoneNotAuthenticated, true},
      {SmartLockState::kPhoneFoundLockedAndDistant, true},
      {SmartLockState::kPhoneFoundLockedAndProximate, true},
      {SmartLockState::kPhoneFoundUnlockedAndDistant, true},
      {SmartLockState::kPhoneAuthenticated, true},
      {SmartLockState::kPrimaryUserAbsent, true}};

  for (const auto& it : state_and_is_auth_method_expected) {
    VerifyUpdatingSmartLockStateSetsAuthMethod(
        big_view, /*smart_lock_state=*/it.first,
        /*should_have_auth_method=*/it.second);
  }
}

TEST_F(LockContentsViewUnitTest, SmartLockStateHidesPasswordView) {
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Build login screen with 1 user.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  LockContentsViewTestApi test_api(contents);
  AddUsers(1);
  const AccountId account_id = test_api.users()[0].account_id;
  SetWidget(CreateWidgetWithContent(contents));
  LoginBigUserView* big_view =
      LockContentsViewTestApi(contents).primary_big_view();
  ASSERT_TRUE(big_view);
  LoginAuthUserView* auth_user_view = big_view->auth_user();
  ASSERT_TRUE(auth_user_view);

  EXPECT_TRUE(auth_user_view->password_view()->GetVisible());

  // Check that password view is still visible when auth
  // factor is in kReady state.
  DataDispatcher()->SetSmartLockState(
      account_id, SmartLockState::kPhoneFoundLockedAndProximate);
  EXPECT_TRUE(auth_user_view->password_view()->GetVisible());

  // Check that password view is no longer visible when auth
  // factor is in kClickRequired state.
  DataDispatcher()->SetSmartLockState(account_id,
                                      SmartLockState::kPhoneAuthenticated);
  EXPECT_FALSE(auth_user_view->password_view()->GetVisible());
}

TEST_F(LockContentsViewUnitTest, SmartLockStateHidesAuthErrorMessage) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ASSERT_NO_FATAL_FAILURE(ShowLockScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();
  LockContentsViewTestApi test_api(contents);
  SetUserCount(1);
  const AccountId account_id = test_api.users()[0].account_id;
  SetWidget(CreateWidgetWithContent(contents));

  // Submit incorrect password to show auth error bubble.
  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);
  EXPECT_CALL(*client,
              AuthenticateUserWithPasswordOrPin_(account_id, _, false, _));
  PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_api.auth_error_bubble()->GetVisible());

  // Check that the auth_error_bubble is no longer visible when the auth factor
  // hides the password. Updating auth factor state to kAuthenticated by
  // notifying a successful auth result will hide the password field.
  DataDispatcher()->NotifySmartLockAuthResult(account_id, /*successful=*/true);
  EXPECT_FALSE(test_api.auth_error_bubble()->GetVisible());
}

TEST_F(LockContentsViewUnitTest,
       LoginNotReactingOnEventsWithLoginExtensionUiShown) {
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(contents));
  SetUserCount(3);

  LockContentsViewTestApi lock_contents(contents);
  ScrollableUsersListView::TestApi users_list(lock_contents.users_list());
  const auto* const list_user_view = users_list.user_views()[0].get();
  LoginBigUserView* auth_view = lock_contents.primary_big_view();

  AccountId auth_view_user =
      auth_view->GetCurrentUser().basic_user_info.account_id;
  AccountId list_user =
      list_user_view->current_user().basic_user_info.account_id;

  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::EXTENSION_LOGIN);

  // Send event to swap users.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(list_user_view->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // User info is not swapped.
  EXPECT_EQ(auth_view_user,
            auth_view->GetCurrentUser().basic_user_info.account_id);
  EXPECT_EQ(list_user,
            list_user_view->current_user().basic_user_info.account_id);

  // Hide Login extension UI.
  DataDispatcher()->NotifyOobeDialogState(
      OobeDialogState::EXTENSION_LOGIN_CLOSED);

  // Attempt swap again.
  generator->MoveMouseTo(list_user_view->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  // User info should be now swapped.
  EXPECT_EQ(list_user, auth_view->GetCurrentUser().basic_user_info.account_id);
  EXPECT_EQ(auth_view_user,
            list_user_view->current_user().basic_user_info.account_id);
}

TEST_F(LockContentsViewUnitTest, LoginExtensionUiWithUsers) {
  // If the device is enrolled, bottom_status_indicator should be visible.
  Shell::Get()->system_tray_model()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"BestCompanyEver", ManagementDeviceMode::kNone});

  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(contents));
  SetUserCount(3);

  LockContentsViewTestApi lock_contents(contents);

  // login_camera_timeout_view not created when there are users.
  EXPECT_EQ(nullptr, lock_contents.login_camera_timeout_view());

  views::View* main_view = lock_contents.main_view();
  EXPECT_TRUE(main_view->GetVisible());
  EXPECT_TRUE(lock_contents.bottom_status_indicator()->GetVisible());

  // Show login screen extension UI.
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::EXTENSION_LOGIN);

  // Main view and bottom_status_indicator not visible any more.
  EXPECT_FALSE(main_view->GetVisible());
  EXPECT_FALSE(lock_contents.bottom_status_indicator()->GetVisible());

  // Close login screen extension UI.
  DataDispatcher()->NotifyOobeDialogState(
      OobeDialogState::EXTENSION_LOGIN_CLOSED);

  EXPECT_TRUE(main_view->GetVisible());
  EXPECT_TRUE(lock_contents.bottom_status_indicator()->GetVisible());
}

TEST_F(LockContentsViewUnitTest, LoginExtensionUiWithNoUsers) {
  // If the device is enrolled, bottom_status_indicator should be visible.
  Shell::Get()->system_tray_model()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"BestCompanyEver", ManagementDeviceMode::kNone});

  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(contents));
  SetUserCount(0);

  LockContentsViewTestApi lock_contents(contents);

  // login_camera_timeout_view is created when there are no users.
  ASSERT_NE(nullptr, lock_contents.login_camera_timeout_view());
  LoginCameraTimeoutView::TestApi login_camera_timeout_view(
      lock_contents.login_camera_timeout_view());

  views::View* main_view = lock_contents.main_view();
  EXPECT_TRUE(main_view->GetVisible());
  EXPECT_TRUE(lock_contents.bottom_status_indicator()->GetVisible());
  EXPECT_FALSE(login_camera_timeout_view.arrow_button()->HasFocus());

  // Show login screen extension UI.
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::EXTENSION_LOGIN);

  // Main view and bottom_status_indicator not visible any more.
  EXPECT_FALSE(main_view->GetVisible());
  EXPECT_FALSE(lock_contents.bottom_status_indicator()->GetVisible());
  EXPECT_FALSE(login_camera_timeout_view.arrow_button()->HasFocus());

  // Close login screen extension UI.
  DataDispatcher()->NotifyOobeDialogState(
      OobeDialogState::EXTENSION_LOGIN_CLOSED);

  EXPECT_TRUE(main_view->GetVisible());
  EXPECT_TRUE(lock_contents.bottom_status_indicator()->GetVisible());
  // LoginCameraTimeoutView's arrow_button is not focused when the login screen
  // extension UI closed.
  EXPECT_FALSE(login_camera_timeout_view.arrow_button()->HasFocus());

  // Hide OOBE dialog.
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::HIDDEN);

  // LoginCameraTimeoutView's arrow_button gets focused when the OOBE dialog is
  // hidden.
  EXPECT_TRUE(login_camera_timeout_view.arrow_button()->HasFocus());
}

class LockContentsViewWithKioskLicenseTest : public LoginTestBase {
 public:
  LockContentsViewWithKioskLicenseTest() {}

  LockContentsViewWithKioskLicenseTest(LockContentsViewWithKioskLicenseTest&) =
      delete;
  LockContentsViewWithKioskLicenseTest& operator=(
      LockContentsViewWithKioskLicenseTest&) = delete;
  ~LockContentsViewWithKioskLicenseTest() override = default;

  void SetUp() override {
    LoginTestBase::SetUp();
    login_shelf_view_ = GetPrimaryShelf()->shelf_widget()->GetLoginShelfView();
    // Set initial states.
    NotifySessionStateChanged(session_manager::SessionState::OOBE);
  }

  void SetNFakeKioskApps(int n) {
    std::vector<KioskAppMenuEntry> kiosk_apps(
        n, KioskAppMenuEntry(KioskAppMenuEntry::AppType::kChromeApp,
                             AccountId::FromUserEmail("fake@email.com"),
                             /*chrome_app_id=*/"abc123chromeappid",
                             /*name=*/u"Fake App",
                             /*icon=*/gfx::ImageSkia()));
    login_shelf_view_->SetKioskApps(kiosk_apps);
  }

  void NotifySessionStateChanged(session_manager::SessionState state) {
    GetSessionControllerClient()->SetSessionState(state);
    GetSessionControllerClient()->FlushForTest();
  }

  raw_ptr<LoginShelfView, DanglingUntriaged> login_shelf_view_ =
      nullptr;  // Unowned.

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Checks default message hides if device is with kiosk license but with apps.
TEST_F(LockContentsViewWithKioskLicenseTest,
       ShouldNotShowKioskDefaultMessageWithApps) {
  // Set up
  const bool is_kiosk_license_mode = true;
  login_shelf_view_->SetKioskLicenseModeForTesting(is_kiosk_license_mode);
  // Show login screen with no user.
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockScreen::TestApi lock_screen_test_api(LockScreen::Get());
  LockContentsViewTestApi lock_contents_view_test_api(
      lock_screen_test_api.contents_view());
  lock_contents_view_test_api.SetKioskLicenseMode(is_kiosk_license_mode);
  SetUserCount(0);
  SetWidget(CreateWidgetWithContent(lock_screen_test_api.contents_view()));

  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  SetNFakeKioskApps(1);

  EXPECT_TRUE(lock_contents_view_test_api.kiosk_default_message());
  EXPECT_FALSE(
      lock_contents_view_test_api.kiosk_default_message()->GetVisible());
}

// Checks default message hidden if device is not with kiosk license and has
// no apps.
TEST_F(LockContentsViewWithKioskLicenseTest, ShouldHideKioskDefaultMessage) {
  // Set up
  const bool is_kiosk_license_mode = false;
  login_shelf_view_->SetKioskLicenseModeForTesting(is_kiosk_license_mode);
  // Show login screen with no user.
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockScreen::TestApi lock_screen_test_api(LockScreen::Get());
  LockContentsViewTestApi lock_contents_view_test_api(
      lock_screen_test_api.contents_view());
  lock_contents_view_test_api.SetKioskLicenseMode(is_kiosk_license_mode);
  SetUserCount(0);
  SetWidget(CreateWidgetWithContent(lock_screen_test_api.contents_view()));

  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  SetNFakeKioskApps(0);

  EXPECT_FALSE(lock_contents_view_test_api.kiosk_default_message());
}

// Checks default message appeared if device is with kiosk license and no
// kiosk app is set up.
TEST_F(LockContentsViewWithKioskLicenseTest,
       ShouldShowKioskDefaultMessageWithoutApps) {
  // Set up
  const bool is_kiosk_license_mode = true;
  login_shelf_view_->SetKioskLicenseModeForTesting(is_kiosk_license_mode);
  // Show login screen with no user.
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockScreen::TestApi lock_screen_test_api(LockScreen::Get());
  LockContentsViewTestApi lock_contents_view_test_api(
      lock_screen_test_api.contents_view());
  lock_contents_view_test_api.SetKioskLicenseMode(is_kiosk_license_mode);
  SetUserCount(0);
  SetWidget(CreateWidgetWithContent(lock_screen_test_api.contents_view()));

  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  SetNFakeKioskApps(0);

  EXPECT_TRUE(lock_contents_view_test_api.kiosk_default_message());
  EXPECT_TRUE(
      lock_contents_view_test_api.kiosk_default_message()->GetVisible());
}

// Checks default message appeared if device is with kiosk license, no
// kiosk app is set up and has users.
TEST_F(LockContentsViewWithKioskLicenseTest,
       ShouldShowKioskDefaultMessageWithoutAppsWithUsers) {
  // Set up
  const bool is_kiosk_license_mode = true;
  login_shelf_view_->SetKioskLicenseModeForTesting(is_kiosk_license_mode);
  // Show login screen with one user.
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockScreen::TestApi lock_screen_test_api(LockScreen::Get());
  LockContentsViewTestApi lock_contents_view_test_api(
      lock_screen_test_api.contents_view());
  lock_contents_view_test_api.SetKioskLicenseMode(is_kiosk_license_mode);
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(lock_screen_test_api.contents_view()));

  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  SetNFakeKioskApps(0);

  EXPECT_TRUE(lock_contents_view_test_api.kiosk_default_message());
  EXPECT_TRUE(
      lock_contents_view_test_api.kiosk_default_message()->GetVisible());
}

// Checks default message appeared if device is with kiosk license and no
// kiosk app is set up. After some kiosk app is set up, the default message
// shall disappear.
TEST_F(LockContentsViewWithKioskLicenseTest,
       ShouldShowAndHideKioskDefaultMessageWithAppChanges) {
  // Set up
  const bool is_kiosk_license_mode = true;
  login_shelf_view_->SetKioskLicenseModeForTesting(is_kiosk_license_mode);
  // Show login screen with no user.
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockScreen::TestApi lock_screen_test_api(LockScreen::Get());
  LockContentsViewTestApi lock_contents_view_test_api(
      lock_screen_test_api.contents_view());
  lock_contents_view_test_api.SetKioskLicenseMode(is_kiosk_license_mode);
  SetUserCount(0);
  SetWidget(CreateWidgetWithContent(lock_screen_test_api.contents_view()));

  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  SetNFakeKioskApps(0);

  EXPECT_TRUE(lock_contents_view_test_api.kiosk_default_message());
  EXPECT_TRUE(
      lock_contents_view_test_api.kiosk_default_message()->GetVisible());

  SetNFakeKioskApps(1);

  EXPECT_TRUE(lock_contents_view_test_api.kiosk_default_message());
  EXPECT_FALSE(
      lock_contents_view_test_api.kiosk_default_message()->GetVisible());
}

// UMA metrics recorded correctly after the successful login attempt.
TEST_F(LockContentsViewUnitTest, MetricsRecordedOnSuccessfulLoginAttempt) {
  int num_failed_attempts = 3;
  base::HistogramTester histogram_tester;
  // Build lock screen with a single user.
  auto contents = std::make_unique<LockContentsView>(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents.get()));

  auto submit_password = [&]() {
    PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    base::RunLoop().RunUntilIdle();
  };

  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);

  // Attempts to submit password.
  for (int i = 0; i < num_failed_attempts; ++i) {
    submit_password();
  }

  // Submit password successfully.
  client->set_authenticate_user_callback_result(true);
  submit_password();
  contents.reset();

  histogram_tester.ExpectTotalCount(
      kNbPasswordAttemptsUntilSuccessHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kNbPasswordAttemptsUntilSuccessHistogramName, num_failed_attempts, 1);
  histogram_tester.ExpectTotalCount(
      kNbPasswordAttemptsUntilFailureHistogramName, 0);
}

// UMA metrics recorded correctly after the first successful login attempt.
TEST_F(LockContentsViewUnitTest, MetricsRecordedOnFirstSuccessfulLoginAttempt) {
  base::HistogramTester histogram_tester;
  // Build lock screen with a single user.
  auto contents = std::make_unique<LockContentsView>(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents.get()));

  auto submit_password = [&]() {
    PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    base::RunLoop().RunUntilIdle();
  };

  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);

  // Submit password successfully.
  client->set_authenticate_user_callback_result(true);
  submit_password();
  contents.reset();

  histogram_tester.ExpectTotalCount(
      kNbPasswordAttemptsUntilSuccessHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kNbPasswordAttemptsUntilSuccessHistogramName, 0, 1);
  histogram_tester.ExpectTotalCount(
      kNbPasswordAttemptsUntilFailureHistogramName, 0);
}

// UMA metrics recorded correctly after the failed login attempt.
TEST_F(LockContentsViewUnitTest, MetricsRecordedOnFailedLoginAttempt) {
  int num_failed_attempts = 3;
  base::HistogramTester histogram_tester;
  // Build lock screen with a single user.
  auto contents = std::make_unique<LockContentsView>(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  SetWidget(CreateWidgetWithContent(contents.get()));

  auto submit_password = [&]() {
    PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
    PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    base::RunLoop().RunUntilIdle();
  };

  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);

  // Attempts to submit password.
  for (int i = 0; i < num_failed_attempts; ++i) {
    submit_password();
  }

  // Exit the login screen.
  contents.reset();

  histogram_tester.ExpectTotalCount(
      kNbPasswordAttemptsUntilSuccessHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      kNbPasswordAttemptsUntilFailureHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kNbPasswordAttemptsUntilFailureHistogramName, num_failed_attempts, 1);
}

TEST_F(LockContentsViewUnitTest, LoginAccessibleProperties) {
  auto contents = std::make_unique<LockContentsView>(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(contents.get()));

  ui::AXNodeData data;
  contents.get()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kWindow);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SCREEN_ACCESSIBLE_NAME));
}

TEST_F(LockContentsViewUnitTest, LockAccessibleProperties) {
  auto contents = std::make_unique<LockContentsView>(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(contents.get()));

  ui::AXNodeData data;
  contents.get()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kWindow);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_ASH_LOCK_SCREEN_ACCESSIBLE_NAME));
}

TEST_F(LockContentsViewUnitTest, LoginToolTipViewAccessibleProperties) {
  auto contents = std::make_unique<LockContentsView>(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLogin,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetWidget(CreateWidgetWithContent(contents.get()));
  LockContentsViewTestApi test_api(contents.get());
  ui::AXNodeData data;

  test_api.management_bubble()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.role, ax::mojom::Role::kTooltip);
}

class LockContentsViewPinTimeoutUnitTest : public LockContentsViewUnitTest {
 public:
  LockContentsViewPinTimeoutUnitTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAllowPinTimeoutSetup);
  }

  void AdvanceClock(base::TimeDelta time_delta) {
    task_environment()->AdvanceClock(time_delta);
    base::RunLoop().RunUntilIdle();
  }

  std::u16string GetExpectedPinStatusMessage(
      const std::u16string& time_string) {
    return l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_POD_PIN_LOCKED_WARNING,
                                      time_string);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LockContentsViewPinTimeoutUnitTest, PinDelayMessageCorrectness) {
  ASSERT_NO_FATAL_FAILURE(ShowLoginScreen());
  LockContentsView* contents =
      LockScreen::TestApi(LockScreen::Get()).contents_view();

  AddUsers(1);

  LoginBigUserView* big_view =
      LockContentsViewTestApi(contents).primary_big_view();
  LoginAuthUserView::TestApi auth_test_api(big_view->auth_user());
  LoginPinView* pin_view = auth_test_api.pin_view();
  PinStatusMessageView* pin_status_message_view =
      auth_test_api.pin_status_message_view();

  AccountId account_id = big_view->GetCurrentUser().basic_user_info.account_id;
  contents->OnPinEnabledForUserChanged(account_id, true,
                                       /*available_at*/ std::nullopt);

  EXPECT_TRUE(pin_view->GetVisible());
  EXPECT_FALSE(pin_status_message_view->GetVisible());

  // Lock pin for 2 hours.
  contents->OnPinEnabledForUserChanged(
      account_id, false,
      /*available_at*/ base::Time::Now() + base::Hours(2));

  // Remaining 1:59:00.
  AdvanceClock(base::Minutes(1));
  EXPECT_FALSE(pin_view->GetVisible());
  EXPECT_TRUE(pin_status_message_view->GetVisible());
  EXPECT_EQ(GetExpectedPinStatusMessage(u"1 hour, 59 minutes"),
            auth_test_api.GetPinStatusMessageContent());

  // Remaining 1:00:00.
  AdvanceClock(base::Minutes(59));
  EXPECT_TRUE(pin_status_message_view->GetVisible());
  EXPECT_EQ(GetExpectedPinStatusMessage(u"1 hour, 0 minutes"),
            auth_test_api.GetPinStatusMessageContent());

  // Remaining 0:59:30.
  AdvanceClock(base::Seconds(30));
  EXPECT_TRUE(pin_status_message_view->GetVisible());
  EXPECT_EQ(GetExpectedPinStatusMessage(u"59 minutes, 30 seconds"),
            auth_test_api.GetPinStatusMessageContent());

  // Pin becomes available again.
  AdvanceClock(base::Hours(1));
  EXPECT_TRUE(pin_view->GetVisible());
  EXPECT_FALSE(pin_status_message_view->GetVisible());
}

}  // namespace ash
