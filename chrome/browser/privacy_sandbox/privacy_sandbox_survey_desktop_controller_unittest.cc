// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/privacy_sandbox/privacy_sandbox_survey_desktop_controller.h"

#include <string>
#include <tuple>

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version_info/channel.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using ::base::test::RunOnceClosure;
using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Eq;
using ::testing::Return;

using SentimentSurveyStatus = ::privacy_sandbox::PrivacySandboxSurveyService::
    PrivacySandboxSentimentSurveyStatus;

class PrivacySandboxSurveyDesktopControllerTest : public testing::Test {
 public:
  PrivacySandboxSurveyDesktopControllerTest() = default;

  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));

    ON_CALL(*mock_hats_service_, CanShowAnySurvey(_))
        .WillByDefault(Return(true));

    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
    survey_desktop_controller_ =
        std::make_unique<PrivacySandboxSurveyDesktopController>(profile());
  }

  void TearDown() override {
    mock_hats_service_ = nullptr;
    survey_desktop_controller_.reset();
    profile_.reset();
  }

 protected:
  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{kPrivacySandboxSentimentSurvey, {}}};
  }
  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {};
  }

  PrivacySandboxSurveyDesktopController* survey_desktop_controller() {
    return survey_desktop_controller_.get();
  }
  TestingProfile* profile() { return profile_.get(); }
  PrefService* prefs() { return profile_->GetPrefs(); }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  void TriggerSurvey() {
    survey_desktop_controller_->OnNewTabPageSeen();
    survey_desktop_controller_->MaybeShowSentimentSurvey();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<TestingProfile> profile_;
  base::HistogramTester histogram_tester_;
  raw_ptr<MockHatsService> mock_hats_service_;
  std::unique_ptr<PrivacySandboxSurveyDesktopController>
      survey_desktop_controller_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PrivacySandboxSurveyDesktopControllerTest,
       SentimentSurveyIsNotLaunchedWithoutNtp) {
  EXPECT_CALL(*mock_hats_service_, LaunchSurvey(_, _, _, _, _, _, _)).Times(0);

  survey_desktop_controller_->MaybeShowSentimentSurvey();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
}

TEST_F(PrivacySandboxSurveyDesktopControllerTest, EmitsSurveyShownHistogram) {
  EXPECT_CALL(*mock_hats_service_, LaunchSurvey)
      .WillOnce(RunOnceClosure<1>());  // run the success callback.
  TriggerSurvey();
  histogram_tester_.ExpectBucketCount("PrivacySandbox.SentimentSurvey.Status",
                                      SentimentSurveyStatus::kSurveyShown, 1);
}

TEST_F(PrivacySandboxSurveyDesktopControllerTest,
       EmitsSurveyLaunchedFailedHistogram) {
  EXPECT_CALL(*mock_hats_service_, LaunchSurvey)
      .WillOnce(RunOnceClosure<2>());  // run the failure callback.
  TriggerSurvey();
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.SentimentSurvey.Status",
      SentimentSurveyStatus::kSurveyLaunchFailed, 1);
}

// --- Tests for specific failure conditions ---
class PrivacySandboxSurveyDesktopControllerNullHatsService
    : public PrivacySandboxSurveyDesktopControllerTest {
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        HatsServiceFactory::GetInstance(),
        base::BindOnce(
            [](content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> { return nullptr; }));
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);

    mock_hats_service_ = nullptr;

    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
    survey_desktop_controller_ =
        std::make_unique<PrivacySandboxSurveyDesktopController>(profile());
  }
};

TEST_F(PrivacySandboxSurveyDesktopControllerNullHatsService,
       EmitsHatsServiceFailedHistogram) {
  TriggerSurvey();
  histogram_tester_.ExpectBucketCount("PrivacySandbox.SentimentSurvey.Status",
                                      SentimentSurveyStatus::kHatsServiceFailed,
                                      1);
}

class PrivacySandboxSurveyDesktopControllerFeatureDisabledTest
    : public PrivacySandboxSurveyDesktopControllerTest {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {};
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {kPrivacySandboxSentimentSurvey};
  }
};

TEST_F(PrivacySandboxSurveyDesktopControllerFeatureDisabledTest,
       EmitsFeatureDisabledHistogram) {
  TriggerSurvey();
  histogram_tester_.ExpectBucketCount("PrivacySandbox.SentimentSurvey.Status",
                                      SentimentSurveyStatus::kFeatureDisabled,
                                      1);
}

#if !BUILDFLAG(IS_CHROMEOS)  // Disable on Chrome OS due to PrimaryAccount
                             // always being present.

// Parameterized test suite for verifying PSB/PSD values.
using TestParams = std::tuple<bool, bool, bool, bool>;

class PrivacySandboxSurveyDesktopControllerPSBTest
    : public PrivacySandboxSurveyDesktopControllerTest,
      public testing::WithParamInterface<TestParams> {
 public:
  bool TopicsEnabled() const { return std::get<0>(GetParam()); }
  bool FledgeEnabled() const { return std::get<2>(GetParam()); }
  bool MeasurementEnabled() const { return std::get<2>(GetParam()); }
  bool SignedIn() const { return std::get<3>(GetParam()); }
};

TEST_P(PrivacySandboxSurveyDesktopControllerPSBTest, SendsCorrectPSBAndPSD) {
  // Arrange: Set prefs and sign-in state based on parameters.
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, TopicsEnabled());
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, FledgeEnabled());
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                      MeasurementEnabled());
  if (SignedIn()) {
    identity_test_env()->MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
  } else {
    identity_test_env()->ClearPrimaryAccount();
  }

  std::map<std::string, bool> expected_psb = {
      {"Topics enabled", TopicsEnabled()},
      {"Protected audience enabled", FledgeEnabled()},
      {"Measurement enabled", MeasurementEnabled()},
      {"Signed in", SignedIn()},
  };

  std::map<std::string, std::string> expected_psd = {
      {"Channel",
       std::string(version_info::GetChannelString(chrome::GetChannel()))}};

  // Expect the survey launch with the correct PSB and PSD.
  EXPECT_CALL(*mock_hats_service_,
              LaunchSurvey(kHatsSurveyTriggerPrivacySandboxSentimentSurvey, _,
                           _, Eq(expected_psb), Eq(expected_psd), _, _));

  // Act: Trigger the survey.
  TriggerSurvey();

  // Verify that the survey was launched with the correct parameters.
  testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxSurveyDesktopControllerPSBTest,
                         PrivacySandboxSurveyDesktopControllerPSBTest,
                         Combine(Bool(), Bool(), Bool(), Bool()));

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace privacy_sandbox
