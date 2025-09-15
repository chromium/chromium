// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_whats_new_survey_service.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_utils.h"
#include "privacy_sandbox_incognito_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

using WhatsNewSurveyStatus = ::privacy_sandbox::
    PrivacySandboxWhatsNewSurveyService::WhatsNewSurveyStatus;

// Helper to run the Nth argument as a base::OnceClosure and return a value.
// For LaunchDelayedSurveyForWebContents, index 6 is success, 7 is failure.
template <size_t I, typename T>
auto RunOnceClosureAndReturn(T output) {
  return [output = std::move(output)](auto&&... args) -> decltype(auto) {
    base::test::RunOnceClosure<I>()(args...);
    return std::move(output);
  };
}

}  // namespace

class PrivacySandboxWhatsNewSurveyServiceTest : public ::testing::Test {
 public:
  PrivacySandboxWhatsNewSurveyServiceTest() = default;

  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});

    SetUpHatsFactory();

    service_ = std::make_unique<PrivacySandboxWhatsNewSurveyService>(profile());
  }

  void TearDown() override {
    service_.reset();
    mock_hats_service_ = nullptr;
    profile_.reset();
  }

 protected:
  virtual void SetUpHatsFactory() {
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(
            &PrivacySandboxWhatsNewSurveyServiceTest::CreateMockHatsService,
            base::Unretained(this)));
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {};
  }

  std::unique_ptr<KeyedService> CreateMockHatsService(
      content::BrowserContext* context) {
    CHECK_EQ(mock_hats_service_, nullptr)
        << "CreateMockHatsService() called more than once for the same test "
           "instance. mock_hats_service_ is already set.";
    auto mock_service = std::make_unique<NiceMock<MockHatsService>>(
        static_cast<Profile*>(context));
    mock_hats_service_ = mock_service.get();
    ON_CALL(*mock_hats_service_, CanShowAnySurvey(_))
        .WillByDefault(Return(true));
    return mock_service;
  }

  PrivacySandboxWhatsNewSurveyService* survey_service() {
    return service_.get();
  }

  MockHatsService* hats_service() { return mock_hats_service_; }
  TestingProfile* profile() { return profile_.get(); }
  void TriggerWhatsNewSurvey() { service_->MaybeShowSurvey(nullptr); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  base::HistogramTester histogram_tester_;
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
  std::unique_ptr<PrivacySandboxWhatsNewSurveyService> service_;
  base::test::ScopedFeatureList feature_list_;
};

class PrivacySandboxWhatsNewSurveyServiceFeatureEnabledTest
    : public PrivacySandboxWhatsNewSurveyServiceTest {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kPrivacySandboxWhatsNewSurvey, {}}};
  }
};

class PrivacySandboxWhatsNewSurveyServiceNullHatsServiceTest
    : public PrivacySandboxWhatsNewSurveyServiceFeatureEnabledTest {
  void SetUpHatsFactory() override {
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>();
        }));
  }
};

TEST_F(PrivacySandboxWhatsNewSurveyServiceTest,
       IsWhatsNewSurveyEnabled_DisabledByDefault) {
  EXPECT_FALSE(service_->IsSurveyEnabled());
}

TEST_F(PrivacySandboxWhatsNewSurveyServiceTest,
       RecordWhatsNewSurveyStatus_EmitsHistogram) {
  survey_service()->RecordSurveyStatus(WhatsNewSurveyStatus::kFeatureDisabled);
  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kFeatureDisabled,
                                      1);
  histogram_tester_.ExpectTotalCount("PrivacySandbox.WhatsNewSurvey.Status", 1);
}

// Test when the main feature flag is disabled.
TEST_F(PrivacySandboxWhatsNewSurveyServiceTest,
       MaybeShowSurvey_FeatureDisabled) {
  ASSERT_NE(HatsServiceFactory::GetForProfile(profile(),
                                              /*create_if_necessary=*/true),
            nullptr);
  EXPECT_CALL(*hats_service(), LaunchDelayedSurveyForWebContents).Times(0);

  TriggerWhatsNewSurvey();
  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kFeatureDisabled,
                                      1);
  histogram_tester_.ExpectTotalCount("PrivacySandbox.WhatsNewSurvey.Status", 1);
}

// Test when the HatsService is not available.
TEST_F(PrivacySandboxWhatsNewSurveyServiceNullHatsServiceTest,
       MaybeShowSurvey_HatsServiceMissing) {
  TriggerWhatsNewSurvey();

  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kHatsServiceFailed,
                                      1);
}

// Test the successful survey launch path.
TEST_F(PrivacySandboxWhatsNewSurveyServiceFeatureEnabledTest,
       MaybeShowSurvey_Launched_Success) {
  ASSERT_NE(HatsServiceFactory::GetForProfile(profile(),
                                              /*create_if_necessary=*/true),
            nullptr);
  EXPECT_CALL(*hats_service(), LaunchDelayedSurveyForWebContents)
      .WillOnce(RunOnceClosureAndReturn<6>(
          true));  // Invoke the success_callback (argument 6)

  TriggerWhatsNewSurvey();

  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kSurveyShown, 1);

  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kSurveyLaunched, 1);
  histogram_tester_.ExpectTotalCount("PrivacySandbox.WhatsNewSurvey.Status", 2);
}

TEST_F(PrivacySandboxWhatsNewSurveyServiceFeatureEnabledTest,
       MaybeShowSurvey_Launched_Failure) {
  ASSERT_NE(HatsServiceFactory::GetForProfile(profile(), true), nullptr);

  EXPECT_CALL(*hats_service(), LaunchDelayedSurveyForWebContents)
      .WillOnce(RunOnceClosureAndReturn<7>(
          true));  // Invoke the failure_callback (argument 7)
  TriggerWhatsNewSurvey();

  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kSurveyLaunchFailed,
                                      1);

  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kSurveyLaunched, 1);
  histogram_tester_.ExpectTotalCount("PrivacySandbox.WhatsNewSurvey.Status", 2);
}

}  // namespace privacy_sandbox
