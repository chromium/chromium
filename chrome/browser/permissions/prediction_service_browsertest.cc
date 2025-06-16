// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_service.h"

#include <vector>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"
#include "chrome/browser/permissions/prediction_model_handler_provider.h"
#include "chrome/browser/permissions/prediction_model_handler_provider_factory.h"
#include "chrome/browser/permissions/prediction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/permissions_aiv3_handler.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace permissions {

namespace {
using ::base::TimeTicks;
using ::base::test::FeatureRef;
using ::base::test::FeatureRefAndParams;
using ::optimization_guide::proto::OptimizationTarget;
using ::permissions::GeneratePredictionsResponse;
using ::permissions::PermissionRequestRelevance;
using ::permissions::PermissionsAiv3Handler;
using ::permissions::PredictionRequestFeatures;
using ::permissions::PredictionService;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Combine;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Truly;
using ::testing::ValuesIn;
using ::testing::WithArg;
using ExperimentId = PredictionRequestFeatures::ExperimentId;

constexpr OptimizationTarget kCpssV1OptTargetNotification =
    OptimizationTarget::OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS;

constexpr OptimizationTarget kAiv3OptTargetNotification = OptimizationTarget::
    OPTIMIZATION_TARGET_NOTIFICATION_IMAGE_PERMISSION_RELEVANCE;

constexpr OptimizationTarget kAiv3OptTargetGeolocation = OptimizationTarget::
    OPTIMIZATION_TARGET_GEOLOCATION_IMAGE_PERMISSION_RELEVANCE;

constexpr auto kLikelihoodUnspecified =
    PermissionUmaUtil::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_DISCRETIZED_LIKELIHOOD_UNSPECIFIED;

// This is the only server side reply that will tirgger quiet UI at the moment.
constexpr auto kLikelihoodVeryUnlikely =
    PermissionUmaUtil::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;

constexpr std::string_view kNotificationsModelExecutionSuccessHistogram =
    "OptimizationGuide.ModelExecutor.ExecutionStatus.NotificationPermissionsV3";
constexpr std::string_view kGeolocationModelExecutionSuccessHistogram =
    "OptimizationGuide.ModelExecutor.ExecutionStatus.GeolocationPermissionsV3";
constexpr std::string_view kSnapshotTakenHistogram =
    "Permissions.AIv3.SnapshotTaken";
constexpr char kAIv3InquiryDurationHistogram[] =
    "Permissions.AIv3.InquiryDuration";
constexpr char kCpssV1InquiryDurationHistogram[] =
    "Permissions.OnDevicePredictionService.InquiryDuration";
constexpr char kCpssV3InquiryDurationHistogram[] =
    "Permissions.PredictionService.InquiryDuration";
// A CPSSv1 model that returns a constant value of 0.5;
// its meaning is defined by the max_likely threshold we use in the
// signature_model_executor to differentiate between
// 'very unlikely' and 'unspecified'.
constexpr std::string_view kZeroDotFiveReturnSignatureModel =
    "signature_model_ret_0.5.tflite";

// An AIv3 model that returns a constant value of 0 which will be converted into
// a 'very unlikely' for notifications and geolocation permission request.
constexpr std::string_view kZeroReturnAiv3Model = "aiv3_ret_0.tflite";

// An AIv3 model that returns a constant value of 1 which will be converted into
// a 'very likely' for notifications and geolocation permission request.
constexpr std::string_view kOneReturnAiv3Model = "aiv3_ret_1.tflite";

// Non existing model file.
constexpr std::string_view kNotExistingModel = "does_not_exist.tflite";

constexpr std::string kNeverHoldBackProbability = "0";
constexpr std::string kAlwaysHoldBackProbability = "1";

base::FilePath ModelFilePath(std::string_view file_name) {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("chrome")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("permissions")
      .AppendASCII(file_name);
}

class PredictionServiceMock : public PredictionService {
 public:
  PredictionServiceMock() : PredictionService(nullptr) {}
  MOCK_METHOD(void,
              StartLookup,
              (const PredictionRequestFeatures& entity,
               LookupRequestCallback request_callback,
               LookupResponseCallback response_callback),
              (override));
};

class PermissionsAiv3HandlerFake : public PermissionsAiv3Handler {
 public:
  PermissionsAiv3HandlerFake(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target,
      RequestType request_type)
      : PermissionsAiv3Handler(model_provider,
                               optimization_target,
                               request_type) {}

  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override {
    PermissionsAiv3Handler::OnModelUpdated(optimization_target, model_info);
    if (model_info.has_value()) {
      model_load_run_loop_for_testing_.Quit();
    }
  }

  void ExecuteModelWrapper(
      PermissionsAiv3Handler::ExecutionCallback callback,
      const std::optional<PermissionsAiv3Encoder::ModelOutput>& output) {
    std::move(callback).Run(output);
    model_execute_run_loop_for_testing_.Quit();
  }

  void ExecuteModel(
      PermissionsAiv3Handler::ExecutionCallback callback,
      std::unique_ptr<PermissionsAiv3Encoder::ModelInput> snapshot) override {
    PermissionsAiv3Handler::ExecuteModel(
        base::BindOnce(&PermissionsAiv3HandlerFake::ExecuteModelWrapper,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        std::move(snapshot));
  }

  void WaitForModelLoadForTesting() { model_load_run_loop_for_testing_.Run(); }
  void WaitForModelExecutionForTesting() {
    model_execute_run_loop_for_testing_.Run();
  }

 private:
  base::RunLoop model_execute_run_loop_for_testing_;
  base::RunLoop model_load_run_loop_for_testing_;
  base::WeakPtrFactory<PermissionsAiv3HandlerFake> weak_ptr_factory_{this};
};

MATCHER_P(PredictionRequestFeatureEq, expected, "") {
  using ActionCounts = PredictionRequestFeatures::ActionCounts;
  auto ActionCountsEq = [&](std::string_view name, const ActionCounts& expected,
                            const ActionCounts& got) {
    *result_listener << "\n";
    *result_listener << name << ": \n\t";
    auto match = ExplainMatchResult(
        AllOf(
            Field("grants", &ActionCounts::grants, expected.grants),

            Field("denies", &ActionCounts::denies, expected.denies),
            Field("dismissals", &ActionCounts::dismissals, expected.dismissals),

            Field("ignores", &ActionCounts::ignores, expected.ignores)),
        got, result_listener);
    *result_listener << "\n";
    return match;
  };

  return ExplainMatchResult(
      AllOf(Field("gesture", &PredictionRequestFeatures::gesture,
                  expected.gesture),
            Field("type", &PredictionRequestFeatures::type, expected.type),
            Field("requested_permission_counts",
                  &PredictionRequestFeatures::requested_permission_counts,
                  Truly([&](const auto& actual) {
                    return ActionCountsEq("requested_permission_counts",
                                          expected.requested_permission_counts,
                                          actual);
                  })),
            Field("all_permission_counts",
                  &PredictionRequestFeatures::all_permission_counts,
                  Truly([&](const auto& actual) {
                    return ActionCountsEq("all_permission_counts",
                                          expected.all_permission_counts,
                                          actual);
                  })),
            Field("url", &PredictionRequestFeatures::url, expected.url),
            Field("experiment_id", &PredictionRequestFeatures::experiment_id,
                  expected.experiment_id),
            Field("permission_relevance",
                  &PredictionRequestFeatures::permission_relevance,
                  expected.permission_relevance)),
      arg, result_listener);
}

PredictionRequestFeatures BuildRequestFeatures(
    RequestType request_type,
    ExperimentId experiment_id,
    PermissionRequestRelevance permission_relevance) {
  return PredictionRequestFeatures{
      .gesture = PermissionRequestGestureType::NO_GESTURE,
      .type = request_type,
      .requested_permission_counts = {},
      .all_permission_counts = {},
      .url = GURL("https://www.google.com"),
      .experiment_id = experiment_id,
      .permission_relevance = permission_relevance};
}
}  // namespace

class PredictionServiceBrowserTestBase : public InProcessBrowserTest {
 public:
  explicit PredictionServiceBrowserTestBase(
      const std::vector<FeatureRefAndParams>& enabled_features = {},
      const std::vector<FeatureRef>& disabled_features = {}) {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
    PredictionServiceFactory::GetInstance()->set_prediction_service_for_testing(
        &prediction_service_);
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

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  PredictionServiceMock& prediction_service() { return prediction_service_; }

  PredictionBasedPermissionUiSelector*
  prediction_based_permission_ui_selector() {
    return static_cast<PredictionBasedPermissionUiSelector*>(
        GetPermissionRequestManager()
            ->get_permission_ui_selectors_for_testing()
            .back()
            .get());
  }

  void TriggerPromptAndVerifyUi(
      std::string test_url,
      PermissionAction permission_action,
      RequestType request_type,
      bool should_expect_quiet_ui,
      std::optional<PermissionRequestRelevance> expected_relevance,
      std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          expected_prediction_likelihood) {
    auto* manager = GetPermissionRequestManager();
    GURL url = embedded_test_server()->GetURL(test_url, "/title1.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    auto req = std::make_unique<MockPermissionRequest>(request_type);
    manager->AddRequest(GetActiveMainFrame(), std::move(req));
    bubble_factory()->WaitForPermissionBubble();

    if (aiv3_model_handler_) {
      aiv3_model_handler_->WaitForModelExecutionForTesting();
    }
    EXPECT_EQ(should_expect_quiet_ui,
              manager->ShouldCurrentRequestUseQuietUI());
    EXPECT_EQ(expected_relevance,
              manager->permission_request_relevance_for_testing());
    EXPECT_EQ(expected_prediction_likelihood,
              manager->prediction_grant_likelihood_for_testing());
    if (permission_action == PermissionAction::DISMISSED) {
      manager->Dismiss();
    } else if (permission_action == PermissionAction::GRANTED) {
      manager->Accept();
    }
  }

 protected:
  OptimizationGuideKeyedService* opt_guide() {
    return OptimizationGuideKeyedServiceFactory::GetForProfile(
        browser()->profile());
  }

  raw_ptr<PermissionsAiv3HandlerFake> aiv3_model_handler_ = nullptr;

 private:
  std::unique_ptr<MockPermissionPromptFactory> mock_permission_prompt_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  PredictionServiceMock prediction_service_;
};

// ---------------------------------------------------------------------------
// ------------------- Prediction Service CPSSv3 Server Side -----------------
// ---------------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(PredictionServiceBrowserTestBase,
                       PredictionServiceEnabled) {
  EXPECT_TRUE(prediction_model_handler());
}

struct PredictionServiceHoldbackProbabilityTestCase {
  std::string test_name;
  std::string holdback_probability;
  bool should_expect_quiet_ui;
  PermissionUmaUtil::PredictionGrantLikelihood prediction_service_likelihood;
};

class PredictionServiceHoldbackBrowserTest
    : public PredictionServiceBrowserTestBase,
      public testing::WithParamInterface<
          PredictionServiceHoldbackProbabilityTestCase> {
 public:
  PredictionServiceHoldbackBrowserTest()
      : PredictionServiceBrowserTestBase(/*enabled_features=*/
                                         {
                                             {permissions::features::
                                                  kPermissionPredictionsV2,
                                              {{permissions::feature_params::
                                                    kPermissionPredictionsV2HoldbackChance
                                                        .name,
                                                GetParam()
                                                    .holdback_probability}}},
                                         },
                                         /*disabled_features=*/
                                         {permissions::features::
                                              kPermissionOnDeviceNotificationPredictions,
                                          permissions::features::
                                              kPermissionsAIv1,
                                          permissions::features::
                                              kPermissionsAIv3}) {}

  void SetUpOnMainThread() override {
    PredictionServiceBrowserTestBase::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }
};

INSTANTIATE_TEST_SUITE_P(
    PredictionServiceHoldbackTest,
    PredictionServiceHoldbackBrowserTest,
    ValuesIn<PredictionServiceHoldbackProbabilityTestCase>({
        {
            /*test_name=*/"TestUnspecifiedLikelihoodAndNoHoldback"
                          "ReturnsDefaultUI",
            /*holdback_probability=*/kNeverHoldBackProbability,
            /*should_expect_quiet_ui=*/false,
            /*prediction_service_likelihood=*/kLikelihoodUnspecified,
        },
        {
            /*test_name=*/"TestUnspecifiedLikelihoodAndHoldback"
                          "ReturnsDefaultUI",
            /*holdback_probability=*/kAlwaysHoldBackProbability,
            /*should_expect_quiet_ui=*/false,
            /*prediction_service_likelihood=*/kLikelihoodUnspecified,
        },
        {
            /*test_name=*/"TestVeryLikelyAndNoHoldback"
                          "ReturnsQuietUI",
            /*holdback_probability=*/kNeverHoldBackProbability,
            /*should_expect_quiet_ui=*/true,
            /*prediction_service_likelihood=*/kLikelihoodVeryUnlikely,
        },
        {
            /*test_name=*/"TestVeryLikelyAndHoldback"
                          "ReturnsDefaultUI",
            /*holdback_probability=*/kAlwaysHoldBackProbability,
            /*should_expect_quiet_ui=*/false,
            /*prediction_service_likelihood=*/kLikelihoodVeryUnlikely,
        },
    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        PredictionServiceHoldbackBrowserTest::ParamType>& info) {
      return info.param.test_name;
    });

IN_PROC_BROWSER_TEST_P(PredictionServiceHoldbackBrowserTest,
                       TestServerSideHoldbackWorkflow) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GeneratePredictionsResponse prediction_service_response;
  prediction_service_response.mutable_prediction()
      ->Add()
      ->mutable_grant_likelihood()
      ->set_discretized_likelihood(GetParam().prediction_service_likelihood);

  std::string test_url = "test.a";
  PredictionRequestFeatures expected_features = BuildRequestFeatures(
      RequestType::kNotifications, ExperimentId::kNoExperimentId,
      PermissionRequestRelevance::kUnspecified);
  EXPECT_CALL(prediction_service(),
              StartLookup(PredictionRequestFeatureEq(expected_features), _, _))
      .WillRepeatedly(WithArg<2>(Invoke(
          [&](PredictionService::LookupResponseCallback response_callback) {
            std::move(response_callback)
                .Run(/*lookup_successful=*/true,
                     /*response_from_cache=*/true, prediction_service_response);
          })));
  TriggerPromptAndVerifyUi(test_url, PermissionAction::DISMISSED,
                           RequestType::kNotifications,
                           GetParam().should_expect_quiet_ui,
                           /*expected_relevance=*/std::nullopt,
                           GetParam().prediction_service_likelihood);
}

// -----------------------------------------------------------------------------
// --------------------- Prediction Service On Device CPSSv1 -------------------
// -----------------------------------------------------------------------------

struct HoldbackProbabilityTestCase {
  std::string test_name;
  float holdback_probability;
  // At the moment, we define everything that the signature model returns that
  // is above that threshold as very unlikely, and everything below that
  // will return unspecified.
  float max_likely_threshold;
  bool should_expect_quiet_ui;
  std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
      expected_prediction_likelihood;
};

class SignatureModelPredictionServiceBrowserTest
    : public PredictionServiceBrowserTestBase,
      public testing::WithParamInterface<HoldbackProbabilityTestCase> {
 public:
  SignatureModelPredictionServiceBrowserTest()
      : PredictionServiceBrowserTestBase(/*enabled_features=*/
                                         {{features::
                                               kPermissionOnDeviceNotificationPredictions,
                                           {}},
                                          {optimization_guide::features::
                                               kOptimizationHints,
                                           {}},
                                          {optimization_guide::features::
                                               kRemoteOptimizationGuideFetching,
                                           {}},
                                          {features::
                                               kCpssUseTfliteSignatureRunner,
                                           {}}},
                                         /*disabled_features=*/
                                         {permissions::features::
                                              kPermissionsAIv1,
                                          permissions::features::
                                              kPermissionsAIv3}) {}

  void TriggerCpssV1AndVerifyUi(
      PermissionAction permission_action,
      bool should_expect_quiet_ui,
      std::optional<PermissionRequestRelevance> expected_relevance,
      std::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          expected_prediction_likelihood) {
    // We need 4 prompts for the CPSS to kick in on the next prompt.
    // This behaviour is defined by kRequestedPermissionMinimumHistoricalActions
    std::string test_urls[] = {"a.test", "b.test", "c.test", "d.test"};
    for (std::string test_url : test_urls) {
      TriggerPromptAndVerifyUi(test_url, PermissionAction::GRANTED,
                               RequestType::kNotifications,
                               /*should_expect_quiet_ui=*/false,
                               /*expected_relevance=*/std::nullopt,
                               /*expected_prediction_likelihood=*/std::nullopt);
    }
    TriggerPromptAndVerifyUi(/*test_url=*/"e.test", permission_action,
                             RequestType::kNotifications,
                             should_expect_quiet_ui, expected_relevance,
                             expected_prediction_likelihood);
    EXPECT_EQ(5, bubble_factory()->show_count());
  }
};

INSTANTIATE_TEST_SUITE_P(
    HoldbackProbabilityTest,
    SignatureModelPredictionServiceBrowserTest,
    ValuesIn<HoldbackProbabilityTestCase>({
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
        SignatureModelPredictionServiceBrowserTest::ParamType>& info) {
      return info.param.test_name;
    });

IN_PROC_BROWSER_TEST_P(SignatureModelPredictionServiceBrowserTest,
                       CheckHoldbackProbabilitiesForDifferentSignatureModels) {
  ASSERT_TRUE(prediction_model_handler());

  WebPermissionPredictionsModelMetadata metadata;
  std::string serialized_metadata;
  metadata.mutable_not_grant_thresholds()->set_max_likely(
      GetParam().max_likely_threshold);
  metadata.set_holdback_probability(GetParam().holdback_probability);
  metadata.set_version(2);
  metadata.SerializeToString(&serialized_metadata);

  auto any = std::make_optional<optimization_guide::proto::Any>();
  any->set_value(serialized_metadata);
  any->set_type_url(
      "type.googleapis.com/"
      "optimization_guide.protos.WebPermissionPredictionsModelMetadata");

  opt_guide()->OverrideTargetModelForTesting(
      kCpssV1OptTargetNotification,
      optimization_guide::TestModelInfoBuilder()
          .SetModelFilePath(ModelFilePath(kZeroDotFiveReturnSignatureModel))
          .SetModelMetadata(any)
          .Build());

  prediction_model_handler()->WaitForModelLoadForTesting();

  ASSERT_TRUE(embedded_test_server()->Start());

  TriggerCpssV1AndVerifyUi(PermissionAction::DISMISSED,
                           GetParam().should_expect_quiet_ui,
                           /*expected_relevance=*/std::nullopt,
                           GetParam().expected_prediction_likelihood);

  histogram_tester().ExpectTotalCount(kCpssV1InquiryDurationHistogram,
                                      /*expected_count=*/1);
}

// -----------------------------------------------------------------------------
// --------------- Prediction Service On Device Permissions AIv3 ---------------
// -----------------------------------------------------------------------------

struct ModelMetadata {
  std::string test_name;
  std::string_view model_name;
  // This is defined by the output of the AIv3 model (and the defined
  // thresholds). It will be used as input to the server-side model
  PermissionRequestRelevance expected_relevance;
  // This is the output of the server-side model (that we mock for this
  // test).
  // It should define the decision shared with the permission request
  // manager.
  PermissionUmaUtil::PredictionGrantLikelihood prediction_service_likelihood;
  bool should_expect_quiet_ui;
  int success_count_model_execution;
};

struct PermissionRequestMetadata {
  OptimizationTarget optimization_target;
  RequestType request_type;
};

using Aiv3ModelTestCase = std::tuple<ModelMetadata, PermissionRequestMetadata>;

class Aiv3ModelPredictionServiceBrowserTest
    : public PredictionServiceBrowserTestBase,
      public testing::WithParamInterface<Aiv3ModelTestCase> {
 public:
  Aiv3ModelPredictionServiceBrowserTest()
      : PredictionServiceBrowserTestBase(/*enabled_features=*/
                                         {
                                             {permissions::features::
                                                  kPermissionPredictionsV2,
                                              {{permissions::feature_params::
                                                    kPermissionPredictionsV2HoldbackChance
                                                        .name,
                                                "0"}}},
                                             {permissions::features::
                                                  kPermissionOnDeviceNotificationPredictions,
                                              {}},
                                             {permissions::features::
                                                  kPermissionOnDeviceGeolocationPredictions,
                                              {}},
                                             {::features::
                                                  kQuietNotificationPrompts,
                                              {}},
                                             {permissions::features::
                                                  kPermissionsAIv3,
                                              {}},
                                         },
                                         /*disabled_features=*/{}) {}

  RequestType request_type() const { return get<1>(GetParam()).request_type; }
  OptimizationTarget optimization_target() const {
    return get<1>(GetParam()).optimization_target;
  }

  void SetUpOnMainThread() override {
    PredictionServiceBrowserTestBase::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

    // Only one model_handler can be registered for the same optimization target
    // at the same time. Registering happens in the constructor, deregistering
    // in the destructor of each ModelHandler. We can either deregister
    // explicitly in the opt_guide service or just destroy the object. Either
    // way, we need to do this before we create our fake handler.
    model_handler_provider()->set_permissions_aiv3_handler_for_testing(
        request_type(), nullptr);

    std::unique_ptr<PermissionsAiv3HandlerFake> model_handler =
        std::make_unique<PermissionsAiv3HandlerFake>(
            opt_guide(), optimization_target(), request_type());
    aiv3_model_handler_ = model_handler.get();

    model_handler_provider()->set_permissions_aiv3_handler_for_testing(
        request_type(), std::move(model_handler));
  }

  void TearDownOnMainThread() override {
    PredictionServiceBrowserTestBase::TearDownOnMainThread();
    aiv3_model_handler_ = nullptr;
  }

  void PushModelFileToModelExecutor(const base::FilePath& model_file_path) {
    opt_guide()->OverrideTargetModelForTesting(
        optimization_target(), optimization_guide::TestModelInfoBuilder()
                                   .SetModelFilePath(model_file_path)
                                   .Build());
    aiv3_model_handler_->WaitForModelLoadForTesting();
  }

  PermissionsAiv3Handler* aiv3_model_handler() {
    return PredictionModelHandlerProviderFactory::GetForBrowserContext(
               browser()->profile())
        ->GetPermissionsAiv3Handler(request_type());
  }

 private:
  PredictionModelHandlerProvider* model_handler_provider() {
    return PredictionModelHandlerProviderFactory::GetForBrowserContext(
        browser()->profile());
  }
};

std::vector<ModelMetadata> model_data_testcase = {
    {
        /*test_name=*/"OnDeviceVeryLowAndServerSideUnspecifiedResponse"
                      "ReturnsDefaultUI",
        /*model_name=*/kZeroReturnAiv3Model,
        /*expected_relevance=*/PermissionRequestRelevance::kVeryLow,
        /*prediction_service_likelihood=*/kLikelihoodUnspecified,
        /*should_expect_quiet_ui=*/false,
        /*success_count_model_execution=*/1,
    },
    {
        /*test_name=*/"OnDeviceVeryLowAndServerSideVeryUnlikelyResponse"
                      "ReturnsQuietUI",
        /*model_name=*/kZeroReturnAiv3Model,
        /*expected_relevance=*/PermissionRequestRelevance::kVeryLow,
        /*prediction_service_likelihood=*/kLikelihoodVeryUnlikely,
        /*should_expect_quiet_ui=*/true,
        /*success_count_model_execution=*/1,
    },
    {
        /*test_name=*/"OnDeviceVeryHighAndServerSideUnspecifiedResponse"
                      "ReturnsDefaultUI",
        /*model_name=*/kOneReturnAiv3Model,
        /*expected_relevance=*/PermissionRequestRelevance::kVeryHigh,
        /*prediction_service_likelihood=*/kLikelihoodUnspecified,
        /*should_expect_quiet_ui=*/false,
        /*success_count_model_execution=*/1,
    },
    {
        /*test_name=*/"OnDeviceVeryHighAndServerSideVeryUnlikelyResponse"
                      "ReturnsQuietUI",
        /*model_name=*/kOneReturnAiv3Model,
        /*expected_relevance=*/PermissionRequestRelevance::kVeryHigh,
        /*prediction_service_likelihood=*/kLikelihoodVeryUnlikely,
        /*should_expect_quiet_ui=*/true,
        /*success_count_model_execution=*/1,
    },
    {
        /*test_name=*/"FailingAiv3ModelStillResultsInValid"
                      "ServerSideExecution",
        /*model_name=*/kNotExistingModel,
        /*expected_relevance=*/PermissionRequestRelevance::kUnspecified,
        /*prediction_service_likelihood=*/kLikelihoodVeryUnlikely,
        /*should_expect_quiet_ui=*/true,
        /*success_count_model_execution=*/0,
    },
};

std::vector<PermissionRequestMetadata> request_data_testcase = {
    {
        /*optimization_target=*/kAiv3OptTargetGeolocation,
        /*request_type=*/RequestType::kGeolocation},
    {
        /*optimization_target=*/kAiv3OptTargetNotification,
        /*request_type=*/RequestType::kNotifications},
};

INSTANTIATE_TEST_SUITE_P(
    Aiv3ModelTest,
    Aiv3ModelPredictionServiceBrowserTest,
    Combine(ValuesIn(model_data_testcase), ValuesIn(request_data_testcase)),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        Aiv3ModelPredictionServiceBrowserTest::ParamType>& info) {
      return (std::get<1>(info.param).request_type ==
                      RequestType::kNotifications
                  ? "Notification"
                  : "Geolocation") +
             std::get<0>(info.param).test_name;
    });

IN_PROC_BROWSER_TEST_P(Aiv3ModelPredictionServiceBrowserTest,
                       TestAiv3Workflow) {
  const auto& test_case = std::get<0>(GetParam());

  ASSERT_TRUE(aiv3_model_handler());
  PushModelFileToModelExecutor(ModelFilePath(test_case.model_name));
  ASSERT_TRUE(embedded_test_server()->Start());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 64);
  bitmap.eraseColor(SkColorSetRGB(0x1E, 0x1C, 0x0F));
  prediction_based_permission_ui_selector()->set_snapshot_for_testing(bitmap);

  GeneratePredictionsResponse prediction_service_response;
  prediction_service_response.mutable_prediction()
      ->Add()
      ->mutable_grant_likelihood()
      ->set_discretized_likelihood(test_case.prediction_service_likelihood);

  PredictionRequestFeatures expected_features =
      BuildRequestFeatures(request_type(), ExperimentId::kAiV3ExperimentId,
                           test_case.expected_relevance);
  EXPECT_CALL(prediction_service(),
              StartLookup(PredictionRequestFeatureEq(expected_features), _, _))
      .WillRepeatedly(WithArg<2>(Invoke(
          [&](PredictionService::LookupResponseCallback response_callback) {
            std::move(response_callback)
                .Run(/*lookup_successful=*/true,
                     /*response_from_cache=*/true, prediction_service_response);
          })));
  TriggerPromptAndVerifyUi(
      /*test_url=*/"test.a", PermissionAction::DISMISSED, request_type(),
      test_case.should_expect_quiet_ui, test_case.expected_relevance,
      test_case.prediction_service_likelihood);

  histogram_tester().ExpectBucketCount(
      request_type() == RequestType::kNotifications
          ? kNotificationsModelExecutionSuccessHistogram
          : kGeolocationModelExecutionSuccessHistogram,
      /*sample=*/true, /*expected_count=*/
      test_case.success_count_model_execution);

  histogram_tester().ExpectBucketCount(kSnapshotTakenHistogram,
                                       /*sample=*/true,
                                       /*expected_count=*/1);
  // We should receive timing information for both, the on-device model
  // and the server-side model.
  histogram_tester().ExpectTotalCount(kCpssV3InquiryDurationHistogram,
                                      /*expected_count=*/1);
  histogram_tester().ExpectTotalCount(kAIv3InquiryDurationHistogram,
                                      /*expected_count=*/1);
}

}  // namespace permissions
