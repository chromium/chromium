// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/contextual_cueing_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_enums.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/optimization_guide/proto/icon_view_metadata.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_button.h"
#endif

class FakeGlicNudgeDelegate : public glic::GlicSplitButtonDelegate {
 public:
  void OnTriggerGlicNudgeUI(glic::NudgeParams params) override {
    last_nudge_label_ = params.label;
    if (!last_nudge_label_.empty()) {
      is_showing_nudge_ = true;
      future_.SetValue();
    }
  }
  void OnHideGlicNudgeUI() override {
    is_showing_nudge_ = false;
    last_nudge_label_ = "";
  }
  bool GetIsShowingGlicNudge() override { return is_showing_nudge_; }
  void WaitUntilValidNudge() { future_.Get(); }
  void ResetFuture() { future_.Clear(); }
  std::string last_nudge_label_;
  bool is_showing_nudge_ = false;
  base::test::TestFuture<void> future_;
};

class ContextualCueingHelperBaseBrowserTest : public glic::GlicBrowserTest {
 public:
  virtual void InitializeFeatureList() = 0;

  void SetUp() override {
    InitializeFeatureList();

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    glic::GlicBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    glic::GlicBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpEnabledHints(
      std::optional<optimization_guide::proto::GlicContextualCueingMetadata>
          override_metadata = std::nullopt) {
    optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
    cueing_metadata.add_cueing_configurations()->set_cue_label("test label");
    if (override_metadata) {
      cueing_metadata = *override_metadata;
    }
    optimization_guide::OptimizationMetadata metadata;
    metadata.set_any_metadata(
        optimization_guide::AnyWrapProto(cueing_metadata));
    OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile())
        ->AddHintForTesting(
            https_server_.GetURL("a.test", "/optimization_guide/hello.html"),
            optimization_guide::proto::GLIC_CONTEXTUAL_CUEING, metadata);
  }

  glic::GlicNudgeController* glic_nudge_controller() {
    tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
    auto* helper = glic::ContextualCueingHelper::From(tab);
    CHECK(helper);
    return helper->GetGlicNudgeController();
  }

  void SwapToFakeDelegate(FakeGlicNudgeDelegate& nudge_delegate) {
    glic_nudge_controller()->SetHorizontalTabsDelegate(&nudge_delegate);
  }

#if !BUILDFLAG(IS_ANDROID)
  glic::TabStripGlicButton* GetGlicButtonForBrowser(
      BrowserWindowInterface* browser) {
    return static_cast<glic::TabStripGlicButton*>(
        glic::TabStripGlicButton::FromBrowser(browser));
  }
#endif

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

class ContextualCueingHelperBrowserTest
    : public ContextualCueingHelperBaseBrowserTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        // Disable feature engagement logic.
        {{glic::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0.0"},
           {"NudgeCapTime", "0h"},
           {"NudgeCapCount", "10"},
           {"MinPageCountBetweenNudges", "0"},
           {"MinTimeBetweenNudges", "0s"},
           {"UseDynamicCues", "true"}}},
         {page_content_annotations::features::kAnnotatedPageContentExtraction,
          {}},
         {contextual_tasks::kContextualTasks, {}}},
        {contextual_cueing::kContextualCueingV2});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelDisplayed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_ABOUT_THIS_PAGE_LABEL),
            nudge_delegate.last_nudge_label_);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      glic::NudgeDecision::kSuccess, 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_NudgeDecision::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::ContextualCueing_NudgeDecision::kOptimizationTypeName,
      static_cast<int64_t>(optimization_guide::proto::GLIC_CONTEXTUAL_CUEING));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::ContextualCueing_NudgeDecision::kNudgeDecisionName,
      static_cast<int64_t>(glic::NudgeDecision::kSuccess));

  // Simulate reload.
  nudge_delegate.ResetFuture();
  GetTabListInterface()->GetActiveTab()->GetContents()->GetController().Reload(
      content::ReloadType::NORMAL, /*check_for_repost=*/true);
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_ABOUT_THIS_PAGE_LABEL),
            nudge_delegate.last_nudge_label_);

  // Simulate new navigation. Should clear nudge.
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      https_server_.GetURL("b.test", "/optimization_guide/hello.html")));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestDynamicCueLabelDisplayed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
  auto* cueing_config = cueing_metadata.add_cueing_configurations();
  cueing_config->set_cue_label("cue label");
  cueing_config->set_dynamic_cue_label("dynamic cue label");
  SetUpEnabledHints(cueing_metadata);

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_EQ("dynamic cue label", nudge_delegate.last_nudge_label_);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      glic::NudgeDecision::kSuccess, 1);
  histogram_tester.ExpectBucketCount("ContextualCueing.NudgeInteraction",
                                     glic::NudgeInteraction::kShown, 1);
  histogram_tester.ExpectBucketCount(
      "ContextualCueing.NudgeInteraction.Dynamic",
      glic::NudgeInteraction::kShown, 1);

  auto decision_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_NudgeDecision::kEntryName);
  EXPECT_EQ(1u, decision_entries.size());
  auto* decision_entry = decision_entries[0].get();

  ukm_recorder.ExpectEntryMetric(
      decision_entry,
      ukm::builders::ContextualCueing_NudgeDecision::kOptimizationTypeName,
      static_cast<int64_t>(optimization_guide::proto::GLIC_CONTEXTUAL_CUEING));
  ukm_recorder.ExpectEntryMetric(
      decision_entry,
      ukm::builders::ContextualCueing_NudgeDecision::kNudgeDecisionName,
      static_cast<int64_t>(glic::NudgeDecision::kSuccess));
  // Simulate nudge click.
  glic_nudge_controller()->OnNudgeActivity(
      glic::GlicNudgeActivity::kNudgeClicked);

  auto interaction_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_NudgeInteraction::kEntryName);
  EXPECT_EQ(1u, interaction_entries.size());
  auto* interaction_entry = interaction_entries[0].get();
  ukm_recorder.ExpectEntryMetric(
      interaction_entry,
      ukm::builders::ContextualCueing_NudgeInteraction::kNudgeIsDynamicName,
      static_cast<int64_t>(true));

  histogram_tester.ExpectBucketCount("ContextualCueing.NudgeInteraction",
                                     glic::NudgeInteraction::kClicked, 1);
  histogram_tester.ExpectBucketCount(
      "ContextualCueing.NudgeInteraction.Dynamic",
      glic::NudgeInteraction::kClicked, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       DoesNotAttemptToCueOnNewTabPage) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(chrome::ChromeUINewTabURLAsGURL());
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.NudgeDecision.GlicContextualCueing", 0);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_NudgeDecision::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest, TestCueNotAvailable) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      glic::NudgeDecision::kServerDataUnavailable, 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_NudgeDecision::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::ContextualCueing_NudgeDecision::kOptimizationTypeName,
      static_cast<int64_t>(optimization_guide::proto::GLIC_CONTEXTUAL_CUEING));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::ContextualCueing_NudgeDecision::kNudgeDecisionName,
      static_cast<int64_t>(glic::NudgeDecision::kServerDataUnavailable));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestServerDataMalformed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(optimization_guide::proto::Any());
  OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile())
      ->AddHintForTesting(
          https_server_.GetURL("a.test", "/optimization_guide/hello.html"),
          optimization_guide::proto::GLIC_CONTEXTUAL_CUEING, metadata);

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      glic::NudgeDecision::kServerDataMalformed, 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_NudgeDecision::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::ContextualCueing_NudgeDecision::kOptimizationTypeName,
      static_cast<int64_t>(optimization_guide::proto::GLIC_CONTEXTUAL_CUEING));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::ContextualCueing_NudgeDecision::kNudgeDecisionName,
      static_cast<int64_t>(glic::NudgeDecision::kServerDataMalformed));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestServerDataNoCueLabel) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
  auto* cueing_config = cueing_metadata.add_cueing_configurations();
  cueing_config->set_cue_label("cue label");
  auto* cond = cueing_config->add_conditions();
  cond->set_signal(optimization_guide::proto::
                       CONTEXTUAL_CUEING_CLIENT_SIGNAL_PDF_PAGE_COUNT);
  cond->set_cueing_operator(
      optimization_guide::proto::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  cond->set_int64_threshold(100);
  SetUpEnabledHints(cueing_metadata);

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      glic::NudgeDecision::kClientConditionsUnmet, 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_NudgeDecision::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::ContextualCueing_NudgeDecision::kOptimizationTypeName,
      static_cast<int64_t>(optimization_guide::proto::GLIC_CONTEXTUAL_CUEING));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::ContextualCueing_NudgeDecision::kNudgeDecisionName,
      static_cast<int64_t>(glic::NudgeDecision::kClientConditionsUnmet));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelNotDisplayed) {
  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      https_server_.GetURL("b.test", "/optimization_guide/hello.html")));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelClearedOnErrorPage) {
  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_ABOUT_THIS_PAGE_LABEL),
            nudge_delegate.last_nudge_label_);
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  // Make sure it's cleared on error page.
  content::NavigateToURLBlockUntilNavigationsComplete(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      GURL("chrome://eeerrrooorrrpage"), 1);
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelClearedOnTabChange) {
  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_ABOUT_THIS_PAGE_LABEL),
            nudge_delegate.last_nudge_label_);
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  CreateAndActivateTab(
      https_server_.GetURL("b.test", "/optimization_guide/hello.html"));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  ActivateTab(GetTabListInterface()->GetTab(1));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  ActivateTab(GetTabListInterface()->GetTab(2));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueShownHistogram) {
  base::HistogramTester histogram_tester;

  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample("ContextualCueing.NudgeInteraction",
                                      glic::NudgeInteraction::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction.Static",
      glic::NudgeInteraction::kShown, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestDynamicCueShownHistogram) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
  auto* cueing_config = cueing_metadata.add_cueing_configurations();
  cueing_config->set_cue_label("cue label");
  cueing_config->set_dynamic_cue_label("dynamic cue label");
  SetUpEnabledHints(cueing_metadata);

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample("ContextualCueing.NudgeInteraction",
                                      glic::NudgeInteraction::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction.Dynamic",
      glic::NudgeInteraction::kShown, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestNudgeDismissedTabChangeHistogramShown) {
  base::HistogramTester histogram_tester;

  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample("ContextualCueing.NudgeInteraction",
                                      glic::NudgeInteraction::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction.Static",
      glic::NudgeInteraction::kShown, 1);

  base::HistogramTester histogram_tester_2;
  CreateAndActivateTab(
      https_server_.GetURL("b.test", "/optimization_guide/hello.html"));

  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
  histogram_tester_2.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction",
      glic::NudgeInteraction::kIgnoredTabChange, 1);
  histogram_tester_2.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction.Static",
      glic::NudgeInteraction::kIgnoredTabChange, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelDisplayedForWordCount) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
  auto* cueing_config = cueing_metadata.add_cueing_configurations();
  cueing_config->set_cue_label("cue label");
  auto* cond = cueing_config->add_conditions();
  cond->set_signal(
      optimization_guide::proto::
          CONTEXTUAL_CUEING_CLIENT_SIGNAL_CONTENT_LENGTH_WORD_COUNT);
  cond->set_cueing_operator(
      optimization_guide::proto::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  cond->set_int64_threshold(1);

  SetUpEnabledHints(cueing_metadata);

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_ABOUT_THIS_PAGE_LABEL),
            nudge_delegate.last_nudge_label_);
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      glic::NudgeDecision::kSuccess, 1);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_NudgeDecision::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::ContextualCueing_NudgeDecision::kOptimizationTypeName,
      static_cast<int64_t>(optimization_guide::proto::GLIC_CONTEXTUAL_CUEING));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::ContextualCueing_NudgeDecision::kNudgeDecisionName,
      static_cast<int64_t>(glic::NudgeDecision::kSuccess));
}

// TODO(crbug.com/529442132): Abstract the unpin tests (NudgeHideAfterUnpin and
// TriggerNudgeWhileUnpinned) to work with either a C++ Views button (on
// Desktop) or a Java-based button (on Android) once the Android entrypoint
// button is supported.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest, NudgeHideAfterUnpin) {
  SetUpEnabledHints();

  PrefService* const pref_service = GetProfile()->GetPrefs();
  glic::TabStripGlicButton* const glic_button =
      GetGlicButtonForBrowser(GetBrowser());
  ASSERT_TRUE(glic_button->GetVisible());

  // Trigger the nudge to show
  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  EXPECT_TRUE(glic_button->GetIsShowingNudge());

  // Unpin the button
  chrome::ExecuteCommand(GetBrowser(), IDC_GLIC_TOGGLE_PIN);
  EXPECT_FALSE(pref_service->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));

  // The nudge is also the glic button so it should also hide when the button is
  // unpinned.
  EXPECT_FALSE(glic_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TriggerNudgeWhileUnpinned) {
  SetUpEnabledHints();

  PrefService* const pref_service = GetProfile()->GetPrefs();
  glic::TabStripGlicButton* const glic_button =
      GetGlicButtonForBrowser(GetBrowser());
  EXPECT_TRUE(glic_button->GetVisible());

  // Unpin the glic button
  chrome::ExecuteCommand(GetBrowser(), IDC_GLIC_TOGGLE_PIN);
  EXPECT_FALSE(pref_service->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));
  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  EXPECT_FALSE(glic_button->GetVisible());
  EXPECT_FALSE(glic_button->GetIsShowingNudge());

  // Pin the glic button.
  chrome::ExecuteCommand(GetBrowser(), IDC_GLIC_TOGGLE_PIN);
  EXPECT_TRUE(pref_service->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));

  // The nudge shouldn't show because the nudge is triggered after the button
  // was unpinned.
  EXPECT_TRUE(glic_button->GetVisible());
  EXPECT_FALSE(glic_button->GetIsShowingNudge());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestNudgeDismissedContextualTasksSidePanelOpened) {
  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  {
    base::HistogramTester histogram_tester;

    CreateAndActivateTab(
        https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
    nudge_delegate.WaitUntilValidNudge();
    EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

    histogram_tester.ExpectUniqueSample("ContextualCueing.NudgeInteraction",
                                        glic::NudgeInteraction::kShown, 1);
  }

  {
    base::HistogramTester histogram_tester;

    // Open the Contextual Tasks Side Panel.
    auto* controller =
        contextual_tasks::ContextualTasksPanelController::From(GetBrowser());
    controller->Show();

    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.NudgeInteraction",
        glic::NudgeInteraction::kIgnoredOpenedContextualTasksSidePanel, 1);
    EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
  }
}
#endif

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueNotShownForMismatchedMimeType) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
  auto* cueing_config = cueing_metadata.add_cueing_configurations();
  cueing_config->set_cue_label("pdf cue label");
  cueing_config->set_dynamic_cue_label("pdf dynamic cue label");
  // This config only applies to PDF pages.
  cueing_config->add_allowed_mime_types("application/pdf");
  SetUpEnabledHints(cueing_metadata);

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  // Navigate to an HTML page - should NOT trigger the nudge since the
  // config requires application/pdf.
  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      glic::NudgeDecision::kClientConditionsUnmet, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueShownForMatchingMimeType) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
  auto* cueing_config = cueing_metadata.add_cueing_configurations();
  cueing_config->set_cue_label("html cue label");
  cueing_config->set_dynamic_cue_label("html dynamic cue label");
  // This config applies to HTML pages, which matches our test navigation.
  cueing_config->add_allowed_mime_types("text/html");
  SetUpEnabledHints(cueing_metadata);

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  // Navigate to an HTML page - SHOULD trigger the nudge since the
  // config allows text/html.
  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      glic::NudgeDecision::kSuccess, 1);
}

class ContextualCueingHelperWithContextualCueingV2BrowserTest
    : public ContextualCueingHelperBaseBrowserTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        // Disable feature engagement logic.
        {{glic::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0.0"},
           {"NudgeCapTime", "0h"},
           {"NudgeCapCount", "10"},
           {"MinPageCountBetweenNudges", "0"},
           {"MinTimeBetweenNudges", "0s"},
           {"UseDynamicCues", "true"}}},
         {page_content_annotations::features::kAnnotatedPageContentExtraction,
          {}},
         {contextual_tasks::kContextualTasks, {}},
         {contextual_cueing::kContextualCueingV2, {}}},
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperWithContextualCueingV2BrowserTest,
                       TestNudgeNotShownForContextualCueingV2) {
  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  base::HistogramTester histogram_tester;

  CreateAndActivateTab(
      https_server_.GetURL("a.test", "/optimization_guide/hello.html"));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      glic::NudgeDecision::kNudgeNotShownContextualCueingV2, 1);
}

// Test fixture to verify that auto-open for PDF bypasses nudge caps.
class ContextualCueingBypassNudgeCapsTest : public glic::GlicBrowserTest {
 public:
  ContextualCueingBypassNudgeCapsTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{glic::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0.0"},
           {"NudgeCapTime", "0h"},
           {"NudgeCapCount", "10"},
           {"MinPageCountBetweenNudges", "0"},
           {"UseDynamicCues", "true"}}},
         {glic::kEnableAutoOpenGlicSidePanel, {}},
         {features::kAutoOpenGlicForPdf, {}},

         {page_content_annotations::features::kAnnotatedPageContentExtraction,
          {}},
         {contextual_tasks::kContextualTasks, {}}},
        {contextual_cueing::kContextualCueingV2});
  }

  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    glic::GlicBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    glic::GlicBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpBypassHints() {
    optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
    auto* cueing_config = cueing_metadata.add_cueing_configurations();
    cueing_config->set_cue_label("auto open label");
    cueing_config->set_dynamic_cue_label("auto open dynamic label");
    cueing_config->set_default_text("Summarize this page");
    cueing_config->set_auto_open_eligible(true);

    optimization_guide::OptimizationMetadata metadata;
    metadata.set_any_metadata(
        optimization_guide::AnyWrapProto(cueing_metadata));
    OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile())
        ->AddHintForTesting(
            https_server_.GetURL("c.test", "/optimization_guide/hello.html"),
            optimization_guide::proto::GLIC_CONTEXTUAL_CUEING, metadata);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Verify that kAutoOpenGlicForPdf + auto_open_eligible=true correctly
// opens the panel via the auto-open path, bypassing nudge caps.
IN_PROC_BROWSER_TEST_F(ContextualCueingBypassNudgeCapsTest,
                       TestAutoOpenBypassesNudgeCaps) {
  SetUpBypassHints();

  auto* tab = CreateAndActivateTab(
      https_server_.GetURL("c.test", "/optimization_guide/hello.html"));

  // kAutoOpenGlicForPdf + auto_open_eligible should open the panel.
  ASSERT_OK(WaitForGlicOpen(tab));
  EXPECT_TRUE(coordinator().IsAnyPanelShowing());
}

// Verify that the auto-open is blocked by the instance-scoped cooldown
// after the user has submitted a prompt on the same tab's instance.
IN_PROC_BROWSER_TEST_F(ContextualCueingBypassNudgeCapsTest,
                       TestAutoOpenRespectsCooldown) {
  base::HistogramTester histogram_tester;
  SetUpBypassHints();

  // Navigate to the pdf to trigger initial auto-open.
  auto* tab = CreateAndActivateTab(
      https_server_.GetURL("c.test", "/optimization_guide/hello.html"));

  ASSERT_OK_AND_ASSIGN(auto* glic_instance, WaitForGlicOpen(tab));
  EXPECT_TRUE(coordinator().IsAnyPanelShowing());
  ASSERT_TRUE(glic_instance);

  // Simulate a user prompt submission to start the 1-hour cooldown.
  {
    auto info = glic::mojom::ConversationInfo::New();
    info->conversation_id = "test_conversation_id";
    info->conversation_title = "Test Conversation";
    glic_instance->host().instance_delegate().RegisterConversation(
        std::move(info), base::DoNothing());
  }
  glic_instance->host().instance_delegate().OnUserInputSubmitted(
      glic::mojom::WebClientMode::kText, glic::mojom::PromptType::kUnspecified);

  // Close the Glic side panel.
  ASSERT_OK(CloseGlicForTabAndWait(tab));

  // Navigate again to trigger auto-open
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      https_server_.GetURL("c.test", "/optimization_guide/hello.html")));

  // Wait for the cooldown decision to be recorded in the histogram.
  ASSERT_OK(glic::RunUntilEqual(
      [&]() {
        return histogram_tester.GetBucketCount(
            "ContextualCueing.GlicAutoOpen.Result",
            glic::GlicAutoOpenResult::kPreventedFromCooldown);
      },
      1));

  // Verify that the Glic panel is still closed (and suppressed by the
  // cooldown).
  EXPECT_FALSE(coordinator().IsAnyPanelShowing());
  histogram_tester.ExpectBucketCount(
      "ContextualCueing.GlicAutoOpen.Result",
      glic::GlicAutoOpenResult::kPreventedFromCooldown, 1);
}

class ContextualCueingAutoOpenCooldownTest : public glic::GlicBrowserTest {
 public:
  ContextualCueingAutoOpenCooldownTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{glic::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0.0"},
           {"NudgeCapTime", "0h"},
           {"NudgeCapCount", "10"},
           {"MinPageCountBetweenNudges", "0"},
           {"UseDynamicCues", "true"}}},
         {glic::kEnableAutoOpenGlicSidePanel, {}},
         {features::kAutoOpenGlicForPdf, {{"AutoOpenGlicCooldown", "2s"}}},
         {page_content_annotations::features::kAnnotatedPageContentExtraction,
          {}},
         {contextual_tasks::kContextualTasks, {}}},
        {contextual_cueing::kContextualCueingV2});
  }

  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    glic::GlicBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    glic::GlicBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpBypassHints() {
    optimization_guide::proto::GlicContextualCueingMetadata cueing_metadata;
    auto* cueing_config = cueing_metadata.add_cueing_configurations();
    cueing_config->set_cue_label("auto open label");
    cueing_config->set_dynamic_cue_label("auto open dynamic label");
    cueing_config->set_default_text("Summarize this page");
    cueing_config->set_auto_open_eligible(true);

    optimization_guide::OptimizationMetadata metadata;
    metadata.set_any_metadata(
        optimization_guide::AnyWrapProto(cueing_metadata));
    OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile())
        ->AddHintForTesting(
            https_server_.GetURL("c.test", "/optimization_guide/hello.html"),
            optimization_guide::proto::GLIC_CONTEXTUAL_CUEING, metadata);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Verify that the auto-open works normally again after the cooldown has
// expired.
IN_PROC_BROWSER_TEST_F(ContextualCueingAutoOpenCooldownTest,
                       TestAutoOpenWorksAfterCooldownExpires) {
  base::HistogramTester histogram_tester;
  SetUpBypassHints();

  // 1. Navigate to the pdf in a new foreground tab to trigger initial
  // auto-open.
  auto* tab = CreateAndActivateTab(
      https_server_.GetURL("c.test", "/optimization_guide/hello.html"));

  ASSERT_OK_AND_ASSIGN(auto* glic_instance, WaitForGlicOpen(tab));
  EXPECT_TRUE(coordinator().IsAnyPanelShowing());
  ASSERT_TRUE(glic_instance);

  // 3. Simulate a user prompt submission to start the 2-second cooldown.
  {
    auto info = glic::mojom::ConversationInfo::New();
    info->conversation_id = "test_conversation_id";
    info->conversation_title = "Test Conversation";
    glic_instance->host().instance_delegate().RegisterConversation(
        std::move(info), base::DoNothing());
  }
  glic_instance->host().instance_delegate().OnUserInputSubmitted(
      glic::mojom::WebClientMode::kText, glic::mojom::PromptType::kUnspecified);

  // 4. Close the Glic side panel.
  ASSERT_OK(CloseGlicForTabAndWait(tab));

  // 5. Wait 3 seconds for the 2-second cooldown to expire safely.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(3));
  run_loop.Run();

  // 6. Navigate again to trigger auto-open.
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(),
      https_server_.GetURL("c.test", "/optimization_guide/hello.html")));

  // 7. Verify that Glic DOES auto-open this time (since the cooldown expired).
  ASSERT_OK(WaitForGlicOpen(tab));
  EXPECT_TRUE(coordinator().IsAnyPanelShowing());
  histogram_tester.ExpectBucketCount("ContextualCueing.GlicAutoOpen.Result",
                                     glic::GlicAutoOpenResult::kSuccess, 2);
}
