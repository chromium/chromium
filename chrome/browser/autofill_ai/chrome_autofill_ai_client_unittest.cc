// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_ai/chrome_autofill_ai_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_ai {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

std::unique_ptr<KeyedService> CreateOptimizationGuideKeyedService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockOptimizationGuideKeyedService>>();
}

std::unique_ptr<KeyedService> CreateTestPersonalDataManager(
    content::BrowserContext* context) {
  return std::make_unique<autofill::TestPersonalDataManager>();
}

std::unique_ptr<KeyedService> CreateTestEntityDataManager(
    scoped_refptr<autofill::AutofillWebDataService> awds,
    content::BrowserContext* context) {
  return std::make_unique<autofill::EntityDataManager>(
      std::move(awds), /*history_service=*/nullptr,
      /*strike_database=*/nullptr);
}

class ChromeAutofillAiClientTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(autofill_ai::AutofillAiIsPlatformAndEnterprisePolicyEligible(
        profile()->GetPrefs()));
    client_ = ChromeAutofillAiClient::MaybeCreateForWebContents(web_contents(),
                                                                profile());
    ASSERT_TRUE(client_);
  }

  void TearDown() override {
    client_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  ChromeAutofillAiClient& client() { return *client_; }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
                OptimizationGuideKeyedServiceFactory::GetInstance(),
                base::BindRepeating(&CreateOptimizationGuideKeyedService)},
            TestingProfile::TestingFactory{
                autofill::PersonalDataManagerFactory::GetInstance(),
                base::BindRepeating(&CreateTestPersonalDataManager)},
            TestingProfile::TestingFactory{
                autofill::AutofillEntityDataManagerFactory::GetInstance(),
                base::BindRepeating(&CreateTestEntityDataManager,
                                    awds_helper_.autofill_webdata_service())}};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      autofill::features::kAutofillAiWithDataSchema};
  std::unique_ptr<ChromeAutofillAiClient> client_;
  autofill::AutofillWebDataServiceTestHelper awds_helper_{
      std::make_unique<autofill::EntityTable>()};
};

TEST_F(ChromeAutofillAiClientTest, GetAXTree) {
  base::MockCallback<autofill_ai::AutofillAiClient::AXTreeCallback> callback;
  EXPECT_CALL(callback, Run);
  client().GetAXTree(callback.Get());
}

TEST_F(ChromeAutofillAiClientTest,
       IsAutofillAiEnabledPrefReturnsTrueIfPrefEnabled) {
  profile()->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillPredictionImprovementsEnabled, true);
  EXPECT_TRUE(client().IsAutofillAiEnabledPref());
}

TEST_F(ChromeAutofillAiClientTest,
       IsAutofillAiEnabledPrefReturnsFalseIfPrefDisabled) {
  profile()->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillPredictionImprovementsEnabled, false);
  EXPECT_FALSE(client().IsAutofillAiEnabledPref());
}

TEST_F(ChromeAutofillAiClientTest, EligibilityOfNotSignedInUser) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder().Build("example@gmail.com"));

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  EXPECT_FALSE(client().IsUserEligible());
}

TEST_F(ChromeAutofillAiClientTest, EligibilityOfSignedInUserWithMlDisabled) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "example@gmail.com", signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  EXPECT_FALSE(client().IsUserEligible());
}

TEST_F(ChromeAutofillAiClientTest, EligibilityOfSignedInUserWithMlEnabled) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "example@gmail.com", signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  EXPECT_TRUE(client().IsUserEligible());
}

// Tests that no ChromeAutofillAiClient is created if
// `AutofillAiIsPlatformAndEnterprisePolicyEligible()` is false.
TEST_F(ChromeAutofillAiClientTest, MaybeCreateForWebContents) {
  ASSERT_TRUE(autofill_ai::AutofillAiIsPlatformAndEnterprisePolicyEligible(
      profile()->GetPrefs()));
  EXPECT_TRUE(ChromeAutofillAiClient::MaybeCreateForWebContents(web_contents(),
                                                                profile()));

  profile()->GetPrefs()->SetBoolean(autofill::prefs::kAutofillProfileEnabled,
                                    false);
  ASSERT_FALSE(autofill_ai::AutofillAiIsPlatformAndEnterprisePolicyEligible(
      profile()->GetPrefs()));
  EXPECT_FALSE(ChromeAutofillAiClient::MaybeCreateForWebContents(web_contents(),
                                                                 profile()));
}

}  // namespace
}  // namespace autofill_ai
