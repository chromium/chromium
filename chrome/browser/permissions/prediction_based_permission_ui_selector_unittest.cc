// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_based_permission_ui_selector.h"

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_data.h"
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
using Decision = PredictionBasedPermissionUiSelector::Decision;
using PredictionSource = PredictionBasedPermissionUiSelector::PredictionSource;
using base::test::FeatureRef;

#define BASIC_CPSS_FEATURES                                              \
  permissions::features::kPermissionPredictionsV2,                       \
      permissions::features::kPermissionOnDeviceNotificationPredictions, \
      permissions::features::kPermissionOnDeviceGeolocationPredictions,  \
      features::kQuietNotificationPrompts
}  // namespace

class PredictionBasedPermissionUiSelectorTestBase : public testing::Test {
 public:
  PredictionBasedPermissionUiSelectorTestBase()
      : testing_profile_(std::make_unique<TestingProfile>()) {}

  void SetUp() override {
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

  TestingProfile* profile() { return testing_profile_.get(); }

  Decision SelectUiToUseAndGetDecision(
      PredictionBasedPermissionUiSelector* selector,
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
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
};

class PredictionBasedPermissionUiSelectorTest
    : public PredictionBasedPermissionUiSelectorTestBase {};

struct CmdLineDecisionTestCase {
  const char* command_line_value;
  const Decision expected_decision;
};

class PredictionBasedPermissionUiDecisionTest
    : public PredictionBasedPermissionUiSelectorTestBase,
      public testing::WithParamInterface<CmdLineDecisionTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    CmdLineValueToDecision,
    PredictionBasedPermissionUiDecisionTest,
    testing::ValuesIn<CmdLineDecisionTestCase>(
        {{"very-unlikely",
          Decision(PredictionBasedPermissionUiSelector::QuietUiReason::
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

  PredictionBasedPermissionUiSelector prediction_selector(profile());

  Decision decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);

  EXPECT_EQ(GetParam().expected_decision.quiet_ui_reason,
            decision.quiet_ui_reason);
  EXPECT_EQ(GetParam().expected_decision.warning_reason,
            decision.warning_reason);
}

TEST_F(PredictionBasedPermissionUiSelectorTest, RequestsWithFewPromptsAreSent) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prediction-service-mock-likelihood", "very-unlikely");
  PredictionBasedPermissionUiSelector prediction_selector(profile());

  // Requests that have 0-3 previous permission prompts will return "quiet".
  for (size_t request_id = 0; request_id < 4; ++request_id) {
    Decision notification_decision = SelectUiToUseAndGetDecision(
        &prediction_selector, permissions::RequestType::kNotifications);

    Decision geolocation_decision = SelectUiToUseAndGetDecision(
        &prediction_selector, permissions::RequestType::kGeolocation);

    EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                  kServicePredictedVeryUnlikelyGrant,
              notification_decision.quiet_ui_reason);
    EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
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

  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            notification_decision.quiet_ui_reason);

  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            geolocation_decision.quiet_ui_reason);
}

TEST_F(PredictionBasedPermissionUiSelectorTest,
       OnlyPromptsForCurrentTypeAreCounted) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prediction-service-mock-likelihood", "very-unlikely");
  PredictionBasedPermissionUiSelector prediction_selector(profile());

  // In CPSSv3 we do not check the action history.
  RecordHistoryActions(/*action_count=*/3,
                       permissions::RequestType::kNotifications);
  RecordHistoryActions(/*action_count=*/3,
                       permissions::RequestType::kGeolocation);

  Decision notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);

  Decision geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kGeolocation);

  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            notification_decision.quiet_ui_reason);
  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            geolocation_decision.quiet_ui_reason);
}
// `kPermissionsAIv1` is not enabled for Android.
// This test verifies that `GetPredictionRequestProto` does not crash if
// `kPermissionsAIv1` is enabled.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(PredictionBasedPermissionUiSelectorTest, GetPredictionTypeToUseCpssV1) {
  // Disable msbb.
  profile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  feature_list_->Reset();
  feature_list_->InitWithFeatures(
      /*enabled_features=*/{BASIC_CPSS_FEATURES,
                            permissions::features::kPermissionsAIv1},
      /*disabled_features=*/{});

  PredictionBasedPermissionUiSelector prediction_selector(profile());

  EXPECT_EQ(PredictionSource::kOnDeviceCpssV1Model,
            prediction_selector.GetPredictionTypeToUse(
                permissions::RequestType::kNotifications));

  auto decided =
      [](ContentSetting, bool, bool,
         const std::unique_ptr<permissions::PermissionRequestData>&) {};
  permissions::PermissionRequest permission_request(
      std::make_unique<permissions::PermissionRequestData>(
          std::make_unique<permissions::ContentSettingPermissionResolver>(
              ContentSettingsType::GEOLOCATION),
          /*user_gesture=*/true, GURL("http://example.com/")),
      base::BindRepeating(decided), /*delete_callback=*/base::NullCallback());

  permissions::PredictionRequestFeatures features =
      prediction_selector.BuildPredictionRequestFeatures(&permission_request);

  auto proto_request = GetPredictionRequestProto(features);
}
#endif

struct PredictionSourceTestCase {
  std::string test_name;
  const std::vector<FeatureRef> enabled_features;
  const std::vector<FeatureRef> disabled_features;
  PredictionSource expected_prediction_source;
};

class PredictionBasedPermissionUiExpectedPredictionSourceTest
    : public PredictionBasedPermissionUiSelectorTestBase,
      public testing::WithParamInterface<PredictionSourceTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    PredictionSourceTest,
    PredictionBasedPermissionUiExpectedPredictionSourceTest,
    testing::ValuesIn<PredictionSourceTestCase>({
#if BUILDFLAG(IS_ANDROID)
        {/*test_name=*/"UseCpssV1OnAndroid",
         /*enabled_features=*/{BASIC_CPSS_FEATURES},
         /*disabled_features=*/
         {permissions::features::kPermissionDedicatedCpssSettingAndroid},
         /*expected_prediction_source=*/PredictionSource::kOnDeviceCpssV1Model},
        {/*test_name=*/"UseServerSideOnAndroid",
         /*enabled_features=*/
         {BASIC_CPSS_FEATURES,
          permissions::features::kPermissionDedicatedCpssSettingAndroid},
         /*disabled_features=*/{},
         /*expected_prediction_source=*/
         PredictionSource::kServerSideCpssV3Model},
#else
        {/*test_name=*/"UseServerSideOnDesktop",
         /*enabled_features=*/{BASIC_CPSS_FEATURES},
         /*disabled_features=*/{},
         /*expected_prediction_source=*/
         PredictionSource::kServerSideCpssV3Model},
        {/*test_name=*/"UsePermissionsAiv1OnDesktop",
         /*enabled_features=*/
         {BASIC_CPSS_FEATURES, permissions::features::kPermissionsAIv1},
         /*disabled_features=*/{},
         /*expected_prediction_source=*/
         PredictionSource::kOnDeviceAiv1AndServerSideModel},
#endif
    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<
        PredictionBasedPermissionUiExpectedPredictionSourceTest::ParamType>&
           info) { return info.param.test_name; });

TEST_P(PredictionBasedPermissionUiExpectedPredictionSourceTest,
       GetPredictionTypeToUse) {
  PredictionBasedPermissionUiSelector prediction_selector(profile());

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

TEST_F(PredictionBasedPermissionUiSelectorTest, HoldbackHistogramTest) {
  PredictionBasedPermissionUiSelector prediction_selector(profile());
  base::HistogramTester histogram_tester;

  // No holdback.
  feature_list_->Reset();
  feature_list_->InitWithFeaturesAndParameters(
      {
          {permissions::features::kPermissionPredictionsV2,
           {{permissions::feature_params::kPermissionPredictionsV2HoldbackChance
                 .name,
             "0"}}},
      },
      {});
  prediction_selector.cpss_v1_model_holdback_probability_ = 0;

  EXPECT_EQ(false, prediction_selector.ShouldHoldBack(
                       /*is_on_device=*/true,
                       permissions::RequestType::kNotifications));
  histogram_tester.ExpectBucketCount(
      "Permissions.OnDevicePredictionService.Response.Notifications",
      /*sample=*/false, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Notifications",
      /*sample=*/false, /*expected_count=*/0);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Geolocation",
      /*sample=*/false, /*expected_count=*/0);

  EXPECT_EQ(false, prediction_selector.ShouldHoldBack(
                       /*is_on_device=*/false,
                       permissions::RequestType::kNotifications));
  histogram_tester.ExpectBucketCount(
      "Permissions.OnDevicePredictionService.Response.Notifications",
      /*sample=*/false, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Notifications",
      /*sample=*/false, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Geolocation",
      /*sample=*/false, /*expected_count=*/0);

  EXPECT_EQ(false, prediction_selector.ShouldHoldBack(
                       /*is_on_device=*/false,
                       permissions::RequestType::kGeolocation));
  histogram_tester.ExpectBucketCount(
      "Permissions.OnDevicePredictionService.Response.Notifications",
      /*sample=*/false, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Notifications",
      /*sample=*/false, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Geolocation",
      /*sample=*/false, /*expected_count=*/1);

  // 100% Holdback chance.
  feature_list_->Reset();
  feature_list_->InitWithFeaturesAndParameters(
      {
          {permissions::features::kPermissionPredictionsV2,
           {{permissions::feature_params::kPermissionPredictionsV2HoldbackChance
                 .name,
             "1"}}},
      },
      {});
  prediction_selector.cpss_v1_model_holdback_probability_ = 1;

  EXPECT_EQ(true, prediction_selector.ShouldHoldBack(
                      /*is_on_device=*/true,
                      permissions::RequestType::kNotifications));
  histogram_tester.ExpectBucketCount(
      "Permissions.OnDevicePredictionService.Response.Notifications",
      /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Permissions.OnDevicePredictionService.Response.Notifications",
      /*count=*/2);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Notifications",
      /*sample=*/true, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Permissions.PredictionService.Response.Notifications",
      /*count=*/1);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Geolocation",
      /*sample=*/true, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Permissions.PredictionService.Response.Geolocation",
      /*count=*/1);

  EXPECT_EQ(true, prediction_selector.ShouldHoldBack(
                      /*is_on_device=*/false,
                      permissions::RequestType::kNotifications));
  histogram_tester.ExpectBucketCount(
      "Permissions.OnDevicePredictionService.Response.Notifications",
      /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Permissions.OnDevicePredictionService.Response.Notifications",
      /*count=*/2);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Notifications",
      /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Permissions.PredictionService.Response.Notifications",
      /*count=*/2);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Geolocation",
      /*sample=*/true, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Permissions.PredictionService.Response.Geolocation",
      /*count=*/1);

  EXPECT_EQ(true, prediction_selector.ShouldHoldBack(
                      /*is_on_device=*/false,
                      permissions::RequestType::kGeolocation));
  histogram_tester.ExpectBucketCount(
      "Permissions.OnDevicePredictionService.Response.Notifications",
      /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Permissions.OnDevicePredictionService.Response.Notifications",
      /*count=*/2);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Notifications",
      /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Permissions.PredictionService.Response.Notifications",
      /*count=*/2);
  histogram_tester.ExpectBucketCount(
      "Permissions.PredictionService.Response.Geolocation",
      /*sample=*/true, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Permissions.PredictionService.Response.Geolocation",
      /*count=*/2);
}
