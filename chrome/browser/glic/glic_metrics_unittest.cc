// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_focused_tab_manager.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_tab_data.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

// This mock is a wrapper around the API in GlicWindowController which is
// exposed to GlicMetrics. It doesn't do anything.
class MockWindowController : public GlicWindowController {
 public:
  MockWindowController(Profile* profile,
                       signin::IdentityManager* identity_manager,
                       GlicEnabling* enabling)
      : GlicWindowController(profile,
                             identity_manager,
                             /*service=*/nullptr,
                             enabling) {}
  ~MockWindowController() override = default;

  bool IsShowing() const override { return showing_; }
  bool IsAttached() override { return attached_; }
  bool showing_ = false;
  bool attached_ = false;
};

class MockTabManager : public GlicFocusedTabManager {
 public:
  MockTabManager(Profile* profile, GlicWindowController& window_controller)
      : GlicFocusedTabManager(profile, window_controller) {}
  ~MockTabManager() override = default;
  FocusedTabData GetFocusedTabData() override {
    return FocusedTabData(contents_, std::nullopt, std::nullopt);
  }
  void SetWebContents(content::WebContents* contents) { contents_ = contents; }
  raw_ptr<content::WebContents> contents_;
};

class GlicMetricsTest : public testing::Test {
 public:
  GlicMetricsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override {
    // Set up profile before enabling is created so we don't fire enabled
    // change notifications.
    ForceSigninAndModelExecutionCapability(&profile_);

    enabling_ = std::make_unique<GlicEnabling>(&profile_);
    controller_ = std::make_unique<MockWindowController>(
        &profile_, identity_env_.identity_manager(), enabling_.get());
    tab_manager_ = std::make_unique<MockTabManager>(&profile_, *controller_);

    metrics_ = std::make_unique<GlicMetrics>(&profile_, enabling_.get());
    metrics_->SetControllers(controller_.get(), tab_manager_.get());
  }

  void ExpectEntryPointImpressionLogged(base::HistogramBase::Sample32 bucket) {
    task_environment_.FastForwardBy(base::Minutes(16));
    histogram_tester_.ExpectTotalCount("Glic.EntryPoint.Impression", 1);
    histogram_tester_.ExpectBucketCount("Glic.EntryPoint.Impression", bucket,
                                        /*expected_count=*/1);
  }

 protected:
  TestingPrefServiceSimple* local_state() { return local_state_.Get(); }

  content::BrowserTaskEnvironment task_environment_;

  content::RenderViewHostTestEnabler enabler_;

  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  ukm::TestAutoSetUkmRecorder ukm_tester_;

  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  TestingProfile profile_;
  signin::IdentityTestEnvironment identity_env_;

  std::unique_ptr<GlicEnabling> enabling_;
  std::unique_ptr<MockWindowController> controller_;
  std::unique_ptr<MockTabManager> tab_manager_;
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
  controller_->showing_ = true;
  controller_->attached_ = true;

  metrics_->OnGlicWindowOpen(/*attached=*/true, InvocationSource::kOsButton);
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
  controller_->showing_ = true;
  metrics_->OnGlicWindowOpen(/*attached=*/false, InvocationSource::kFre);
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
    ukm_tester_.ExpectEntryMetric(entry, "InvocationSource",
                                  static_cast<int64_t>(InvocationSource::kFre));
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
          static_cast<int64_t>(InvocationSource::kFre));
      auto* source = ukm_tester_.GetSourceForSourceId(entry->source_id);
      EXPECT_FALSE(source);
    }
  }
}
TEST_F(GlicMetricsTest, BasicUkmWithTarget) {
  // Create a SiteInstance, which is required to build a WebContents.
  scoped_refptr<content::SiteInstance> site_instance =
      content::SiteInstance::Create(&profile_);

  // Use WebContentsTester::CreateTestWebContents(...) to create a real
  // WebContents suitable for unit testing.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(&profile_,
                                                        site_instance.get());
  auto* tester = content::WebContentsTester::For(web_contents.get());

  GURL url("https://www.google.com");
  tester->NavigateAndCommit(url);

  tab_manager_->SetWebContents(web_contents.get());

  controller_->showing_ = true;
  metrics_->DidRequestContextFromFocusedTab();
  metrics_->OnGlicWindowOpen(/*attached=*/false, InvocationSource::kFre);
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

  tab_manager_->SetWebContents(nullptr);
}

TEST_F(GlicMetricsTest, SegmentationOsButtonAttachedText) {
  controller_->showing_ = true;
  controller_->attached_ = true;

  metrics_->OnGlicWindowOpen(/*attached=*/true, InvocationSource::kOsButton);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText);
  metrics_->OnResponseStarted();
  metrics_->OnResponseStopped();
  metrics_->OnGlicWindowClose();

  histogram_tester_.ExpectTotalCount("Glic.Response.Segmentation", 1);
  histogram_tester_.ExpectBucketCount("Glic.Response.Segmentation",
                                      /*kOsButtonAttachedText=*/1,
                                      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, SegmentationChroMenuDetachedAudio) {
  controller_->showing_ = true;
  controller_->attached_ = false;

  metrics_->OnGlicWindowOpen(/*attached=*/false, InvocationSource::kChroMenu);
  metrics_->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
  metrics_->OnResponseStarted();
  metrics_->OnResponseStopped();
  metrics_->OnGlicWindowClose();

  histogram_tester_.ExpectTotalCount("Glic.Response.Segmentation", 1);
  histogram_tester_.ExpectBucketCount("Glic.Response.Segmentation",
                                      /*kChroMenuDetachedAudio=*/32,
                                      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, SessionDuration_LogsDuration) {
  metrics_->OnGlicWindowOpen(/*attached=*/true, InvocationSource::kOsButton);
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
  histogram_tester_.ExpectBucketCount(
      "Glic.Metrics.Error",
      /*Error::kWindowCloseWithoutWindowOpen=*/3,
      /*expected_count=*/1);
}

TEST_F(GlicMetricsTest, ImpressionBeforeFre) {
  profile_.GetPrefs()->SetBoolean(prefs::kGlicCompletedFre, false);

  ExpectEntryPointImpressionLogged(/*kBeforeFre=*/0);
}

// kGeminiSettings is by default enabled, however if we initialize a scoped
// feature list in a test, since the features were initially off during setup,
// glic is considered disabled until the kGeminiSettings pref changes and
// subscribers are notified. The following tests turn the feature flags on
// before setup happens, so that glic is enabled from the start.
class GlicMetricsFeaturesEnabledTest : public GlicMetricsTest {
 public:
  void SetUp() override {
    scoped_feature_list.InitWithFeatures(
        {
            features::kGlic,
            features::kTabstripComboButton,
        },
        {});
    GlicMetricsTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreDisabledPolicy) {
  profile_.GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));

  ExpectEntryPointImpressionLogged(/*kAfterFreDisabled=*/4);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreBrowserOnly) {
  // kGeminiSettings is enabled
  // kGlicPinnedToTabstrip is true
  // kGlicLauncherEnabled is false

  ExpectEntryPointImpressionLogged(/*kAfterFreBrowserOnly=*/1);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreOsOnly) {
  // kGeminiSettings is enabled
  profile_.GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  ExpectEntryPointImpressionLogged(/*kAfterFreOsOnly=*/2);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreEnabled) {
  // kGeminiSettings is enabled
  // kGlicPinnedToTabstrip is true
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  ExpectEntryPointImpressionLogged(/*kAfterFreEnabled=*/3);
}

TEST_F(GlicMetricsFeaturesEnabledTest, ImpressionAfterFreDisabled) {
  // kGeminiSettings is enabled
  profile_.GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  // kGlicLauncherEnabled is false

  ExpectEntryPointImpressionLogged(/*kAfterFreDisabled=*/4);
}

TEST_F(GlicMetricsFeaturesEnabledTest, EnablingChanged) {
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Disabled"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Enabled"), 0);
  profile_.GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Disabled"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Enabled"), 0);
  profile_.GetPrefs()->SetInteger(
      ::prefs::kGeminiSettings,
      static_cast<int>(glic::prefs::SettingsPolicyState::kEnabled));
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Disabled"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Enabled"), 1);
}

TEST_F(GlicMetricsFeaturesEnabledTest, PinnedChanged) {
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Pinned"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Unpinned"), 0);
  profile_.GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Pinned"), 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Unpinned"), 1);
  profile_.GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, true);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Pinned"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Unpinned"), 1);
}

}  // namespace
}  // namespace glic
