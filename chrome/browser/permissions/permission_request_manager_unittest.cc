// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_manager.h"

#include <stddef.h>

#include <memory>
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
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
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
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#endif

const double kTestEngagementScore = 29;

class PermissionRequestManagerTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<std::pair<std::string, bool>> {
 public:
  PermissionRequestManagerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        params_request1_(permissions::RequestType::kGeolocation,
                         permissions::PermissionRequestGestureType::GESTURE),
        params_request2_(permissions::RequestType::kMultipleDownloads,
                         permissions::PermissionRequestGestureType::NO_GESTURE),
        params_request_mic_(
            permissions::RequestType::kMicStream,
            permissions::PermissionRequestGestureType::NO_GESTURE),
        params_request_camera_(
            permissions::RequestType::kCameraStream,
            permissions::PermissionRequestGestureType::NO_GESTURE) {}

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

#if BUILDFLAG(IS_CHROMEOS)
  void SetKioskBrowserPermissionsAllowedForOrigins(const std::string& origin) {
    profile()->GetPrefs()->SetList(
        prefs::kKioskBrowserPermissionsAllowedForOrigins,
        base::Value::List().Append(std::move(origin)));
  }

  std::unique_ptr<
      permissions::MockPermissionRequest::MockPermissionRequestState>
  MakeRequestInWebKioskMode(const GURL& url, const GURL& app_url) {
    const AccountId account_id = AccountId::FromUserEmail("lala@example.com");

    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    // Stealing the pointer from unique ptr before it goes to the scoped user
    // manager.
    ash::FakeChromeUserManager* user_manager = fake_user_manager.get();
    auto scoped_user_manager =
        std::make_unique<user_manager::ScopedUserManager>(
            std::move(fake_user_manager));
    user_manager->AddKioskWebAppUser(account_id);
    user_manager->LoginUser(account_id);

    ash::KioskCryptohomeRemover cryptohome_remover(
        TestingBrowserProcess::GetGlobal()->local_state());
    auto kiosk_app_manager = std::make_unique<ash::KioskWebAppManager>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        TestingBrowserProcess::GetGlobal()->shared_url_loader_factory(),
        &cryptohome_remover);
    kiosk_app_manager->AddAppForTesting(account_id, app_url);

    NavigateAndCommit(url);
    auto request_state = std::make_unique<
        permissions::MockPermissionRequest::MockPermissionRequestState>();
    auto request = std::make_unique<permissions::MockPermissionRequest>(
        url, permissions::RequestType::kGeolocation,
        request_state->GetWeakPtr());
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    return request_state;
  }
#endif

 protected:
  std::unique_ptr<permissions::MockPermissionRequest> CreateRequest(
      std::pair<permissions::RequestType,
                permissions::PermissionRequestGestureType> request_params) {
    return std::make_unique<permissions::MockPermissionRequest>(
        request_params.first, request_params.second);
  }

  std::pair<permissions::RequestType, permissions::PermissionRequestGestureType>
      params_request1_;
  std::pair<permissions::RequestType, permissions::PermissionRequestGestureType>
      params_request2_;
  std::pair<permissions::RequestType, permissions::PermissionRequestGestureType>
      params_request_mic_;
  std::pair<permissions::RequestType, permissions::PermissionRequestGestureType>
      params_request_camera_;
  raw_ptr<permissions::PermissionRequestManager, DanglingUntriaged> manager_;
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
};

TEST_F(PermissionRequestManagerTest, UMAForSimpleAcceptedGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request1_));
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::PERMISSION_GEOLOCATION),
      1);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::PERMISSION_GEOLOCATION),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture, 0);
  histograms.ExpectTotalCount("Permissions.Engagement.Accepted.Geolocation", 0);

  Accept();
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAccepted,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::PERMISSION_GEOLOCATION),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied, 0);

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptAcceptedGesture,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::PERMISSION_GEOLOCATION),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptAcceptedNoGesture, 0);
  histograms.ExpectUniqueSample("Permissions.Engagement.Accepted.Geolocation",
                                kTestEngagementScore, 1);
}

TEST_F(PermissionRequestManagerTest, UMAForSimpleDeniedNoGestureBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request2_));
  WaitForBubbleToBeShown();

  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptShownGesture, 0);
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShownNoGesture,
      static_cast<base::HistogramBase::Sample32>(
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
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::DOWNLOAD),
      1);

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDeniedNoGesture,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::DOWNLOAD),
      1);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::kPermissionsPromptDeniedGesture, 0);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Denied.MultipleDownload", kTestEngagementScore,
      1);
}

TEST_F(PermissionRequestManagerTest, UMAForMergedAcceptedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request_mic_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request_camera_));
  WaitForBubbleToBeShown();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptShown,
      static_cast<base::HistogramBase::Sample32>(
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
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE),
      1);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Accepted.AudioAndVideoCapture",
      kTestEngagementScore, 1);
}

TEST_F(PermissionRequestManagerTest, UMAForMergedDeniedBubble) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request_mic_));
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request_camera_));
  WaitForBubbleToBeShown();
  histograms.ExpectTotalCount(
      "Permissions.Engagement.Denied.AudioAndVideoCapture", 0);
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForMergedAcceptedBubble.

  Deny();

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::kPermissionsPromptDenied,
      static_cast<base::HistogramBase::Sample32>(
          permissions::RequestTypeForUma::MULTIPLE_AUDIO_AND_VIDEO_CAPTURE),
      1);
  histograms.ExpectUniqueSample(
      "Permissions.Engagement.Denied.AudioAndVideoCapture",
      kTestEngagementScore, 1);
}

TEST_F(PermissionRequestManagerTest, UMAForIgnores) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       CreateRequest(params_request1_));
  WaitForBubbleToBeShown();
  histograms.ExpectTotalCount("Permissions.Engagement.Ignored.Geolocation", 0);

  GURL youtube("http://www.youtube.com/");
  NavigateAndCommit(youtube);
  histograms.ExpectUniqueSample("Permissions.Engagement.Ignored.Geolocation",
                                kTestEngagementScore, 1);

  auto youtube_request = std::make_unique<permissions::MockPermissionRequest>(
      youtube, permissions::RequestType::kCameraStream);
  manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                       std::move(youtube_request));
  WaitForBubbleToBeShown();

  NavigateAndCommit(GURL(permissions::MockPermissionRequest::kDefaultOrigin));
  histograms.ExpectUniqueSample("Permissions.Engagement.Ignored.VideoCapture",
                                0, 1);
}

TEST_F(PermissionRequestManagerTest, TestEmbargoForEmbeddedPermissionRequest) {
  GURL url(permissions::MockPermissionRequest::kDefaultOrigin);
  permissions::RequestType request_type =
      permissions::RequestType::kCameraStream;
  permissions::PermissionDecisionAutoBlocker* autoblocker =
      permissions::PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          browser_context());

  // Do not count permission element requests towards embargo
  {
    auto request = std::make_unique<permissions::MockPermissionRequest>(
        GURL(permissions::MockPermissionRequest::kDefaultOrigin), request_type,
        /* embedded_permission_element_initiated= */ true);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();
    Closing();

    EXPECT_EQ(
        autoblocker->GetDismissCount(
            url, permissions::RequestTypeToContentSettingsType(request_type)
                     .value()),
        0);
  }

  // Count normal permission towards embargo (used in next step)
  {
    auto request = std::make_unique<permissions::MockPermissionRequest>(
        GURL(permissions::MockPermissionRequest::kDefaultOrigin), request_type,
        /* embedded_permission_element_initiated= */ false);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();
    Closing();

    EXPECT_EQ(
        autoblocker->GetDismissCount(
            url, permissions::RequestTypeToContentSettingsType(request_type)
                     .value()),
        1);
  }

  // Reset embargo counter when accepting this time and using permission element
  {
    auto request = std::make_unique<permissions::MockPermissionRequest>(
        GURL(permissions::MockPermissionRequest::kDefaultOrigin), request_type,
        /* embedded_permission_element_initiated= */ true);
    manager_->AddRequest(web_contents()->GetPrimaryMainFrame(),
                         std::move(request));
    WaitForBubbleToBeShown();
    AcceptThisTime();

    EXPECT_EQ(
        autoblocker->GetDismissCount(
            url, permissions::RequestTypeToContentSettingsType(request_type)
                     .value()),
        0);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PermissionRequestManagerTest, TestWebKioskModeSameOrigin) {
  auto request_state =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://google.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();
  // It should be granted by default.
  EXPECT_TRUE(request_state->granted);
}

TEST_F(PermissionRequestManagerTest, TestWebKioskModeDifferentOrigin) {
  auto request_state =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://example.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();
  // It should not be granted by default.
  EXPECT_FALSE(request_state->granted);
  EXPECT_TRUE(request_state->finished);
}

TEST_F(PermissionRequestManagerTest,
       TestWebKioskModeDifferentOriginWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      permissions::features::kAllowMultipleOriginsForWebKioskPermissions);
  SetKioskBrowserPermissionsAllowedForOrigins("https://example.com/page");

  auto request_state =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://example.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();

  // It should not be granted as the origin is allowlisted.
  EXPECT_EQ(request_state->granted, false);
  EXPECT_TRUE(request_state->finished);
}

TEST_P(PermissionRequestManagerTest,
       TestWebKioskModeDifferentOriginWhenAllowedByFeature) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams feature_params;
  feature_params
      [permissions::feature_params::kWebKioskBrowserPermissionsAllowlist.name] =
          GetParam().first;
  feature_list.InitAndEnableFeatureWithParameters(
      permissions::features::kAllowMultipleOriginsForWebKioskPermissions,
      feature_params);

  auto request_state =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://example.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();

  // It should be granted as the origin is allowlisted.
  EXPECT_EQ(request_state->granted, GetParam().second);
  EXPECT_TRUE(request_state->finished);
}

TEST_P(PermissionRequestManagerTest,
       TestWebKioskModeDifferentOriginAllowedByKioskBrowserPref) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      permissions::features::kAllowMultipleOriginsForWebKioskPermissions);
  SetKioskBrowserPermissionsAllowedForOrigins(GetParam().first);

  auto request_state =
      MakeRequestInWebKioskMode(/*url*/ GURL("https://example.com/page"),
                                /*app_url*/ GURL("https://google.com/launch"));

  WaitForBubbleToBeShown();

  // It should be granted as the origin is allowlisted.
  EXPECT_EQ(request_state->granted, GetParam().second);
  EXPECT_TRUE(request_state->finished);
}

INSTANTIATE_TEST_SUITE_P(
    TestWebKioskModeDifferentOriginWhenAllowedByFeature,
    PermissionRequestManagerTest,
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

#endif  // BUILDFLAG(IS_CHROMEOS)

class ChromePermissionRequestManagerAdaptiveQuietUiActivationTest
    : public PermissionRequestManagerTest {
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
