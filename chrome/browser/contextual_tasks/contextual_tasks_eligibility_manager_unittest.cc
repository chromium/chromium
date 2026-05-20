// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace contextual_tasks {

class ContextualTasksEligibilityManagerTest : public testing::Test {
 public:
  ContextualTasksEligibilityManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(kContextualTasks);
  }
  ~ContextualTasksEligibilityManagerTest() override = default;

  void SetUp() override {
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    aim_eligibility_service_ = std::make_unique<MockAimEligibilityService>(
        prefs_, nullptr, nullptr, nullptr);

    contextual_search::ContextualSearchService::RegisterProfilePrefs(
        prefs_.registry());

    ON_CALL(*aim_eligibility_service_, IsCobrowseEligible())
        .WillByDefault(Return(true));
    ON_CALL(*aim_eligibility_service_, RegisterEligibilityChangedCallback(_))
        .WillByDefault([&](base::RepeatingClosure callback) {
          return aim_eligibility_callbacks_.Add(std::move(callback));
        });
  }

  void CreateManager() {
    manager_ = std::make_unique<ContextualTasksEligibilityManager>(
        &prefs_, identity_test_env_->identity_manager(),
        aim_eligibility_service_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<MockAimEligibilityService> aim_eligibility_service_;
  std::unique_ptr<ContextualTasksEligibilityManager> manager_;
  base::RepeatingClosureList aim_eligibility_callbacks_;
};

TEST_F(ContextualTasksEligibilityManagerTest,
       InitialState_Ineligible_NotSignedIn) {
  CreateManager();
  EXPECT_FALSE(manager_->IsEligible());
}

TEST_F(ContextualTasksEligibilityManagerTest, InitialState_Eligible_SignedIn) {
  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  CreateManager();
  EXPECT_TRUE(manager_->IsEligible());
}

TEST_F(ContextualTasksEligibilityManagerTest,
       InitialState_Ineligible_AimIneligible) {
  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  EXPECT_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillOnce(Return(false));

  CreateManager();
  EXPECT_FALSE(manager_->IsEligible());
}

TEST_F(ContextualTasksEligibilityManagerTest,
       InitialState_Ineligible_CookieJarEmpty) {
  identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  CreateManager();
  EXPECT_FALSE(manager_->IsEligible());
}

TEST_F(ContextualTasksEligibilityManagerTest,
       InitialState_Ineligible_ContextSharingDisabled) {
  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  prefs_.SetInteger(contextual_search::kSearchContentSharingSettings,
                    1);  // Disabled

  CreateManager();
  EXPECT_FALSE(manager_->IsEligible());
}

TEST_F(ContextualTasksEligibilityManagerTest, Transition_AimEligibility) {
  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  CreateManager();
  EXPECT_TRUE(manager_->IsEligible());

  base::test::TestFuture<bool> eligibility_future;
  auto subscription = manager_->RegisterEligibilityChangedCallback(
      eligibility_future.GetRepeatingCallback());

  // Simulate AimEligibility changing to false.
  EXPECT_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillOnce(Return(false));
  aim_eligibility_callbacks_.Notify();

  EXPECT_FALSE(eligibility_future.Get());
  EXPECT_FALSE(manager_->IsEligible());
}

TEST_F(ContextualTasksEligibilityManagerTest, Transition_SignIn) {
  auto account_info =
      identity_test_env_->MakeAccountAvailable("test@example.com");
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  CreateManager();
  EXPECT_FALSE(manager_->IsEligible());

  base::test::TestFuture<bool> eligibility_future;
  auto subscription = manager_->RegisterEligibilityChangedCallback(
      eligibility_future.GetRepeatingCallback());

  // Make primary (Sign in).
  identity_test_env_->SetPrimaryAccount(account_info.email,
                                        signin::ConsentLevel::kSignin);

  EXPECT_TRUE(eligibility_future.Get());
  EXPECT_TRUE(manager_->IsEligible());
}

TEST_F(ContextualTasksEligibilityManagerTest, Transition_CookieJar) {
  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  CreateManager();
  EXPECT_TRUE(manager_->IsEligible());

  base::test::TestFuture<bool> eligibility_future;
  auto subscription = manager_->RegisterEligibilityChangedCallback(
      eligibility_future.GetRepeatingCallback());

  // Empty the cookie jar.
  identity_test_env_->SetCookieAccounts({});

  EXPECT_FALSE(eligibility_future.Get());
  EXPECT_FALSE(manager_->IsEligible());
}

TEST_F(ContextualTasksEligibilityManagerTest,
       Transition_ContextSharingSettings) {
  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  CreateManager();
  EXPECT_TRUE(manager_->IsEligible());

  base::test::TestFuture<bool> eligibility_future;
  auto subscription = manager_->RegisterEligibilityChangedCallback(
      eligibility_future.GetRepeatingCallback());

  // Disable context sharing.
  prefs_.SetInteger(contextual_search::kSearchContentSharingSettings, 1);

  EXPECT_FALSE(eligibility_future.Get());
  EXPECT_FALSE(manager_->IsEligible());
}

TEST_F(ContextualTasksEligibilityManagerTest, NoRedundantNotifications) {
  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  CreateManager();
  EXPECT_TRUE(manager_->IsEligible());

  int notification_count = 0;
  auto subscription =
      manager_->RegisterEligibilityChangedCallback(base::BindRepeating(
          [](int* count, bool eligible) { (*count)++; }, &notification_count));

  // Trigger cookie update that keeps the accounts exactly the same.
  identity_test_env_->SetCookieAccounts(
      {{.email = account_info.email, .gaia_id = account_info.gaia}});

  EXPECT_EQ(0, notification_count);
  EXPECT_TRUE(manager_->IsEligible());
}

}  // namespace contextual_tasks
