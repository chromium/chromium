// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service/permissions_ai_ui_selector.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider_factory.h"
#include "chrome/browser/permissions/test/enums_to_string.h"
#include "chrome/browser/permissions/test/mock_passage_embedder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/prediction_service/prediction_common.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using Decision = PermissionsAiUiSelector::Decision;
using ::test::EmbedderMetadataProviderFake;
using ::test::PassageEmbedderMock;
using PredictionSource = permissions::PermissionPredictionSource;
using ::base::test::FeatureRef;
using ::permissions::PredictionModelHandlerProvider;
using ::testing::NiceMock;

constexpr char kOnDevPredServiceResponseNotificationsHistogram[] =
    "Permissions.OnDevicePredictionService.Response.Notifications";
constexpr char kOnDevPredServiceResponseGeolocationHistogram[] =
    "Permissions.OnDevicePredictionService.Response.Geolocation";
constexpr char kPredServiceResponseNotificationsHistogram[] =
    "Permissions.PredictionService.Response.Notifications";
constexpr char kPredServiceResponseGeolocationHistogram[] =
    "Permissions.PredictionService.Response.Geolocation";
constexpr char kAIv3ResponseNotificationsHistogram[] =
    "Permissions.AIv3.Response.Notifications";
constexpr char kAIv3ResponseGeolocationHistogram[] =
    "Permissions.AIv3.Response.Geolocation";
constexpr char kAIv4ResponseNotificationsHistogram[] =
    "Permissions.AIv4.Response.Notifications";
constexpr char kAIv4ResponseGeolocationHistogram[] =
    "Permissions.AIv4.Response.Geolocation";

std::unique_ptr<KeyedService> BuildPredictionModelHandler(
    OptimizationGuideKeyedService* optimization_guide,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
    passage_embeddings::Embedder* passage_embedder,
    content::BrowserContext* context) {
  return std::make_unique<PredictionModelHandlerProvider>(
      optimization_guide, embedder_metadata_provider, passage_embedder);
}

}  // namespace

class PermissionsAiUiSelectorTestBase : public testing::Test {
 public:
  PermissionsAiUiSelectorTestBase()
      : testing_profile_(std::make_unique<TestingProfile>()) {}

  void SetUp() override {
    model_handler_provider_ = SetupPredictionModelHandlerForTesting();
    // Required to get correct prediction type in case of AIv4.
    embedder_metadata_provider_fake_.NotifyObservers(
        EmbedderMetadataProviderFake::GetValidEmbedderMetadata());

    InitFeatureList();

    // Enable msbb.
    testing_profile_->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

    // Enable cpss for both notification and geolocation.
    testing_profile_->GetPrefs()->SetBoolean(prefs::kEnableNotificationCPSS,
                                             true);
    testing_profile_->GetPrefs()->SetBoolean(prefs::kEnableGeolocationCPSS,
                                             true);
  }

  void InitFeatureList(const std::string holdback_chance_string = "0") {
    if (feature_list_) {
      feature_list_->Reset();
    }
    feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    feature_list_->InitWithFeaturesAndParameters(
        {{features::kQuietNotificationPrompts, {}},
         {permissions::features::kPermissionPredictionsV2,
          {{permissions::feature_params::kPermissionPredictionsV2HoldbackChance
                .name,
            holdback_chance_string}}}},
        /*disabled_features=*/{});
  }

  void RecordHistoryActions(size_t action_count,
                            permissions::RequestType request_type) {
    while (action_count--) {
      PermissionActionsHistoryFactory::GetForProfile(profile())->RecordAction(
          permissions::PermissionAction::DENIED, request_type,
          permissions::PermissionPromptDisposition::ANCHORED_BUBBLE);
    }
  }

  Decision SelectUiToUseAndGetDecision(PermissionsAiUiSelector* selector,
                                       permissions::RequestType request_type) {
    std::optional<Decision> actual_decision;
    base::RunLoop run_loop;

    permissions::MockPermissionRequest request(
        request_type, permissions::PermissionRequestGestureType::GESTURE);

    selector->SelectUiToUse(
        /*web_contents=*/nullptr, &request,
        base::BindLambdaForTesting([&](const Decision& decision) {
          actual_decision = decision;
          run_loop.Quit();
        }));
    run_loop.Run();

    return actual_decision.value();
  }

  TestingProfile* profile() { return testing_profile_.get(); }
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;

 private:
  PredictionModelHandlerProvider* SetupPredictionModelHandlerForTesting() {
    return static_cast<PredictionModelHandlerProvider*>(
        PredictionModelHandlerProviderFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating(&BuildPredictionModelHandler,
                                    /*optimization_guide=*/nullptr,
                                    &embedder_metadata_provider_fake_,
                                    &passage_embedder_)));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  PassageEmbedderMock passage_embedder_;
  EmbedderMetadataProviderFake embedder_metadata_provider_fake_;
  raw_ptr<PredictionModelHandlerProvider> model_handler_provider_;
};

class PermissionsAiUiSelectorTest : public PermissionsAiUiSelectorTestBase {};

struct CmdLineDecisionTestCase {
  const char* command_line_value;
  const Decision expected_decision;
};

class PredictionBasedPermissionUiDecisionTest
    : public PermissionsAiUiSelectorTestBase,
      public testing::WithParamInterface<CmdLineDecisionTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    CmdLineValueToDecision,
    PredictionBasedPermissionUiDecisionTest,
    testing::ValuesIn<CmdLineDecisionTestCase>(
        {{"very-unlikely", Decision(PermissionsAiUiSelector::QuietUiReason::
                                        kServicePredictedVeryUnlikelyGrant,
                                    Decision::ShowNoWarning())},
         {"unlikely", Decision::UseNormalUiAndShowNoWarning()},
         {"neutral", Decision::UseNormalUiAndShowNoWarning()},
         {"likely", Decision::UseNormalUiAndShowNoWarning()},
         {"very-likely", Decision::UseNormalUiAndShowNoWarning()}}));

TEST_P(PredictionBasedPermissionUiDecisionTest,
       CommandLineMocksDecisionCorrectly) {
  RecordHistoryActions(/*action_count=*/4,
                       permissions::RequestType::kNotifications);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prediction-service-mock-likelihood", GetParam().command_line_value);

  PermissionsAiUiSelector prediction_selector(profile());

  Decision decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);

  EXPECT_EQ(GetParam().expected_decision.quiet_ui_reason,
            decision.quiet_ui_reason);
  EXPECT_EQ(GetParam().expected_decision.warning_reason,
            decision.warning_reason);
}

TEST_F(PermissionsAiUiSelectorTest, ConcurrentRequestsTest) {
  base::HistogramTester histogram_tester;
  PermissionsAiUiSelector prediction_selector(profile());

  // Imitate that there is a still running model execution and the callback
  // has not been called yet.
  prediction_selector.set_callback_for_testing(
      base::BindLambdaForTesting([&](const Decision& decision) {}));

  permissions::MockPermissionRequest request(
      permissions::RequestType::kNotifications,
      permissions::PermissionRequestGestureType::GESTURE);

  prediction_selector.SelectUiToUse(
      /*web_contents=*/nullptr, &request,
      base::BindLambdaForTesting([&](const Decision& decision) {}));

  histogram_tester.ExpectUniqueSample(
      "Permissions.PredictionService.ConcurrentRequests",
      permissions::PermissionPredictionSupportedType::kNotifications, 1);
}

TEST_F(PermissionsAiUiSelectorTest, RequestsWithFewPromptsAreSent) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prediction-service-mock-likelihood", "very-unlikely");
  PermissionsAiUiSelector prediction_selector(profile());

  // Requests that have 0-3 previous permission prompts will return "quiet".
  for (size_t request_id = 0; request_id < 4; ++request_id) {
    Decision notification_decision = SelectUiToUseAndGetDecision(
        &prediction_selector, permissions::RequestType::kNotifications);

    Decision geolocation_decision = SelectUiToUseAndGetDecision(
        &prediction_selector, permissions::RequestType::kGeolocation);

    EXPECT_EQ(PermissionsAiUiSelector::QuietUiReason::
                  kServicePredictedVeryUnlikelyGrant,
              notification_decision.quiet_ui_reason);
    EXPECT_EQ(PermissionsAiUiSelector::QuietUiReason::
                  kServicePredictedVeryUnlikelyGrant,
              geolocation_decision.quiet_ui_reason);

    RecordHistoryActions(/*action_count=*/1,
                         permissions::RequestType::kNotifications);
    RecordHistoryActions(/*action_count=*/1,
                         permissions::RequestType::kGeolocation);
  }

  // Since there are 4 previous prompts, the prediction service request will be
  // made and will return a "kServicePredictedVeryUnlikelyGrant" quiet reason.
  Decision notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);

  Decision geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kGeolocation);

  EXPECT_EQ(PermissionsAiUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            notification_decision.quiet_ui_reason);

  EXPECT_EQ(PermissionsAiUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            geolocation_decision.quiet_ui_reason);
}

TEST_F(PermissionsAiUiSelectorTest, OnlyPromptsForCurrentTypeAreCounted) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prediction-service-mock-likelihood", "very-unlikely");
  PermissionsAiUiSelector prediction_selector(profile());

  // In CPSSv3 we do not check the action history.
  RecordHistoryActions(/*action_count=*/3,
                       permissions::RequestType::kNotifications);
  RecordHistoryActions(/*action_count=*/3,
                       permissions::RequestType::kGeolocation);

  Decision notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);

  Decision geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kGeolocation);

  EXPECT_EQ(PermissionsAiUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            notification_decision.quiet_ui_reason);
  EXPECT_EQ(PermissionsAiUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            geolocation_decision.quiet_ui_reason);
}

struct PredictionSourceTestCase {
  std::string test_name;
  const std::vector<FeatureRef> enabled_features;
  const std::vector<FeatureRef> disabled_features;
  PredictionSource expected_prediction_source;
};

class PredictionBasedPermissionUiExpectedPredictionSourceTest
    : public PermissionsAiUiSelectorTestBase,
      public testing::WithParamInterface<PredictionSourceTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    PredictionSourceTest,
    PredictionBasedPermissionUiExpectedPredictionSourceTest,
    testing::ValuesIn<PredictionSourceTestCase>({
#if BUILDFLAG(IS_ANDROID)
        {/*test_name=*/"UseCpssV1OnAndroid",
         /*enabled_features=*/{},
         /*disabled_features=*/
         {permissions::features::kPermissionDedicatedCpssSettingAndroid},
         /*expected_prediction_source=*/PredictionSource::kOnDeviceCpssV1Model},
        {/*test_name=*/"UseServerSideOnAndroid",
         /*enabled_features=*/
         {permissions::features::kPermissionDedicatedCpssSettingAndroid},
         /*disabled_features=*/{},
         /*expected_prediction_source=*/
         PredictionSource::kServerSideCpssV3Model},
#else
        {/*test_name=*/"UseServerSideOnDesktop",
         /*enabled_features=*/{},
         /*disabled_features=*/{},
         /*expected_prediction_source=*/
         PredictionSource::kServerSideCpssV3Model},
        {/*test_name=*/"UsePermissionsAiv3OnDesktop",
         /*enabled_features=*/
         {permissions::features::kPermissionsAIv3},
         /*disabled_features=*/{},
         /*expected_prediction_source=*/
         PredictionSource::kOnDeviceAiv3AndServerSideModel},
        {/*test_name=*/"UsePermissionsAiv4OverAiv4OnDesktop",
         /*enabled_features=*/
         {permissions::features::kPermissionsAIv3,
          permissions::features::kPermissionsAIv4},
         /*disabled_features=*/{},
         /*expected_prediction_source=*/
         PredictionSource::kOnDeviceAiv4AndServerSideModel},
#endif
    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        PredictionBasedPermissionUiExpectedPredictionSourceTest::ParamType>&
           info) { return info.param.test_name; });

TEST_P(PredictionBasedPermissionUiExpectedPredictionSourceTest,
       GetPredictionTypeToUse) {
  PermissionsAiUiSelector prediction_selector(profile());

  feature_list_->Reset();
  feature_list_->InitWithFeatures(GetParam().enabled_features,
                                  GetParam().disabled_features);

  EXPECT_EQ(GetParam().expected_prediction_source,
            prediction_selector.GetPredictionTypeToUse(
                permissions::RequestType::kNotifications));
  EXPECT_EQ(GetParam().expected_prediction_source,
            prediction_selector.GetPredictionTypeToUse(
                permissions::RequestType::kGeolocation));
}

struct HoldbackChanceTestCase {
  int holdback_chance;
  PredictionSource prediction_source;
  permissions::RequestType request_type;
  std::vector<std::string_view> updated_histograms;
};

class PredictionBasedPermissionUiExpectedHoldbackChanceTest
    : public PermissionsAiUiSelectorTestBase,
      public testing::WithParamInterface<HoldbackChanceTestCase> {
 public:
  void SetUp() override {
    PermissionsAiUiSelectorTestBase::SetUp();
    feature_list_->Reset();
    feature_list_->InitWithFeaturesAndParameters(
        {
            {permissions::features::kPermissionPredictionsV2,
             {{permissions::feature_params::
                   kPermissionPredictionsV2HoldbackChance.name,
               GetParam().holdback_chance == 1 ? "1" : "0"}}},
        },
        {});
  }

  // Checks for the selected histogram that has a bucket count of 1 and also
  // ensures that no other histogram was changed.
  void CheckHistogramsAreEmptyExcept(
      const std::vector<std::string_view>& updated_histograms) {
    // Static list of all histogram names to check
    static const std::vector<std::string> kAllHistogramNames = {
        kOnDevPredServiceResponseNotificationsHistogram,
        kOnDevPredServiceResponseGeolocationHistogram,
        kPredServiceResponseNotificationsHistogram,
        kPredServiceResponseGeolocationHistogram,
        kAIv3ResponseNotificationsHistogram,
        kAIv3ResponseGeolocationHistogram,
    };

    for (const auto& histogram_name : kAllHistogramNames) {
      // If the histogram is not in the allowed set, ensure its count is 0
      if (!base::Contains(updated_histograms, histogram_name)) {
        histogram_tester_.ExpectTotalCount(histogram_name, 0);
      }
    }

    for (const auto& histogram_name : updated_histograms) {
      histogram_tester_.ExpectBucketCount(
          histogram_name,
          /*sample=*/GetParam().holdback_chance == 1,
          /*expected_count=*/1);

      histogram_tester_.ExpectTotalCount(histogram_name, /*expected_count=*/1);
    }
  }

 private:
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(
    HoldbackChanceTest,
    PredictionBasedPermissionUiExpectedHoldbackChanceTest,
    testing::ValuesIn<HoldbackChanceTestCase>({
        // ----------------------- on-device CPSSV1
        {
            /*holdback_chance=*/0,
            /*prediction_source=*/PredictionSource::kOnDeviceCpssV1Model,
            /*request_type=*/permissions::RequestType::kNotifications,
            /*updated_histograms=*/
            {kOnDevPredServiceResponseNotificationsHistogram},
        },
        {
            /*holdback_chance=*/1,
            /*prediction_source=*/PredictionSource::kOnDeviceCpssV1Model,
            /*request_type=*/permissions::RequestType::kNotifications,
            /*updated_histograms=*/
            {kOnDevPredServiceResponseNotificationsHistogram},
        },
        {
            /*holdback_chance=*/0,
            /*prediction_source=*/PredictionSource::kOnDeviceCpssV1Model,
            /*request_type=*/permissions::RequestType::kGeolocation,
            /*updated_histograms=*/
            {kOnDevPredServiceResponseGeolocationHistogram},
        },
        {
            /*holdback_chance=*/1,
            /*prediction_source=*/PredictionSource::kOnDeviceCpssV1Model,
            /*request_type=*/permissions::RequestType::kGeolocation,
            /*updated_histograms=*/
            {kOnDevPredServiceResponseGeolocationHistogram},
        },
        // ----------------------- server-side CPSSv3
        {
            /*holdback_chance=*/0,
            /*prediction_source=*/PredictionSource::kServerSideCpssV3Model,
            /*request_type=*/permissions::RequestType::kNotifications,
            /*updated_histograms=*/
            {kPredServiceResponseNotificationsHistogram},
        },
        {
            /*holdback_chance=*/1,
            /*prediction_source=*/PredictionSource::kServerSideCpssV3Model,
            /*request_type=*/permissions::RequestType::kNotifications,
            /*updated_histograms=*/
            {kPredServiceResponseNotificationsHistogram},
        },
        {
            /*holdback_chance=*/0,
            /*prediction_source=*/PredictionSource::kServerSideCpssV3Model,
            /*request_type=*/permissions::RequestType::kGeolocation,
            /*updated_histograms=*/
            {kPredServiceResponseGeolocationHistogram},
        },
        {
            /*holdback_chance=*/1,
            /*prediction_source=*/PredictionSource::kServerSideCpssV3Model,
            /*request_type=*/permissions::RequestType::kGeolocation,
            /*updated_histograms=*/
            {kPredServiceResponseGeolocationHistogram},
        },
        // ----------------------- on-device AIv3 + server-side CPSSv3
        {
            /*holdback_chance=*/0,
            /*prediction_source=*/
            PredictionSource::kOnDeviceAiv3AndServerSideModel,
            /*request_type=*/permissions::RequestType::kNotifications,
            /*updated_histograms=*/{kAIv3ResponseNotificationsHistogram},
        },
        {
            /*holdback_chance=*/1,
            /*prediction_source=*/
            PredictionSource::kOnDeviceAiv3AndServerSideModel,
            /*request_type=*/permissions::RequestType::kGeolocation,
            /*updated_histograms=*/{kAIv3ResponseGeolocationHistogram},
        },
        // ----------------------- on-device AIv4 + server-side CPSSv3
        {
            /*holdback_chance=*/0,
            /*prediction_source=*/
            PredictionSource::kOnDeviceAiv4AndServerSideModel,
            /*request_type=*/permissions::RequestType::kNotifications,
            /*updated_histograms=*/{kAIv4ResponseNotificationsHistogram},
        },
        {
            /*holdback_chance=*/1,
            /*prediction_source=*/
            PredictionSource::kOnDeviceAiv4AndServerSideModel,
            /*request_type=*/permissions::RequestType::kGeolocation,
            /*updated_histograms=*/{kAIv4ResponseGeolocationHistogram},
        },

    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        PredictionBasedPermissionUiExpectedHoldbackChanceTest::ParamType>&
           info) {
      return base::StrCat(
          {(info.param.holdback_chance == 1) ? "FullHoldbackChance"
                                             : "NoHoldbackChance",
           "For", test::ToString(info.param.prediction_source), "Execution",
           "And",
           (info.param.request_type == permissions::RequestType::kNotifications)
               ? "Notifications"
               : "Geolocation",
           "Permission"});
    });

TEST_P(PredictionBasedPermissionUiExpectedHoldbackChanceTest,
       HoldbackHistogramTest) {
  PermissionsAiUiSelector prediction_selector(profile());
  prediction_selector.cpss_v1_model_holdback_probability_ =
      GetParam().holdback_chance;

  // 1 means 100% holdback chance and as we only test with 0 or 1 here this is
  // basically the expected result.
  bool expected_holdback = GetParam().holdback_chance == 1;
  EXPECT_EQ(expected_holdback,
            prediction_selector.ShouldHoldBack(
                {.prediction_source = GetParam().prediction_source,
                 .request_type = GetParam().request_type}));

  CheckHistogramsAreEmptyExcept(GetParam().updated_histograms);
}
