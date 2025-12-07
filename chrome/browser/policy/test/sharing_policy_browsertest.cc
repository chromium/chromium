// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/collaboration/public/pref_names.h"
#include "components/collaboration/public/service_status.h"
#include "components/data_sharing/public/features.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace policy {

namespace {

class MockCollaborationServiceObserver
    : public collaboration::CollaborationService::Observer {
 public:
  MockCollaborationServiceObserver() = default;
  ~MockCollaborationServiceObserver() override = default;

  MOCK_METHOD(void,
              OnServiceStatusChanged,
              (const ServiceStatusUpdate& update),
              (override));
};

}  // namespace

class TabGroupSharingTest : public PolicyTest {
 public:
  TabGroupSharingTest() {
    feature_list_.InitWithFeatureStates({
        {data_sharing::features::kDataSharingFeature, true},
        {data_sharing::features::kCollaborationEntrepriseV2, true},
    });
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    // Sign in.
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    auto account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@google.com",
                                          signin::ConsentLevel::kSignin);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &TabGroupSharingTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  base::test::ScopedFeatureList feature_list_;

  // Identity test support.
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(TabGroupSharingTest, TabGroupSharingEnableToDisable) {
  const PrefService* const prefs =
      chrome_test_utils::GetProfile(this)->GetPrefs();
  EXPECT_EQ(0 /*enabled*/,
            prefs->GetInteger(
                collaboration::prefs::kSharedTabGroupsManagedAccountSetting));

  testing::StrictMock<MockCollaborationServiceObserver> mock_observer;
  auto* profile = browser()->profile();
  auto* service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  service->AddObserver(&mock_observer);
  collaboration::CollaborationService::Observer::ServiceStatusUpdate update;
  EXPECT_CALL(mock_observer, OnServiceStatusChanged(testing::_))
      .WillOnce(testing::SaveArg<0>(&update));

  EXPECT_TRUE(service->GetServiceStatus().IsAllowedToJoin());

  // Now set the policy and check that shared tab group managed account is
  // turned off by policy.
  PolicyMap policies;
  policies.Set(key::kTabGroupSharingSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(1 /*disabled*/,
            prefs->GetInteger(
                collaboration::prefs::kSharedTabGroupsManagedAccountSetting));
  EXPECT_FALSE(service->GetServiceStatus().IsAllowedToJoin());
  EXPECT_EQ(update.new_status.collaboration_status,
            collaboration::CollaborationStatus::kDisabledForPolicy);
}

}  // namespace policy
