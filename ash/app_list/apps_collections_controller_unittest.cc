// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/apps_collections_controller.h"

#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_collections_page.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/controls/button/label_button.h"

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

    SimulateNewUserFirstLogin("primary@test");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppsCollectionsControllerTest,
       ShowAppsPageOnFirstShowAfterDismissingNudge) {
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  AppListToastContainerView* toast_container =
      apps_collections_page->GetToastContainerViewForTest();
  EXPECT_TRUE(toast_container->IsToastVisible());

  // Click on close button to dismiss the toast.
  LeftClickOn(toast_container->GetToastButton());
  EXPECT_FALSE(toast_container->IsToastVisible());

  // Apps page is not visible.
  EXPECT_FALSE(apps_collections_page->GetVisible());

  helper->Dismiss();
  helper->ShowAppList();

  // Apps page is not visible.
  EXPECT_FALSE(apps_collections_page->GetVisible());
  EXPECT_TRUE(helper->GetBubbleAppsPage()->GetVisible());
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
          user_manager::UserType>> {
 public:
  AppsCollectionsControllerUserElegibilityTest() {
    scoped_feature_list_.InitWithFeatures({app_list_features::kAppsCollections},
                                          {});
  }

  // Returns the user type based on test parameterization.
  user_manager::UserType GetUserType() const { return std::get<2>(GetParam()); }

  // Returns whether the user is managed based on test parameterization.
  bool IsManagedUser() const { return std::get<1>(GetParam()); }

  // Returns whether the user should be considered "new" locally based on test
  // parameterization.
  bool IsNewUserLocally() const { return std::get<0>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AppsCollectionsControllerUserElegibilityTest,
    ::testing::Combine(
        /*is_new_user_locally=*/::testing::Bool(),
        /*is_managed_user=*/::testing::Bool(),
        ::testing::Values(user_manager::UserType::kArcKioskApp,
                          user_manager::UserType::kChild,
                          user_manager::UserType::kGuest,
                          user_manager::UserType::kKioskApp,
                          user_manager::UserType::kPublicAccount,
                          user_manager::UserType::kRegular,
                          user_manager::UserType::kWebKioskApp)));

TEST_P(AppsCollectionsControllerUserElegibilityTest, EnforcesUserEligibility) {
  // A user is eligible for showing AppsCollections if and only if the user
  // satisfies the following conditions:
  // (1) known to be "new" locally, and
  // (2) not a managed user, and
  // (3) a regular user.
  const bool is_user_eligibility_expected =
      (IsNewUserLocally() && !IsManagedUser() &&
       GetUserType() == user_manager::UserType::kRegular);

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

}  // namespace
}  // namespace ash
