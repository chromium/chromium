// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service_factory.h"
#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_test_base.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "media/base/picture_in_picture_events_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using media::PictureInPictureEventsInfo;
using PromptResult = AutoPipSettingHelper::PromptResult;
using testing::_;
using testing::AllOf;
using testing::Contains;
using testing::Key;
using testing::Pair;

class AutoPictureInPictureHatsServiceTest
    : public AutoPictureInPictureHatsTestBase {
 public:
  AutoPictureInPictureHatsServiceTest() = default;

  ~AutoPictureInPictureHatsServiceTest() override = default;

  std::vector<base::test::FeatureRef> GetEnabledFeatures() override {
    return {media::kAutoPictureInPictureSurveys};
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {};
  }

  // Returns a ScopedFeatureList that enables AutoPip surveys for a specific
  // segment defined by the target reason and prompt result.
  std::unique_ptr<base::test::ScopedFeatureList> CreateFinchScopedFeatureList(
      const std::string& target_reason,
      const std::string& target_result) {
    auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
    feature_list->InitAndEnableFeatureWithParameters(
        media::kAutoPictureInPictureSurveys,
        {{"autopip_reason", target_reason}, {"prompt_result", target_result}});
    return feature_list;
  }

  AutoPictureInPictureHatsService* service() {
    ON_CALL(*mock_hats_service(), CanShowAnySurvey(false))
        .WillByDefault(testing::Return(true));
    AutoPictureInPictureHatsService* service =
        AutoPictureInPictureHatsServiceFactory::GetForProfile(profile());
    service->set_clock_for_testing(task_environment()->GetMockTickClock());
    return service;
  }
};

TEST_F(AutoPictureInPictureHatsServiceTest,
       AutoPictureInPictureWindowOpenedStartsActiveWindowContext) {
  GURL test_url("https://example.com");
  auto reason = PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing;

  EXPECT_FALSE(service()->get_active_window_context_for_testing().has_value());

  service()->AutoPictureInPictureWindowOpened(reason, test_url);

  auto& context = service()->get_active_window_context_for_testing();
  ASSERT_TRUE(context.has_value());
  EXPECT_EQ(context->reason, reason);
  EXPECT_EQ(context->origin, test_url);
  EXPECT_FALSE(context->prompt_result.has_value());
  EXPECT_EQ(context->start_time, task_environment()->NowTicks());
}

TEST_F(AutoPictureInPictureHatsServiceTest, SetPromptResultUpdatesContext) {
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));

  service()->SetPromptResult(PromptResult::kBlock);

  auto& context = service()->get_active_window_context_for_testing();
  ASSERT_TRUE(context.has_value());
  EXPECT_EQ(context->prompt_result, PromptResult::kBlock);
}

TEST_F(AutoPictureInPictureHatsServiceTest, WindowClosedSetsDurationInContext) {
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));

  task_environment()->FastForwardBy(base::Seconds(10));
  service()->AutoPictureInPictureWindowClosed();

  auto& context = service()->get_active_window_context_for_testing();
  ASSERT_TRUE(context.has_value());
  EXPECT_TRUE(context->window_duration.has_value());
  EXPECT_EQ(*context->window_duration, base::Seconds(10));
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       MaybeLaunchSurveyResetsContextOnNoPromptResult) {
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));
  service()->AutoPictureInPictureWindowClosed();

  ASSERT_TRUE(service()->get_active_window_context_for_testing().has_value());

  service()->MaybeLaunchSurvey(web_contents());

  EXPECT_FALSE(service()->get_active_window_context_for_testing().has_value());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       MaybeLaunchSurveyFailsIfWindowNotClosed) {
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));
  service()->SetPromptResult(PromptResult::kBlock);

  // Context should still exist, but window_duration is not set.
  ASSERT_TRUE(service()->get_active_window_context_for_testing().has_value());
  EXPECT_FALSE(service()
                   ->get_active_window_context_for_testing()
                   ->window_duration.has_value());

  service()->MaybeLaunchSurvey(web_contents());

  // Context should not be reset because we might still want to launch it later
  // when the window closes.
  EXPECT_TRUE(service()->get_active_window_context_for_testing().has_value());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       SetPromptResultWithoutContextIsNoOp) {
  service()->SetPromptResult(PromptResult::kBlock);
  EXPECT_FALSE(service()->get_active_window_context_for_testing().has_value());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       MultipleWindowOpenedOverwritesContext) {
  GURL url1("https://site1.com");
  GURL url2("https://site2.com");

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing, url1);
  base::TimeTicks time1 = task_environment()->NowTicks();

  task_environment()->FastForwardBy(base::Seconds(10));

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback, url2);
  base::TimeTicks time2 = task_environment()->NowTicks();

  auto& context = service()->get_active_window_context_for_testing();
  ASSERT_TRUE(context.has_value());
  EXPECT_EQ(context->reason,
            PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback);
  EXPECT_EQ(context->origin, url2);
  EXPECT_EQ(context->start_time, time2);
  EXPECT_NE(context->start_time, time1);
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       SetPromptResultAfterMaybeLaunchSurveyIsNoOp) {
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));
  service()->SetPromptResult(PromptResult::kBlock);
  service()->AutoPictureInPictureWindowClosed();
  service()->MaybeLaunchSurvey(web_contents());

  // This should be a no-op since the context was reset.
  service()->SetPromptResult(PromptResult::kBlock);

  EXPECT_FALSE(service()->get_active_window_context_for_testing().has_value());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       TriggersSurveyWhenSegmentsMatch_Allowed) {
  auto feature_list =
      CreateFinchScopedFeatureList("VideoConferencing", "AllowOnce");
  GURL test_url("https://example.com");

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing, test_url);
  service()->SetPromptResult(PromptResult::kAllowOnce);
  service()->AutoPictureInPictureWindowClosed();

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(kHatsSurveyTriggerAutoPipAllowed,
                                         web_contents(), _, _, _, _, _, _));

  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       TriggersSurveyWhenSegmentsMatch_Blocked) {
  auto feature_list =
      CreateFinchScopedFeatureList("VideoConferencing", "Block");
  GURL test_url("https://example.com");

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing, test_url);
  service()->SetPromptResult(PromptResult::kBlock);
  service()->AutoPictureInPictureWindowClosed();

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(kHatsSurveyTriggerAutoPipBlocked,
                                         web_contents(), _, _, _, _, _, _));

  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       TriggersSurveyWhenSegmentsMatch_Ignored) {
  auto feature_list =
      CreateFinchScopedFeatureList("VideoConferencing", "Ignored");
  GURL test_url("https://example.com");

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing, test_url);
  service()->SetPromptResult(PromptResult::kIgnored);
  service()->AutoPictureInPictureWindowClosed();

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(
                  kHatsSurveyTriggerAutoPipPermissionPromptIgnored,
                  web_contents(), _, _, _, _, _, _));

  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest, DoesNotTriggerOnReasonMismatch) {
  auto feature_list =
      CreateFinchScopedFeatureList("MediaPlayback", "AllowOnce");

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));
  service()->SetPromptResult(PromptResult::kAllowOnce);
  service()->AutoPictureInPictureWindowClosed();

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(_, _, _, _, _, _, _, _))
      .Times(0);

  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest, DoesNotTriggerOnResultMismatch) {
  auto feature_list =
      CreateFinchScopedFeatureList("VideoConferencing", "AllowOnce");

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));
  service()->SetPromptResult(PromptResult::kBlock);
  service()->AutoPictureInPictureWindowClosed();

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(_, _, _, _, _, _, _, _))
      .Times(0);

  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       DoesNotTriggerForBrowserInitiatedPip) {
  auto feature_list =
      CreateFinchScopedFeatureList("BrowserInitiated", "AllowOnce");

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kBrowserInitiated,
      GURL("https://example.com"));
  service()->SetPromptResult(PromptResult::kAllowOnce);
  service()->AutoPictureInPictureWindowClosed();

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(_, _, _, _, _, _, _, _))
      .Times(0);

  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest, RecordsCorrectPSD) {
  auto feature_list =
      CreateFinchScopedFeatureList("VideoConferencing", "AllowOnce");

  GURL test_url("https://example.com/");
  profile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing, test_url);
  task_environment()->FastForwardBy(base::Seconds(15));
  service()->SetPromptResult(PromptResult::kAllowOnce);
  service()->AutoPictureInPictureWindowClosed();

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(
                  kHatsSurveyTriggerAutoPipAllowed, web_contents(), _,
                  AllOf(Contains(Pair("AutoPip Reason", "VideoConferencing")),
                        Contains(Pair("Opener site URL", test_url.spec())),
                        Contains(Pair("Pip window duration", "15s")),
                        Contains(Pair("Prompt Result", "AllowOnce"))),
                  _, _, _, _));

  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest, DoesNotRecordUrlWhenUkmDisabled) {
  auto feature_list =
      CreateFinchScopedFeatureList("VideoConferencing", "AllowOnce");

  GURL test_url("https://example.com/");
  profile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing, test_url);
  service()->SetPromptResult(PromptResult::kAllowOnce);
  service()->AutoPictureInPictureWindowClosed();

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(
                  _, _, _, Contains(Pair("Opener site URL", "")), _, _, _, _));

  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       MediaPlaybackAllowedTriggersOnAnyAllowedResult) {
  // Finch targets "AllowOnce", but user chooses "AllowOnEveryVisit"
  auto feature_list =
      CreateFinchScopedFeatureList("MediaPlayback", "AllowOnce");
  GURL test_url("https://example.com");

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback, test_url);
  service()->SetPromptResult(PromptResult::kAllowOnEveryVisit);
  service()->AutoPictureInPictureWindowClosed();

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(kHatsSurveyTriggerAutoPipAllowed,
                                         web_contents(), _, _, _, _, _, _));

  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       MaybeLaunchSurveyWithoutContextIsNoOp) {
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(_, _, _, _, _, _, _, _))
      .Times(0);
  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest, MaybeLaunchSurveyAfterResetIsNoOp) {
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));
  service()->AutoPictureInPictureWindowClosed();

  // First call (fails because no prompt result, resets context).
  service()->MaybeLaunchSurvey(web_contents());
  EXPECT_FALSE(service()->get_active_window_context_for_testing().has_value());

  // Second call is no-op.
  service()->MaybeLaunchSurvey(web_contents());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       MaybeLaunchSurveyCalledTwiceBeforeWindowClose) {
  auto feature_list =
      CreateFinchScopedFeatureList("VideoConferencing", "AllowOnce");

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));
  service()->SetPromptResult(PromptResult::kAllowOnce);

  // Both calls should do nothing because window is not closed.
  service()->MaybeLaunchSurvey(web_contents());
  service()->MaybeLaunchSurvey(web_contents());

  EXPECT_TRUE(service()->get_active_window_context_for_testing().has_value());
  EXPECT_FALSE(service()
                   ->get_active_window_context_for_testing()
                   ->window_duration.has_value());
}

TEST_F(AutoPictureInPictureHatsServiceTest, PSDFieldsMatchConfig) {
  auto feature_list =
      CreateFinchScopedFeatureList("VideoConferencing", "AllowOnce");
  hats::SurveyConfigs survey_configs;
  hats::GetActiveSurveyConfigs(survey_configs);

  GURL test_url("https://example.com/");
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing, test_url);
  service()->SetPromptResult(PromptResult::kAllowOnce);
  service()->AutoPictureInPictureWindowClosed();

  // We want to capture the PSD data passed to LaunchSurveyForWebContents.
  SurveyStringData captured_psd;
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurveyForWebContents(kHatsSurveyTriggerAutoPipAllowed,
                                         web_contents(), _, _, _, _, _, _))
      .WillOnce(testing::SaveArg<3>(&captured_psd));

  service()->MaybeLaunchSurvey(web_contents());

  // Verify that the PSD keys provided by the service exactly match those
  // defined in the survey configuration (bidirectional match). This is
  // important because the real HaTS service performs a CHECK that the keys
  // match exactly at runtime.
  ASSERT_TRUE(survey_configs.contains(kHatsSurveyTriggerAutoPipAllowed));
  const auto& config = survey_configs.at(kHatsSurveyTriggerAutoPipAllowed);

  std::vector<std::string> captured_keys;
  for (const auto& [key, value] : captured_psd) {
    captured_keys.push_back(key);
  }
  EXPECT_THAT(captured_keys, testing::UnorderedElementsAreArray(
                                 config.product_specific_string_data_fields));
}
