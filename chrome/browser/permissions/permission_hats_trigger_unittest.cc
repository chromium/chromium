// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_hats_trigger_helper.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

class PermissionHatsTriggerUnitTest : public testing::Test {
 public:
  PermissionHatsTriggerUnitTest() = default;

  PermissionHatsTriggerUnitTest(const PermissionHatsTriggerUnitTest&) = delete;
  PermissionHatsTriggerUnitTest& operator=(
      const PermissionHatsTriggerUnitTest&) = delete;

 protected:
  struct FeatureParams {
    std::string action_filter = "";
    std::string request_type_filter = "";
    std::string prompt_disposition_filter = "";
    std::string prompt_disposition_reason_filter = "";
    std::string had_gesture_filter = "";
    std::string release_channel_filter = "beta";
    std::string ignored_prompts_maximum_age = "10m";
    std::string survey_display_time = "OnPromptResolved";
  };

  void SetupFeatureParams(FeatureParams params) {
    feature_list()->InitWithFeaturesAndParameters(
        {{permissions::features::kPermissionsPromptSurvey,
          {{"action_filter", params.action_filter},
           {"request_type_filter", params.request_type_filter},
           {"prompt_disposition_filter", params.prompt_disposition_filter},
           {"prompt_disposition_reason_filter",
            params.prompt_disposition_reason_filter},
           {"had_gesture_filter", params.had_gesture_filter},
           {"release_channel_filter", params.release_channel_filter},
           {"ignored_prompts_maximum_age", params.ignored_prompts_maximum_age},
           {"survey_display_time", params.survey_display_time}}}},
        {});
  }

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PermissionHatsTriggerUnitTest, SingleValuedFiltersTriggerCorrectly) {
  FeatureParams params;
  params.action_filter = "Accepted";
  params.request_type_filter = "Notifications";
  params.prompt_disposition_filter = "AnchoredBubble";
  params.prompt_disposition_reason_filter = "DefaultFallback";
  params.had_gesture_filter = "true";
  params.release_channel_filter = "beta";
  SetupFeatureParams(params);

  // Matching call, should trigger
  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::GRANTED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // // Wrong action, should not trigger
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::DENIED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // // Wrong request type, should not trigger
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kCameraStream,
                  permissions::PermissionAction::GRANTED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Wrong prompt disposition, should not trigger
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::GRANTED,
                  permissions::PermissionPromptDisposition::MESSAGE_UI,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Wrong prompt disposition reason, should not trigger
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::GRANTED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      SAFE_BROWSING_VERDICT,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // No gesture, should not trigger
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::GRANTED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::NO_GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Wrong channel, should not trigger
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::GRANTED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::NO_GESTURE,
                  "stable", permissions::kOnPromptResolved, base::Minutes(1))));
}

TEST_F(PermissionHatsTriggerUnitTest, EmptyFiltersShouldAlwaysTrigger) {
  FeatureParams params;
  // The same logic is reused for all filters. As representative example this
  // test configures and tests an empty string for the action filter. The empty
  // string is also the default value used by the feature flag, if no value is
  // set.
  params.action_filter = "";
  params.request_type_filter = "Notifications";
  params.prompt_disposition_filter = "AnchoredBubble";
  params.prompt_disposition_reason_filter = "DefaultFallback";
  params.had_gesture_filter = "true";
  params.release_channel_filter = "beta";
  SetupFeatureParams(params);

  // Matching call, should trigger

  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::GRANTED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Matching call, should trigger
  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::GRANTED_ONCE,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Matching call, should trigger
  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::DENIED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Matching call, should trigger
  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::DISMISSED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Matching call, should trigger
  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::IGNORED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));
}

TEST_F(PermissionHatsTriggerUnitTest, CSVFiltersTriggerForAllConfiguredValues) {
  FeatureParams params;
  // The same logic is reused for all filters. As representative example this
  // test configures a CSV for the action filter.
  params.action_filter = "Accepted,Dismissed";
  params.request_type_filter = "Notifications";
  params.prompt_disposition_filter = "AnchoredBubble";
  params.prompt_disposition_reason_filter = "DefaultFallback";
  params.had_gesture_filter = "true";
  params.release_channel_filter = "beta";
  SetupFeatureParams(params);

  // Matching call, should trigger
  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::GRANTED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Matching call, should trigger
  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::DISMISSED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Wrong action, should not trigger
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::DENIED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Wrong action, should not trigger
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::GRANTED_ONCE,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Wrong action, should not trigger
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::IGNORED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));
}

TEST_F(PermissionHatsTriggerUnitTest, FilterConfigurationHandlesEdgeCases) {
  FeatureParams params;
  params.action_filter =
      " Accepted ,   Dismissed ";  // arbitrary whitespace between distinct
                                   // filter values
  params.request_type_filter =
      "Notifications,Geolocation,Notifications";        // same value twice
  params.prompt_disposition_filter = "ANCHOREDBUBBLE";  // case insensitive
  params.prompt_disposition_reason_filter = "DefaultFallback";
  params.had_gesture_filter = "true";
  params.release_channel_filter = "beta";
  SetupFeatureParams(params);

  // Matching call, should trigger
  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::GRANTED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));

  // Matching call, should trigger
  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kGeolocation,
                  permissions::PermissionAction::GRANTED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));
}

TEST_F(PermissionHatsTriggerUnitTest, ProductSpecificFieldsAreReported) {
  FeatureParams params;
  params.action_filter = "Accepted";
  params.request_type_filter = "Notifications";
  params.prompt_disposition_filter = "AnchoredBubble";
  params.prompt_disposition_reason_filter = "DefaultFallback";
  params.had_gesture_filter = "true";
  params.release_channel_filter = "beta";
  SetupFeatureParams(params);

  auto survey_data = permissions::PermissionHatsTriggerHelper::
      SurveyProductSpecificData::PopulateFrom(
          permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
              permissions::RequestType::kNotifications,
              permissions::PermissionAction::GRANTED,
              permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
              permissions::PermissionPromptDispositionReason::DEFAULT_FALLBACK,
              permissions::PermissionRequestGestureType::GESTURE, "beta",
              permissions::kOnPromptResolved, base::Minutes(1)));

  EXPECT_EQ(survey_data.survey_bits_data.at(
                permissions::kPermissionsPromptSurveyHadGestureKey),
            true);
  EXPECT_EQ(survey_data.survey_string_data.at(
                permissions::kPermissionsPromptSurveyPromptDispositionKey),
            "AnchoredBubble");
  EXPECT_EQ(
      survey_data.survey_string_data.at(
          permissions::kPermissionsPromptSurveyPromptDispositionReasonKey),
      "DefaultFallback");
  EXPECT_EQ(survey_data.survey_string_data.at(
                permissions::kPermissionsPromptSurveyActionKey),
            "Accepted");
  EXPECT_EQ(survey_data.survey_string_data.at(
                permissions::kPermissionsPromptSurveyRequestTypeKey),
            "Notifications");
  EXPECT_EQ(survey_data.survey_string_data.at(
                permissions::kPermissionsPromptSurveyReleaseChannelKey),
            "beta");
}

TEST_F(PermissionHatsTriggerUnitTest, VerifyIgnoreSafeguardFunctionality) {
  FeatureParams params;
  params.action_filter = "Ignored";
  params.request_type_filter = "Notifications";
  params.prompt_disposition_filter = "AnchoredBubble";
  params.prompt_disposition_reason_filter = "DefaultFallback";
  params.had_gesture_filter = "true";
  params.release_channel_filter = "beta";
  params.ignored_prompts_maximum_age = "10m";
  SetupFeatureParams(params);

  // The safeguard is active, but the display time is less than the configured
  // value. Thus, this should trigger.
  EXPECT_TRUE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::IGNORED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(5))));

  // The safeguard is active, and the display time is higher than the configured
  // value. Thus, this should not trigger.
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::IGNORED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(15))));
}

TEST_F(PermissionHatsTriggerUnitTest, VerifyUnconfiguredFiltersSafeguard) {
  // No filter at all configured
  FeatureParams params;
  params.action_filter = "";
  params.request_type_filter = "";
  params.prompt_disposition_filter = "";
  params.prompt_disposition_reason_filter = "";
  params.had_gesture_filter = "";
  params.release_channel_filter = "";
  params.survey_display_time = "";
  SetupFeatureParams(params);

  // Matching call, but should not trigger due to safeguard
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::IGNORED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));
}

TEST_F(PermissionHatsTriggerUnitTest, VerifyMisconfiguredFiltersSafeguard) {
  FeatureParams params;
  params.action_filter = "asdf";
  params.request_type_filter = "";
  params.prompt_disposition_filter = "";
  params.prompt_disposition_reason_filter = "";
  params.had_gesture_filter = "";
  params.release_channel_filter = "";
  SetupFeatureParams(params);

  // One filter is configured with a nonsensical value, should not trigger
  EXPECT_FALSE(
      permissions::PermissionHatsTriggerHelper::
          ArePromptTriggerCriteriaSatisfied(
              permissions::PermissionHatsTriggerHelper::PromptParametersForHaTS(
                  permissions::RequestType::kNotifications,
                  permissions::PermissionAction::IGNORED,
                  permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
                  permissions::PermissionPromptDispositionReason::
                      DEFAULT_FALLBACK,
                  permissions::PermissionRequestGestureType::GESTURE, "beta",
                  permissions::kOnPromptResolved, base::Minutes(1))));
}
