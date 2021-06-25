// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/util/values/values_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/permission_actions_history.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
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
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromePermissionRequestManagerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        request1_("test1",
                  permissions::RequestType::kDiskQuota,
                  permissions::PermissionRequestGestureType::GESTURE),
        request2_("test2",
                  permissions::RequestType::kMultipleDownloads,
                  permissions::PermissionRequestGestureType::NO_GESTURE),
        request_mic_("mic",
                     permissions::RequestType::kMicStream,
                     permissions::PermissionRequestGestureType::NO_GESTURE),
        request_camera_("cam",
                        permissions::RequestType::kCameraStream,
                        permissions::PermissionRequestGestureType::NO_GESTURE) {
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    url_ = GURL("http://www.google.com");
    NavigateAndCommit(url_);

    site_engagement::SiteEngagementService::Get(profile())
        ->ResetBaseScoreForURL(url_, kTestEngagementScore);

    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
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

  void Deny() {
    manager_->Deny();
    base::RunLoop().RunUntilIdle();
  }

  void Closing() {
    manager_->Closing();
    base::RunLoop().RunUntilIdle();
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInMainFrame(main_rfh());
    base::RunLoop().RunUntilIdle();
  }

  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) {
    manager_->NavigationEntryCommitted(details);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
        /*text*/ "test", permissions::RequestType::kGeolocation, url);
    manager_->AddRequest(web_contents()->GetMainFrame(), request.get());
    return request;
  }
#endif

 protected:
  GURL url_;
  permissions::MockPermissionRequest request1_;
  permissions::MockPermissionRequest request2_;
  permissions::MockPermissionRequest request_mic_;
  permissions::MockPermissionRequest request_camera_;
  permissions::PermissionRequestManager* manager_;
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
};

TEST_F(ChromePermissionRequestManagerTest, UMAForSimpleAcceptedGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::QUOTA),
      1);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::QUOTA),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);
  histograms.ExpectTotalCount("Permissions.Engagement.Accepted.Quota", 0);

  Accept();
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::QUOTA),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied, 0);

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAcceptedGesture,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::QUOTA),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptAcceptedNoGesture, 0);
  histograms.ExpectUniqueSample("Permissions.Engagement.Accepted.Quota",
                                kTestEngagementScore, 1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForSimpleDeniedNoGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
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

  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::MULTIPLE),
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
          permissions::RequestTypeForUma::MULTIPLE),
      1);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Accepted.AudioAndVideoCapture",
      kTestEngagementScore, 1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForMergedDeniedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();
  histograms.ExpectTotalCount(
      "Permissions.Engagement.Denied.AudioAndVideoCapture", 0);
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForMergedAcceptedBubble.

  Deny();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample>(
          permissions::RequestTypeForUma::MULTIPLE),
      1);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Denied.AudioAndVideoCapture",
      kTestEngagementScore, 1);
}

TEST_F(ChromePermissionRequestManagerTest, UMAForIgnores) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectTotalCount("Permissions.Engagement.Ignored.Quota", 0);

  GURL youtube("http://www.youtube.com/");
  NavigateAndCommit(youtube);
  histograms.ExpectUniqueSample("Permissions.Engagement.Ignored.Quota",
                                kTestEngagementScore, 1);

  permissions::MockPermissionRequest youtube_request(
      "request2", permissions::RequestType::kGeolocation, youtube);
  manager_->AddRequest(web_contents()->GetMainFrame(), &youtube_request);
  WaitForBubbleToBeShown();

  NavigateAndCommit(GURL("http://www.google.com/"));
  histograms.ExpectUniqueSample("Permissions.Engagement.Ignored.Geolocation", 0,
                                1);
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
  base::Optional<bool> has_three_consecutive_denies =
      permissions::PermissionsClient::Get()
          ->HadThreeConsecutiveNotificationPermissionDenies(profile());
  ASSERT_TRUE(has_three_consecutive_denies.has_value());
  EXPECT_FALSE(has_three_consecutive_denies.value());

  for (const char* origin_spec :
       {"https://a.com", "https://b.com", "https://c.com"}) {
    GURL requesting_origin(origin_spec);
    NavigateAndCommit(requesting_origin);
    permissions::MockPermissionRequest notification_request(
        "request", permissions::RequestType::kNotifications, requesting_origin);
    manager_->AddRequest(web_contents()->GetMainFrame(), &notification_request);
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
        "request", permissions::RequestType::kNotifications, requesting_origin);
    manager_->AddRequest(web_contents()->GetMainFrame(), &notification_request);
    WaitForBubbleToBeShown();
    // Only show quiet UI after 3 consecutive denies of the permission prompt.
    EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
    Deny();
  }
  auto entries = ukm_recorder.GetEntriesByName("Permission");
  ASSERT_EQ(4u, entries.size());
  auto* entry = entries.back();
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "SatisfiedAdaptiveTriggers"),
            1);

  GURL requesting_origin("http://www.notification2.com/");
  NavigateAndCommit(requesting_origin);
  permissions::MockPermissionRequest notification_request(
      "request2", permissions::RequestType::kNotifications, requesting_origin);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification_request);
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
      base::TimeDelta::FromDays(90),  // Default value.
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
      base::TimeDelta::FromDays(7),
      QuietNotificationPermissionUiConfig::GetAdaptiveActivationWindowSize());

  const char* origin_spec[]{"https://a.com", "https://b.com", "https://c.com",
                            "https://d.com"};
  for (int i = 0; i < 4; ++i) {
    GURL requesting_origin(origin_spec[i]);
    NavigateAndCommit(requesting_origin);
    permissions::MockPermissionRequest notification_request(
        "request", permissions::RequestType::kNotifications, requesting_origin);
    manager_->AddRequest(web_contents()->GetMainFrame(), &notification_request);
    WaitForBubbleToBeShown();
    EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
    Deny();

    if (i == 0) {
      // The history window size is 7 days. That will ignore previous denied
      // permission request as obsolete.
      task_environment()->AdvanceClock(base::TimeDelta::FromDays(10));
    }
  }

  GURL requesting_origin("http://www.notification.com/");
  NavigateAndCommit(requesting_origin);
  permissions::MockPermissionRequest notification_request(
      "request", permissions::RequestType::kNotifications, requesting_origin);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification_request);
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
      "request1", permissions::RequestType::kNotifications, notification1);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification1_request);
  WaitForBubbleToBeShown();
  Deny();

  GURL notification2("http://www.notification2.com/");
  NavigateAndCommit(notification2);
  permissions::MockPermissionRequest notification2_request(
      "request2", permissions::RequestType::kNotifications, notification2);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification2_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification3("http://www.notification3.com/");
  NavigateAndCommit(notification3);
  permissions::MockPermissionRequest notification3_request(
      "request3", permissions::RequestType::kNotifications, notification3);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification3_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  // Only show quiet UI after 3 consecutive denies of the permission prompt.
  GURL notification4("http://www.notification4.com/");
  NavigateAndCommit(notification4);
  permissions::MockPermissionRequest notification4_request(
      "request4", permissions::RequestType::kNotifications, notification4);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification4_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification5("http://www.notification5.com/");
  NavigateAndCommit(notification5);
  permissions::MockPermissionRequest notification5_request(
      "request5", permissions::RequestType::kNotifications, notification5);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification5_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  // Only denying the notification permission should count toward the threshold,
  // other permissions should not.
  GURL camera_url("http://www.camera.com/");
  NavigateAndCommit(camera_url);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL microphone_url("http://www.microphone.com/");
  NavigateAndCommit(microphone_url);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  GURL notification6("http://www.notification6.com/");
  NavigateAndCommit(notification6);
  permissions::MockPermissionRequest notification6_request(
      "request6", permissions::RequestType::kNotifications, notification6);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification6_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  // After the 3rd consecutive denies, show the quieter version of the
  // permission prompt.
  GURL notification7("http://www.notification7.com/");
  NavigateAndCommit(notification7);
  permissions::MockPermissionRequest notification7_request(
      "request7", permissions::RequestType::kNotifications, notification7);
  // For the first quiet permission prompt, show a promo.
  EXPECT_TRUE(QuietNotificationPermissionUiState::ShouldShowPromo(profile()));
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification7_request);
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
      "request8", permissions::RequestType::kNotifications, notification8);
  // For the rest of the quiet permission prompts, do not show promo.
  EXPECT_TRUE(QuietNotificationPermissionUiState::ShouldShowPromo(profile()));
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification8_request);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());

  // Clearing interaction history, or turning off quiet mode in preferences does
  // not change the state of the currently showing quiet UI.
  PermissionActionsHistory::GetForProfile(profile())->ClearHistory(
      base::Time(), base::Time::Max());
  profile()->GetPrefs()->ClearPref(prefs::kEnableQuietNotificationPermissionUi);
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  base::Time recorded_time = base::Time::Now();
  task_environment()->AdvanceClock(base::TimeDelta::FromDays(1));
  base::Time from_time = base::Time::Now();
  GURL notification9("http://www.notification9.com/");
  NavigateAndCommit(notification9);
  permissions::MockPermissionRequest notification9_request(
      "request9", permissions::RequestType::kNotifications, notification9);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification9_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  task_environment()->AdvanceClock(base::TimeDelta::FromDays(1));
  base::Time to_time = base::Time::Now();
  GURL notification10("http://www.notification10.com/");
  NavigateAndCommit(notification10);
  permissions::MockPermissionRequest notification10_request(
      "request10", permissions::RequestType::kNotifications, notification10);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification10_request);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Deny();

  task_environment()->AdvanceClock(base::TimeDelta::FromDays(1));
  GURL notification11("http://www.notification11.com/");
  NavigateAndCommit(notification11);
  permissions::MockPermissionRequest notification11_request(
      "request11", permissions::RequestType::kNotifications, notification11);
  manager_->AddRequest(web_contents()->GetMainFrame(), &notification11_request);
  WaitForBubbleToBeShown();
  Deny();

  DictionaryPrefUpdate update(profile()->GetPrefs(), prefs::kPermissionActions);
  const auto permissions_actions =
      update->FindListPath("notifications")->GetList();
  PermissionActionsHistory::GetForProfile(profile())->ClearHistory(from_time,
                                                                   to_time);

  // Check that we have cleared all entries >= |from_time| and <|end_time|.
  EXPECT_EQ(permissions_actions.size(), 3u);
  EXPECT_EQ((util::ValueToTime(permissions_actions[0].FindKey("time")))
                .value_or(base::Time()),
            recorded_time);
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
