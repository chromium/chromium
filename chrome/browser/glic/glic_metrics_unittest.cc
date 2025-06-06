// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/background/startup_launch_manager.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"

namespace glic {
namespace {

class MockDelegate : public GlicMetrics::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  bool IsWindowShowing() const override { return showing_; }
  bool IsWindowAttached() const override { return attached_; }
  gfx::Size GetWindowSize() const override { return gfx::Size(); }
  FocusedTabData GetFocusedTabData() override {
    return FocusedTabData(contents_ ? contents_->GetWeakPtr() : nullptr);
  }

  void SetWebContents(content::WebContents* contents) { contents_ = contents; }
  raw_ptr<content::WebContents> contents_;

  bool showing_ = false;
  bool attached_ = false;
};

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

class TestStartupLaunchManager : public StartupLaunchManager {
 public:
  TestStartupLaunchManager() = default;
  ~TestStartupLaunchManager() override = default;
};

class GlicMetricsTest : public testing::Test {
 public:
  GlicMetricsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override {
    testing::Test::SetUp();
    SetUpProfile();
    SetUpGlicMetrics();
  }

  void SetUpProfile() {
    StartupLaunchManager::SetInstanceForTesting(&startup_launch_manager_);

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    TestingBrowserProcess::GetGlobal()->SetStatusTray(
        std::make_unique<MockStatusTray>());
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
    profile_ = testing_profile_manager_->CreateTestingProfile("profile");
    ForceSigninAndModelExecutionCapability(profile_);
  }

  void SetUpGlicMetrics() {
    enabling_ = std::make_unique<GlicEnabling>(
        profile_, &testing_profile_manager_->profile_manager()
                       ->GetProfileAttributesStorage());
    metrics_ = std::make_unique<GlicMetrics>(profile_, enabling_.get());
    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    metrics_->SetDelegateForTesting(std::move(delegate));
  }

  void TearDown() override {
    delegate_ = nullptr;
    metrics_.reset();
    enabling_.reset();
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();
    profile_ = nullptr;
    testing_profile_manager_.reset();
    StartupLaunchManager::SetInstanceForTesting(nullptr);
    testing::Test::TearDown();
  }

  void ExpectEntryPointImpressionLogged(
      EntryPointStatus entry_point_impression) {
    task_environment_.FastForwardBy(base::Minutes(16));
    histogram_tester_.ExpectTotalCount("Glic.EntryPoint.Status", 1);
    histogram_tester_.ExpectBucketCount("Glic.EntryPoint.Status",
                                        entry_point_impression,
                                        /*expected_count=*/1);
  }

 protected:
  TestingPrefServiceSimple* local_state() {
    return testing_profile_manager_->local_state()->Get();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestStartupLaunchManager startup_launch_manager_;

  content::RenderViewHostTestEnabler enabler_;

  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  ukm::TestAutoSetUkmRecorder ukm_tester_;

  raw_ptr<TestingProfile> profile_ = nullptr;
  signin::IdentityTestEnvironment identity_env_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  // Owned by `metrics_`.
  raw_ptr<MockDelegate> delegate_;
  std::unique_ptr<GlicEnabling> enabling_;
  std::unique_ptr<GlicMetrics> metrics_;
};

TEST_F(GlicMetricsTest, Basic) {
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->OnResponseStarted();
  metrics_->OnResponseStopped();
  metrics_->OnResponseRated(/*positive=*/true);
  metrics_->OnSessionTerminated();

  histogram_tester_.ExpectTotalCount("Glic.Response.StopTime", 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseInputSubmit"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseStart"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseStop"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponse"), 0);
}

TEST_F(GlicMetricsTest, BasicVisible) {
  delegate_->showing_ = true;
  delegate_->attached_ = true;

  metrics_->OnGlicWindowOpen(/*attached=*/true,
                             mojom::InvocationSource::kOsButton);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->OnResponseStarted();
  metrics_->OnResponseStopped();
  metrics_->OnResponseRated(/*positive=*/true);
  metrics_->OnSessionTerminated();
  metrics_->OnGlicWindowClose();

  histogram_tester_.ExpectTotalCount("Glic.Response.StopTime", 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseInputSubmit"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseStart"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponseStop"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("GlicResponse"), 1);
}

TEST_F(GlicMetricsTest, BasicUkm) {
  delegate_->showing_ = true;
  metrics_->OnGlicWindowOpen(/*attached=*/false, mojom::InvocationSource::kFre);
  for (int i = 0; i < 2; ++i) {
    metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
    metrics_->OnResponseStarted();
    metrics_->OnResponseStopped();
  }

  {
    auto entries = ukm_tester_.GetEntriesByName("Glic.WindowOpen");
    ASSERT_EQ(entries.size(), 1u);
    auto entry = entries[0];
    ukm_tester_.ExpectEntryMetric(entry, "Attached", false);
    ukm_tester_.ExpectEntryMetric(
        entry, "InvocationSource",
        static_cast<int64_t>(mojom::InvocationSource::kFre));
    auto* source = ukm_tester_.GetSourceForSourceId(entry->source_id);
    EXPECT_FALSE(source);
  }

  {
    auto entries = ukm_tester_.GetEntriesByName("Glic.Response");
    ASSERT_EQ(entries.size(), 2u);
    for (int i = 0; i < 2; ++i) {
      auto entry = entries[i];
      ukm_tester_.ExpectEntryMetric(entry, "Attached", false);
      ukm_tester_.ExpectEntryMetric(
          entry, "WebClientMode",
          static_cast<int64_t>(mojom::WebClientMode::kText));
      ukm_tester_.ExpectEntryMetric(
          entry, "InvocationSource",
          static_cast<int64_t>(mojom::InvocationSource::kFre));
      auto* source = ukm_tester_.GetSourceForSourceId(entry->source_id);
      EXPECT_FALSE(source);
    }
  }
}
TEST_F(GlicMetricsTest, BasicUkmWithTarget) {
  // Create a SiteInstance, which is required to build a WebContents.
  scoped_refptr<content::SiteInstance> site_instance =
      content::SiteInstance::Create(profile_);

  // Use WebContentsTester::CreateTestWebContents(...) to create a real
  // WebContents suitable for unit testing.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_,
                                                        site_instance.get());
  auto* tester = content::WebContentsTester::For(web_contents.get());

  GURL url("https://www.google.com");
  tester->NavigateAndCommit(url);

  delegate_->SetWebContents(web_contents.get());

  delegate_->showing_ = true;
  metrics_->DidRequestContextFromFocusedTab();
  metrics_->OnGlicWindowOpen(/*attached=*/false, mojom::InvocationSource::kFre);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->OnResponseStarted();
  metrics_->OnResponseStopped();

  ukm::SourceId ukm_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  {
    auto entries = ukm_tester_.GetEntriesByName("Glic.WindowOpen");
    ASSERT_EQ(entries.size(), 1u);
    auto entry = entries[0];
    EXPECT_EQ(entry->source_id, ukm_id);
  }

  {
    auto entries = ukm_tester_.GetEntriesByName("Glic.Response");
    ASSERT_EQ(entries.size(), 1u);
    auto entry = entries[0];
    EXPECT_EQ(entry->source_id, ukm_id);
  }

  delegate_->SetWebContents(nullptr);
}

TEST_F(GlicMetricsTest, SegmentationOsButtonAttachedText) {
  delegate_->showing_ = true;
  delegate_->attached_ = true;

  metrics_->OnGlicWindowOpen(/*attached=*/true,
                             mojom::InvocationSource::kOsButton);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->OnResponseStarted();
  metrics_->OnResponseStopped();
  metrics_->OnGlicWindowClose();

  histogram_tester_.ExpectTotalCount("Glic.Response.Segmentation", 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Response.Segmentation", ResponseSegmentation::kOsButtonAttachedText,
      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, Segmentation3DotsMenuDetachedAudio) {
  delegate_->showing_ = true;
  delegate_->attached_ = false;

  metrics_->OnGlicWindowOpen(/*attached=*/false,
                             mojom::InvocationSource::kThreeDotsMenu);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  metrics_->OnResponseStarted();
  metrics_->OnResponseStopped();
  metrics_->OnGlicWindowClose();

  histogram_tester_.ExpectTotalCount("Glic.Response.Segmentation", 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Response.Segmentation",
      ResponseSegmentation::kThreeDotsMenuDetachedAudio,
      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, SessionDuration_LogsDuration) {
  metrics_->OnGlicWindowOpen(/*attached=*/true,
                             mojom::InvocationSource::kOsButton);
  int minutes = 10;
  task_environment_.FastForwardBy(base::Minutes(minutes));
  metrics_->OnGlicWindowClose();

  histogram_tester_.ExpectTotalCount("Glic.Session.Duration", 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Session.Duration", base::Minutes(minutes), /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, SessionDuration_LogsError) {
  // Trigger a call to |OnGlicWindowClose()| without opening the window first.
  metrics_->OnGlicWindowClose();

  histogram_tester_.ExpectTotalCount("Glic.Session.Duration", 0);
  histogram_tester_.ExpectTotalCount("Glic.Metrics.Error", 1);
  histogram_tester_.ExpectBucketCount("Glic.Metrics.Error",
                                      Error::kWindowCloseWithoutWindowOpen,
                                      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, ImpressionBeforeFreNotPermittedByPolicy) {
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  ExpectEntryPointImpressionLogged(EntryPointStatus::kBeforeFreNotEligible);
}

TEST_F(GlicMetricsTest, ImpressionIncompleteFreNotPermittedByPolicy) {
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  ExpectEntryPointImpressionLogged(EntryPointStatus::kIncompleteFreNotEligible);
}

// kGeminiSettings is by default enabled, however if we initialize a scoped
// feature list in a test, since the features were initially off during setup,
// glic is considered disabled until the kGeminiSettings pref changes and
// subscribers are notified. The following tests turn the feature flags on
// before setup happens, so that glic is enabled from the start.
class GlicMetricsFeaturesEnabledTest : public GlicMetricsTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
            features::kTabstripComboButton,
            features::kGlicRollout,
        },
        {});
    SetUpProfile();
    // When Glic is enabled before the profile is setup GlicKeyedService starts
    // and creates it's own GlicMetrics. Do not setup GlicMetrics again here so
    // that duplicate metrics observers are not bound.
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    GlicMetricsTest::TearDown();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionBeforeFre) {
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  ExpectEntryPointImpressionLogged(EntryPointStatus::kBeforeFreAndEligible);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionIncompleteFre) {
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

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
  profile_->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
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
  profile_->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  // kGlicLauncherEnabled is false

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreThreeDotOnly);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreNotPermittedByPolicy) {
  // kGeminiSettings is enabled
  // kGlicPinnedToTabstrip is true
  // kGlicLauncherEnabled is true

  // Disable kGeminiSettings
  profile_->GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));

  ExpectEntryPointImpressionLogged(EntryPointStatus::kAfterFreNotEligible);
}

TEST_F(GlicMetricsFeaturesEnabledTest, EnablingChanged) {
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Disabled"), 0);
  // Glic starts enabled and during profile creation GlicMetrics records a user
  // action.
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Enabled"), 1);

  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Disabled"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Enabled"), 1);

  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Disabled"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Enabled"), 2);

  profile_->GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Disabled"), 2);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Enabled"), 2);

  profile_->GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Disabled"), 2);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Enabled"), 3);

  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Disabled"), 3);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Enabled"), 3);
}

TEST_F(GlicMetricsFeaturesEnabledTest, PinnedChanged) {
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Pinned"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Unpinned"), 0);
  profile_->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Pinned"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Unpinned"), 1);
  profile_->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, true);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Pinned"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Unpinned"), 1);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ShortcutStatus) {
  task_environment_.FastForwardBy(base::Minutes(16));
  histogram_tester_.ExpectTotalCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", /*true*/1,
      /*expected_count=*/1);

  local_state()->SetString(prefs::kGlicLauncherHotkey,
                           ui::Command::AcceleratorToString(ui::Accelerator()));

  task_environment_.FastForwardBy(base::Minutes(16));
  histogram_tester_.ExpectTotalCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", 2);
  histogram_tester_.ExpectBucketCount(
      "Glic.OsEntrypoint.Settings.ShortcutStatus", /*false*/0,
      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, InputModesUsed) {
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->OnGlicWindowClose();
  histogram_tester_.ExpectTotalCount("Glic.Session.InputModesUsed", 1);
  histogram_tester_.ExpectBucketCount("Glic.Session.InputModesUsed",
                                      InputModesUsed::kOnlyText, 1);

  metrics_->OnGlicWindowClose();
  histogram_tester_.ExpectTotalCount("Glic.Session.InputModesUsed", 2);
  histogram_tester_.ExpectBucketCount("Glic.Session.InputModesUsed",
                                      InputModesUsed::kNone, 1);

  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  metrics_->OnGlicWindowClose();
  histogram_tester_.ExpectTotalCount("Glic.Session.InputModesUsed", 3);
  histogram_tester_.ExpectBucketCount("Glic.Session.InputModesUsed",
                                      InputModesUsed::kTextAndAudio, 1);

  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  metrics_->OnGlicWindowClose();
  histogram_tester_.ExpectTotalCount("Glic.Session.InputModesUsed", 4);
  histogram_tester_.ExpectBucketCount("Glic.Session.InputModesUsed",
                                      InputModesUsed::kOnlyAudio, 1);
}

TEST_F(GlicMetricsTest, AttachStateChanges) {
  // Attach changes during initialization should not be counted.
  metrics_->OnAttachedToBrowser(AttachChangeReason::kInit);
  metrics_->OnGlicWindowClose();
  histogram_tester_.ExpectTotalCount("Glic.Session.AttachStateChanges", 1);
  histogram_tester_.ExpectBucketCount("Glic.Session.AttachStateChanges", 0, 1);

  metrics_->OnAttachedToBrowser(AttachChangeReason::kDrag);
  metrics_->OnGlicWindowClose();
  histogram_tester_.ExpectTotalCount("Glic.Session.AttachStateChanges", 2);
  histogram_tester_.ExpectBucketCount("Glic.Session.AttachStateChanges", 1, 1);

  metrics_->OnAttachedToBrowser(AttachChangeReason::kMenu);
  metrics_->OnDetachedFromBrowser(AttachChangeReason::kMenu);
  metrics_->OnAttachedToBrowser(AttachChangeReason::kMenu);
  metrics_->OnDetachedFromBrowser(AttachChangeReason::kMenu);
  metrics_->OnGlicWindowClose();
  histogram_tester_.ExpectTotalCount("Glic.Session.AttachStateChanges", 3);
  histogram_tester_.ExpectBucketCount("Glic.Session.AttachStateChanges", 4, 1);
}

TEST_F(GlicMetricsTest, TimeElapsedBetweenSessions) {
  base::TimeDelta elapsed_time = base::Hours(2);

  metrics_->OnGlicWindowClose();
  task_environment_.FastForwardBy(elapsed_time);

  metrics_->OnGlicWindowOpen(/*attached=*/true,
                             mojom::InvocationSource::kOsButton);
  histogram_tester_.ExpectTotalCount(
      "Glic.PanelWebUi.ElapsedTimeBetweenSessions",
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Glic.PanelWebUi.ElapsedTimeBetweenSessions", elapsed_time.InSeconds(),
      1);
}

}  // namespace
}  // namespace glic
