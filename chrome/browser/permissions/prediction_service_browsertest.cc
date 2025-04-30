// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/permissions/prediction_model_handler_provider.h"
#include "chrome/browser/permissions/prediction_model_handler_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

namespace {
constexpr auto kLikelihoodUnspecified =
    PermissionUmaUtil::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_DISCRETIZED_LIKELIHOOD_UNSPECIFIED;
constexpr auto kLikelihoodVeryUnlikely =
    PermissionUmaUtil::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;

// The model returns a constant value of 0.5; its meaning is defined by the
// max_likely threshold we use in the signature_model_executor to differentiate
// between very unlikely and unspecified.
base::FilePath& ModelFilePath() {
  static base::NoDestructor<base::FilePath> file_path([]() {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    return source_root_dir.AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("permissions")
        .AppendASCII("signature_model_ret_0.5.tflite");
  }());
  return *file_path;
}
}  // namespace

class PredictionServiceBrowserTestBase : public InProcessBrowserTest {
 public:
  PredictionServiceBrowserTestBase() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPermissionOnDeviceNotificationPredictions, {}},
         {optimization_guide::features::kOptimizationHints, {}},
         {optimization_guide::features::kRemoteOptimizationGuideFetching, {}},
         {features::kCpssUseTfliteSignatureRunner, {}}},
        {permissions::features::kPermissionsAIv1});
  }

  ~PredictionServiceBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    PermissionRequestManager* manager = GetPermissionRequestManager();
    mock_permission_prompt_factory_ =
        std::make_unique<MockPermissionPromptFactory>(manager);
    host_resolver()->AddRule("*", "127.0.0.1");
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kEnableNotificationCPSS,
                                                 true);
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kEnableGeolocationCPSS,
                                                 true);
  }

  void TearDownOnMainThread() override {
    mock_permission_prompt_factory_.reset();
  }

  content::RenderFrameHost* GetActiveMainFrame() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  PermissionRequestManager* GetPermissionRequestManager() {
    return PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  MockPermissionPromptFactory* bubble_factory() {
    return mock_permission_prompt_factory_.get();
  }

  PredictionModelHandler* prediction_model_handler() {
    return PredictionModelHandlerProviderFactory::GetForBrowserContext(
               browser()->profile())
        ->GetPredictionModelHandler(RequestType::kNotifications);
  }

  void TriggerPromptAndVerifyUI(
      std::string test_url,
      PermissionAction permission_action,
      bool should_expect_quiet_ui,
      std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          expected_prediction_likelihood) {
    auto* manager = GetPermissionRequestManager();
    GURL url = embedded_test_server()->GetURL(test_url, "/title1.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    MockPermissionRequest req(RequestType::kNotifications);
    manager->AddRequest(GetActiveMainFrame(), &req);
    bubble_factory()->WaitForPermissionBubble();
    EXPECT_EQ(should_expect_quiet_ui,
              manager->ShouldCurrentRequestUseQuietUI());
    EXPECT_EQ(expected_prediction_likelihood,
              manager->prediction_grant_likelihood_for_testing());
    if (permission_action == PermissionAction::DISMISSED) {
      manager->Dismiss();
    } else if (permission_action == PermissionAction::GRANTED) {
      manager->Accept();
    }
  }

 private:
  std::unique_ptr<MockPermissionPromptFactory> mock_permission_prompt_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

using PredictionServiceBrowserTest = PredictionServiceBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PredictionServiceBrowserTest, PredictionServiceEnabled) {
  EXPECT_TRUE(prediction_model_handler());
}

struct HoldbackProbabilityTestCase {
  std::string test_name;
  float holdback_probability;
  // At the moment, we define everything that the signature model returns that
  // is above that threshold as very unlikely, and everything below that will
  // return unspecified.
  float max_likely_threshold;
  bool should_expect_quiet_ui;
  std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
      expected_prediction_likelihood;
};

class ParametrizedPredictionServiceBrowserTest
    : public PredictionServiceBrowserTestBase,
      public testing::WithParamInterface<HoldbackProbabilityTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    HoldbackProbabilityTest,
    ParametrizedPredictionServiceBrowserTest,
    testing::ValuesIn<HoldbackProbabilityTestCase>({
        {
            /*test_name=*/"TestUnspecifiedLikelihoodAndNoHoldback"
                          "ReturnsDefaultUI",
            /*holdback_probability=*/0,
            /*max_likely_threshold=*/0.5,
            /*should_expect_quiet_ui=*/false,
            /*expected_prediction_likelihood=*/kLikelihoodUnspecified,
        },
        {
            /*test_name=*/"TestUnspecifiedLikelihoodAndHoldback"
                          "ReturnsDefaultUI",
            /*holdback_probability=*/1,
            /*max_likely_threshold=*/0.5,
            /*should_expect_quiet_ui=*/false,
            /*expected_prediction_likelihood=*/kLikelihoodUnspecified,
        },
        {
            /*test_name=*/"TestVeryLikelyAndNoHoldback"
                          "ReturnsQuietUI",
            /*holdback_probability=*/0,
            /*max_likely_threshold=*/0.49,
            /*should_expect_quiet_ui=*/true,
            /*expected_prediction_likelihood=*/kLikelihoodVeryUnlikely,
        },
        {
            /*test_name=*/"TestVeryLikelyAndHoldback"
                          "ReturnsDefaultUI",
            /*holdback_probability=*/1,
            /*max_likely_threshold=*/0.49,
            /*should_expect_quiet_ui=*/false,
            /*expected_prediction_likelihood=*/kLikelihoodVeryUnlikely,
        },
    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        ParametrizedPredictionServiceBrowserTest::ParamType>& info) {
      return info.param.test_name;
    });

IN_PROC_BROWSER_TEST_P(ParametrizedPredictionServiceBrowserTest,
                       CheckHoldbackProbabilitiesForDifferentSignatureModels) {
  ASSERT_TRUE(prediction_model_handler());

  WebPermissionPredictionsModelMetadata metadata;
  std::string serialized_metadata;
  metadata.mutable_not_grant_thresholds()->set_max_likely(
      GetParam().max_likely_threshold);
  metadata.set_holdback_probability(GetParam().holdback_probability);
  metadata.SerializeToString(&serialized_metadata);

  auto any = std::make_optional<optimization_guide::proto::Any>();
  any->set_value(serialized_metadata);
  any->set_type_url(
      "type.googleapis.com/"
      "optimization_guide.protos.WebPermissionPredictionsModelMetadata");

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::
              OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(ModelFilePath())
              .SetModelMetadata(any)
              .Build());

  prediction_model_handler()->WaitForModelLoadForTesting();

  ASSERT_TRUE(embedded_test_server()->Start());

  // We need 4 prompts for the cpss to kick in on the next prompt.
  std::string test_urls[] = {"a.test", "b.test", "c.test", "d.test"};
  for (std::string test_url : test_urls) {
    TriggerPromptAndVerifyUI(test_url, PermissionAction::GRANTED,
                             /*should_expect_quiet_ui=*/false, std::nullopt);
  }
  TriggerPromptAndVerifyUI("e.test", PermissionAction::DISMISSED,
                           GetParam().should_expect_quiet_ui,
                           GetParam().expected_prediction_likelihood);
  EXPECT_EQ(5, bubble_factory()->show_count());
}

}  // namespace permissions
