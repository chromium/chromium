// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/selection/inline_cue_blocklist_utils.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

class MockStatusIcon : public StatusIcon {
 public:
  explicit MockStatusIcon(const std::u16string& tool_tip)
      : tool_tip_(tool_tip) {}
  void SetImage(const gfx::ImageSkia& image) override {}
  void SetToolTip(const std::u16string& tool_tip) override {
    tool_tip_ = tool_tip;
  }
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override {}
  void UpdatePlatformContextMenu(StatusIconMenuModel* menu) override {
    menu_item_ = menu;
  }
  const std::u16string& tool_tip() const { return tool_tip_; }
  StatusIconMenuModel* menu_item() const { return menu_item_; }

 private:
  raw_ptr<StatusIconMenuModel> menu_item_ = nullptr;
  std::u16string tool_tip_;
};

class MockStatusTray : public StatusTray {
 public:
  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override {
    return std::make_unique<MockStatusIcon>(tool_tip);
  }

  const StatusIcons& GetStatusIconsForTest() const { return status_icons(); }
};

class GlicMetricsTestBase : public testing::Test {
 public:
  GlicMetricsTestBase()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    startup_metric_utils::GetBrowser().ResetSessionForTesting();
    TestingBrowserProcess::GetGlobal()->SetStatusTray(
        std::make_unique<MockStatusTray>());
    raw_ptr<TestingProfileManager> testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
    startup_metric_utils::GetBrowser().RecordWebContentsStartTime(
        base::TimeTicks::Now());
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ = testing_profile_manager->CreateTestingProfile("profile");
  }

  void TearDown() override {
    // The order of some of these operations is important to avoid
    // dangling pointer crashes.
    profile_ = nullptr;

    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void ExpectEntryPointImpressionLogged(
      EntryPointStatus entry_point_impression) {
    task_environment_.FastForwardBy(base::Minutes(16));
    histogram_tester_.ExpectUniqueSample("Glic.EntryPoint.Status",
                                         entry_point_impression,
                                         /*expected_count=*/1);
  }

 protected:
  TestingPrefServiceSimple* local_state() {
    return TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  base::UserActionTester& user_action_tester() { return user_action_tester_; }

  ukm::TestAutoSetUkmRecorder& ukm_tester() { return ukm_tester_; }

  ProfileManager* profile_manager() {
    return TestingBrowserProcess::GetGlobal()->profile_manager();
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  content::RenderViewHostTestEnabler enabler_;

  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  ukm::TestAutoSetUkmRecorder ukm_tester_;

#if BUILDFLAG(IS_CHROMEOS)
  // This test needs to run in user session, so set it up for ChromeOS cases.
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  signin::IdentityTestEnvironment identity_env_;
  raw_ptr<TestingProfile> profile_ = nullptr;
};

class GlicMetricsTest : public GlicMetricsTestBase {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicCaptureRegion);
    GlicMetricsTestBase::SetUp();

    enabling_ = GlicEnabling::CreateForTesting(
        profile(), &profile_manager()->GetProfileAttributesStorage());
    metrics_ = std::make_unique<GlicMetrics>(profile(), enabling_.get());
  }

  void TearDown() override {
    metrics_.reset();
    enabling_.reset();
    GlicMetricsTestBase::TearDown();
  }

  GlicMetrics* metrics() { return metrics_.get(); }
  GlicEnabling* enabling() { return enabling_.get(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<GlicEnabling> enabling_;
  std::unique_ptr<GlicMetrics> metrics_;
};

TEST_F(GlicMetricsTest, RecordGlicProfilePreferences) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicSelectionPrompt,
      {{features::kGlicSelectionDefaultBlockedSites.name,
        "https://default-blocked-1.com,https://default-blocked-2.com"}});

  // Set up preferences to true.
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, true);
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);
  profile()->GetPrefs()->SetBoolean(
      prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicGeolocationEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, true);
  enabling()->SetUserEnabledActuationOnWeb(true);

  metrics()->RecordGlicProfilePreferences();

  // Only true should be recorded.
  histogram_tester().ExpectUniqueSample("Glic.Preferences.PinnedToTabstrip",
                                        true, 1);
  histogram_tester().ExpectUniqueSample("Glic.Preferences.LauncherEnabled",
                                        true, 1);
  histogram_tester().ExpectUniqueSample(
      "Glic.Preferences.KeepSidepanelOpenOnNewTabsEnabled", true, 1);
  histogram_tester().ExpectUniqueSample("Glic.Preferences.GeolocationEnabled",
                                        true, 1);
  histogram_tester().ExpectUniqueSample("Glic.Preferences.MicrophoneEnabled",
                                        true, 1);
  histogram_tester().ExpectUniqueSample(
      "Glic.Preferences.DefaultTabContextEnabled", true, 1);
  histogram_tester().ExpectUniqueSample("Glic.Preferences.ShakeTriggerEnabled",
                                        true, 1);
  histogram_tester().ExpectUniqueSample("Glic.Preferences.ActuationOnWeb", true,
                                        1);
#if !BUILDFLAG(IS_ANDROID)
  histogram_tester().ExpectUniqueSample("Glic.Selection.InlineCueMenuEnabled",
                                        true, 1);
  histogram_tester().ExpectUniqueSample("Glic.Selection.SiteExceptionsCount", 0,
                                        1);
  histogram_tester().ExpectUniqueSample(
      "Glic.Selection.RemovedDefaultBlockedSitesCount", 0, 1);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Set up preferences to false.
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, false);
  profile()->GetPrefs()->SetBoolean(
      prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicGeolocationEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicMicrophoneEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                    false);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicShakeTriggerEnabled, false);
  enabling()->SetUserEnabledActuationOnWeb(false);

#if !BUILDFLAG(IS_ANDROID)
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  settings_map->SetDefaultContentSetting(ContentSettingsType::INLINE_CUE_MENU,
                                         CONTENT_SETTING_BLOCK);
  settings_map->SetContentSettingDefaultScope(
      GURL("https://example1.com"), GURL("https://example1.com"),
      ContentSettingsType::INLINE_CUE_MENU, CONTENT_SETTING_BLOCK);
  settings_map->SetContentSettingDefaultScope(
      GURL("https://example2.com"), GURL("https://example2.com"),
      ContentSettingsType::INLINE_CUE_MENU, CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(UnblockDefaultSiteForInlineCue(profile(),
                                             "https://default-blocked-1.com"));
  EXPECT_TRUE(UnblockDefaultSiteForInlineCue(profile(),
                                             "https://default-blocked-2.com"));
#endif  // !BUILDFLAG(IS_ANDROID)

  metrics()->RecordGlicProfilePreferences();

  // Both true and false should be recorded once.
  histogram_tester().ExpectBucketCount("Glic.Preferences.PinnedToTabstrip",
                                        true, 1);
  histogram_tester().ExpectBucketCount("Glic.Preferences.PinnedToTabstrip",
                                        false, 1);

  histogram_tester().ExpectBucketCount("Glic.Preferences.LauncherEnabled", true,
                                       1);
  histogram_tester().ExpectBucketCount("Glic.Preferences.LauncherEnabled",
                                       false, 1);

  histogram_tester().ExpectBucketCount(
      "Glic.Preferences.KeepSidepanelOpenOnNewTabsEnabled", true, 1);
  histogram_tester().ExpectBucketCount(
      "Glic.Preferences.KeepSidepanelOpenOnNewTabsEnabled", false, 1);

  histogram_tester().ExpectBucketCount("Glic.Preferences.GeolocationEnabled",
                                       true, 1);
  histogram_tester().ExpectBucketCount("Glic.Preferences.GeolocationEnabled",
                                       false, 1);

  histogram_tester().ExpectBucketCount("Glic.Preferences.MicrophoneEnabled",
                                       true, 1);
  histogram_tester().ExpectBucketCount("Glic.Preferences.MicrophoneEnabled",
                                       false, 1);

  histogram_tester().ExpectBucketCount(
      "Glic.Preferences.DefaultTabContextEnabled", true, 1);
  histogram_tester().ExpectBucketCount(
      "Glic.Preferences.DefaultTabContextEnabled", false, 1);

  histogram_tester().ExpectBucketCount("Glic.Preferences.ShakeTriggerEnabled",
                                       true, 1);
  histogram_tester().ExpectBucketCount("Glic.Preferences.ShakeTriggerEnabled",
                                       false, 1);

  histogram_tester().ExpectBucketCount("Glic.Preferences.ActuationOnWeb", true,
                                       1);
  histogram_tester().ExpectBucketCount("Glic.Preferences.ActuationOnWeb", false,
                                       1);

#if !BUILDFLAG(IS_ANDROID)
  histogram_tester().ExpectBucketCount("Glic.Selection.InlineCueMenuEnabled",
                                       true, 1);
  histogram_tester().ExpectBucketCount("Glic.Selection.InlineCueMenuEnabled",
                                       false, 1);

  histogram_tester().ExpectBucketCount("Glic.Selection.SiteExceptionsCount", 2,
                                       1);
  histogram_tester().ExpectBucketCount("Glic.Selection.SiteExceptionsCount", 0,
                                       1);

  histogram_tester().ExpectBucketCount(
      "Glic.Selection.RemovedDefaultBlockedSitesCount", 2, 1);
  histogram_tester().ExpectBucketCount(
      "Glic.Selection.RemovedDefaultBlockedSitesCount", 0, 1);
#endif  // !BUILDFLAG(IS_ANDROID)
}

TEST_F(GlicMetricsTest, FreUserInputEntrypointRecorded) {
  metrics()->OnGlicWindowStartedOpening(/*attached=*/true,
                                        mojom::InvocationSource::kOsButton);
  metrics()->OnFreAccepted();

  metrics()->OnUserInputSubmitted();
  // Reset time and submit another input to make sure it's only recorded once.
  metrics()->OnUserInputSubmitted();

  histogram_tester().ExpectBucketCount("Glic.Fre.UserInput.InvocationSource",
                                       mojom::InvocationSource::kOsButton, 1);
  histogram_tester().ExpectTotalCount("Glic.Fre.UserInput.InvocationSource", 1);
}

TEST_F(GlicMetricsTest, WindowOpenUkm) {
  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kFre);

  auto entries = ukm_tester().GetEntriesByName("Glic.WindowOpen");
  ASSERT_EQ(entries.size(), 1u);
  auto entry = entries[0];
  ukm_tester().ExpectEntryMetric(entry, "Attached", false);
  ukm_tester().ExpectEntryMetric(
      entry, "InvocationSource",
      static_cast<int64_t>(mojom::InvocationSource::kFre));
  // TODO(b/452120577): No page source ID is attached yet.
  EXPECT_EQ(entry->source_id, ukm::NoURLSourceId());
  auto* source = ukm_tester().GetSourceForSourceId(entry->source_id);
  EXPECT_FALSE(source);
}

TEST_F(GlicMetricsTest, ClosedCaptionsResponse_PrefLogsFalse) {
  metrics()->LogClosedCaptionsShown();

  histogram_tester().ExpectUniqueSample("Glic.Response.ClosedCaptionsShown",
                                        false, 1);
}

TEST_F(GlicMetricsTest, ClosedCaptionsResponse_PrefLogsTrue) {
  profile()->GetPrefs()->SetBoolean(prefs::kGlicClosedCaptioningEnabled, true);
  metrics()->LogClosedCaptionsShown();

  histogram_tester().ExpectUniqueSample("Glic.Response.ClosedCaptionsShown",
                                        true, 1);
}

TEST_F(GlicMetricsTest, OnTabPinSharedSuccessful) {
  metrics()->OnTabPinnedForSharing(
      GlicTabPinnedForSharingResult::kPinTabForSharingSucceeded);

  histogram_tester().ExpectUniqueSample(
      "Glic.Sharing.TabPinnedForSharing",
      GlicTabPinnedForSharingResult::kPinTabForSharingSucceeded, 1);
}

TEST_F(GlicMetricsTest, OnTabPinSharedUnsuccessfulTooMany) {
  metrics()->OnTabPinnedForSharing(
      GlicTabPinnedForSharingResult::kPinTabForSharingFailedTooManyTabs);

  histogram_tester().ExpectUniqueSample(
      "Glic.Sharing.TabPinnedForSharing",
      GlicTabPinnedForSharingResult::kPinTabForSharingFailedTooManyTabs, 1);
}

TEST_F(GlicMetricsTest, OnTabPinSharedUnsuccessfulNotValid) {
  metrics()->OnTabPinnedForSharing(
      GlicTabPinnedForSharingResult::kPinTabForSharingFailedNotValidForSharing);

  histogram_tester().ExpectUniqueSample(
      "Glic.Sharing.TabPinnedForSharing",
      GlicTabPinnedForSharingResult::kPinTabForSharingFailedNotValidForSharing,
      1);
}

TEST_F(GlicMetricsTest, LogGetContextFromFocusedTabError_UnknownMode) {
  // Unknown is the default mode.
  metrics()->LogGetContextFromFocusedTabError(
      GlicGetContextFromTabError::kTabNotFound);

  histogram_tester().ExpectTotalCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Text", 0);
  histogram_tester().ExpectTotalCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Audio", 0);
  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetContextFromFocusedTab.Error.Unknown",
      GlicGetContextFromTabError::kTabNotFound, 1);
}

TEST_F(GlicMetricsTest, LogGetContextFromTabError_UnknownMode) {
  // Unknown is the default mode.
  metrics()->LogGetContextFromTabError(
      GlicGetContextFromTabError::kTabNotFound);

  histogram_tester().ExpectTotalCount("Glic.Api.GetContextFromTab.Error.Text",
                                      0);
  histogram_tester().ExpectTotalCount("Glic.Api.GetContextFromTab.Error.Audio",
                                      0);
  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetContextFromTab.Error.Unknown",
      GlicGetContextFromTabError::kTabNotFound, 1);
}

TEST_F(GlicMetricsTest, LogGetContextForActorFromTabError_UnknownMode) {
  // Unknown is the default mode.
  metrics()->LogGetContextForActorFromTabError(
      GlicGetContextFromTabError::kTabNotFound);

  histogram_tester().ExpectTotalCount(
      "Glic.Api.GetContextForActorFromTab.Error.Text", 0);
  histogram_tester().ExpectTotalCount(
      "Glic.Api.GetContextForActorFromTab.Error.Audio", 0);
  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetContextForActorFromTab.Error.Unknown",
      GlicGetContextFromTabError::kTabNotFound, 1);
}

TEST_F(GlicMetricsTest, LogGetContextFromFocusedTabError_ChangingModes) {
  // Simulates the client starting in text mode and later switching to audio.
  metrics()->SetWebClientMode(mojom::WebClientMode::kText);
  metrics()->LogGetContextFromFocusedTabError(
      GlicGetContextFromTabError::kWebContentsChanged);
  metrics()->SetWebClientMode(mojom::WebClientMode::kAudio);
  metrics()->LogGetContextFromFocusedTabError(
      GlicGetContextFromTabError::kPermissionDenied);

  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetContextFromFocusedTab.Error.Text",
      GlicGetContextFromTabError::kWebContentsChanged, 1);
  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetContextFromFocusedTab.Error.Audio",
      GlicGetContextFromTabError::kPermissionDenied, 1);
  histogram_tester().ExpectTotalCount(
      "Glic.Api.GetContextFromFocusedTab.Error.Unknown", 0);
}

TEST_F(GlicMetricsTest, LogGetImageBytesFromTabError) {
  metrics()->LogGetImageBytesFromTabError(
      GlicGetContextFromTabError::kTabNotFound);

  histogram_tester().ExpectUniqueSample(
      "Glic.Api.GetImageBytesFromTab.Error",
      GlicGetContextFromTabError::kTabNotFound, 1);
}

TEST_F(GlicMetricsTest, ImpressionBeforeFreNotPermittedByPolicy) {
  enabling()->SetCompletedFre(prefs::FreStatus::kNotStarted);

  ExpectEntryPointImpressionLogged(EntryPointStatus::kBeforeFreNotEligible);
}

TEST_F(GlicMetricsTest, ImpressionIncompleteFreNotPermittedByPolicy) {
  enabling()->SetCompletedFre(prefs::FreStatus::kIncomplete);

  ExpectEntryPointImpressionLogged(EntryPointStatus::kIncompleteFreNotEligible);
}

// kGeminiSettings is by default enabled, however if we initialize a scoped
// feature list in a test, since the features were initially off during setup,
// glic is considered disabled until the kGeminiSettings pref changes and
// subscribers are notified. The following tests turn the feature flags on
// before setup happens, so that glic is enabled from the start.
class GlicMetricsFeaturesEnabledTest : public GlicMetricsTestBase {
 public:
  void SetUp() override {
    GlicMetricsTestBase::SetUp();
    glic_test_env_.SetupProfile(profile());
  }

  GlicMetrics* metrics() { return GlicKeyedService::Get(profile())->metrics(); }

 private:
  GlicUnitTestEnvironment glic_test_env_;
};

TEST_F(GlicMetricsFeaturesEnabledTest, TimeToEnabledFromStartupRecorded) {
  // Glic is enabled by default in SetUp().
  histogram_tester().ExpectTotalCount(
      "Glic.ProfileEnablement.TimeToEnabledFromStartup", 1);
}

TEST_F(GlicMetricsFeaturesEnabledTest, TimeToEnabledFromStartupDelayed) {
  // Disable glic capability.
  SetGlicCapability(profile(), false);

  // Create new GlicMetrics that starts with glic disabled.
  // We use a manual instance here to avoid interference with the one in
  // GlicKeyedService which might already have recorded something.
  auto enabling = GlicEnabling::CreateForTesting(
      profile(), &profile_manager()->GetProfileAttributesStorage());
  base::HistogramTester delayed_histogram_tester;
  auto manual_metrics =
      std::make_unique<GlicMetrics>(profile(), enabling.get());

  delayed_histogram_tester.ExpectTotalCount(
      "Glic.ProfileEnablement.TimeToEnabledFromStartup", 0);

  // Enable glic capability.
  SetGlicCapability(profile(), true);

  delayed_histogram_tester.ExpectTotalCount(
      "Glic.ProfileEnablement.TimeToEnabledFromStartup", 1);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionBeforeFre) {
  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kNotStarted);

  ExpectEntryPointImpressionLogged(EntryPointStatus::kBeforeFreAndEligible);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionIncompleteFre) {
  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kIncomplete);

  ExpectEntryPointImpressionLogged(EntryPointStatus::kIncompleteFreAndEligible);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreBrowserOnly) {
  // kGeminiSettings is enabled
  // kGlicPinnedToTabstrip is true
  // kGlicLauncherEnabled is false

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreBrowserOnly);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreOsOnly) {
  // kGeminiSettings is enabled
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreOsOnly);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreEnabled) {
  // kGeminiSettings is enabled
  // kGlicPinnedToTabstrip is true
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreBrowserAndOs);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreDisabledEntrypoints) {
  // kGeminiSettings is enabled
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  // kGlicLauncherEnabled is false

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreThreeDotOnly);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreNotPermittedByPolicy) {
  // kGeminiSettings is enabled
  // kGlicPinnedToTabstrip is true
  // kGlicLauncherEnabled is true

  // Disable kGeminiSettings
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreNotEligible);
}

TEST_F(GlicMetricsFeaturesEnabledTest, EnablingChanged) {
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 0);

  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kNotStarted);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 1);

  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 1);

  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 2);

  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 2);

  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kIncomplete);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Disabled"), 3);
}

TEST_F(GlicMetricsFeaturesEnabledTest, PinnedChanged) {
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Pinned"), 0);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Unpinned"), 0);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Pinned"), 0);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Unpinned"), 1);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, true);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Pinned"), 1);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Unpinned"), 1);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ShortcutStatus) {
  task_environment().FastForwardBy(base::Minutes(16));
  histogram_tester().ExpectTotalCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", 1);
  histogram_tester().ExpectBucketCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", /*true*/ 1,
      /*expected_count=*/1);

  local_state()->SetString(prefs::kGlicLauncherHotkey,
                           ui::Command::AcceleratorToString(ui::Accelerator()));

  task_environment().FastForwardBy(base::Minutes(16));
  histogram_tester().ExpectTotalCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", 2);
  histogram_tester().ExpectBucketCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", /*false*/ 0,
      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, TimeElapsedBetweenSessions) {
  base::TimeDelta elapsed_time = base::Hours(2);

  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kOsButton);
  task_environment().FastForwardBy(elapsed_time);

  metrics()->OnGlicWindowStartedOpening(/*attached=*/true,
                                        mojom::InvocationSource::kOsButton);
  histogram_tester().ExpectTotalCount(
      "Glic.PanelWebUi.ElapsedTimeBetweenSessions",
      /*expected_count=*/1);
  histogram_tester().ExpectBucketCount(
      "Glic.PanelWebUi.ElapsedTimeBetweenSessions", elapsed_time.InSeconds(),
      1);
}

TEST_F(GlicMetricsTest, FreToFirstQueryElapsedTimeReportedOnce) {
  metrics()->OnFreAccepted();
  task_environment().FastForwardBy(base::Milliseconds(100));
  metrics()->OnUserInputSubmitted();
  histogram_tester().ExpectTotalCount("Glic.FreToFirstQueryTime", 1);
  histogram_tester().ExpectUniqueSample("Glic.FreToFirstQueryTime", 100, 1);
  histogram_tester().ExpectUniqueSample("Glic.FreToFirstQueryTimeMax24H", 100,
                                        1);
}

TEST_F(GlicMetricsTest, FreToFirstQueryElapsedTimeReportedOnlyOnce) {
  metrics()->OnFreAccepted();
  task_environment().FastForwardBy(base::Milliseconds(100));
  metrics()->OnUserInputSubmitted();
  // Second time should be ignored.
  metrics()->OnUserInputSubmitted();
  histogram_tester().ExpectTotalCount("Glic.FreToFirstQueryTime", 1);
  histogram_tester().ExpectUniqueSample("Glic.FreToFirstQueryTime", 100, 1);
  histogram_tester().ExpectUniqueSample("Glic.FreToFirstQueryTimeMax24H", 100,
                                        1);
}

class GlicMetricsTrustFirstOnboardingTest : public GlicMetricsTest {
 public:
  void SetUp() override {
    GlicMetricsTestBase::SetUp();

    enabling_ = GlicEnabling::CreateForTesting(
        profile(), &profile_manager()->GetProfileAttributesStorage());
    metrics_ = std::make_unique<GlicMetrics>(profile(), enabling_.get());

    // Revert FRE status to NotStarted to simulate new user for this experiment.
    enabling_->SetCompletedFre(prefs::FreStatus::kNotStarted);
  }
};

TEST_F(GlicMetricsTrustFirstOnboardingTest, ShownAndDismissed) {
  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kOsButton);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Shown"), 1);
  histogram_tester().ExpectUniqueSample("Glic.Fre.Shown.FlowSource",
                                        OptInFlow::kGlicFre, 1);

  // Closing without accept triggers "Dismissed".
  metrics()->OnInstanceClosed();
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Dismissed"), 1);
  histogram_tester().ExpectTotalCount("Glic.Fre.TotalTime.Dismissed.Onboarding",
                                      1);
  histogram_tester().ExpectUniqueSample("Glic.Fre.Shown.InvocationSource",
                                        mojom::InvocationSource::kOsButton, 1);
  histogram_tester().ExpectUniqueSample("Glic.Fre.Dismissed.InvocationSource",
                                        mojom::InvocationSource::kOsButton, 1);
  histogram_tester().ExpectUniqueSample("Glic.Fre.Dismissed.FlowSource",
                                        OptInFlow::kGlicFre, 1);
}

TEST_F(GlicMetricsTrustFirstOnboardingTest, ShownAndAccepted) {
  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kOsButton);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Shown"), 1);
  histogram_tester().ExpectUniqueSample("Glic.Fre.Shown.FlowSource",
                                        OptInFlow::kGlicFre, 1);

  metrics()->OnTrustFirstOnboardingAccept();
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Onboarding.OptInAccept"),
            1);
  histogram_tester().ExpectTotalCount("Glic.Fre.TotalTime.Accepted.Onboarding",
                                      1);

  // Closing after accept should NOT trigger "Dismissed".
  metrics()->OnInstanceClosed();
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Dismissed"), 0);
  histogram_tester().ExpectUniqueSample("Glic.Fre.Shown.InvocationSource",
                                        mojom::InvocationSource::kOsButton, 1);
  histogram_tester().ExpectUniqueSample("Glic.Fre.Accept.InvocationSource",
                                        mojom::InvocationSource::kOsButton, 1);
  histogram_tester().ExpectUniqueSample("Glic.Fre.Accept.FlowSource",
                                        OptInFlow::kGlicFre, 1);
  histogram_tester().ExpectTotalCount("Glic.Fre.Dismissed.InvocationSource", 0);
}

TEST_F(GlicMetricsTrustFirstOnboardingTest, NotShownIfConsented) {
  enabling()->SetCompletedFre(prefs::FreStatus::kCompleted);
  EXPECT_TRUE(enabling()->HasConsented());

  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kOsButton);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Shown"), 0);

  metrics()->OnInstanceClosed();
  EXPECT_EQ(
      user_action_tester().GetActionCount("Glic.Fre.Onboarding.Dismissed"), 0);
}

TEST_F(GlicMetricsTrustFirstOnboardingTest, FreToFirstQueryTimeRecorded) {
  metrics()->OnGlicWindowStartedOpening(/*attached=*/false,
                                        mojom::InvocationSource::kOsButton);
  metrics()->OnTrustFirstOnboardingAccept();

  task_environment().FastForwardBy(base::Seconds(1));
  metrics()->OnUserInputSubmitted();

  histogram_tester().ExpectUniqueSample("Glic.FreToFirstQueryTime", 1000, 1);
  EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.InputSubmitted"), 1);
}

}  // namespace
}  // namespace glic
