// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_service.h"

#include <vector>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/permissions/prediction_service/language_detection_observer.h"
#include "chrome/browser/permissions/prediction_service/passage_embedder_delegate.h"
#include "chrome/browser/permissions/prediction_service/permissions_ai_ui_selector.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider_factory.h"
#include "chrome/browser/permissions/prediction_service/prediction_service_factory.h"
#include "chrome/browser/permissions/test/mock_passage_embedder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/passage_embeddings/passage_embeddings_test_util.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/permissions_aiv3_handler.h"
#include "components/permissions/prediction_service/prediction_model_handler.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "components/permissions/test/aivx_modelhandler_utils.h"
#include "components/permissions/test/enums_to_string.h"
#include "components/permissions/test/fake_permissions_aivx_modelhandlers.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace permissions {

namespace {
using ::base::TimeTicks;
using ::base::test::FeatureRef;
using ::base::test::FeatureRefAndParams;
using ::optimization_guide::proto::OptimizationTarget;
using ::passage_embeddings::ComputeEmbeddingsStatus;
using ::permissions::GeneratePredictionsResponse;
using ::permissions::LanguageDetectionStatus;
using ::permissions::PassageEmbedderDelegate;
using ::permissions::PermissionRequestRelevance;
using ::permissions::PermissionsAiv3Handler;
using ::permissions::PredictionRequestFeatures;
using ::permissions::PredictionService;
using ::test::BuildBitmap;
using ::test::DelayedPassageEmbedderMock;
using ::test::EmbedderMetadataProviderFake;
using ::test::PassageEmbedderMock;
using ::test::PermissionsAiv3HandlerFake;
using ::test::PermissionsAiv4HandlerFake;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Combine;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Truly;
using ::testing::ValuesIn;
using ::testing::WithArg;
using ::translate::LanguageDetectionDetails;
using ExperimentId = PredictionRequestFeatures::ExperimentId;

constexpr OptimizationTarget kCpssV1OptTargetNotification =
    OptimizationTarget::OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS;

constexpr OptimizationTarget kAiv3OptTargetNotification = OptimizationTarget::
    OPTIMIZATION_TARGET_NOTIFICATION_IMAGE_PERMISSION_RELEVANCE;

constexpr OptimizationTarget kAiv3OptTargetGeolocation = OptimizationTarget::
    OPTIMIZATION_TARGET_GEOLOCATION_IMAGE_PERMISSION_RELEVANCE;

constexpr OptimizationTarget kAiv4OptTargetNotification = OptimizationTarget::
    OPTIMIZATION_TARGET_PERMISSIONS_AIV4_NOTIFICATIONS_DESKTOP;

constexpr OptimizationTarget kAiv4OptTargetGeolocation = OptimizationTarget::
    OPTIMIZATION_TARGET_PERMISSIONS_AIV4_GEOLOCATION_DESKTOP;

constexpr auto kLikelihoodUnspecified =
    PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_DISCRETIZED_LIKELIHOOD_UNSPECIFIED;

constexpr auto kLikelihoodUnlikely =
    PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_UNLIKELY;

constexpr auto kLikelihoodLikely =
    PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_LIKELY;

constexpr std::string kNoHoldbackChance = "0";

// Just a meaningless color used to create snapshot dummies for the AIv3 and
// Aiv4 models.
constexpr SkColor kDefaultColor = SkColorSetRGB(0x1E, 0x1C, 0x0F);

// This is the only server side reply that will trigger quiet UI at the
// moment.
constexpr auto kLikelihoodVeryUnlikely =
    PermissionUiSelector::PredictionGrantLikelihood::
        PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_UNLIKELY;

constexpr char kCpssV1InquiryDurationHistogram[] =
    "Permissions.OnDevicePredictionService.InquiryDuration";
constexpr char kCpssV3InquiryDurationHistogram[] =
    "Permissions.PredictionService.InquiryDuration";
constexpr char kPredictionServiceTimeoutHistogram[] =
    "Permissions.PredictionService.Timeout";
constexpr char kTFLiteLibAvailableHistogram[] =
    "Permissions.PredictionService.TFLiteLibAvailable";
constexpr char kMSBBHistogram[] = "Permissions.PredictionService.MSBB";

// Aiv3 relevant histograms
constexpr std::string_view kAiv3NotificationsModelExecutionSuccessHistogram =
    "OptimizationGuide.ModelExecutor.ExecutionStatus."
    "NotificationPermissionsV3";
constexpr std::string_view kAiv3GeolocationModelExecutionSuccessHistogram =
    "OptimizationGuide.ModelExecutor.ExecutionStatus."
    "GeolocationPermissionsV3";
constexpr std::string_view kAiv3SnapshotTakenHistogram =
    "Permissions.AIv3.SnapshotTaken";
constexpr std::string_view kAiv3SnapshotTakenDurationHistogram =
    "Permissions.AIv3.SnapshotTakenDuration";
constexpr char kAIv3InquiryDurationHistogram[] =
    "Permissions.AIv3.InquiryDuration";
constexpr char kAIv3GeolocationHoldbackResponseHistogram[] =
    "Permissions.AIv3.Response.Geolocation";
constexpr char kAIv3NotificationsHoldbackResponseHistogram[] =
    "Permissions.AIv3.Response.Notifications";

// Aiv4 relevant histograms
constexpr std::string_view kAiv4NotificationsModelExecutionSuccessHistogram =
    "OptimizationGuide.ModelExecutor.ExecutionStatus."
    "PermissionsAiv4NotificationsDesktop";
constexpr std::string_view kAiv4GeolocationModelExecutionSuccessHistogram =
    "OptimizationGuide.ModelExecutor.ExecutionStatus."
    "PermissionsAiv4GeolocationDesktop";
constexpr std::string_view kAiv4SnapshotTakenHistogram =
    "Permissions.AIv4.SnapshotTaken";
constexpr std::string_view kAiv4SnapshotTakenDurationHistogram =
    "Permissions.AIv4.SnapshotTakenDuration";
constexpr char kAIv4InquiryDurationHistogram[] =
    "Permissions.AIv4.InquiryDuration";
constexpr char kAIv4GeolocationHoldbackResponseHistogram[] =
    "Permissions.AIv4.Response.Geolocation";
constexpr char kAIv4NotificationsHoldbackResponseHistogram[] =
    "Permissions.AIv4.Response.Notifications";
constexpr char kAiv4LanguageDetectionStatusHistogram[] =
    "Permissions.AIv4.LanguageDetectionStatus";
constexpr char kAiv4RenderedTextAcquireSuccessHistogram[] =
    "Permissions.AIv4.RenderedTextAcquireSuccess";
constexpr char kAiv4TryCancelPreviousEmbeddingsModelExecutionHistogram[] =
    "Permissions.AIv4.TryCancelPreviousEmbeddingsModelExecution";
constexpr char kAiv4FinishedPassageEmbeddingsTaskOutdatedHistogram[] =
    "Permissions.AIv4.FinishedPassageEmbeddingsTaskOutdated";
constexpr char kAiv4ComputeEmbeddingsStatusHistogram[] =
    "Permissions.AIv4.ComputeEmbeddingsStatus";
constexpr char kAiv4ComputeEmbeddingsDurationHistogram[] =
    "Permissions.AIv4.ComputeEmbeddingsDuration";
constexpr char kAiv4PassageEmbeddingsComputationTimeoutHistogram[] =
    "Permissions.AIv4.PassageEmbeddingsComputationTimeout";
constexpr char kAiv4EmbedderMetadataValidHistogram[] =
    "Permissions.AIv4.EmbedderMetadataValid";
constexpr char kAiv4NotificationsPermissionRequestRelevanceHistogram[] =
    "Permissions.AIv4.Notifications.PermissionRequestRelevance";
constexpr char kAiv4GeolocationPermissionRequestRelevanceHistogram[] =
    "Permissions.AIv4.Geolocation.PermissionRequestRelevance";
constexpr char kAiv4NotificationsRenderedTextSizeHistogram[] =
    "Permissions.AIv4.Notifications.RenderedTextSize";
constexpr char kAiv4GeolocationRenderedTextSizeHistogram[] =
    "Permissions.AIv4.Geolocation.RenderedTextSize";
// A CPSSv1 model that returns a constant value of 0.5;
// its meaning is defined by the max_likely threshold we use in the
// signature_model_executor to differentiate between
// 'very unlikely' and 'unspecified'.
constexpr std::string_view kZeroDotFiveReturnSignatureModel =
    "signature_model_ret_0.5.tflite";

// An AIvX model that returns a constant value of 0 which will be converted
// into a 'very unlikely' for notifications and geolocation permission
// request.
constexpr std::string_view kZeroReturnAiv3Model = "aiv3_ret_0.tflite";
constexpr std::string_view kZeroReturnAiv4Model = "aiv4_ret_0.tflite";

// An AIvX model that returns a constant value of 1 which will be converted
// into a 'very likely' for notifications and geolocation permission request.
constexpr std::string_view kOneReturnAiv3Model = "aiv3_ret_1.tflite";
constexpr std::string_view kOneReturnAiv4Model = "aiv4_ret_1.tflite";

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

class LanguageDetectionObserverFake : public LanguageDetectionObserver {
 public:
  LanguageDetectionObserverFake() = default;

  void Init(content::WebContents* web_contents,
            base::OnceCallback<void()> on_english_detected,
            base::OnceCallback<void()> on_fallback) override {
    LanguageDetectionObserver::Init(
        web_contents, std::move(on_english_detected), std::move(on_fallback));
    // Prevent real OnLanguageDetected events from messing with tests.
    LanguageDetectionObserver::RemoveAsObserver();
    init_run_loop_for_testing_.Quit();
  }

  void RemoveAsObserver() override {}

  void RunLoop() { init_run_loop_for_testing_.Run(); }

 private:
  base::RunLoop init_run_loop_for_testing_;
};

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

GeneratePredictionsResponse BuildPredictionServiceResponse(
    PermissionUiSelector::PredictionGrantLikelihood likelihood) {
  GeneratePredictionsResponse prediction_service_response;
  prediction_service_response.mutable_prediction()
      ->Add()
      ->mutable_grant_likelihood()
      ->set_discretized_likelihood(likelihood);
  return prediction_service_response;
}
}  // namespace

class PredictionServiceBrowserTestBase : public InProcessBrowserTest {
 public:
  explicit PredictionServiceBrowserTestBase(
      const std::vector<FeatureRefAndParams>& enabled_features = {},
      const std::vector<FeatureRef>& disabled_features = {
          permissions::features::kPermissionsAIv3,
          permissions::features::kPermissionsAIv4}) {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
    PredictionServiceFactory::GetInstance()->set_prediction_service_for_testing(
        &prediction_service_);
  }

  ~PredictionServiceBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    PermissionRequestManager* manager = permission_request_manager();
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

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* primary_main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  PermissionRequestManager* permission_request_manager() {
    return PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  MockPermissionPromptFactory* bubble_factory() {
    return mock_permission_prompt_factory_.get();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  PredictionServiceMock& prediction_service() { return prediction_service_; }

  PermissionsAiUiSelector* permissions_ai_ui_selector() {
    return static_cast<PermissionsAiUiSelector*>(
        permission_request_manager()
            ->get_permission_ui_selectors_for_testing()
            .back()
            .get());
  }

  virtual RequestType request_type() const {
    return RequestType::kNotifications;
  }

  PredictionModelHandlerProvider* model_handler_provider() {
    return PredictionModelHandlerProviderFactory::GetForBrowserContext(
        browser()->profile());
  }

  PredictionModelHandler* prediction_model_handler() {
    return model_handler_provider()->GetPredictionModelHandler(request_type());
  }

  PermissionsAiv3Handler* aiv3_model_handler() {
    return model_handler_provider()->GetPermissionsAiv3Handler(request_type());
  }

  PermissionsAiv4Handler* aiv4_model_handler() {
    return model_handler_provider()->GetPermissionsAiv4Handler(request_type());
  }

  ChromeTranslateClient* GetChromeTranslateClient() {
    return ChromeTranslateClient::FromWebContents(web_contents());
  }

  void SetTranslateSourceLanguage(const std::string& language) {
    GetChromeTranslateClient()
        ->GetTranslateManager()
        ->GetLanguageState()
        ->SetSourceLanguage(language);
  }

  void TriggerPromptAndWaitForBubble(
      std::string test_url,
      std::string translate_source_language = "en") {
    auto* manager = permission_request_manager();
    GURL url = embedded_test_server()->GetURL(test_url, "/title1.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    SetTranslateSourceLanguage(translate_source_language);

    auto req = std::make_unique<MockPermissionRequest>(request_type());
    manager->AddRequest(primary_main_frame(), std::move(req));

    WaitForModelExecutionIfNecessary();

    bubble_factory()->WaitForPermissionBubble();
  }

  void TriggerPromptAndVerifyUi(
      std::string test_url,
      PermissionAction permission_action,
      bool should_expect_quiet_ui,
      std::optional<PermissionRequestRelevance> expected_relevance,
      std::optional<PermissionUiSelector::PredictionGrantLikelihood>
          expected_prediction_likelihood,
      std::string translate_source_language = "en") {
    TriggerPromptAndWaitForBubble(test_url, translate_source_language);

    auto* manager = permission_request_manager();
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
  raw_ptr<PermissionsAiv4HandlerFake> aiv4_model_handler_ = nullptr;

 private:
  virtual void WaitForModelExecutionIfNecessary() {
    if (aiv3_model_handler_) {
      aiv3_model_handler_->WaitForModelExecutionForTesting();
    }
    if (aiv4_model_handler_) {
      aiv4_model_handler_->WaitForModelExecutionForTesting();
    }
  }

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
  EXPECT_FALSE(aiv3_model_handler());
  EXPECT_FALSE(aiv4_model_handler());
  EXPECT_TRUE(prediction_model_handler());
}

struct PredictionServiceHoldbackProbabilityTestCase {
  std::string test_name;
  std::string holdback_probability;
  bool should_expect_quiet_ui;
  PermissionUiSelector::PredictionGrantLikelihood prediction_service_likelihood;
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
                                              kPermissionsAIv3,
                                          permissions::features::
                                              kPermissionsAIv4,
                                          permissions::features::
                                              kPermissionsAIP92}) {}

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
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ASSERT_TRUE(embedded_test_server()->Start());

  GeneratePredictionsResponse prediction_service_response =
      BuildPredictionServiceResponse(GetParam().prediction_service_likelihood);

  std::string test_url = "test.a";
  PredictionRequestFeatures expected_features = BuildRequestFeatures(
      RequestType::kNotifications, ExperimentId::kNoExperimentId,
      PermissionRequestRelevance::kUnspecified);
  EXPECT_CALL(prediction_service(),
              StartLookup(PredictionRequestFeatureEq(expected_features), _, _))
      .WillRepeatedly(WithArg<2>(
          [&](PredictionService::LookupResponseCallback response_callback) {
            std::move(response_callback)
                .Run(/*lookup_successful=*/true,
                     /*response_from_cache=*/true, prediction_service_response);
          }));

  TriggerPromptAndVerifyUi(test_url, PermissionAction::DISMISSED,
                           GetParam().should_expect_quiet_ui,
                           /*expected_relevance=*/std::nullopt,
                           GetParam().prediction_service_likelihood);

  EXPECT_EQ(std::nullopt,
            permissions_ai_ui_selector()->PermissionAiRelevanceModelForUKM());

  histogram_tester().ExpectUniqueSample(kPredictionServiceTimeoutHistogram,
                                        false, 1);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Permission::kEntryName);
  ASSERT_FALSE(entries.empty());
  const auto* entry = entries.back().get();
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::Permission::kPermissionAiRelevanceModelName));
}

IN_PROC_BROWSER_TEST_P(PredictionServiceHoldbackBrowserTest,
                       TestOverallTimeout) {
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  EXPECT_CALL(prediction_service(), StartLookup(_, _, _))
      .WillOnce(WithArg<2>(
          [&](PredictionService::LookupResponseCallback response_callback) {
            task_runner->FastForwardBy(base::Seconds(
                PermissionsAiUiSelector::kPermissionRequestUiDecisionTimeout));
          }));

  ASSERT_TRUE(embedded_test_server()->Start());

  TriggerPromptAndVerifyUi(
      /*test_url=*/"test.a", PermissionAction::DISMISSED,
      /*should_expect_quiet_ui=*/false,
      /*expected_relevance=*/std::nullopt,
      /*expected_prediction_likelihood=*/std::nullopt);

  histogram_tester().ExpectUniqueSample(kPredictionServiceTimeoutHistogram,
                                        true, 1);
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
  std::optional<PermissionUiSelector::PredictionGrantLikelihood>
      expected_prediction_likelihood;
};

class SignatureModelPredictionServiceBrowserTest
    : public PredictionServiceBrowserTestBase,
      public testing::WithParamInterface<HoldbackProbabilityTestCase> {
 public:
  SignatureModelPredictionServiceBrowserTest()
      : PredictionServiceBrowserTestBase(/*enabled_features=*/
                                         {{optimization_guide::features::
                                               kOptimizationHints,
                                           {}},
                                          {features::
                                               kCpssUseTfliteSignatureRunner,
                                           {}}},
                                         /*disabled_features=*/
                                         {permissions::features::
                                              kPermissionsAIv3,
                                          permissions::features::
                                              kPermissionsAIv4,
                                          permissions::features::
                                              kPermissionsAIP92}) {}

  void TriggerCpssV1AndVerifyUi(
      PermissionAction permission_action,
      bool should_expect_quiet_ui,
      std::optional<PermissionRequestRelevance> expected_relevance,
      std::optional<PermissionUiSelector::PredictionGrantLikelihood>
          expected_prediction_likelihood) {
    // We need 4 prompts for the CPSS to kick in on the next prompt.
    // This behaviour is defined by
    // kRequestedPermissionMinimumHistoricalActions
    std::string test_urls[] = {"a.test", "b.test", "c.test", "d.test"};
    for (std::string test_url : test_urls) {
      TriggerPromptAndVerifyUi(test_url, PermissionAction::GRANTED,
                               /*should_expect_quiet_ui=*/false,
                               /*expected_relevance=*/std::nullopt,
                               /*expected_prediction_likelihood=*/std::nullopt);
    }
    TriggerPromptAndVerifyUi(/*test_url=*/"e.test", permission_action,
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
  // Because of the action history we need to trigger the CPSSv1 model we expect
  // 5 records here.
  histogram_tester().ExpectUniqueSample(kPredictionServiceTimeoutHistogram,
                                        false, 5);
}

// -----------------------------------------------------------------------------
// --------------- Prediction Service On Device Permissions AIv3 ---------------
// -----------------------------------------------------------------------------
// Since AivX models will call the server side mock in the end, we need to
// prevent holdback from suppressing the result of model evaluation randomly.
// For this we set holdback chance to 0 (no holdback).
#define CONFIGURE_NO_HOLDBACK_CHANCE                                        \
  {                                                                         \
    permissions::features::kPermissionPredictionsV2, {                      \
      {                                                                     \
        permissions::feature_params::kPermissionPredictionsV2HoldbackChance \
            .name,                                                          \
            kNoHoldbackChance                                               \
      }                                                                     \
    }                                                                       \
  }

template <class AivXHandler>
class AivXModelPredictionServiceBrowserTest
    : public PredictionServiceBrowserTestBase {
 public:
  AivXModelPredictionServiceBrowserTest(
      const std::vector<FeatureRefAndParams>& enabled_features,
      const std::vector<FeatureRef>& disabled_features)
      : PredictionServiceBrowserTestBase(enabled_features, disabled_features) {}

  virtual OptimizationTarget optimization_target() = 0;
  virtual AivXHandler* model_handler() = 0;
  virtual void set_model_handler(AivXHandler* handler) = 0;

  virtual void UpdateAivXHandlerInModelProvider(
      std::unique_ptr<AivXHandler> handler) = 0;

  void SetUpOnMainThread() override {
    PredictionServiceBrowserTestBase::SetUpOnMainThread();

    // AIvX model workflows end with calling the CPSSv3 server side model,
    // providing it with the additional AIvX permission relevance field. Because
    // of this we only provide those workflows to users that agreed to data
    // collection.
    browser()->profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

    // Only one model_handler can be registered for the same optimization
    // target at the same time. Registering happens in the constructor,
    // deregistering in the destructor of each ModelHandler. We therefore
    // destroy the object kept in the ModelHandlerProvider class, before we
    // create our fake handler.
    UpdateAivXHandlerInModelProvider(nullptr);

    std::unique_ptr<AivXHandler> model_handler = std::make_unique<AivXHandler>(
        opt_guide(), optimization_target(), request_type());
    set_model_handler(model_handler.get());

    UpdateAivXHandlerInModelProvider(std::move(model_handler));
  }

  void TearDownOnMainThread() override {
    PredictionServiceBrowserTestBase::TearDownOnMainThread();
    set_model_handler(nullptr);
  }

  void PushModelFileToModelExecutor(const base::FilePath& model_file_path) {
    opt_guide()->OverrideTargetModelForTesting(
        optimization_target(), optimization_guide::TestModelInfoBuilder()
                                   .SetModelFilePath(model_file_path)
                                   .Build());
    model_handler()->WaitForModelLoadForTesting();
  }

  // We do not test screenshot handling here; this is so the code does not fail.
  void set_dummy_screenshot_for_testing() {
    permissions_ai_ui_selector()->set_snapshot_for_testing(
        BuildBitmap(64, 64, kDefaultColor));
  }

  // We do not test inner text content extraction here; this is so the code does
  // not fail.
  void set_dummy_inner_text_for_testing(
      std::string inner_text =
          "dummy text that is more than min length characters long") {
    permissions_ai_ui_selector()->set_inner_text_for_testing(
        {.inner_text = std::move(inner_text)});
  }
};

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
  PermissionUiSelector::PredictionGrantLikelihood prediction_service_likelihood;
  bool should_expect_quiet_ui;
  int success_count_model_execution;
};

struct PermissionRequestMetadata {
  OptimizationTarget optimization_target;
  RequestType request_type;
};

using Aiv3ModelTestCase = std::tuple<ModelMetadata, PermissionRequestMetadata>;

class Aiv3ModelPredictionServiceBrowserTest
    : public AivXModelPredictionServiceBrowserTest<PermissionsAiv3HandlerFake>,
      public testing::WithParamInterface<Aiv3ModelTestCase> {
 public:
  Aiv3ModelPredictionServiceBrowserTest()
      : AivXModelPredictionServiceBrowserTest(/*enabled_features=*/
                                              {
                                                  CONFIGURE_NO_HOLDBACK_CHANCE,
                                                  {permissions::features::
                                                       kPermissionsAIv3,
                                                   {}},
                                              }, /*disabled_features=*/
                                              {permissions::features::
                                                   kPermissionsAIv4,
                                               permissions::features::
                                                   kPermissionsAIP92}) {}

  RequestType request_type() const override {
    return get<1>(GetParam()).request_type;
  }

  OptimizationTarget optimization_target() override {
    return get<1>(GetParam()).optimization_target;
  }

  void UpdateAivXHandlerInModelProvider(
      std::unique_ptr<PermissionsAiv3HandlerFake> handler) override {
    model_handler_provider()->set_permissions_aiv3_handler_for_testing(
        request_type(), std::move(handler));
  }

  PermissionsAiv3HandlerFake* model_handler() override {
    return aiv3_model_handler_;
  }

  void set_model_handler(PermissionsAiv3HandlerFake* handler) override {
    aiv3_model_handler_ = handler;
  }
};

std::vector<ModelMetadata> aiv3_model_data_testcase = {
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

std::vector<PermissionRequestMetadata> aiv3_request_data_testcase = {
    {/*optimization_target=*/kAiv3OptTargetGeolocation,
     /*request_type=*/RequestType::kGeolocation},
    {/*optimization_target=*/kAiv3OptTargetNotification,
     /*request_type=*/RequestType::kNotifications},
};

INSTANTIATE_TEST_SUITE_P(
    Aiv3ModelTest,
    Aiv3ModelPredictionServiceBrowserTest,
    Combine(ValuesIn(aiv3_model_data_testcase),
            ValuesIn(aiv3_request_data_testcase)),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        Aiv3ModelPredictionServiceBrowserTest::ParamType>& info) {
      return base::StrCat({test::ToString(std::get<1>(info.param).request_type),
                           std::get<0>(info.param).test_name});
    });

IN_PROC_BROWSER_TEST_P(Aiv3ModelPredictionServiceBrowserTest,
                       Aiv3ModelHandlerDefined) {
  EXPECT_TRUE(aiv3_model_handler());
}

IN_PROC_BROWSER_TEST_P(Aiv3ModelPredictionServiceBrowserTest,
                       TestAiv3Workflow) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ASSERT_TRUE(aiv3_model_handler());

  const auto& test_case = std::get<0>(GetParam());

  PushModelFileToModelExecutor(ModelFilePath(test_case.model_name));
  ASSERT_TRUE(embedded_test_server()->Start());

  set_dummy_screenshot_for_testing();

  GeneratePredictionsResponse prediction_service_response =
      BuildPredictionServiceResponse(test_case.prediction_service_likelihood);

  PredictionRequestFeatures expected_features =
      BuildRequestFeatures(request_type(), ExperimentId::kAiV3ExperimentId,
                           test_case.expected_relevance);
  EXPECT_CALL(prediction_service(),
              StartLookup(PredictionRequestFeatureEq(expected_features), _, _))
      .WillRepeatedly(WithArg<2>(
          [&](PredictionService::LookupResponseCallback response_callback) {
            std::move(response_callback)
                .Run(/*lookup_successful=*/true,
                     /*response_from_cache=*/true, prediction_service_response);
          }));
  TriggerPromptAndVerifyUi(
      /*test_url=*/"test.a", PermissionAction::DISMISSED,
      test_case.should_expect_quiet_ui, test_case.expected_relevance,
      test_case.prediction_service_likelihood);

  EXPECT_EQ(permissions::PermissionAiRelevanceModel::kAIv3,
            permissions_ai_ui_selector()->PermissionAiRelevanceModelForUKM());

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Permission::kEntryName);
  ASSERT_FALSE(entries.empty());
  const auto* entry = entries.back().get();
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::Permission::kPermissionAiRelevanceModelName,
      static_cast<int64_t>(permissions::PermissionAiRelevanceModel::kAIv3));

  histogram_tester().ExpectBucketCount(
      request_type() == RequestType::kNotifications
          ? kAiv3NotificationsModelExecutionSuccessHistogram
          : kAiv3GeolocationModelExecutionSuccessHistogram,
      /*sample=*/true, /*expected_count=*/
      test_case.success_count_model_execution);

  histogram_tester().ExpectBucketCount(kTFLiteLibAvailableHistogram,
                                       /*sample=*/true,
                                       /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(kAiv3SnapshotTakenHistogram,
                                       /*sample=*/true,
                                       /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(kMSBBHistogram,
                                       /*sample=*/true,
                                       /*expected_count=*/1);
  histogram_tester().ExpectTotalCount(kAiv3SnapshotTakenDurationHistogram,
                                      /*expected_count=*/1);
  // We should receive timing information for both, the on-device model
  // and the server-side model.
  histogram_tester().ExpectTotalCount(kCpssV3InquiryDurationHistogram,
                                      /*expected_count=*/1);
  histogram_tester().ExpectTotalCount(kAIv3InquiryDurationHistogram,
                                      /*expected_count=*/1);

  histogram_tester().ExpectBucketCount(
      request_type() == RequestType::kNotifications
          ? kAIv3NotificationsHoldbackResponseHistogram
          : kAIv3GeolocationHoldbackResponseHistogram,
      /*sample=*/false, /*expected_count=*/1);

  histogram_tester().ExpectUniqueSample(kPredictionServiceTimeoutHistogram,
                                        false, 1);
}

// -----------------------------------------------------------------------------
// --------------- Prediction Service On Device Permissions AIv4 ---------------
// -----------------------------------------------------------------------------

class Aiv4ModelPredictionServiceBrowserTestBase
    : public AivXModelPredictionServiceBrowserTest<PermissionsAiv4HandlerFake> {
 public:
  Aiv4ModelPredictionServiceBrowserTestBase()
      : AivXModelPredictionServiceBrowserTest(/*enabled_features=*/
                                              {
                                                  CONFIGURE_NO_HOLDBACK_CHANCE,
                                                  {permissions::features::
                                                       kPermissionsAIv3,
                                                   {}},
                                                  {permissions::features::
                                                       kPermissionsAIv4,
                                                   {}},
                                              }, /*disabled_features=*/
                                              {permissions::features::
                                                   kPermissionsAIP92}) {}

  void SetUpOnMainThread() override {
    AivXModelPredictionServiceBrowserTest<
        PermissionsAiv4HandlerFake>::SetUpOnMainThread();

    // Required to preprocess the inner_text string as input for AIv4.
    model_handler_provider()->set_passage_embedder_for_testing(
        &passage_embedder_);
    passage_embedder_.set_status(
        passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

    embedder_metadata_provider_fake.AddObserver(model_handler_provider());
    embedder_metadata_provider_fake.NotifyObservers(
        EmbedderMetadataProviderFake::GetValidEmbedderMetadata());
  }

  void TearDownOnMainThread() override {
    AivXModelPredictionServiceBrowserTest<
        PermissionsAiv4HandlerFake>::TearDownOnMainThread();
    embedder_metadata_provider_fake.RemoveObserver(model_handler_provider());
  }

  RequestType request_type() const override {
    return RequestType::kNotifications;
  }

  OptimizationTarget optimization_target() override {
    return kAiv4OptTargetNotification;
  }

  void UpdateAivXHandlerInModelProvider(
      std::unique_ptr<PermissionsAiv4HandlerFake> handler) override {
    model_handler_provider()->set_permissions_aiv4_handler_for_testing(
        request_type(), std::move(handler));
  }

  PermissionsAiv4HandlerFake* model_handler() override {
    return aiv4_model_handler_;
  }

  void set_model_handler(PermissionsAiv4HandlerFake* handler) override {
    aiv4_model_handler_ = handler;
  }

  EmbedderMetadataProviderFake embedder_metadata_provider_fake;

 private:
  PassageEmbedderMock passage_embedder_;
};

IN_PROC_BROWSER_TEST_F(Aiv4ModelPredictionServiceBrowserTestBase,
                       Aiv4ModelHandlerDefined) {
  // If AIv4 flag is defined, no other AIvX model should get initialized.
  EXPECT_FALSE(aiv3_model_handler());
  EXPECT_TRUE(aiv4_model_handler());
}

struct Aiv4ModelLanguageDetectionTestCase {
  std::string test_name;
  std::string immediate_page_language;
  std::string delayed_page_language;
  LanguageDetectionStatus expected_status;
  std::optional<PermissionRequestRelevance> expected_relevance;
};

class Aiv4ModelLanguageDetectionBrowserTest
    : public Aiv4ModelPredictionServiceBrowserTestBase,
      public testing::WithParamInterface<Aiv4ModelLanguageDetectionTestCase> {
 public:
  Aiv4ModelLanguageDetectionBrowserTest() = default;

  void SetUpOnMainThread() override {
    Aiv4ModelPredictionServiceBrowserTestBase::SetUpOnMainThread();
    auto language_detection_observer =
        std::make_unique<LanguageDetectionObserverFake>();
    language_detection_observer_ = language_detection_observer.get();
    permissions_ai_ui_selector()->set_language_detection_observer_for_testing(
        std::move(language_detection_observer));
  }

  void TearDownOnMainThread() override {
    Aiv4ModelPredictionServiceBrowserTestBase::TearDownOnMainThread();
    // Avoid dangling ptr warning.
    language_detection_observer_ = nullptr;
  }

  void WaitForModelExecutionIfNecessary() override {
    language_detection_observer_->RunLoop();
    if (language_detection_observer_->WaitingForLanguageDetection() &&
        !GetParam().delayed_page_language.empty()) {
      LanguageDetectionDetails details;
      details.adopted_language = GetParam().delayed_page_language;
      language_detection_observer_->OnLanguageDetermined(details);
    }

    if (GetParam().expected_relevance != std::nullopt) {
      aiv4_model_handler_->WaitForModelExecutionForTesting();
    }
  }
  raw_ptr<LanguageDetectionObserverFake> language_detection_observer_;
};

INSTANTIATE_TEST_SUITE_P(
    Aiv4ModelLanguageDetectionTest,
    Aiv4ModelLanguageDetectionBrowserTest,
    ValuesIn<Aiv4ModelLanguageDetectionTestCase>(
        {{
             /*test_name=*/"TimeoutDuringLanguageDetection",
             /*immediate_page_language=*/"",
             /*delayed_page_language=*/"",
             /*expected_status=*/LanguageDetectionStatus::kNoResultDueToTimeout,
             /*expected_relevance=*/std::nullopt,
         },
         {
             /*test_name=*/"EnglishDetectedImmediately",
             /*immediate_page_language=*/"en",
             /*delayed_page_language=*/"",
             /*expected_status=*/
             LanguageDetectionStatus::kImmediatelyAvailableEnglish,
             /*expected_relevance=*/PermissionRequestRelevance::kVeryHigh,
         },
         {
             /*test_name=*/"NoEnglishDetectedImmediately",
             /*immediate_page_language=*/"de-DE",
             /*delayed_page_language=*/"",
             /*expected_status=*/
             LanguageDetectionStatus::kImmediatelyAvailableNotEnglish,
             /*expected_relevance=*/std::nullopt,
         },
         {
             /*test_name=*/"NoEnglishDetectedDelayed",
             /*immediate_page_language=*/"",
             /*delayed_page_language=*/"de-DE",
             /*expected_status=*/
             LanguageDetectionStatus::kDelayedDetectedNotEnglish,
             /*expected_relevance=*/std::nullopt,
         },
         {
             /*test_name=*/"EnglishDetectedDelayed",
             /*immediate_page_language=*/"",
             /*delayed_page_language=*/"en-GB",
             /*expected_status=*/
             LanguageDetectionStatus::kDelayedDetectedEnglish,
             /*expected_relevance=*/PermissionRequestRelevance::kVeryHigh,
         }}),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        Aiv4ModelLanguageDetectionBrowserTest::ParamType>& info) {
      return info.param.test_name;
    });

IN_PROC_BROWSER_TEST_P(Aiv4ModelLanguageDetectionBrowserTest,
                       CheckLanguageDetectionHistogramStatus) {
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  ASSERT_TRUE(aiv4_model_handler());
  ASSERT_TRUE(embedded_test_server()->Start());
  PushModelFileToModelExecutor(ModelFilePath(kOneReturnAiv4Model));

  GeneratePredictionsResponse prediction_service_response =
      BuildPredictionServiceResponse(kLikelihoodVeryUnlikely);

  EXPECT_CALL(prediction_service(), StartLookup(_, _, _))
      .WillRepeatedly(WithArg<2>(
          [&](PredictionService::LookupResponseCallback response_callback) {
            std::move(response_callback)
                .Run(/*lookup_successful=*/true,
                     /*response_from_cache=*/true, prediction_service_response);
          }));

  set_dummy_screenshot_for_testing();
  set_dummy_inner_text_for_testing();

  TriggerPromptAndVerifyUi(
      /*test_url=*/"test.a", PermissionAction::DISMISSED,
      /*should_expect_quiet_ui=*/true, GetParam().expected_relevance,
      /*expected_prediction_likelihood=*/kLikelihoodVeryUnlikely,
      /*translate_source_language=*/
      GetParam().immediate_page_language);

  // This will speed up the test in case of a timeout.
  task_runner->FastForwardBy(
      base::Seconds(LanguageDetectionObserver::kLanguageDetectionTimeout));

  histogram_tester().ExpectBucketCount(kAiv4LanguageDetectionStatusHistogram,
                                       /*sample=*/GetParam().expected_status,
                                       /*expected_count=*/1);

  histogram_tester().ExpectUniqueSample(kPredictionServiceTimeoutHistogram,
                                        false, 1);

  // Avoid dangling raw_ptr warning:
  model_handler_provider()->set_passage_embedder_for_testing(nullptr);
}

struct Aiv4ModelFailureTestCase {
  std::string test_name;
  std::string page_language;
  std::string inner_text;
  SkBitmap snapshot;
  ComputeEmbeddingsStatus compute_embeddings_status;
  std::optional<PassageEmbedderMock> passage_embedder;
  passage_embeddings::EmbedderMetadata embedder_metadata;
};

class Aiv4ModelFailureBrowserTest
    : public Aiv4ModelPredictionServiceBrowserTestBase,
      public testing::WithParamInterface<Aiv4ModelFailureTestCase> {
 public:
  Aiv4ModelFailureBrowserTest() = default;

  void WaitForModelExecutionIfNecessary() override {
    // This test will not start any model execution.
  }
};

// Each of the testcases targets a different point of failure and we
// want all of them to get handled gracefully by skipping on-device
// model execution and just calling CPSSv3 server side model without
// permission relevance calculated by the on-device model.
INSTANTIATE_TEST_SUITE_P(
    Aiv4ModelFailureTest,
    Aiv4ModelFailureBrowserTest,
    ValuesIn<Aiv4ModelFailureTestCase>({
        {
            /*test_name=*/"NoScreenshotAvailable",
            /*page_language=*/"en",
            /*inner_text=*/"some valid text for aiv4 model",
            /*snapshot=*/SkBitmap(),
            /*compute_embeddings_status=*/
            ComputeEmbeddingsStatus::kSuccess,
            /*passage_embedder=*/PassageEmbedderMock(),
            /*embedder_metadata=*/
            EmbedderMetadataProviderFake::GetValidEmbedderMetadata(),
        },
        {
            /*test_name=*/"EmptyInnerText",
            /*page_language=*/"en-GB",
            /*inner_text=*/"",
            /*snapshot=*/BuildBitmap(64, 64, kDefaultColor),
            /*compute_embeddings_status=*/
            ComputeEmbeddingsStatus::kSuccess,
            /*passage_embedder=*/PassageEmbedderMock(),
            /*embedder_metadata=*/
            EmbedderMetadataProviderFake::GetValidEmbedderMetadata(),
        },
        {
            /*test_name=*/"EmbedderModelFails",
            /*page_language=*/"en-US",
            /*inner_text=*/"some valid text for aiv4 model",
            /*snapshot=*/BuildBitmap(64, 64, kDefaultColor),
            /*compute_embeddings_status=*/
            ComputeEmbeddingsStatus::kExecutionFailure,
            /*passage_embedder=*/PassageEmbedderMock(),
            /*embedder_metadata=*/
            EmbedderMetadataProviderFake::GetValidEmbedderMetadata(),
        },
        {
            /*test_name=*/"EmbedderModelDoesNotExist",
            /*page_language=*/"en-VU",
            /*inner_text=*/"some valid text for aiv4 model",
            /*snapshot=*/BuildBitmap(64, 64, kDefaultColor),
            /*compute_embeddings_status=*/
            ComputeEmbeddingsStatus::kSuccess,
            /*passage_embedder=*/std::nullopt,
            /*embedder_metadata=*/
            EmbedderMetadataProviderFake::GetValidEmbedderMetadata(),
        },
        {
            /*test_name=*/"PageIsNotInEnglish",
            /*page_language=*/"de-DE",
            /*inner_text=*/"some valid text for aiv4 model",
            /*snapshot=*/BuildBitmap(64, 64, kDefaultColor),
            /*compute_embeddings_status=*/
            ComputeEmbeddingsStatus::kSuccess,
            /*passage_embedder=*/PassageEmbedderMock(),
            /*embedder_metadata=*/
            EmbedderMetadataProviderFake::GetValidEmbedderMetadata(),
        },
        {
            /*test_name=*/"InvalidEmbedderMetadata",
            /*page_language=*/"en",
            /*inner_text=*/"some valid text for aiv4 model",
            /*snapshot=*/BuildBitmap(64, 64, kDefaultColor),
            /*compute_embeddings_status=*/
            ComputeEmbeddingsStatus::kSuccess,
            /*passage_embedder=*/PassageEmbedderMock(),
            /*embedder_metadata=*/
            EmbedderMetadataProviderFake::GetInvalidEmbedderMetadata(),
        },
    }), /*name_generator=*/
    [](const testing::TestParamInfo<Aiv4ModelFailureBrowserTest::ParamType>&
           info) { return info.param.test_name; });

IN_PROC_BROWSER_TEST_P(Aiv4ModelFailureBrowserTest,
                       ShouldCallCPSSv3ModelWithoutRelevance) {
  ASSERT_TRUE(aiv4_model_handler());
  ASSERT_TRUE(embedded_test_server()->Start());
  PushModelFileToModelExecutor(ModelFilePath(kOneReturnAiv4Model));

  embedder_metadata_provider_fake.NotifyObservers(GetParam().embedder_metadata);

  // We setup various failure conditions defined by the testcases.
  permissions_ai_ui_selector()->set_snapshot_for_testing(GetParam().snapshot);
  set_dummy_inner_text_for_testing(GetParam().inner_text);
  std::unique_ptr<PassageEmbedderMock> passage_embedder;

  if (GetParam().passage_embedder.has_value()) {
    passage_embedder = std::make_unique<PassageEmbedderMock>(
        GetParam().passage_embedder.value());
    passage_embedder->set_status(GetParam().compute_embeddings_status);
    model_handler_provider()->set_passage_embedder_for_testing(
        passage_embedder.get());
  } else {
    model_handler_provider()->set_passage_embedder_for_testing(nullptr);
  }
  embedder_metadata_provider_fake.NotifyObservers(GetParam().embedder_metadata);

  // We expect a vanilla CPSSv3 call without input from the
  // on-device model.
  GeneratePredictionsResponse prediction_service_response =
      BuildPredictionServiceResponse(kLikelihoodVeryUnlikely);
  PredictionRequestFeatures expected_features =
      BuildRequestFeatures(request_type(), ExperimentId::kAiV4ExperimentId,
                           PermissionRequestRelevance::kUnspecified);
  EXPECT_CALL(prediction_service(),
              StartLookup(PredictionRequestFeatureEq(expected_features), _, _))
      .WillRepeatedly(WithArg<2>(
          [&](PredictionService::LookupResponseCallback response_callback) {
            std::move(response_callback)
                .Run(/*lookup_successful=*/true,
                     /*response_from_cache=*/true, prediction_service_response);
          }));

  TriggerPromptAndVerifyUi(
      /*test_url=*/"test.a", PermissionAction::DISMISSED,
      /*should_expect_quiet_ui=*/true,
      /*expected_relevance=*/std::nullopt,
      /*expected_prediction_likelihood=*/kLikelihoodVeryUnlikely,
      /*translate_source_language=*/GetParam().page_language);

  histogram_tester().ExpectTotalCount(
      kAiv4NotificationsPermissionRequestRelevanceHistogram,
      /*expected_count=*/0);

  histogram_tester().ExpectUniqueSample(kPredictionServiceTimeoutHistogram,
                                        false, 1);

  // Avoid dangling raw_ptr warning:
  model_handler_provider()->set_passage_embedder_for_testing(nullptr);
}

class Aiv4ModelTimeoutBrowserTest
    : public Aiv4ModelPredictionServiceBrowserTestBase {
 public:
  Aiv4ModelTimeoutBrowserTest() = default;

  void WaitForModelExecutionIfNecessary() override {
    // We intentionally run into the timeout (faster).
    task_runner_->FastForwardBy(base::Seconds(
        PassageEmbedderDelegate::kPassageEmbedderDelegateTimeout));
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
};

IN_PROC_BROWSER_TEST_F(Aiv4ModelTimeoutBrowserTest,
                       PassageEmbedderTestTimeout) {
  ASSERT_TRUE(aiv4_model_handler());
  ASSERT_TRUE(embedded_test_server()->Start());
  PushModelFileToModelExecutor(ModelFilePath(kOneReturnAiv4Model));

  set_dummy_inner_text_for_testing();
  DelayedPassageEmbedderMock passage_embedder;
  model_handler_provider()->set_passage_embedder_for_testing(&passage_embedder);

  // We expect a vanilla CPSSv3 call without input from the
  // on-device model since we won't call AIv4 without text embedding.
  GeneratePredictionsResponse prediction_service_response =
      BuildPredictionServiceResponse(kLikelihoodVeryUnlikely);
  PredictionRequestFeatures expected_features =
      BuildRequestFeatures(request_type(), ExperimentId::kAiV4ExperimentId,
                           PermissionRequestRelevance::kUnspecified);
  EXPECT_CALL(prediction_service(),
              StartLookup(PredictionRequestFeatureEq(expected_features), _, _))
      .WillRepeatedly(WithArg<2>(
          [&](PredictionService::LookupResponseCallback response_callback) {
            std::move(response_callback)
                .Run(/*lookup_successful=*/true,
                     /*response_from_cache=*/true, prediction_service_response);
          }));
  TriggerPromptAndVerifyUi(
      /*test_url=*/"test.a", PermissionAction::DISMISSED,
      /*should_expect_quiet_ui=*/true,
      /*expected_relevance=*/std::nullopt,
      /*expected_prediction_likelihood=*/kLikelihoodVeryUnlikely);

  histogram_tester().ExpectBucketCount(
      kAiv4PassageEmbeddingsComputationTimeoutHistogram,
      /*sample=*/1,
      /*expected_count=*/1);

  // This will finish the stalled and already stale passage embeddings task.
  // We should handle this case gracefully, and log it as outdated task.
  passage_embedder.ReleaseCallback();

  histogram_tester().ExpectBucketCount(
      kAiv4FinishedPassageEmbeddingsTaskOutdatedHistogram,
      /*sample=*/1,
      /*expected_count=*/1);

  histogram_tester().ExpectUniqueSample(kPredictionServiceTimeoutHistogram,
                                        false, 1);

  // Avoid dangling raw_ptr warning:
  model_handler_provider()->set_passage_embedder_for_testing(nullptr);
}

std::vector<PermissionRequestMetadata> aiv4_request_data_testcase = {
    {/*optimization_target=*/kAiv4OptTargetGeolocation,
     /*request_type=*/RequestType::kGeolocation},
    {/*optimization_target=*/kAiv4OptTargetNotification,
     /*request_type=*/RequestType::kNotifications},
};

std::vector<ModelMetadata> aiv4_model_data_testcase = {
    {
        /*test_name=*/"OnDeviceVeryLowAndServerSideUnspecifiedResponse"
                      "ReturnsDefaultUI",
        /*model_name=*/kZeroReturnAiv4Model,
        /*expected_relevance=*/PermissionRequestRelevance::kVeryLow,
        /*prediction_service_likelihood=*/kLikelihoodUnspecified,
        /*should_expect_quiet_ui=*/false,
        /*success_count_model_execution=*/1,
    },
    {
        /*test_name=*/"OnDeviceVeryLowAndServerSideVeryUnlikelyRespons"
                      "eReturnsQuietUI",
        /*model_name=*/kZeroReturnAiv4Model,
        /*expected_relevance=*/PermissionRequestRelevance::kVeryLow,
        /*prediction_service_likelihood=*/kLikelihoodVeryUnlikely,
        /*should_expect_quiet_ui=*/true,
        /*success_count_model_execution=*/1,
    },
    {
        /*test_name=*/"OnDeviceVeryHighAndServerSideUnspecifiedRespons"
                      "eReturnsDefaultUI",
        /*model_name=*/kOneReturnAiv4Model,
        /*expected_relevance=*/PermissionRequestRelevance::kVeryHigh,
        /*prediction_service_likelihood=*/kLikelihoodUnspecified,
        /*should_expect_quiet_ui=*/false,
        /*success_count_model_execution=*/1,
    },
    {
        /*test_name=*/"OnDeviceVeryHighAndServerSideVeryUnlikelyRespon"
                      "seReturnsQuietUI",
        /*model_name=*/kOneReturnAiv4Model,
        /*expected_relevance=*/PermissionRequestRelevance::kVeryHigh,
        /*prediction_service_likelihood=*/kLikelihoodVeryUnlikely,
        /*should_expect_quiet_ui=*/true,
        /*success_count_model_execution=*/1,
    },
    {
        /*test_name=*/"FailingAiv4ModelStillResultsInValid"
                      "ServerSideExecution",
        /*model_name=*/kNotExistingModel,
        /*expected_relevance=*/
        PermissionRequestRelevance::kUnspecified,
        /*prediction_service_likelihood=*/kLikelihoodVeryUnlikely,
        /*should_expect_quiet_ui=*/true,
        /*success_count_model_execution=*/0,
    },
};

using Aiv4ModelTestCase = std::tuple<ModelMetadata, PermissionRequestMetadata>;

class Aiv4ModelPredictionServiceBrowserTest
    : public Aiv4ModelPredictionServiceBrowserTestBase,
      public testing::WithParamInterface<Aiv4ModelTestCase> {
 public:
  Aiv4ModelPredictionServiceBrowserTest() = default;

  RequestType request_type() const override {
    return get<1>(GetParam()).request_type;
  }

  OptimizationTarget optimization_target() override {
    return get<1>(GetParam()).optimization_target;
  }
};

INSTANTIATE_TEST_SUITE_P(
    Aiv4ModelTest,
    Aiv4ModelPredictionServiceBrowserTest,
    Combine(ValuesIn(aiv4_model_data_testcase),
            ValuesIn(aiv4_request_data_testcase)),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        Aiv4ModelPredictionServiceBrowserTest::ParamType>& info) {
      return base::StrCat({test::ToString(std::get<1>(info.param).request_type),
                           std::get<0>(info.param).test_name});
    });

IN_PROC_BROWSER_TEST_P(Aiv4ModelPredictionServiceBrowserTest,
                       TestAiv4Workflow) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ASSERT_TRUE(aiv4_model_handler());

  const auto& test_case = std::get<0>(GetParam());

  PushModelFileToModelExecutor(ModelFilePath(test_case.model_name));
  ASSERT_TRUE(embedded_test_server()->Start());

  set_dummy_screenshot_for_testing();
  set_dummy_inner_text_for_testing();

  GeneratePredictionsResponse prediction_service_response =
      BuildPredictionServiceResponse(test_case.prediction_service_likelihood);

  PredictionRequestFeatures expected_features =
      BuildRequestFeatures(request_type(), ExperimentId::kAiV4ExperimentId,
                           test_case.expected_relevance);
  EXPECT_CALL(prediction_service(),
              StartLookup(PredictionRequestFeatureEq(expected_features), _, _))
      .WillRepeatedly(WithArg<2>(
          [&](PredictionService::LookupResponseCallback response_callback) {
            std::move(response_callback)
                .Run(/*lookup_successful=*/true,
                     /*response_from_cache=*/true, prediction_service_response);
          }));
  TriggerPromptAndVerifyUi(
      /*test_url=*/"test.a", PermissionAction::DISMISSED,
      test_case.should_expect_quiet_ui, test_case.expected_relevance,
      test_case.prediction_service_likelihood);

  EXPECT_EQ(permissions::PermissionAiRelevanceModel::kAIv4,
            permissions_ai_ui_selector()->PermissionAiRelevanceModelForUKM());

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Permission::kEntryName);
  ASSERT_FALSE(entries.empty());
  const auto* entry = entries.back().get();
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::Permission::kPermissionAiRelevanceModelName,
      static_cast<int64_t>(permissions::PermissionAiRelevanceModel::kAIv4));

  histogram_tester().ExpectBucketCount(
      request_type() == RequestType::kNotifications
          ? kAiv4NotificationsModelExecutionSuccessHistogram
          : kAiv4GeolocationModelExecutionSuccessHistogram,
      /*sample=*/true, /*expected_count=*/
      test_case.success_count_model_execution);

  histogram_tester().ExpectBucketCount(kAiv4SnapshotTakenHistogram,
                                       /*sample=*/true,
                                       /*expected_count=*/1);
  histogram_tester().ExpectTotalCount(kAiv4SnapshotTakenDurationHistogram,
                                      /*expected_count=*/1);
  // We should receive timing information for both, the on-device model
  // and the server-side model.
  histogram_tester().ExpectTotalCount(kCpssV3InquiryDurationHistogram,
                                      /*expected_count=*/1);
  histogram_tester().ExpectTotalCount(kAIv4InquiryDurationHistogram,
                                      /*expected_count=*/1);

  histogram_tester().ExpectBucketCount(kAiv4RenderedTextAcquireSuccessHistogram,
                                       /*sample=*/1,
                                       /*expected_count=*/1);

  histogram_tester().ExpectBucketCount(
      kAiv4TryCancelPreviousEmbeddingsModelExecutionHistogram,
      /*sample=*/0,
      /*expected_count=*/1);

  histogram_tester().ExpectBucketCount(
      kAiv4FinishedPassageEmbeddingsTaskOutdatedHistogram,
      /*sample=*/0,
      /*expected_count=*/1);

  histogram_tester().ExpectBucketCount(
      kAiv4PassageEmbeddingsComputationTimeoutHistogram,
      /*sample=*/0,
      /*expected_count=*/1);

  histogram_tester().ExpectUniqueSample(
      request_type() == RequestType::kNotifications
          ? kAiv4NotificationsPermissionRequestRelevanceHistogram
          : kAiv4GeolocationPermissionRequestRelevanceHistogram,
      /*sample=*/test_case.expected_relevance,
      // The permission request relevance is always logged.
      /*expected_bucket_count=*/1);

  histogram_tester().ExpectBucketCount(
      kAiv4ComputeEmbeddingsStatusHistogram,
      /*sample=*/ComputeEmbeddingsStatus::kSuccess,
      /*expected_count=*/1);

  histogram_tester().ExpectTotalCount(kAiv4ComputeEmbeddingsDurationHistogram,
                                      /*expected_count=*/1);

  histogram_tester().ExpectBucketCount(kAiv4EmbedderMetadataValidHistogram,
                                       /*sample=*/true, /*expected_count=*/1);

  histogram_tester().ExpectBucketCount(
      request_type() == RequestType::kNotifications
          ? kAIv4NotificationsHoldbackResponseHistogram
          : kAIv4GeolocationHoldbackResponseHistogram,
      /*sample=*/false,
      /*expected_count=*/1);

  histogram_tester().ExpectUniqueSample(
      request_type() == RequestType::kNotifications
          ? kAiv4NotificationsRenderedTextSizeHistogram
          : kAiv4GeolocationRenderedTextSizeHistogram,
      /*sample=*/55,
      /*expected_bucket_count=*/1);

  histogram_tester().ExpectUniqueSample(kPredictionServiceTimeoutHistogram,
                                        false, 1);
}

// ---------------------------------------------------------------------------
// -------------------- Prediction Service AIP92 -----------------------------
// ---------------------------------------------------------------------------
struct PredictionServiceAIP92TestCase {
  std::string test_name;
  bool feature_enabled;
  PermissionUiSelector::PredictionGrantLikelihood prediction_service_likelihood;
  bool should_expect_quiet_ui;
};

class PredictionServiceAIP92BrowserTest
    : public PredictionServiceBrowserTestBase,
      public testing::WithParamInterface<PredictionServiceAIP92TestCase> {
 public:
  PredictionServiceAIP92BrowserTest()
      : PredictionServiceBrowserTestBase(
            /*enabled_features=*/GetParam().feature_enabled
                ? std::vector<FeatureRefAndParams>{{permissions::features::
                                                        kPermissionsAIP92,
                                                    {}},
                                                   CONFIGURE_NO_HOLDBACK_CHANCE}
                : std::vector<FeatureRefAndParams>{},
            /*disabled_features=*/GetParam().feature_enabled
                ? std::vector<
                      FeatureRef>{permissions::features::kPermissionsAIv3,
                                  permissions::features::kPermissionsAIv4}
                : std::vector<FeatureRef>{
                      permissions::features::kPermissionsAIv3,
                      permissions::features::kPermissionsAIv4,
                      permissions::features::kPermissionsAIP92}) {}

  void SetUpOnMainThread() override {
    PredictionServiceBrowserTestBase::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }
};

INSTANTIATE_TEST_SUITE_P(
    PredictionServiceAIP92Test,
    PredictionServiceAIP92BrowserTest,
    ValuesIn<PredictionServiceAIP92TestCase>({
        {/*test_name=*/"FeatureEnabledUnlikelyPrediction",
         /*feature_enabled=*/true,
         /*prediction_service_likelihood=*/kLikelihoodVeryUnlikely,
         /*should_expect_quiet_ui=*/true},
        {/*test_name=*/"FeatureEnabledVeryUnlikelyPrediction",
         /*feature_enabled=*/true,
         /*prediction_service_likelihood=*/kLikelihoodUnlikely,
         /*should_expect_quiet_ui=*/true},
        {/*test_name=*/"FeatureEnabledLikelyPrediction",
         /*feature_enabled=*/true,
         /*prediction_service_likelihood=*/kLikelihoodLikely,
         /*should_expect_quiet_ui=*/false},
        {/*test_name=*/"FeatureDisabledUnlikelyPrediction",
         /*feature_enabled=*/false,
         /*prediction_service_likelihood=*/kLikelihoodUnlikely,
         /*should_expect_quiet_ui=*/false},
    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        PredictionServiceAIP92BrowserTest::ParamType>& info) {
      return info.param.test_name;
    });

IN_PROC_BROWSER_TEST_P(PredictionServiceAIP92BrowserTest, TestAIP92Workflow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GeneratePredictionsResponse prediction_service_response =
      BuildPredictionServiceResponse(GetParam().prediction_service_likelihood);

  PredictionRequestFeatures expected_features = BuildRequestFeatures(
      RequestType::kNotifications, ExperimentId::kNoExperimentId,
      PermissionRequestRelevance::kUnspecified);
  EXPECT_CALL(prediction_service(),
              StartLookup(PredictionRequestFeatureEq(expected_features), _, _))
      .WillOnce(WithArg<2>(
          [&](PredictionService::LookupResponseCallback response_callback) {
            std::move(response_callback)
                .Run(/*lookup_successful=*/true,
                     /*response_from_cache=*/true, prediction_service_response);
          }));

  TriggerPromptAndVerifyUi(/*test_url=*/"test.a", PermissionAction::DISMISSED,
                           GetParam().should_expect_quiet_ui,
                           /*expected_relevance=*/std::nullopt,
                           GetParam().prediction_service_likelihood);

  histogram_tester().ExpectUniqueSample(kPredictionServiceTimeoutHistogram,
                                        false, 1);
}

struct PredictionServiceGeolocationAccuracyTestCase {
  PermissionPrediction::GeolocationPrediction::Accuracy response_accuracy;
  GeolocationAccuracy expected_accuracy;
};

class PredictionServiceGeolocationAccuracyBrowserTest
    : public PredictionServiceBrowserTestBase,
      public testing::WithParamInterface<
          PredictionServiceGeolocationAccuracyTestCase> {
 public:
  PredictionServiceGeolocationAccuracyBrowserTest()
      : PredictionServiceBrowserTestBase(
            /*enabled_features=*/
            {{permissions::features::kPermissionPredictionsV2, {}},
             {content_settings::features::kApproximateGeolocationPermission,
              {}},
             {features::kPermissionPredictionsGeolocationAccuracy, {}}}) {}

  void SetUpOnMainThread() override {
    PredictionServiceBrowserTestBase::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }

  RequestType request_type() const override {
    return RequestType::kGeolocation;
  }
};

IN_PROC_BROWSER_TEST_P(PredictionServiceGeolocationAccuracyBrowserTest,
                       UseGeolocationAccuracyFromResponse) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GeneratePredictionsResponse prediction_service_response =
      BuildPredictionServiceResponse(kLikelihoodLikely);
  prediction_service_response.mutable_prediction(0)
      ->mutable_geolocation_prediction()
      ->set_accuracy(GetParam().response_accuracy);

  PredictionRequestFeatures expected_features = BuildRequestFeatures(
      RequestType::kGeolocation, ExperimentId::kNoExperimentId,
      PermissionRequestRelevance::kUnspecified);
  EXPECT_CALL(prediction_service(),
              StartLookup(PredictionRequestFeatureEq(expected_features), _, _))
      .WillOnce(WithArg<2>(
          [&](PredictionService::LookupResponseCallback response_callback) {
            std::move(response_callback)
                .Run(/*lookup_successful=*/true,
                     /*response_from_cache=*/true, prediction_service_response);
          }));

  TriggerPromptAndWaitForBubble(/*test_url=*/"test.a");
  auto* manager = permission_request_manager();
  EXPECT_FALSE(manager->ShouldCurrentRequestUseQuietUI());
  EXPECT_EQ(GetParam().expected_accuracy,
            manager->GetInitialGeolocationAccuracySelection());
}

INSTANTIATE_TEST_SUITE_P(
    GeolocationAccuracy,
    PredictionServiceGeolocationAccuracyBrowserTest,
    ValuesIn<PredictionServiceGeolocationAccuracyTestCase>({
        {
            .response_accuracy = permissions::PermissionPrediction::
                GeolocationPrediction::ACCURACY_UNSPECIFIED,
            .expected_accuracy = GeolocationAccuracy::kPrecise,
        },
        {
            .response_accuracy = permissions::PermissionPrediction::
                GeolocationPrediction::ACCURACY_PRECISE,
            .expected_accuracy = GeolocationAccuracy::kPrecise,
        },
        {
            .response_accuracy = permissions::PermissionPrediction::
                GeolocationPrediction::ACCURACY_APPROXIMATE,
            .expected_accuracy = GeolocationAccuracy::kApproximate,
        },
    }));

}  // namespace permissions
