// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_desktop_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class PrivacySandboxSurveyDesktopControllerTest : public testing::Test {
 public:
  PrivacySandboxSurveyDesktopControllerTest() {
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    survey_service_ = std::make_unique<PrivacySandboxSurveyService>(
        prefs(), identity_test_env_->identity_manager());
  }

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));
    ON_CALL(*mock_hats_service_, CanShowAnySurvey(_))
        .WillByDefault(Return(true));
    survey_desktop_controller_ =
        std::make_unique<PrivacySandboxSurveyDesktopController>(
            survey_service());
  }

  void TearDown() override {
    survey_desktop_controller_.reset();
    survey_service_.reset();
  }

 protected:
  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{kPrivacySandboxSentimentSurvey, {}}};
  }
  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {};
  }
  PrivacySandboxSurveyService* survey_service() {
    return survey_service_.get();
  }
  PrivacySandboxSurveyDesktopController* survey_desktop_controller() {
    return survey_desktop_controller_.get();
  }
  TestingPrefServiceSimple* prefs() { return &prefs_; }
  TestingProfile* profile() { return &profile_; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  TestingProfile profile_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  raw_ptr<MockHatsService, DanglingUntriaged> mock_hats_service_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<PrivacySandboxSurveyDesktopController>
      survey_desktop_controller_;
  std::unique_ptr<PrivacySandboxSurveyService> survey_service_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PrivacySandboxSurveyDesktopControllerTest, SurveyIsLaunched) {
  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurvey(_, _, _, survey_service()->GetSentimentSurveyPsb(), _));

  survey_desktop_controller_->MaybeShowSentimentSurvey(profile());
  testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
}

TEST_F(PrivacySandboxSurveyDesktopControllerTest, EmitsSurveyShownHistogram) {
  EXPECT_CALL(*mock_hats_service_, LaunchSurvey(_, _, _, _, _))
      .WillOnce(Invoke([](const std::string& trigger,
                          base::OnceClosure success_callback,
                          base::OnceClosure failure_callback,
                          const SurveyBitsData& survey_specific_bits_data,
                          const SurveyStringData& survey_specific_string_data) {
        // Force a failure by calling the failure callback.
        std::move(success_callback).Run();
      }));
  survey_desktop_controller_->MaybeShowSentimentSurvey(profile());
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.SentimentSurvey.Status",
      PrivacySandboxSurveyService::PrivacySandboxSentimentSurveyStatus::
          kSurveyShown,
      1);
}

TEST_F(PrivacySandboxSurveyDesktopControllerTest,
       EmitsHatsServiceFailedHistogram) {
  HatsServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(
                     [](content::BrowserContext* context)
                         -> std::unique_ptr<KeyedService> { return nullptr; }));
  survey_desktop_controller_->MaybeShowSentimentSurvey(profile());
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.SentimentSurvey.Status",
      PrivacySandboxSurveyService::PrivacySandboxSentimentSurveyStatus::
          kHatsServiceFailed,
      1);
}

TEST_F(PrivacySandboxSurveyDesktopControllerTest,
       EmitsSurveyLaunchedFailedHistogram) {
  EXPECT_CALL(*mock_hats_service_, LaunchSurvey(_, _, _, _, _))
      .WillOnce(Invoke([](const std::string& trigger,
                          base::OnceClosure success_callback,
                          base::OnceClosure failure_callback,
                          const SurveyBitsData& survey_specific_bits_data,
                          const SurveyStringData& survey_specific_string_data) {
        // Force a failure by calling the failure callback.
        std::move(failure_callback).Run();
      }));
  survey_desktop_controller_->MaybeShowSentimentSurvey(profile());
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.SentimentSurvey.Status",
      PrivacySandboxSurveyService::PrivacySandboxSentimentSurveyStatus::
          kSurveyLaunchFailed,
      1);
}

class PrivacySandboxSurveyDesktopControllerFeatureDisabledTest
    : public PrivacySandboxSurveyDesktopControllerTest {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{}};
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {kPrivacySandboxSentimentSurvey};
  }
};

TEST_F(PrivacySandboxSurveyDesktopControllerFeatureDisabledTest,
       EmitsFeatureDisabledHistogram) {
  survey_desktop_controller_->MaybeShowSentimentSurvey(profile());
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.SentimentSurvey.Status",
      PrivacySandboxSurveyService::PrivacySandboxSentimentSurveyStatus::
          kFeatureDisabled,
      1);
}

}  // namespace
}  // namespace privacy_sandbox
