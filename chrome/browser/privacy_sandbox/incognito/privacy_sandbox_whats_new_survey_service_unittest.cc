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
#include "chrome/browser/ui/webui/whats_new/whats_new.mojom.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_interaction_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "privacy_sandbox_incognito_features.h"

namespace privacy_sandbox {

namespace {

using ::testing::_;
using ::testing::ContainerEq;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using WhatsNewSurveyStatus = ::privacy_sandbox::
    PrivacySandboxWhatsNewSurveyService::WhatsNewSurveyStatus;

// Helper to run the Nth argument as a base::OnceClosure and return a value.
// For LaunchSurveyForWebContents, index 4 is success, 5 is failure.
template <size_t I, typename T>
auto RunOnceClosureAndReturn(T output) {
  return [output = std::move(output)](auto&&... args) -> decltype(auto) {
    base::test::RunOnceClosure<I>()(args...);
    return std::move(output);
  };
}

}  // namespace

class PrivacySandboxWhatsNewSurveyServiceTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PrivacySandboxWhatsNewSurveyServiceTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});

    SetUpHatsFactory();

    service_ = std::make_unique<PrivacySandboxWhatsNewSurveyService>(profile());
  }

  void TearDown() override {
    service_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
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
    auto mock_service = std::make_unique<NiceMock<MockHatsService>>(
        static_cast<Profile*>(context));
    ON_CALL(*mock_service, CanShowAnySurvey(_)).WillByDefault(Return(true));
    return mock_service;
  }

  PrivacySandboxWhatsNewSurveyService* survey_service() {
    return service_.get();
  }

  MockHatsService* hats_service() {
    return static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->GetForProfile(
            profile(), /*create_if_necessary=*/true));
  }
  void TriggerWhatsNewSurvey() { service_->MaybeShowSurvey(web_contents()); }

  base::HistogramTester histogram_tester_;
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

class PrivacySandboxWhatsNewSurveyServicePsdTest
    : public PrivacySandboxWhatsNewSurveyServiceFeatureEnabledTest {
 public:
  PrivacySandboxWhatsNewSurveyServicePsdTest() = default;

 protected:
  void AddModuleShown(const std::string& name,
                      whats_new::mojom::ModulePosition position) {
    WhatsNewInteractionData::CreateForWebContents(web_contents());
    WhatsNewInteractionData* interaction_data =
        WhatsNewInteractionData::FromWebContents(web_contents());
    ASSERT_NE(interaction_data, nullptr);
    interaction_data->add_module_shown(name, position);
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
  EXPECT_CALL(*hats_service(), LaunchSurveyForWebContents).Times(0);

  TriggerWhatsNewSurvey();

  // No need to wait here, this condition is checked before setting up a task.

  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kFeatureDisabled,
                                      1);
  histogram_tester_.ExpectTotalCount("PrivacySandbox.WhatsNewSurvey.Status", 1);
}

TEST_F(PrivacySandboxWhatsNewSurveyServiceFeatureEnabledTest,
       MaybeShowSurvey_WebContentsDestructedBeforeDelay) {
  EXPECT_CALL(*hats_service(), LaunchSurveyForWebContents).Times(0);

  service_->MaybeShowSurvey(web_contents());

  // Delete the WebContents
  DeleteContents();

  // Fast forward time
  base::TimeDelta delay = kPrivacySandboxWhatsNewSurveyDelay.Get();
  task_environment()->FastForwardBy(delay);

  // Survey should not have been shown, and an appropriate status recorded.
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.WhatsNewSurvey.Status",
      WhatsNewSurveyStatus::kWebContentsDestructed, 1);
  histogram_tester_.ExpectTotalCount("PrivacySandbox.WhatsNewSurvey.Status", 1);
}

// Test when the HatsService is not available.
TEST_F(PrivacySandboxWhatsNewSurveyServiceNullHatsServiceTest,
       MaybeShowSurvey_HatsServiceMissing) {
  TriggerWhatsNewSurvey();
  base::TimeDelta delay = kPrivacySandboxWhatsNewSurveyDelay.Get();
  task_environment()->FastForwardBy(delay);

  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kHatsServiceFailed,
                                      1);
}

// Test the successful survey launch path with default PSD.
TEST_F(PrivacySandboxWhatsNewSurveyServicePsdTest,
       MaybeShowSurvey_Launched_Success_DefaultPsd) {
  // No InteractionData created.
  SurveyStringData expected_psd = {{kHasSeenActFeaturesPsdKey, "unknown"}};
  ASSERT_NE(HatsServiceFactory::GetForProfile(profile(),
                                              /*create_if_necessary=*/true),
            nullptr);

  EXPECT_CALL(*hats_service(),
              LaunchSurveyForWebContents(
                  /*trigger=*/_,
                  /*web_contents=*/web_contents(),
                  /*product_specific_bits_data=*/IsEmpty(),
                  /*product_specific_string_data=*/ContainerEq(expected_psd),
                  /*success_callback=*/_,
                  /*failure_callback=*/_,
                  /*supplied_trigger_id=*/_,
                  /*survey_options=*/_))
      .WillOnce(RunOnceClosureAndReturn<4>(true));

  service_->MaybeShowSurvey(web_contents());

  // skip the delay
  base::TimeDelta delay = kPrivacySandboxWhatsNewSurveyDelay.Get();
  task_environment()->FastForwardBy(delay);

  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kSurveyShown, 1);
  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kSurveyLaunched, 1);
  histogram_tester_.ExpectTotalCount("PrivacySandbox.WhatsNewSurvey.Status", 2);
}

// Test survey launch failure path with PSD.
TEST_F(PrivacySandboxWhatsNewSurveyServicePsdTest,
       MaybeShowSurvey_Launched_Failure_WithPsd) {
  ASSERT_NE(HatsServiceFactory::GetForProfile(profile(),
                                              /*create_if_necessary=*/true),
            nullptr);

  // No InteractionData created.
  SurveyStringData expected_psd = {{kHasSeenActFeaturesPsdKey, "unknown"}};

  EXPECT_CALL(*hats_service(),
              LaunchSurveyForWebContents(
                  /*trigger=*/_,
                  /*web_contents=*/web_contents(),
                  /*product_specific_bits_data=*/IsEmpty(),
                  /*product_specific_string_data=*/ContainerEq(expected_psd),
                  /*success_callback=*/_,
                  /*failure_callback=*/_,
                  /*supplied_trigger_id=*/_,
                  /*survey_options=*/_))
      .WillOnce(RunOnceClosureAndReturn<5>(true));

  service_->MaybeShowSurvey(web_contents());
  // skip the delay
  base::TimeDelta delay = kPrivacySandboxWhatsNewSurveyDelay.Get();
  task_environment()->FastForwardBy(delay);

  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kSurveyLaunchFailed,
                                      1);
  histogram_tester_.ExpectBucketCount("PrivacySandbox.WhatsNewSurvey.Status",
                                      WhatsNewSurveyStatus::kSurveyLaunched, 1);
  histogram_tester_.ExpectTotalCount("PrivacySandbox.WhatsNewSurvey.Status", 2);
}

TEST_F(PrivacySandboxWhatsNewSurveyServicePsdTest, Psd_ActModuleShown) {
  AddModuleShown(privacy_sandbox::kPrivacySandboxActWhatsNew.name,
                 whats_new::mojom::ModulePosition::kSpotlight1);

  SurveyStringData expected_psd = {{kHasSeenActFeaturesPsdKey, "true"}};

  EXPECT_CALL(*hats_service(),
              LaunchSurveyForWebContents(
                  /*trigger=*/_,
                  /*web_contents=*/web_contents(),
                  /*product_specific_bits_data=*/IsEmpty(),
                  /*product_specific_string_data=*/ContainerEq(expected_psd),
                  /*success_callback=*/_,
                  /*failure_callback=*/_,
                  /*supplied_trigger_id=*/_,
                  /*survey_options=*/_));
  service_->MaybeShowSurvey(web_contents());
  task_environment()->FastForwardBy(kPrivacySandboxWhatsNewSurveyDelay.Get());
}

TEST_F(PrivacySandboxWhatsNewSurveyServicePsdTest, Psd_OtherModuleShown) {
  AddModuleShown("SomeOtherModule",
                 whats_new::mojom::ModulePosition::kSpotlight1);

  SurveyStringData expected_psd = {{kHasSeenActFeaturesPsdKey, "false"}};

  EXPECT_CALL(*hats_service(),
              LaunchSurveyForWebContents(
                  /*trigger=*/_,
                  /*web_contents=*/web_contents(),
                  /*product_specific_bits_data=*/IsEmpty(),
                  /*product_specific_string_data=*/ContainerEq(expected_psd),
                  /*success_callback=*/_,
                  /*failure_callback=*/_,
                  /*supplied_trigger_id=*/_,
                  /*survey_options=*/_));
  service_->MaybeShowSurvey(web_contents());
  task_environment()->FastForwardBy(kPrivacySandboxWhatsNewSurveyDelay.Get());
}

}  // namespace privacy_sandbox
