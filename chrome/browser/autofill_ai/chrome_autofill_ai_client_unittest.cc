// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_ai/chrome_autofill_ai_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_annotations/test_user_annotations_service.h"
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

std::unique_ptr<KeyedService> CreateUserAnnotationsServiceFactory(
    content::BrowserContext* context) {
  return std::make_unique<user_annotations::TestUserAnnotationsService>();
}

std::unique_ptr<KeyedService> CreateTestPersonalDataManager(
    content::BrowserContext* context) {
  return std::make_unique<autofill::TestPersonalDataManager>();
}

class ChromeAutofillAiClientTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(autofill_ai::IsAutofillAiSupported(profile()->GetPrefs()));
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
                UserAnnotationsServiceFactory::GetInstance(),
                base::BindRepeating(&CreateUserAnnotationsServiceFactory)},
            TestingProfile::TestingFactory{
                autofill::PersonalDataManagerFactory::GetInstance(),
                base::BindRepeating(&CreateTestPersonalDataManager)}};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{autofill_ai::kAutofillAi};
  std::unique_ptr<ChromeAutofillAiClient> client_;
};

TEST_F(ChromeAutofillAiClientTest, GetAXTree) {
  base::MockCallback<autofill_ai::AutofillAiClient::AXTreeCallback> callback;
  EXPECT_CALL(callback, Run);
  client().GetAXTree(callback.Get());
}

TEST_F(ChromeAutofillAiClientTest, GetUserAnnotationsService) {
  EXPECT_TRUE(client().GetUserAnnotationsService());
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

// Tests that the Autofill AI model executor is initialized and returned.
TEST_F(ChromeAutofillAiClientTest, GetModelExecutor) {
  EXPECT_TRUE(client().GetModelExecutor());
}

// Tests that GetLastCommittedURL() accurately returns the last committed URL.
TEST_F(ChromeAutofillAiClientTest, GetLastCommittedURL) {
  const GURL about_blank = GURL("about:blank");
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(about_blank);
  EXPECT_EQ(client().GetLastCommittedURL(), about_blank);
}

// Tests that GetTitle() returns an empty string if no navigation had happened
// before.
TEST_F(ChromeAutofillAiClientTest, GetTitle) {
  EXPECT_EQ(client().GetTitle(), "");
}

// Tests that TryToOpenFeedbackPage() doesn't emit histogram
// "Feedback.RequestSource" when
// `LogAiDataRequest::FeatureCase::kFormsPredictions` should not be allowed for
// feedback. The emission of the histogram is an indicator that the feedback
// page would be opened. Unfortunately there isn't a more robust way to test
// this. Also the case where the feature should be allowed for feedback is hard
// to test in this environment because it involves views and crashes the test.
TEST_F(ChromeAutofillAiClientTest, TryToOpenFeedbackPage) {
  auto* mock_optimization_guide_service =
      static_cast<NiceMock<MockOptimizationGuideKeyedService>*>(
          OptimizationGuideKeyedServiceFactory::GetInstance()->GetForProfile(
              profile()));
  EXPECT_CALL(*mock_optimization_guide_service,
              ShouldFeatureBeCurrentlyAllowedForFeedback(
                  optimization_guide::proto::LogAiDataRequest::FeatureCase::
                      kFormsPredictions))
      .WillOnce(Return(false));
  base::HistogramTester histogram_tester_;
  client().TryToOpenFeedbackPage("feedback id");
  histogram_tester_.ExpectUniqueSample("Feedback.RequestSource",
                                       feedback::kFeedbackSourceAI, 0);
}

// Tests that no ChromeAutofillAiClient is created if
// IsAutofillAiSupported() is false.
TEST_F(ChromeAutofillAiClientTest, MaybeCreateForWebContents) {
  ASSERT_TRUE(autofill_ai::IsAutofillAiSupported(profile()->GetPrefs()));
  EXPECT_TRUE(ChromeAutofillAiClient::MaybeCreateForWebContents(web_contents(),
                                                                profile()));

  profile()->GetPrefs()->SetBoolean(autofill::prefs::kAutofillProfileEnabled,
                                    false);
  ASSERT_FALSE(autofill_ai::IsAutofillAiSupported(profile()->GetPrefs()));
  EXPECT_FALSE(ChromeAutofillAiClient::MaybeCreateForWebContents(web_contents(),
                                                                 profile()));
}

// Tests that
// ChromeAutofillAiClient::GetAutofillNameFillingValue
// returns accurate information about the value for filling of an
// AutofillProfile for a given name type.
TEST_F(ChromeAutofillAiClientTest, GetAutofillNameFillingValue) {
  autofill::FormFieldData test_field;
  autofill::AutofillProfile test_autofill_profile =
      autofill::test::GetFullProfile();

  // Currently the client should not see any info since no
  // `test_autofill_profile` is not stored.
  EXPECT_TRUE(client()
                  .GetAutofillNameFillingValue(test_autofill_profile.guid(),
                                               autofill::NAME_FIRST, test_field)
                  .empty());

  // Add `test_profile` to the autofill profile storage.
  autofill::PersonalDataManager* pdm =
      autofill::PersonalDataManagerFactory::GetForBrowserContext(
          browser_context());
  pdm->address_data_manager().AddProfile(test_autofill_profile);

  // Now, the client should have access to the profile since it was stored in
  // memory, and should return an accurate value for filling.
  ASSERT_TRUE(test_autofill_profile.HasInfo(autofill::NAME_FIRST));
  EXPECT_EQ(client().GetAutofillNameFillingValue(
                test_autofill_profile.guid(), autofill::NAME_FIRST, test_field),
            test_autofill_profile.GetRawInfo(autofill::NAME_FIRST));

  // Nevertheless, the client should not have access to more than the values
  // of the profiles for the name types, since by design those additional values
  // are not needed.
  ASSERT_TRUE(
      test_autofill_profile.HasInfo(autofill::ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_TRUE(client()
                  .GetAutofillNameFillingValue(
                      test_autofill_profile.guid(),
                      autofill::ADDRESS_HOME_STREET_ADDRESS, test_field)
                  .empty());
}

}  // namespace
}  // namespace autofill_ai
