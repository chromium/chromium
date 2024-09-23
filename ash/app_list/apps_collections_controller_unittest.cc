// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/apps_collections_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_collections_page.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/apps_collections_dismiss_dialog.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/search_result_page_anchored_dialog.h"
#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

class AppsCollectionsControllerTest : public NoSessionAshTestBase {
 public:
  AppsCollectionsControllerTest() {
    scoped_feature_list_.InitWithFeatures({app_list_features::kAppsCollections},
                                          {});
  }

  // NoSessionAshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    GetTestAppListClient()->set_is_new_user(true);
    SimulateNewUserFirstLogin("primary@test");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppsCollectionsControllerTest,
       ShowAppsPageOnFirstShowAfterDismissingNudge) {
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();
  base::HistogramTester histograms;

  histograms.ExpectBucketCount(
      "Apps.AppList.AppsCollections.DismissedReason",
      AppsCollectionsController::DismissReason::kExitNudge, 0);

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  AppListToastContainerView* toast_container =
      apps_collections_page->GetToastContainerViewForTest();
  EXPECT_TRUE(toast_container->IsToastVisible());

  // Click on close button to dismiss the toast.
  LeftClickOn(toast_container->GetToastButton());
  EXPECT_FALSE(toast_container->IsToastVisible());

  // Apps page is not visible.
  EXPECT_FALSE(apps_collections_page->GetVisible());

  histograms.ExpectBucketCount(
      "Apps.AppList.AppsCollections.DismissedReason",
      AppsCollectionsController::DismissReason::kExitNudge, 1);

  helper->Dismiss();
  helper->ShowAppList();

  // Apps page is not visible.
  EXPECT_FALSE(apps_collections_page->GetVisible());
  EXPECT_TRUE(helper->GetBubbleAppsPage()->GetVisible());

  histograms.ExpectBucketCount(
      "Apps.AppList.AppsCollections.DismissedReason",
      AppsCollectionsController::DismissReason::kExitNudge, 1);
}

TEST_F(AppsCollectionsControllerTest, ShowAppsPageAfterSortingGrid) {
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();
  base::HistogramTester histograms;

  histograms.ExpectBucketCount(
      "Apps.AppList.AppsCollections.DismissedReason",
      AppsCollectionsController::DismissReason::kSorting, 0);

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  AppsGridContextMenu* context_menu =
      apps_collections_page->context_menu_for_test();
  EXPECT_FALSE(context_menu->IsMenuShowing());

  // Get a point in `apps_collections_page` that doesn't have an item on it.
  const gfx::Point empty_space =
      apps_collections_page->GetBoundsInScreen().CenterPoint();

  // Open the menu to test the alphabetical sort option.
  GetEventGenerator()->MoveMouseTo(empty_space);
  GetEventGenerator()->ClickRightButton();
  EXPECT_TRUE(context_menu->IsMenuShowing());

  // Click on any reorder option and accept the dialog.
  views::MenuItemView* reorder_option =
      context_menu->root_menu_item_view()->GetSubmenu()->GetMenuItemAt(1);
  LeftClickOn(reorder_option);
  ASSERT_TRUE(helper->GetBubbleSearchPageDialog());

  views::Widget* widget = helper->GetBubbleSearchPageDialog()->widget();
  views::WidgetDelegate* widget_delegate = widget->widget_delegate();
  views::test::WidgetDestroyedWaiter widget_waiter(widget);
  LeftClickOn(static_cast<AppsCollectionsDismissDialog*>(widget_delegate)
                  ->accept_button_for_test());
  widget_waiter.Wait();

  // Apps collections page is not visible.
  EXPECT_FALSE(apps_collections_page->GetVisible());
  EXPECT_EQ(AppListSortOrder::kNameAlphabetical,
            helper->model()->requested_sort_order());

  // Apps page is not visible.
  EXPECT_FALSE(apps_collections_page->GetVisible());
  EXPECT_TRUE(helper->GetBubbleAppsPage()->GetVisible());

  histograms.ExpectBucketCount(
      "Apps.AppList.AppsCollections.DismissedReason",
      AppsCollectionsController::DismissReason::kSorting, 1);
}

// Tests that sorting on tablet mode updates apps collections.
TEST_F(AppsCollectionsControllerTest,
       AppListSortOnTabletModeUpdatesAppsCollections) {
  auto* helper = GetAppListTestHelper();
  helper->AddAppListItemsWithCollection(AppCollection::kEntertainment, 2);
  helper->ShowAppList();
  EXPECT_EQ(AppListSortOrder::kCustom, helper->model()->requested_sort_order());

  // Apps collections page is visible.
  EXPECT_TRUE(helper->GetBubbleAppsCollectionsPage()->GetVisible());
  EXPECT_FALSE(helper->GetBubbleAppsPage()->GetVisible());

  // Enter tablet mode.
  TabletModeControllerTestApi().EnterTabletMode();

  auto* apps_grid_view = helper->GetRootPagedAppsGridView();
  // Get a point in `apps_grid` that doesn't have an item on it.
  const gfx::Point empty_space =
      apps_grid_view->GetBoundsInScreen().CenterPoint();

  // Open the menu to test the alphabetical sort option.
  ui::test::EventGenerator* generator = GetEventGenerator();
  ui::GestureEvent long_press(
      empty_space.x(), empty_space.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  generator->Dispatch(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();

  AppMenuModelAdapter* context_menu =
      Shell::GetPrimaryRootWindowController()->menu_model_adapter_for_testing();
  ASSERT_TRUE(context_menu);
  EXPECT_TRUE(context_menu->IsShowingMenu());

  // Cache the current context menu view.
  views::MenuItemView* reorder_submenu =
      context_menu->root_for_testing()->GetSubmenu()->GetMenuItemAt(2);
  ASSERT_EQ(reorder_submenu->title(), u"Sort by");
  GetEventGenerator()->GestureTapAt(
      reorder_submenu->GetBoundsInScreen().CenterPoint());

  // Click on any reorder option and accept the dialog.
  views::MenuItemView* reorder_option =
      reorder_submenu->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_EQ(reorder_option->title(), u"Name");
  GetEventGenerator()->GestureTapAt(
      reorder_option->GetBoundsInScreen().CenterPoint());
  helper->WaitUntilIdle();
  EXPECT_EQ(AppListSortOrder::kNameAlphabetical,
            helper->model()->requested_sort_order());

  helper->model()->RequestCommitTemporarySortOrder();

  // Leave tablet mode.
  TabletModeControllerTestApi().LeaveTabletMode();

  helper->ShowAppList();

  // Apps collections page is visible.
  EXPECT_FALSE(helper->GetBubbleAppsCollectionsPage()->GetVisible());
  EXPECT_TRUE(helper->GetBubbleAppsPage()->GetVisible());
}

// Verifies that the apps collections is not shown after the user logs back in
// again.
TEST_F(AppsCollectionsControllerTest, AppsCollectionsDismissedAfterRestart) {
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  EXPECT_TRUE(helper->GetBubbleAppsCollectionsPage()->GetVisible());

  // Logout and simulate that the user logs back in again.
  helper->Dismiss();
  ClearLogin();
  SimulateUserLogin("primary@test");

  // The bubble should not be shown.
  helper->ShowAppList();
  EXPECT_FALSE(helper->GetBubbleAppsCollectionsPage()->GetVisible());
}

// Class for tests of the `AppsCollectionsController` which are
// concerned with user eligibility, parameterized by:
// (a) whether the user should be considered "new" locally
// (b) whether the user is managed
// (c) the user type.
class AppsCollectionsControllerUserElegibilityTest
    : public NoSessionAshTestBase,
      public ::testing::WithParamInterface<std::tuple<
          /*is_new_user_locally=*/bool,
          /*is_managed_user=*/bool,
          user_manager::UserType,
          /*is_user_first_login_to_chromeos=*/std::optional<bool>>> {
 public:
  AppsCollectionsControllerUserElegibilityTest() {
    scoped_feature_list_.InitWithFeatures({app_list_features::kAppsCollections},
                                          {});
  }

  // NoSessionAshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    GetTestAppListClient()->set_is_new_user(IsUserFirstLogInToChromeOS());
  }

  // Returns the user type based on test parameterization.
  user_manager::UserType GetUserType() const { return std::get<2>(GetParam()); }

  // Returns whether the user is managed based on test parameterization.
  bool IsManagedUser() const { return std::get<1>(GetParam()); }

  // Returns whether the user should be considered "new" locally based on test
  // parameterization.
  bool IsNewUserLocally() const { return std::get<0>(GetParam()); }

  // Returns whether the user should be considered "new" across all devices
  // based on test parameterization.
  std::optional<bool> IsUserFirstLogInToChromeOS() const {
    return std::get<3>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AppsCollectionsControllerUserElegibilityTest,
    ::testing::Combine(
        /*is_new_user_locally=*/::testing::Bool(),
        /*is_managed_user=*/::testing::Bool(),
        ::testing::Values(user_manager::UserType::kChild,
                          user_manager::UserType::kGuest,
                          user_manager::UserType::kKioskApp,
                          user_manager::UserType::kPublicAccount,
                          user_manager::UserType::kRegular,
                          user_manager::UserType::kWebKioskApp),
        /*is_user_first_login_to_chromeos=*/
        ::testing::Values(std::make_optional(true),
                          std::make_optional(false),
                          std::nullopt)));

TEST_P(AppsCollectionsControllerUserElegibilityTest, EnforcesUserEligibility) {
  // A user is eligible for showing AppsCollections if and only if the user
  // satisfies the following conditions:
  // (1) known to be "new" locally, and
  // (2) not a managed user, and
  // (3) a regular user.
  // (4) a known to be 'new' user across the ChromeOS ecosystem.
  const bool is_user_eligibility_expected =
      IsNewUserLocally() && !IsManagedUser() &&
      GetUserType() == user_manager::UserType::kRegular &&
      IsUserFirstLogInToChromeOS().value_or(false);
  // Add a user based on test parameterization.
  const AccountId primary_account_id = AccountId::FromUserEmail("primary@test");
  TestSessionControllerClient* const session = GetSessionControllerClient();
  session->AddUserSession(primary_account_id.GetUserEmail(), GetUserType(),
                          /*provide_pref_service=*/true,
                          /*is_new_profile=*/IsNewUserLocally(),
                          /*given_name=*/std::string(), IsManagedUser());
  session->SwitchActiveUser(primary_account_id);

  // Activate the user session.
  session->SetSessionState(session_manager::SessionState::ACTIVE);

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  EXPECT_EQ(apps_collections_page->GetVisible(), is_user_eligibility_expected);
}

// Verifies that regardless of the user elegibility parameters, secondary users
// are not presented with apps collections. This is a self-imposed restriction.
TEST_P(AppsCollectionsControllerUserElegibilityTest, SecondaryUserNotElegible) {
  SimulateNewUserFirstLogin("primary@test");
  // Add a user based on test parameterization.
  const AccountId secondary_account_id =
      AccountId::FromUserEmail("secondary@test");
  TestSessionControllerClient* const session = GetSessionControllerClient();
  session->AddUserSession(secondary_account_id.GetUserEmail(), GetUserType(),
                          /*provide_pref_service=*/true,
                          /*is_new_profile=*/IsNewUserLocally(),
                          /*given_name=*/std::string(), IsManagedUser());
  session->SwitchActiveUser(secondary_account_id);

  // Activate the user session.
  session->SetSessionState(session_manager::SessionState::ACTIVE);

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  EXPECT_FALSE(apps_collections_page->GetVisible());
}

// Class for tests of the `AppsCollectionsController` which are
// concerned with the experiment prefs.
class AppsCollectionsControllerPrefTest
    : public NoSessionAshTestBase,
      public ::testing::WithParamInterface<std::tuple<
          /*is_apps_collections_active=*/bool,
          /*is_counterfactual=*/bool,
          /*is_modified_order=*/bool>> {
 public:
  AppsCollectionsControllerPrefTest() {
    if (IsAppsCollectionsEnabled()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          app_list_features::kAppsCollections,
          {{"is-counterfactual",
            IsAppsCollectionsEnabledCounterfactually() ? "true" : "false"},
           {"is-modified-order",
            IsAppsCollectionsEnabledWithModifiedOrder() ? "true" : "false"}});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          app_list_features::kAppsCollections);
    }
  }

  // NoSessionAshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    GetTestAppListClient()->set_is_new_user(true);
    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    const AccountId& account_id = AccountId::FromUserEmail("primary@test");

    auto user_prefs = std::make_unique<TestingPrefServiceSimple>();
    RegisterUserProfilePrefs(user_prefs->registry(), /*country=*/"",
                             /*for_test=*/true);
    session_controller->AddUserSession("primary@test",
                                       user_manager::UserType::kRegular,
                                       /*provide_pref_service=*/false,
                                       /*is_new_profile=*/true);
    GetSessionControllerClient()->SetUserPrefService(account_id,
                                                     std::move(user_prefs));
    session_controller->SwitchActiveUser(account_id);
    session_controller->SetSessionState(session_manager::SessionState::ACTIVE);
  }

  // Returns whether apps collectionns feature is enabled.
  bool IsAppsCollectionsEnabled() const { return std::get<0>(GetParam()); }

  // Returns whether apps collections feature is enabled counterfactrually.
  bool IsAppsCollectionsEnabledCounterfactually() const {
    return IsAppsCollectionsEnabled() && std::get<1>(GetParam());
  }
  // Returns whether apps collections feature is enabled counterfactrually.
  bool IsAppsCollectionsEnabledWithModifiedOrder() const {
    return IsAppsCollectionsEnabled() && std::get<2>(GetParam());
  }

  AppsCollectionsController::ExperimentalArm GetExpectedExperimentalArm() {
    if (!IsAppsCollectionsEnabled()) {
      return AppsCollectionsController::ExperimentalArm::kControl;
    }

    if (IsAppsCollectionsEnabledCounterfactually()) {
      return AppsCollectionsController::ExperimentalArm::kCounterfactual;
    }

    return IsAppsCollectionsEnabledWithModifiedOrder()
               ? AppsCollectionsController::ExperimentalArm::kModifiedOrder
               : AppsCollectionsController::ExperimentalArm::kEnabled;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AppsCollectionsControllerPrefTest,
                         ::testing::Combine(
                             /*is_apps_collections_active=*/::testing::Bool(),
                             /*is_counterfactual=*/::testing::Bool(),
                             /*is_modified_order=*/::testing::Bool()));

// Verifies that the experimental arm for the user is calculated and stored
// correctly.
TEST_P(AppsCollectionsControllerPrefTest, GetExperimentalArm) {
  EXPECT_EQ(AppsCollectionsController::Get()->GetUserExperimentalArm(),
            AppsCollectionsController::ExperimentalArm::kDefaultValue);

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  EXPECT_EQ(AppsCollectionsController::Get()->GetUserExperimentalArm(),
            GetExpectedExperimentalArm());
}

}  // namespace
}  // namespace ash
