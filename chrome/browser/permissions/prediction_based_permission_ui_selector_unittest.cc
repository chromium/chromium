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
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using Decision = PredictionBasedPermissionUiSelector::Decision;
using PredictionSource = PredictionBasedPermissionUiSelector::PredictionSource;
}  // namespace

class PredictionBasedPermissionUiSelectorTest : public testing::Test {
 public:
  PredictionBasedPermissionUiSelectorTest()
      : testing_profile_(std::make_unique<TestingProfile>()) {}

  void SetUp() override {
    InitFeatureList();

    // enable msbb
    testing_profile_->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

    // enable cpss for both notification and geolocation
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
        {} /* disabled_features */);
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
        &request, base::BindLambdaForTesting([&](const Decision& decision) {
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

TEST_F(PredictionBasedPermissionUiSelectorTest,
       CommandLineMocksDecisionCorrectly) {
  struct {
    const char* command_line_value;
    const Decision expected_decision;
  } const kTests[] = {
      {"very-unlikely",
       Decision(PredictionBasedPermissionUiSelector::QuietUiReason::
                    kServicePredictedVeryUnlikelyGrant,
                Decision::ShowNoWarning())},
      {"unlikely", Decision::UseNormalUiAndShowNoWarning()},
      {"neutral", Decision::UseNormalUiAndShowNoWarning()},
      {"likely", Decision::UseNormalUiAndShowNoWarning()},
      {"very-likely", Decision::UseNormalUiAndShowNoWarning()},
  };

  RecordHistoryActions(/*action_count=*/4,
                       permissions::RequestType::kNotifications);

  for (const auto& test : kTests) {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "prediction-service-mock-likelihood", test.command_line_value);

    PredictionBasedPermissionUiSelector prediction_selector(profile());

    Decision decision = SelectUiToUseAndGetDecision(
        &prediction_selector, permissions::RequestType::kNotifications);

    EXPECT_EQ(test.expected_decision.quiet_ui_reason, decision.quiet_ui_reason);
    EXPECT_EQ(test.expected_decision.warning_reason, decision.warning_reason);
  }
}

TEST_F(PredictionBasedPermissionUiSelectorTest,
       RequestsWithFewPromptsAreNotSent) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "prediction-service-mock-likelihood", "very-unlikely");
  PredictionBasedPermissionUiSelector prediction_selector(profile());

  // Requests that have 0-3 previous permission prompts will return "normal"
  // without making a prediction service request.
  for (size_t request_id = 0; request_id < 4; ++request_id) {
    Decision notification_decision = SelectUiToUseAndGetDecision(
        &prediction_selector, permissions::RequestType::kNotifications);

    Decision geolocation_decision = SelectUiToUseAndGetDecision(
        &prediction_selector, permissions::RequestType::kGeolocation);

    EXPECT_EQ(Decision::UseNormalUi(), notification_decision.quiet_ui_reason);
    EXPECT_EQ(Decision::UseNormalUi(), geolocation_decision.quiet_ui_reason);

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

  // Set up history to need one more prompt before we actually send requests.
  RecordHistoryActions(/*action_count=*/3,
                       permissions::RequestType::kNotifications);
  RecordHistoryActions(/*action_count=*/3,
                       permissions::RequestType::kGeolocation);

  Decision notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);

  Decision geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kGeolocation);

  EXPECT_EQ(Decision::UseNormalUi(), notification_decision.quiet_ui_reason);
  EXPECT_EQ(Decision::UseNormalUi(), geolocation_decision.quiet_ui_reason);

  // Record a bunch of history actions for other request types.
  RecordHistoryActions(/*action_count=*/2,
                       permissions::RequestType::kCameraStream);
  RecordHistoryActions(/*action_count=*/1,
                       permissions::RequestType::kClipboard);
  RecordHistoryActions(/*action_count=*/10,
                       permissions::RequestType::kMidiSysex);

  // Should still not send requests.
  notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);
  EXPECT_EQ(Decision::UseNormalUi(), notification_decision.quiet_ui_reason);

  geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);
  EXPECT_EQ(Decision::UseNormalUi(), geolocation_decision.quiet_ui_reason);

  // Record one more notification prompt, now it should send requests.
  RecordHistoryActions(1, permissions::RequestType::kNotifications);

  notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);
  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            notification_decision.quiet_ui_reason);

  // Geolocation still has too few actions.
  geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kGeolocation);
  EXPECT_EQ(Decision::UseNormalUi(), geolocation_decision.quiet_ui_reason);

  // Now both notifications and geolocation send requests.
  RecordHistoryActions(1, permissions::RequestType::kGeolocation);

  notification_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kNotifications);
  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            notification_decision.quiet_ui_reason);

  geolocation_decision = SelectUiToUseAndGetDecision(
      &prediction_selector, permissions::RequestType::kGeolocation);

  EXPECT_EQ(PredictionBasedPermissionUiSelector::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant,
            geolocation_decision.quiet_ui_reason);
}

TEST_F(PredictionBasedPermissionUiSelectorTest, GetPredictionTypeToUse) {
  PredictionBasedPermissionUiSelector prediction_selector(profile());

  // All desktop CPSS related flags enabled, android cpssv2 flag disabled.
  feature_list_->Reset();
  feature_list_->InitWithFeatures(
      {
          permissions::features::kPermissionPredictionsV2,
          permissions::features::kPermissionOnDeviceNotificationPredictions,
          permissions::features::kPermissionOnDeviceGeolocationPredictions,
          features::kQuietNotificationPrompts,
      },
      {
#if BUILDFLAG(IS_ANDROID)
          permissions::features::kPermissionDedicatedCpssSettingAndroid,
#endif
      });
// Use server side for desktop but not for android
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(PredictionSource::USE_ONDEVICE,
            prediction_selector.GetPredictionTypeToUse(
                permissions::RequestType::kNotifications));
  EXPECT_EQ(PredictionSource::USE_ONDEVICE,
            prediction_selector.GetPredictionTypeToUse(
                permissions::RequestType::kGeolocation));
#else
  EXPECT_EQ(PredictionSource::USE_SERVER_SIDE,
            prediction_selector.GetPredictionTypeToUse(
                permissions::RequestType::kNotifications));
  EXPECT_EQ(PredictionSource::USE_SERVER_SIDE,
            prediction_selector.GetPredictionTypeToUse(
                permissions::RequestType::kGeolocation));
#endif

  // All desktop and android CPSS flags enabled
  feature_list_->Reset();
  feature_list_->InitWithFeatures(
      {
          permissions::features::kPermissionPredictionsV2,
          permissions::features::kPermissionOnDeviceNotificationPredictions,
          permissions::features::kPermissionOnDeviceGeolocationPredictions,
          features::kQuietNotificationPrompts,
#if BUILDFLAG(IS_ANDROID)
          permissions::features::kPermissionDedicatedCpssSettingAndroid,
#endif
      },
      {});
  // Use server side for both desktop and android
  EXPECT_EQ(PredictionSource::USE_SERVER_SIDE,
            prediction_selector.GetPredictionTypeToUse(
                permissions::RequestType::kNotifications));
  EXPECT_EQ(PredictionSource::USE_SERVER_SIDE,
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
          {permissions::features::kPermissionOnDeviceNotificationPredictions,
           {{permissions::feature_params::
                 kPermissionOnDeviceNotificationPredictionsHoldbackChance.name,
             "0"}}},
      },
      {});
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
          {permissions::features::kPermissionOnDeviceNotificationPredictions,
           {{permissions::feature_params::
                 kPermissionOnDeviceNotificationPredictionsHoldbackChance.name,
             "1"}}},
      },
      {});

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
