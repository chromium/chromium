// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/pref_names.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#endif

const double kTestEngagementScore = 29;

class ChromePermissionRequestManagerTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<std::pair<std::string, bool>> {
 public:
  ChromePermissionRequestManagerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        request1_(permissions::RequestType::kGeolocation,
                  permissions::PermissionRequestGestureType::GESTURE),
        request2_(permissions::RequestType::kMultipleDownloads,
                  permissions::PermissionRequestGestureType::NO_GESTURE),
        request_mic_(permissions::RequestType::kMicStream,
                     permissions::PermissionRequestGestureType::NO_GESTURE),
        request_camera_(permissions::RequestType::kCameraStream,
                        permissions::PermissionRequestGestureType::NO_GESTURE) {
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL(permissions::MockPermissionRequest::kDefaultOrigin));

    site_engagement::SiteEngagementService::Get(profile())
        ->ResetBaseScoreForURL(
            GURL(permissions::MockPermissionRequest::kDefaultOrigin),
            kTestEngagementScore);

    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    manager_->set_enabled_app_level_notification_permission_for_testing(true);
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager_);
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void Accept() {
    manager_->Accept();
    base::RunLoop().RunUntilIdle();
  }

  void AcceptThisTime() {
    manager_->AcceptThisTime();
    base::RunLoop().RunUntilIdle();
  }

  void Deny() {
    manager_->Deny();
    base::RunLoop().RunUntilIdle();
  }

  void Closing() {
    manager_->Dismiss();
    base::RunLoop().RunUntilIdle();
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInPrimaryMainFrame();
    base::RunLoop().RunUntilIdle();
  }

  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) {
    manager_->NavigationEntryCommitted(details);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetKioskBrowserPermissionsAllowedForOrigins(const std::string& origin) {
    profile()->GetPrefs()->SetList(
        prefs::kKioskBrowserPermissionsAllowedForOrigins,
        base::Value::List().Append(std::move(origin)));
  }

  std::unique_ptr<permissions::MockPermissionRequest> MakeRequestInWebKioskMode(
      const GURL& url,
      const GURL& app_url) {
    const AccountId account_id = AccountId::FromUserEmail("lala@example.com");

    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    // Stealing the pointer from unique ptr before it goes to the scoped user
    // manager.
    ash::FakeChromeUserManager* user_manager = fake_user_manager.get();
    auto scoped_user_manager =
        std::make_unique<user_manager::ScopedUserManager>(
            std::move(fake_user_manager));
    user_manager->AddWebKioskAppUser(account_id);
    user_manager->LoginUser(account_id);

    auto kiosk_app_manager = std::make_unique<ash::WebKioskAppManager>();
    kiosk_app_manager->AddAppForTesting(account_id, app_url);

    NavigateAndCommit(url);
    auto request = std::make_unique<permissions::MockPermissionRequest>(
        url, permissions::RequestType::kGeolocation);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), request.get());
    return request;
  }
#endif

 protected:
  permissions::MockPermissionRequest request1_;
  permissions::MockPermissionRequest request2_;
  permissions::MockPermissionRequest request_mic_;
  permissions::MockPermissionRequest request_camera_;
  raw_ptr<permissions::PermissionRequestManager, DanglingUntriaged> manager_;
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
};

TEST_F(ChromePermissionRequestManagerTest, UMAForSimpleAcceptedGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::PERMISSION_GEOLOCATION),
      1);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::PERMISSION_GEOLOCATION),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);
  histograms.ExpectTotalCount("Permissions.Engagement.Accepted.Geolocation", 0);

  Accept();
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::PERMISSION_GEOLOCATION),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied, 0);

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAcceptedGesture,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::PERMISSION_GEOLOCATION),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptAcceptedNoGesture, 0);
  histograms.ExpectUniqueSample("Permissions.Engagement.Accepted.Geolocation",
                                kTestEngagementScore, 1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForSimpleDeniedNoGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request2_);
  WaitForBubbleToBeShown();

  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::DOWNLOAD),
      1);
  histograms.ExpectTotalCount("Permissions.Engagement.Denied.MultipleDownload",
                              0);
  // No need to test the other UMA for showing prompts again, they were tested
  // in UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted, 0);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::DOWNLOAD),
      1);

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDeniedNoGesture,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::DOWNLOAD),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptDeniedGesture, 0);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Denied.MultipleDownload", kTestEngagementScore,
      1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForMergedAcceptedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);
  histograms.ExpectTotalCount(
      "Permissions.Engagement.Accepted.AudioAndVideoCapture", 0);

  Accept();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE),
      1);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Accepted.AudioAndVideoCapture",
      kTestEngagementScore, 1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForMergedDeniedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();
  histograms.ExpectTotalCount(
      "Permissions.Engagement.Denied.AudioAndVideoCapture", 0);
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForMergedAcceptedBubble.

  Deny();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE),
      1);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Denied.AudioAndVideoCapture",
      kTestEngagementScore, 1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForIgnores) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectTotalCount("Permissions.Engagement.Ignored.Geolocation", 0);

  GURL youtube("http://www.youtube.com/");
  NavigateAndCommit(youtube);
  histograms.ExpectUniqueSample("Permissions.Engagement.Ignored.Geolocation",
                                kTestEngagementScore, 1);

  permissions::MockPermissionRequest youtube_request(
      youtube, permissions::RequestType::kCameraStream);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &youtube_request);
  WaitForBubbleToBeShown();

  NavigateAndCommit(GURL(permissions::MockPermissionRequest::kDefaultOrigin));
  histograms.ExpectUniqueSample("Permissions.Engagement.Ignored.VideoCapture",
                                0, 1);
}

TEST_F(ChromePermissionRequestManagerTest,
       NotificationsAdaptiveActivationQuietUIDryRunUKM) {
  ASSERT_FALSE(
      QuietNotificationPermissionUiConfig::IsAdaptiveActivationDryRunEnabled());
  ASSERT_FALSE(permissions::PermissionsClient::Get()
                   ->HadThreeConsecutiveNotificationPermissionDenies(profile())
                   .has_value());

  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
          "true"},
         {QuietNotificationPermissionUiConfig::kEnableAdaptiveActivationDryRun,
          "true"}}}},
      {});

  ASSERT_TRUE(
      QuietNotificationPermissionUiConfig::IsAdaptiveActivationDryRunEnabled());
  std::optional<bool> has_three_consecutive_denies =
      permissions::PermissionsClient::Get()
          ->HadThreeConsecutiveNotificationPermissionDenies(profile());
  ASSERT_TRUE(has_three_consecutive_denies.has_value());
  EXPECT_FALSE(has_three_consecutive_denies.value());

  for (const char* origin_spec :
       {"https://a.com", "https://b.com", "https://c.com"}) {
    GURL requesting_origin(origin_spec);
    NavigateAndCommit(requesting_origin);
    permissions::MockPermissionRequest notification_request(
        requesting_origin, permissions::RequestType::kNotifications);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         &notification_request);
    WaitForBubbleToBeShown();
    EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
    Deny();
  }

  //  It verifies that the transition from FALSE->TRUE indeed happens after the
  //  third deny, so there aren't off-by-one errors.
  has_three_consecutive_denies =
      permissions::PermissionsClient::Get()
          ->HadThreeConsecutiveNotificationPermissionDenies(profile());
  ASSERT_TRUE(has_three_consecutive_denies.has_value());
  EXPECT_TRUE(has_three_consecutive_denies.value());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi));

  {
    GURL requesting_origin("http://www.notification.com/");
    NavigateAndCommit(requesting_origin);
    permissions::MockPermissionRequest notification_request(
        requesting_origin, permissions::RequestType::kNotifications);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         &notification_request);
    WaitForBubbleToBeShown();
    // Only show quiet UI after 3 consecutive denies of the permission prompt.
    EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
    Deny();
  }
  auto entries = ukm_recorder.GetEntriesByName("Permission");
  ASSERT_EQ(4u, entries.size());
  auto* entry = entries.back().get();
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "SatisfiedAdaptiveTriggers"),
            1);

  GURL requesting_origin("http://www.notification2.com/");
  NavigateAndCommit(requesting_origin);
  permissions::MockPermissionRequest notification_request(
      requesting_origin, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification_request);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  // Verify that an "ALLOW" response does not reset the state.
  has_three_consecutive_denies =
      permissions::PermissionsClient::Get()
          ->HadThreeConsecutiveNotificationPermissionDenies(profile());
  ASSERT_TRUE(has_three_consecutive_denies.has_value());
  EXPECT_TRUE(has_three_consecutive_denies.value());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi));
}

TEST_F(ChromePermissionRequestManagerTest,
       NotificationsAdaptiveActivationQuietUIWindowSize) {
  EXPECT_EQ(
      base::Days(90),  // Default value.
      QuietNotificationPermissionUiConfig::GetAdaptiveActivationWindowSize());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
          "true"},
         {QuietNotificationPermissionUiConfig::
              kAdaptiveActivationActionWindowSizeInDays,
          "7"}}}},
      {});

  ASSERT_EQ(
      base::Days(7),
      QuietNotificationPermissionUiConfig::GetAdaptiveActivationWindowSize());

  const char* origin_spec[]{"https://a.com", "https://b.com", "https://c.com",
                            "https://d.com"};
  for (int i = 0; i < 4; ++i) {
    GURL requesting_origin(origin_spec[i]);
    NavigateAndCommit(requesting_origin);
    permissions::MockPermissionRequest notification_request(
        requesting_origin, permissions::RequestType::kNotifications);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         &notification_request);
    WaitForBubbleToBeShown();
    EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
    Deny();

    if (i == 0) {
      // The history window size is 7 days. That will ignore previous denied
      // permission request as obsolete.
      task_environment()->AdvanceClock(base::Days(10));
    }
  }

  GURL requesting_origin("http://www.notification.com/");
  NavigateAndCommit(requesting_origin);
  permissions::MockPermissionRequest notification_request(
      requesting_origin, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification_request);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi));
  Deny();
}

TEST_F(ChromePermissionRequestManagerTest,
       NotificationsUnderClientSideEmbargoAfterSeveralDenies) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
          "true"}}}},
      {permissions::features::kBlockRepeatedNotificationPermissionPrompts});

  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi));
  // TODO(hkamila): Collapse the below blocks into a single for statement.
  GURL notification1("http://www.notification1.com/");
  NavigateAndCommit(notification1);
  permissions::MockPermissionRequest notification1_request(
      notification1, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification1_request);
  WaitForBubbleToBeShown();
  Deny();

  GURL notification2("http://www.notification2.com/");
  NavigateAndCommit(notification2);
  permissions::MockPermissionRequest notification2_request(
      notification2, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification2_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification3("http://www.notification3.com/");
  NavigateAndCommit(notification3);
  permissions::MockPermissionRequest notification3_request(
      notification3, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification3_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  // Only show quiet UI after 3 consecutive denies of the permission prompt.
  GURL notification4("http://www.notification4.com/");
  NavigateAndCommit(notification4);
  permissions::MockPermissionRequest notification4_request(
      notification4, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification4_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification5("http://www.notification5.com/");
  NavigateAndCommit(notification5);
  permissions::MockPermissionRequest notification5_request(
      notification5, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification5_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  // Only denying the notification permission should count toward the threshold,
  // other permissions should not.
  GURL camera_url("http://www.camera.com/");
  NavigateAndCommit(camera_url);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL microphone_url("http://www.microphone.com/");
  NavigateAndCommit(microphone_url);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification6("http://www.notification6.com/");
  NavigateAndCommit(notification6);
  permissions::MockPermissionRequest notification6_request(
      notification6, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification6_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  // After the 3rd consecutive denies, show the quieter version of the
  // permission prompt.
  GURL notification7("http://www.notification7.com/");
  NavigateAndCommit(notification7);
  permissions::MockPermissionRequest notification7_request(
      notification7, permissions::RequestType::kNotifications);
  // For the first quiet permission prompt, show a promo.
  EXPECT_TRUE(QuietNotificationPermissionUiState::ShouldShowPromo(profile()));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification7_request);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi));
  Accept();

  // One accept through the quiet UI, doesn't switch the user back to the
  // disabled state once the permission is set.
  GURL notification8("http://www.notification8.com/");
  NavigateAndCommit(notification8);
  permissions::MockPermissionRequest notification8_request(
      notification8, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification8_request);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());

  // Clearing interaction history, or turning off quiet mode in preferences does
  // not change the state of the currently showing quiet UI.
  PermissionActionsHistoryFactory::GetForProfile(profile())->ClearHistory(
      base::Time(), base::Time::Max());
  profile()->GetPrefs()->ClearPref(prefs::kEnableQuietNotificationPermissionUi);
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  base::Time recorded_time = base::Time::Now();
  task_environment()->AdvanceClock(base::Days(1));
  base::Time from_time = base::Time::Now();
  GURL notification9("http://www.notification9.com/");
  NavigateAndCommit(notification9);
  permissions::MockPermissionRequest notification9_request(
      notification9, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification9_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  task_environment()->AdvanceClock(base::Days(1));
  base::Time to_time = base::Time::Now();
  GURL notification10("http://www.notification10.com/");
  NavigateAndCommit(notification10);
  permissions::MockPermissionRequest notification10_request(
      notification10, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification10_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  task_environment()->AdvanceClock(base::Days(1));
  GURL notification11("http://www.notification11.com/");
  NavigateAndCommit(notification11);
  permissions::MockPermissionRequest notification11_request(
      notification11, permissions::RequestType::kNotifications);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       &notification11_request);
  WaitForBubbleToBeShown();
  Deny();

  ScopedDictPrefUpdate update(profile()->GetPrefs(),
                              permissions::prefs::kPermissionActions);
  const auto& permissions_actions = *update->FindList("notifications");
  PermissionActionsHistoryFactory::GetForProfile(profile())->ClearHistory(
      from_time, to_time);

  // Check that we have cleared all entries >= |from_time| and <|end_time|.
  EXPECT_EQ(permissions_actions.size(), 3u);
  EXPECT_EQ((base::ValueToTime(permissions_actions[0].GetDict().Find("time")))
                .value_or(base::Time()),
            recorded_time);
}

TEST_F(ChromePermissionRequestManagerTest,
       TestEmbargoForEmbeddedPermissionRequest) {
  GURL url(permissions::MockPermissionRequest::kDefaultOrigin);
  permissions::RequestType request_type =
      permissions::RequestType::kCameraStream;
  permissions::PermissionDecisionAutoBlocker* autoblocker =
      permissions::PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          browser_context());

  // Do not count permission element requests towards embargo
  {
    permissions::MockPermissionRequest request(
        request_type, /* embedded_permission_element_initiated= */ true);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
    WaitForBubbleToBeShown();
    Closing();

    EXPECT_EQ(
        autoblocker->GetDismissCount(url, request.GetContentSettingsType()), 0);
  }

  // Count normal permission towards embargo (used in next step)
  {
    permissions::MockPermissionRequest request(
        request_type, /* embedded_permission_element_initiated= */ false);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
    WaitForBubbleToBeShown();
    Closing();

    EXPECT_EQ(
        autoblocker->GetDismissCount(url, request.GetContentSettingsType()), 1);
  }

  // Reset embargo counter when accepting this time and using permission element
  {
    permissions::MockPermissionRequest request(
        request_type, /* embedded_permission_element_initiated= */ true);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(), &request);
    WaitForBubbleToBeShown();
    AcceptThisTime();

    EXPECT_EQ(
        autoblocker->GetDismissCount(url, request.GetContentSettingsType()), 0);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromePermissionRequestManagerTest, TestWebKioskModeSameOrigin) {
  auto request =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://google.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();
  // It should be granted by default.
  EXPECT_TRUE(request->granted());
}

TEST_F(ChromePermissionRequestManagerTest, TestWebKioskModeDifferentOrigin) {
  auto request =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://example.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();
  // It should not be granted by default.
  EXPECT_FALSE(request->granted());
  EXPECT_TRUE(request->finished());
}

TEST_F(ChromePermissionRequestManagerTest,
       TestWebKioskModeDifferentOriginWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      permissions::features::kAllowMultipleOriginsForWebKioskPermissions);
  SetKioskBrowserPermissionsAllowedForOrigins("https://example.com/page");

  auto request =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://example.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();

  // It should not be granted as the origin is allowlisted.
  EXPECT_EQ(request->granted(), false);
  EXPECT_TRUE(request->finished());
}

TEST_P(ChromePermissionRequestManagerTest,
       TestWebKioskModeDifferentOriginWhenAllowedByFeature) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams feature_params;
  feature_params
      [permissions::feature_params::kWebKioskBrowserPermissionsAllowlist.name] =
          GetParam().first;
  feature_list.InitAndEnableFeatureWithParameters(
      permissions::features::kAllowMultipleOriginsForWebKioskPermissions,
      feature_params);

  auto request =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://example.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();

  // It should be granted as the origin is allowlisted.
  EXPECT_EQ(request->granted(), GetParam().second);
  EXPECT_TRUE(request->finished());
}

TEST_P(ChromePermissionRequestManagerTest,
       TestWebKioskModeDifferentOriginAllowedByKioskBrowserPref) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      permissions::features::kAllowMultipleOriginsForWebKioskPermissions);
  SetKioskBrowserPermissionsAllowedForOrigins(GetParam().first);

  auto request =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://example.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();

  // It should be granted as the origin is allowlisted.
  EXPECT_EQ(request->granted(), GetParam().second);
  EXPECT_TRUE(request->finished());
}

INSTANTIATE_TEST_SUITE_P(
    TestWebKioskModeDifferentOriginWhenAllowedByFeature,
    ChromePermissionRequestManagerTest,
    testing::ValuesIn(
        {std::pair<std::string, bool>("*", false),
         std::pair<std::string, bool>(".example.com", false),
         std::pair<std::string, bool>("example.", false),
         std::pair<std::string, bool>("file://example*", false),
         std::pair<std::string, bool>("invalid-example.com", false),
         std::pair<std::string, bool>("https://example.com", true),
         std::pair<std::string, bool>("https://example.com/sample", true),
         std::pair<std::string, bool>("example.com", true),
         std::pair<std::string, bool>("*://example.com:*/", true),
         std::pair<std::string, bool>("[*.]example.com", true)}));

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class ChromePermissionRequestManagerAdaptiveQuietUiActivationTest
    : public ChromePermissionRequestManagerTest {
 public:
  ChromePermissionRequestManagerAdaptiveQuietUiActivationTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kQuietNotificationPrompts,
          {{QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation,
            "true"}}}},
        {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ChromePermissionRequestManagerAdaptiveQuietUiActivationTest,
       EnableDisabledQuietNotificationToggleUponThreeConsecutiveBlocks) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi));

  for (const char* origin_spec :
       {"https://a.com", "https://b.com", "https://c.com"}) {
    GURL requesting_origin(origin_spec);
    NavigateAndCommit(requesting_origin);
    permissions::MockPermissionRequest notification_request(
        requesting_origin, permissions::RequestType::kNotifications);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         &notification_request);
    WaitForBubbleToBeShown();
    Deny();
  }

  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi));
}
