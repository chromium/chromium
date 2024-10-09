// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_prediction_improvements/chrome_autofill_prediction_improvements_client.h"

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_annotations/test_user_annotations_service.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class ChromeAutofillPredictionImprovementsClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ChromeAutofillPredictionImprovementsClient::CreateForWebContents(
        web_contents());
  }

  ChromeAutofillPredictionImprovementsClient* client() {
    return ChromeAutofillPredictionImprovementsClient::FromWebContents(
        web_contents());
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
                OptimizationGuideKeyedServiceFactory::GetInstance(),
                base::BindRepeating(&CreateOptimizationGuideKeyedService)},
            TestingProfile::TestingFactory{
                UserAnnotationsServiceFactory::GetInstance(),
                base::BindRepeating(&CreateUserAnnotationsServiceFactory)}};
  }
};

TEST_F(ChromeAutofillPredictionImprovementsClientTest, GetAXTree) {
  base::MockCallback<autofill_prediction_improvements::
                         AutofillPredictionImprovementsClient::AXTreeCallback>
      callback;
  EXPECT_CALL(callback, Run);
  client()->GetAXTree(callback.Get());
}

TEST_F(ChromeAutofillPredictionImprovementsClientTest,
       GetUserAnnotationsService) {
  EXPECT_TRUE(client()->GetUserAnnotationsService());
}

TEST_F(ChromeAutofillPredictionImprovementsClientTest,
       IsAutofillPredictionImprovementsEnabledPrefReturnsTrueIfPrefEnabled) {
  profile()->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillPredictionImprovementsEnabled, true);
  EXPECT_TRUE(client()->IsAutofillPredictionImprovementsEnabledPref());
}

TEST_F(ChromeAutofillPredictionImprovementsClientTest,
       IsAutofillPredictionImprovementsEnabledPrefReturnsFalseIfPrefDisabled) {
  profile()->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillPredictionImprovementsEnabled, false);
  EXPECT_FALSE(client()->IsAutofillPredictionImprovementsEnabledPref());
}

TEST_F(ChromeAutofillPredictionImprovementsClientTest,
       EligibilityOfNotSignedInUser) {
  signin::MakeAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile()),
      signin::AccountAvailabilityOptionsBuilder().Build("example@gmail.com"));
  EXPECT_FALSE(client()->IsUserEligible());
}

TEST_F(ChromeAutofillPredictionImprovementsClientTest,
       EligibilityOfSignedInUser) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile()), "example@gmail.com",
      signin::ConsentLevel::kSignin);
  EXPECT_TRUE(client()->IsUserEligible());
}

// Tests that the filling engine is initialized and returned.
TEST_F(ChromeAutofillPredictionImprovementsClientTest, GetFillingEngine) {
  EXPECT_TRUE(client()->GetFillingEngine());
}

// Tests that GetLastCommittedURL() accurately returns the last committed URL.
TEST_F(ChromeAutofillPredictionImprovementsClientTest, GetLastCommittedURL) {
  const GURL about_blank = GURL("about:blank");
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(about_blank);
  EXPECT_EQ(client()->GetLastCommittedURL(), about_blank);
}

// Tests that GetTitle() returns an empty string if no navigation had happened
// before.
TEST_F(ChromeAutofillPredictionImprovementsClientTest, GetTitle) {
  EXPECT_EQ(client()->GetTitle(), "");
}

// Tests that TryToOpenFeedbackPage() doesn't emit histogram
// "Feedback.RequestSource" when
// `LogAiDataRequest::FeatureCase::kFormsPredictions` should not be allowed for
// feedback. The emission of the histogram is an indicator that the feedback
// page would be opened. Unfortunately there isn't a more robust way to test
// this. Also the case where the feature should be allowed for feedback is hard
// to test in this environment because it involves views and crashes the test.
TEST_F(ChromeAutofillPredictionImprovementsClientTest, TryToOpenFeedbackPage) {
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
  client()->TryToOpenFeedbackPage("feedback id");
  histogram_tester_.ExpectUniqueSample("Feedback.RequestSource",
                                       feedback::kFeedbackSourceAI, 0);
}

}  // namespace
