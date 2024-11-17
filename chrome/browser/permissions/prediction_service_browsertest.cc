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
#include "components/permissions/prediction_service/prediction_model_handler_provider.h"
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

class PredictionServiceBrowserTest : public InProcessBrowserTest {
 public:
  PredictionServiceBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPermissionOnDeviceNotificationPredictions,
          {{feature_params::
                kPermissionOnDeviceNotificationPredictionsHoldbackChance.name,
            "0"}}},
         {optimization_guide::features::kOptimizationHints, {}},
         {optimization_guide::features::kRemoteOptimizationGuideFetching, {}},
         {features::kCpssUseTfliteSignatureRunner, {}}},
        {});
  }

  ~PredictionServiceBrowserTest() override = default;

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

base::FilePath& model_file_path() {
  static base::NoDestructor<base::FilePath> file_path([]() {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    return source_root_dir.AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("permissions")
        .AppendASCII("signature_model.tflite");
  }());
  return *file_path;
}

IN_PROC_BROWSER_TEST_F(PredictionServiceBrowserTest, PredictionServiceEnabled) {
  EXPECT_TRUE(prediction_model_handler());
}

IN_PROC_BROWSER_TEST_F(PredictionServiceBrowserTest,
                       SignatureModelReturnsLikely) {
  ASSERT_TRUE(prediction_model_handler());

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::
              OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  prediction_model_handler()->WaitForModelLoadForTesting();

  ASSERT_TRUE(embedded_test_server()->Start());

  // We need 4 prompts for the cpss to kick in on the next prompt.
  std::string test_urls[] = {"a.test", "b.test", "c.test", "d.test"};
  for (std::string test_url : test_urls) {
    TriggerPromptAndVerifyUI(test_url, PermissionAction::GRANTED,
                             /*should_expect_quiet_ui=*/false, std::nullopt);
  }
  TriggerPromptAndVerifyUI(
      "e.test", PermissionAction::DISMISSED,
      /*should_expect_quiet_ui=*/false,
      PermissionUmaUtil::PredictionGrantLikelihood::
          PermissionPrediction_Likelihood_DiscretizedLikelihood_DISCRETIZED_LIKELIHOOD_UNSPECIFIED);
  EXPECT_EQ(5, bubble_factory()->show_count());
}

}  // namespace permissions
