// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/user/rounded_image_view.h"
#include "ash/system/user/tray_user.h"
#include "ash/system/user/user_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/account_id/account_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/animation/animation_container_element.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const char* kPredefinedUserEmails[] = {
    // This is intended to be capitalized.
    "First@tray",
    // This is intended to be capitalized.
    "Second@tray"};

// Creates an ImageSkia with a 1x rep of the given dimensions and filled with
// the given color.
gfx::ImageSkia CreateImageSkiaWithColor(int width, int height, SkColor color) {
  SkBitmap bitmap = gfx::test::CreateBitmap(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

class TrayUserTest : public NoSessionAshTestBase {
 public:
  TrayUserTest() = default;

  // testing::Test:
  void SetUp() override;

  // This has to be called prior to first use with the proper configuration.
  void InitializeParameters(int users_logged_in, bool multiprofile);

  // Show the system tray menu using the provided event generator.
  void ShowTrayMenu(ui::test::EventGenerator* generator);

  // Move the mouse over the user item.
  void MoveOverUserItem(ui::test::EventGenerator* generator);

  // Click on the user item. Note that the tray menu needs to be shown.
  void ClickUserItem(ui::test::EventGenerator* generator);

  // Accessors to various system components.
  SystemTray* tray() { return tray_; }
  SessionController* controller() { return Shell::Get()->session_controller(); }
  TrayUser* tray_user() { return tray_user_; }

 private:
  SystemTray* tray_ = nullptr;

  // Note that the ownership of these items is on the shelf.
  TrayUser* tray_user_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TrayUserTest);
};

void TrayUserTest::SetUp() {
  AshTestBase::SetUp();
  tray_ = GetPrimarySystemTray();
}

void TrayUserTest::InitializeParameters(int users_logged_in,
                                        bool can_add_users) {
  // Set our default assumptions. Note that it is sufficient to set these
  // after everything was created.
  TestSessionControllerClient* session = GetSessionControllerClient();
  session->Reset();
  ASSERT_LE(users_logged_in,
            static_cast<int>(arraysize(kPredefinedUserEmails)));
  for (int i = 0; i < users_logged_in; ++i)
    SimulateUserLogin(kPredefinedUserEmails[i]);

  session->SetAddUserSessionPolicy(
      can_add_users ? AddUserSessionPolicy::ALLOWED
                    : AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS);

  // Instead of using the existing tray panels we create new ones which makes
  // the access easier.
  tray_user_ = new TrayUser(tray_);
  tray_->AddTrayItem(base::WrapUnique(tray_user_));
}

void TrayUserTest::ShowTrayMenu(ui::test::EventGenerator* generator) {
  gfx::Point center = tray()->GetBoundsInScreen().CenterPoint();

  generator->MoveMouseTo(center.x(), center.y());
  EXPECT_FALSE(tray()->IsSystemBubbleVisible());
  generator->ClickLeftButton();
}

void TrayUserTest::MoveOverUserItem(ui::test::EventGenerator* generator) {
  gfx::Point center =
      tray_user()->GetUserPanelBoundsInScreenForTest().CenterPoint();

  generator->MoveMouseTo(center.x(), center.y());
}

void TrayUserTest::ClickUserItem(ui::test::EventGenerator* generator) {
  MoveOverUserItem(generator);
  generator->ClickLeftButton();
}

}  // namespace

// Make sure that we show items for all users in the tray accordingly.
TEST_F(TrayUserTest, CheckTrayItemSize) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  InitializeParameters(1, false);
  tray_user()->UpdateAfterLoginStatusChangeForTest(LoginStatus::GUEST);
  gfx::Size size = tray_user()->GetLayoutSizeForTest();
  EXPECT_EQ(kTrayItemSize, size.height());
  tray_user()->UpdateAfterLoginStatusChangeForTest(LoginStatus::USER);
  size = tray_user()->GetLayoutSizeForTest();
  EXPECT_EQ(kTrayItemSize, size.height());
}

// Make sure that in single user mode the user panel cannot be activated.
TEST_F(TrayUserTest, SingleUserModeDoesNotAllowAddingUser) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  InitializeParameters(1, false);

  // Move the mouse over the status area and click to open the status menu.
  ui::test::EventGenerator* generator = GetEventGenerator();

  EXPECT_FALSE(tray()->IsSystemBubbleVisible());

  EXPECT_EQ(TrayUser::HIDDEN, tray_user()->GetStateForTest());

  ShowTrayMenu(generator);

  EXPECT_TRUE(tray()->HasSystemBubble());
  EXPECT_TRUE(tray()->IsSystemBubbleVisible());

  EXPECT_EQ(TrayUser::SHOWN, tray_user()->GetStateForTest());
  tray()->CloseBubble();
}

TEST_F(TrayUserTest, AccessibleLabelContainsSingleUserInfo) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  InitializeParameters(1, false);
  ui::test::EventGenerator* generator = GetEventGenerator();
  ShowTrayMenu(generator);

  views::View* view =
      tray_user()->user_view_for_test()->user_card_view_for_test();
  ui::AXNodeData node_data;
  view->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(
      base::UTF8ToUTF16("Über tray Über tray Über tray Über tray First@tray"),
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(ax::mojom::Role::kStaticText, node_data.role);
}

TEST_F(TrayUserTest, AccessibleLabelContainsMultiUserInfo) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  InitializeParameters(1, true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  ShowTrayMenu(generator);

  views::View* view =
      tray_user()->user_view_for_test()->user_card_view_for_test();
  ui::AXNodeData node_data;
  view->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(
      base::UTF8ToUTF16("Über tray Über tray Über tray Über tray First@tray"),
      node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(ax::mojom::Role::kButton, node_data.role);
}

// Make sure that in multi user mode the user panel can be activated and there
// will be one panel for each user.
// Note: the mouse watcher (for automatic closing upon leave) cannot be tested
// here since it does not work with the event system in unit tests.
TEST_F(TrayUserTest, MultiUserModeDoesNotAllowToAddUser) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  // Sign in more than one user.
  InitializeParameters(2, true);

  // Move the mouse over the status area and click to open the status menu.
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Verify that nothing is shown.
  EXPECT_FALSE(tray()->IsSystemBubbleVisible());
  EXPECT_FALSE(tray_user()->GetStateForTest());
  // After clicking on the tray the menu should get shown and for each logged
  // in user we should get a visible item.
  ShowTrayMenu(generator);

  EXPECT_TRUE(tray()->HasSystemBubble());
  EXPECT_TRUE(tray()->IsSystemBubbleVisible());
  EXPECT_EQ(TrayUser::SHOWN, tray_user()->GetStateForTest());

  // Move the mouse over the user item and it should hover.
  MoveOverUserItem(generator);
  EXPECT_EQ(TrayUser::HOVERED, tray_user()->GetStateForTest());

  // Check that clicking the button allows to add item if we have still room
  // for one more user.
  ClickUserItem(generator);
  EXPECT_EQ(TrayUser::ACTIVE, tray_user()->GetStateForTest());

  // Click the button again to see that the menu goes away.
  ClickUserItem(generator);
  MoveOverUserItem(generator);
  EXPECT_EQ(TrayUser::HOVERED, tray_user()->GetStateForTest());

  // Close and check that everything is deleted.
  tray()->CloseBubble();
  EXPECT_FALSE(tray()->IsSystemBubbleVisible());
  EXPECT_EQ(TrayUser::HIDDEN, tray_user()->GetStateForTest());
}

// Make sure that user changing gets properly executed.
TEST_F(TrayUserTest, MultiUserModeButtonClicks) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  // Have two users.
  InitializeParameters(2, true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  ShowTrayMenu(generator);

  // Gets the second user before user switching.
  const mojom::UserSession* second_user = controller()->GetUserSession(1);

  // Switch to a new user "Second@tray" - which has a capitalized name.
  ClickUserItem(generator);
  gfx::Rect user_card_bounds = tray_user()->GetUserPanelBoundsInScreenForTest();
  gfx::Point second_user_point = user_card_bounds.CenterPoint() +
                                 gfx::Vector2d(0, user_card_bounds.height());
  generator->MoveMouseTo(second_user_point);
  generator->ClickLeftButton();

  // SwitchActiverUser is an async mojo call. Spin the loop to let it finish.
  RunAllPendingInMessageLoop();

  const mojom::UserSession* active_user = controller()->GetUserSession(0);
  EXPECT_EQ(active_user->user_info->account_id,
            second_user->user_info->account_id);
  // Since the name is capitalized, the email should be different than the
  // user_id.
  EXPECT_NE(active_user->user_info->account_id.GetUserEmail(),
            second_user->user_info->display_email);
  tray()->CloseBubble();
}

// Test SessionController updates avatar image.
TEST_F(TrayUserTest, AvatarChange) {
  // TODO(tetsui): Remove the test after UnifiedSystemTray launch.
  // https://crbug.com/847104
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  InitializeParameters(1, false);

  // Expect empty avatar initially (that is how the test sets up).
  EXPECT_TRUE(tray_user()->avatar_view_for_test()->image_for_test().isNull());

  // Change user avatar via SessionController and verify.
  const gfx::ImageSkia red_icon =
      CreateImageSkiaWithColor(kTrayItemSize, kTrayItemSize, SK_ColorRED);
  mojom::UserSessionPtr user = controller()->GetUserSession(0)->Clone();
  user->user_info->avatar->image = red_icon;
  controller()->UpdateUserSession(std::move(user));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(red_icon),
      gfx::Image(tray_user()->avatar_view_for_test()->image_for_test())));
}

}  // namespace ash
