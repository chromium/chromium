// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_util.h"

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "ash/user_education/user_education_types.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/help_bubble_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash::user_education_util {
namespace {

// Aliases.
using ::session_manager::SessionState;
using ::testing::AnyOf;
using ::testing::Eq;
using ::user_education::HelpBubbleParams;

// Helpers ---------------------------------------------------------------------

std::unique_ptr<views::Widget> ShowFramelessTestWidgetOnDisplay(
    int64_t display_id,
    std::unique_ptr<views::View> contents_view) {
  auto* manager = Shell::Get()->window_tree_host_manager();
  auto widget =
      TestWidgetBuilder()
          .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
          .SetParent(manager->GetRootWindowForDisplayId(display_id))
          .BuildOwnsNativeWidget();
  widget->SetContentsView(std::move(contents_view));
  widget->Show();
  return widget;
}

}  // namespace

// UserEducationUtilTest -------------------------------------------------------

// Base class for tests of user education utilities.
using UserEducationUtilTest = ::testing::Test;

// Tests -----------------------------------------------------------------------

TEST_F(UserEducationUtilTest, CreateExtendedProperties) {
  const user_education::HelpBubbleParams::ExtendedProperties
      extended_properties = CreateExtendedProperties(
          CreateExtendedProperties(HelpBubbleId::kTest),
          CreateExtendedProperties(ui::mojom::ModalType::kSystem));

  EXPECT_EQ(GetHelpBubbleId(extended_properties), HelpBubbleId::kTest);
  EXPECT_EQ(GetHelpBubbleModalType(extended_properties),
            ui::mojom::ModalType::kSystem);
}

// Verifies that `CreateExtendedProperties()` can be used to create extended
// properties for a help bubble having set body icon, and that
// `GetHelpBubbleBodyIcon()` can be used to retrieve help bubble body icon from
// extended properties.
TEST_F(UserEducationUtilTest, CreateExtendedPropertiesWithBodyIcon) {
  EXPECT_EQ(
      &GetHelpBubbleBodyIcon(CreateExtendedProperties(gfx::kNoneIcon))->get(),
      &gfx::kNoneIcon);

  // It is permissible to query help bubble body icon even when absent.
  EXPECT_EQ(GetHelpBubbleBodyIcon(HelpBubbleParams::ExtendedProperties()),
            std::nullopt);
}

// Verifies that `CreateExtendedProperties()` can be used to create extended
// properties for a help bubble having set ID, and that `GetHelpBubbleId()` can
// be used to retrieve help bubble ID from extended properties.
TEST_F(UserEducationUtilTest, ExtendedPropertiesWithId) {
  EXPECT_EQ(GetHelpBubbleId(CreateExtendedProperties(HelpBubbleId::kTest)),
            HelpBubbleId::kTest);
}

// Verifies that `CreateExtendedProperties()` can be used to create extended
// properties for a help bubble having set modal type, and that
// `GetHelpBubbleModalType()` can be used to retrieve help bubble modal type
// from extended properties.
TEST_F(UserEducationUtilTest, CreateExtendedPropertiesWithModalType) {
  EXPECT_EQ(GetHelpBubbleModalType(
                CreateExtendedProperties(ui::mojom::ModalType::kSystem)),
            ui::mojom::ModalType::kSystem);

  // It is permissible to query help bubble modal type even when absent.
  EXPECT_EQ(GetHelpBubbleModalType(HelpBubbleParams::ExtendedProperties()),
            ui::mojom::ModalType::kNone);
}

// Verifies that `CreateExtendedPropertiesWithAccessibleName()` can be used to
// create extended properties for a help bubble having set accessible name, and
// that `GetHelpBubbleAccessibleName()` can be used to retrieve help bubble
// accessible name from extended properties.
TEST_F(UserEducationUtilTest, ExtendedPropertiesWithAccessibleName) {
  std::string accessible_name = "Accessible Name";
  EXPECT_EQ(GetHelpBubbleAccessibleName(
                CreateExtendedPropertiesWithAccessibleName(accessible_name)),
            accessible_name);

  // It is permissible to query help bubble accessible name even when absent.
  EXPECT_EQ(GetHelpBubbleAccessibleName(HelpBubbleParams::ExtendedProperties()),
            std::nullopt);
}

// Verifies that `CreateExtendedPropertiesWithBodyText()` can be used to create
// extended properties for a help bubble having set body text, and that
// `GetHelpBubbleBodyText()` can be used to retrieve help bubble body text from
// extended properties.
TEST_F(UserEducationUtilTest, ExtendedPropertiesWithBodyText) {
  std::string body_text = "Body Text";
  EXPECT_EQ(
      GetHelpBubbleBodyText(CreateExtendedPropertiesWithBodyText(body_text)),
      body_text);

  // It is permissible to query help bubble body text even when absent.
  EXPECT_EQ(GetHelpBubbleBodyText(HelpBubbleParams::ExtendedProperties()),
            std::nullopt);
}

// Verifies that `ToString()` is working as intended.
TEST_F(UserEducationUtilTest, ToString) {
  std::set<std::string> tutorial_id_strs;
  for (size_t i = static_cast<size_t>(TutorialId::kMinValue);
       i <= static_cast<size_t>(TutorialId::kMaxValue); ++i) {
    // Currently the only constraint on `ToString()` is that it returns a unique
    // value for each distinct tutorial ID.
    auto tutorial_id_str = ToString(static_cast<TutorialId>(i));
    EXPECT_TRUE(tutorial_id_strs.emplace(std::move(tutorial_id_str)).second);
  }
}

// UserEducationUtilAshTest ----------------------------------------------------

// Base class for tests of user education utilities which require Ash.
using UserEducationUtilAshTest = NoSessionAshTestBase;

// Tests -----------------------------------------------------------------------

// Verifies that `GetAccountId()` is working as intended.
TEST_F(UserEducationUtilAshTest, GetAccountId) {
  // Case: null `UserSession`.
  AccountId account_id;
  EXPECT_EQ(GetAccountId(/*user_session=*/nullptr), account_id);

  // Case: non-null `UserSession`.
  account_id = AccountId::FromUserEmail("user@test");
  UserSession user_session;
  user_session.user_info.account_id = account_id;
  EXPECT_EQ(GetAccountId(&user_session), account_id);
}

// Verifies that `GetMatchingViewInRootWindow()` is working as intended.
TEST_F(UserEducationUtilAshTest, GetMatchingViewInRootWindow) {
  // Set up a primary and secondary display.
  UpdateDisplay("1024x768,1024x768");

  // Cache display IDs.
  const int64_t primary_display_id = GetPrimaryDisplay().id();
  const int64_t secondary_display_id = GetSecondaryDisplay().id();

  // Show a widget with a tracked element on the primary display.
  views::View* primary_display_view = nullptr;
  auto primary_display_widget = ShowFramelessTestWidgetOnDisplay(
      primary_display_id,
      views::Builder<views::View>()
          .CopyAddressTo(&primary_display_view)
          .SetProperty(views::kElementIdentifierKey,
                       ui::ElementTracker::kTemporaryIdentifier)
          .Build());

  // The tracked element *should* match for the primary root window.
  EXPECT_EQ(GetMatchingViewInRootWindow(
                primary_display_id, ui::ElementTracker::kTemporaryIdentifier),
            primary_display_view);

  // The tracked element should *not* match for the secondary root window.
  EXPECT_FALSE(GetMatchingViewInRootWindow(
      secondary_display_id, ui::ElementTracker::kTemporaryIdentifier));

  // Show a widget with a tracked element on the secondary display.
  views::View* secondary_display_view = nullptr;
  auto secondary_display_widget = ShowFramelessTestWidgetOnDisplay(
      secondary_display_id,
      views::Builder<views::View>()
          .CopyAddressTo(&secondary_display_view)
          .SetProperty(views::kElementIdentifierKey,
                       ui::ElementTracker::kTemporaryIdentifier)
          .Build());

  // The tracked element on the secondary display should *not* match for the
  // primary root window.
  EXPECT_EQ(GetMatchingViewInRootWindow(
                primary_display_id, ui::ElementTracker::kTemporaryIdentifier),
            primary_display_view);

  // The tracked element on the secondary display *should* match for the
  // secondary root window.
  EXPECT_EQ(GetMatchingViewInRootWindow(
                secondary_display_id, ui::ElementTracker::kTemporaryIdentifier),
            secondary_display_view);

  // Show another widget with a tracked element on the secondary display.
  views::View* another_secondary_display_view = nullptr;
  auto another_secondary_display_widget = ShowFramelessTestWidgetOnDisplay(
      secondary_display_id,
      views::Builder<views::View>()
          .CopyAddressTo(&another_secondary_display_view)
          .SetProperty(views::kElementIdentifierKey,
                       ui::ElementTracker::kTemporaryIdentifier)
          .Build());

  // Both tracked elements on the secondary display should *not* match for the
  // primary root window when there *is* a match on the primary display.
  EXPECT_EQ(GetMatchingViewInRootWindow(
                primary_display_id, ui::ElementTracker::kTemporaryIdentifier),
            primary_display_view);

  // Both tracked elements on the secondary display should *not* match for the
  // primary root window when there is *no* match on the primary display.
  primary_display_view = nullptr;
  primary_display_widget.reset();
  EXPECT_FALSE(GetMatchingViewInRootWindow(
      primary_display_id, ui::ElementTracker::kTemporaryIdentifier));

  // Either of the tracked elements shown in a widget on the secondary display
  // *should* match for the secondary root window. The utility method does *not*
  // guarantee which match will be returned.
  EXPECT_THAT(
      GetMatchingViewInRootWindow(secondary_display_id,
                                  ui::ElementTracker::kTemporaryIdentifier),
      AnyOf(Eq(secondary_display_view), Eq(another_secondary_display_view)));
}

// Verifies that `GetUserType()` is working as intended.
TEST_F(UserEducationUtilAshTest, GetUserType) {
  AccountId guest_account_id = AccountId::FromUserEmail("guest@test");
  AccountId regular_account_id = AccountId::FromUserEmail("regular@test");

  // Case: no user sessions added.
  EXPECT_FALSE(GetUserType(AccountId()));
  EXPECT_FALSE(GetUserType(guest_account_id));
  EXPECT_FALSE(GetUserType(regular_account_id));

  auto* session_controller = GetSessionControllerClient();
  session_controller->AddUserSession(guest_account_id.GetUserEmail(),
                                     user_manager::UserType::kGuest);
  session_controller->AddUserSession(regular_account_id.GetUserEmail(),
                                     user_manager::UserType::kRegular);

  // Case: multiple user sessions added.
  EXPECT_FALSE(GetUserType(AccountId()));
  EXPECT_EQ(GetUserType(guest_account_id), user_manager::UserType::kGuest);
  EXPECT_EQ(GetUserType(regular_account_id), user_manager::UserType::kRegular);
}

// Verifies that `IsPrimaryAccountActive()` is working as intended.
TEST_F(UserEducationUtilAshTest, IsPrimaryAccountActive) {
  AccountId primary_account_id = AccountId::FromUserEmail("primary@test");
  AccountId secondary_account_id = AccountId::FromUserEmail("secondary@test");

  // Case: no user sessions added.
  EXPECT_FALSE(IsPrimaryAccountActive());

  // Case: primary user session added but inactive.
  auto* session_controller_client = GetSessionControllerClient();
  session_controller_client->AddUserSession(primary_account_id.GetUserEmail());
  EXPECT_FALSE(IsPrimaryAccountActive());

  // Case: primary user session activated.
  session_controller_client->SetSessionState(SessionState::ACTIVE);
  EXPECT_TRUE(IsPrimaryAccountActive());

  // Case: primary user session locked and then unlocked.
  session_controller_client->SetSessionState(SessionState::LOCKED);
  EXPECT_FALSE(IsPrimaryAccountActive());
  session_controller_client->SetSessionState(SessionState::ACTIVE);
  EXPECT_TRUE(IsPrimaryAccountActive());

  // Case: secondary user session added but inactive.
  session_controller_client->AddUserSession(
      secondary_account_id.GetUserEmail());
  EXPECT_TRUE(IsPrimaryAccountActive());

  // Case: secondary user activated and then deactivated.
  session_controller_client->SwitchActiveUser(secondary_account_id);
  EXPECT_FALSE(IsPrimaryAccountActive());
  session_controller_client->SwitchActiveUser(primary_account_id);
  EXPECT_TRUE(IsPrimaryAccountActive());
}

// Verifies that `IsPrimaryAccountId()` is working as intended.
TEST_F(UserEducationUtilAshTest, IsPrimaryAccountId) {
  AccountId primary_account_id = AccountId::FromUserEmail("primary@test");
  AccountId secondary_account_id = AccountId::FromUserEmail("secondary@test");

  // Case: no user sessions added.
  EXPECT_FALSE(IsPrimaryAccountId(AccountId()));
  EXPECT_FALSE(IsPrimaryAccountId(primary_account_id));
  EXPECT_FALSE(IsPrimaryAccountId(secondary_account_id));

  auto* session_controller_client = GetSessionControllerClient();
  session_controller_client->AddUserSession(primary_account_id.GetUserEmail());
  session_controller_client->AddUserSession(
      secondary_account_id.GetUserEmail());

  // Case: multiple user sessions added.
  EXPECT_FALSE(IsPrimaryAccountId(AccountId()));
  EXPECT_TRUE(IsPrimaryAccountId(primary_account_id));
  EXPECT_FALSE(IsPrimaryAccountId(secondary_account_id));
}

}  // namespace ash::user_education_util
