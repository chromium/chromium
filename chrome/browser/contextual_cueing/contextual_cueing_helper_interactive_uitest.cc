// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/browser/ui/tabs/glic_nudge_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/glic/glic_button_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/optimization_guide/proto/icon_view_metadata.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/l10n/l10n_util.h"

class FakeGlicNudgeDelegate : public GlicNudgeDelegate {
 public:
  void OnTriggerGlicNudgeUI(std::string label) override {
    last_nudge_label_ = label;
    if (!last_nudge_label_.empty()) {
      is_showing_nudge_ = true;
      future_.SetValue();
    }
  }
  void OnHideGlicNudgeUI() override { is_showing_nudge_ = false; }
  bool GetIsShowingGlicNudge() override { return is_showing_nudge_; }
  void WaitUntilValidNudge() { future_.Get(); }
  std::string last_nudge_label_;
  bool is_showing_nudge_ = false;
  base::test::TestFuture<void> future_;
};

class ContextualCueingHelperBrowserTest
    : public glic::test::InteractiveGlicTest {
 public:
  ContextualCueingHelperBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        // Disable feature engagement logic.
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0.0"},
           {"NudgeCapTime", "0h"},
           {"NudgeCapCount", "10"},
           {"MinPageCountBetweenNudges", "0"},
           {"UseDynamicCues", "true"}}},
         {page_content_annotations::features::kAnnotatedPageContentExtraction,
          {}},
         {contextual_tasks::kContextualTasks, {}}},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    glic::test::InteractiveGlicTest::SetUp();
  }

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
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
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddHintForTesting(
            https_server_.GetURL("enabled.com",
                                 "/optimization_guide/hello.html"),
            optimization_guide::proto::GLIC_CONTEXTUAL_CUEING, metadata);
  }

  tabs::GlicNudgeController* glic_nudge_controller() {
    return browser()->browser_window_features()->glic_nudge_controller();
  }

  void SwapToFakeDelegate(FakeGlicNudgeDelegate& nudge_delegate) {
    glic_nudge_controller()->SetDelegate(&nudge_delegate);
  }

  glic::TabStripGlicButton* GetGlicButtonForBrowser(Browser* browser) {
    return static_cast<glic::TabStripGlicButton*>(
        glic::TabStripGlicButton::FromBrowser(browser));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelDisplayed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_ABOUT_THIS_PAGE_LABEL),
            nudge_delegate.last_nudge_label_);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      contextual_cueing::NudgeDecision::kSuccess, 1);

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
      static_cast<int64_t>(contextual_cueing::NudgeDecision::kSuccess));

  // Simulate reload.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_ABOUT_THIS_PAGE_LABEL),
            nudge_delegate.last_nudge_label_);

  // Simulate new navigation. Should clear nudge.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.disabled.com")));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ("dynamic cue label", nudge_delegate.last_nudge_label_);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      contextual_cueing::NudgeDecision::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      "ContextualCueing.NudgeInteraction",
      contextual_cueing::NudgeInteraction::kShown, 1);
  histogram_tester.ExpectBucketCount(
      "ContextualCueing.NudgeInteraction.Dynamic",
      contextual_cueing::NudgeInteraction::kShown, 1);

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
      static_cast<int64_t>(contextual_cueing::NudgeDecision::kSuccess));
  // Simulate nudge click.
  glic_nudge_controller()->OnNudgeActivity(
      tabs::GlicNudgeActivity::kNudgeClicked);

  auto interaction_entries = ukm_recorder.GetEntriesByName(
      ukm::builders::ContextualCueing_NudgeInteraction::kEntryName);
  EXPECT_EQ(1u, interaction_entries.size());
  auto* interaction_entry = interaction_entries[0].get();
  ukm_recorder.ExpectEntryMetric(
      interaction_entry,
      ukm::builders::ContextualCueing_NudgeInteraction::kNudgeIsDynamicName,
      static_cast<int64_t>(true));

  histogram_tester.ExpectBucketCount(
      "ContextualCueing.NudgeInteraction",
      contextual_cueing::NudgeInteraction::kClicked, 1);
  histogram_tester.ExpectBucketCount(
      "ContextualCueing.NudgeInteraction.Dynamic",
      contextual_cueing::NudgeInteraction::kClicked, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       DoesNotAttemptToCueOnNewTabPage) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      contextual_cueing::NudgeDecision::kServerDataUnavailable, 1);

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
      static_cast<int64_t>(
          contextual_cueing::NudgeDecision::kServerDataUnavailable));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestServerDataMalformed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(optimization_guide::proto::Any());
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(
          https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
          optimization_guide::proto::GLIC_CONTEXTUAL_CUEING, metadata);

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      contextual_cueing::NudgeDecision::kServerDataMalformed, 1);

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
      static_cast<int64_t>(
          contextual_cueing::NudgeDecision::kServerDataMalformed));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      contextual_cueing::NudgeDecision::kClientConditionsUnmet, 1);

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
      static_cast<int64_t>(
          contextual_cueing::NudgeDecision::kClientConditionsUnmet));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelNotDisplayed) {
  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://disabled.com/"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelClearedOnErrorPage) {
  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_ABOUT_THIS_PAGE_LABEL),
            nudge_delegate.last_nudge_label_);
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  // Make sure it's cleared on error page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("chrome://eeerrrooorrrpage")));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelClearedOnTabChange) {
  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_ABOUT_THIS_PAGE_LABEL),
            nudge_delegate.last_nudge_label_);
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://disabled.com/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  browser()->tab_strip_model()->ActivateTabAt(2);
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueShownHistogram) {
  base::HistogramTester histogram_tester;

  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction",
      contextual_cueing::NudgeInteraction::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction.Static",
      contextual_cueing::NudgeInteraction::kShown, 1);
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

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction",
      contextual_cueing::NudgeInteraction::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction.Dynamic",
      contextual_cueing::NudgeInteraction::kShown, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestNudgeDismissedTabChangeHistogramShown) {
  base::HistogramTester histogram_tester;

  SetUpEnabledHints();

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction",
      contextual_cueing::NudgeInteraction::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction.Static",
      contextual_cueing::NudgeInteraction::kShown, 1);

  base::HistogramTester histogram_tester_2;
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://disabled.com/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
  histogram_tester_2.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction",
      contextual_cueing::NudgeInteraction::kIgnoredTabChange, 1);
  histogram_tester_2.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction.Static",
      contextual_cueing::NudgeInteraction::kIgnoredTabChange, 1);
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
  cond->set_int64_threshold(3);

  SetUpEnabledHints(cueing_metadata);

  FakeGlicNudgeDelegate nudge_delegate;
  SwapToFakeDelegate(nudge_delegate);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  nudge_delegate.WaitUntilValidNudge();
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_GLIC_BUTTON_ENTRYPOINT_ASK_ABOUT_THIS_PAGE_LABEL),
            nudge_delegate.last_nudge_label_);
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      contextual_cueing::NudgeDecision::kSuccess, 1);

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
      static_cast<int64_t>(contextual_cueing::NudgeDecision::kSuccess));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest, NudgeHideAfterUnpin) {
  SetUpEnabledHints();

  PrefService* const pref_service = browser()->profile()->GetPrefs();
  glic::TabStripGlicButton* const glic_button =
      GetGlicButtonForBrowser(browser());
  ASSERT_TRUE(glic_button->GetVisible());

  // Trigger the nudge to show
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(glic_button->GetIsShowingNudge());

  // Unpin the button
  chrome::ExecuteCommand(browser(), IDC_GLIC_TOGGLE_PIN);
  EXPECT_FALSE(pref_service->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));

  // The nudge is also the glic button so it should also hide when the button is
  // unpinned.
  EXPECT_FALSE(glic_button->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TriggerNudgeWhileUnpinned) {
  SetUpEnabledHints();

  PrefService* const pref_service = browser()->profile()->GetPrefs();
  glic::TabStripGlicButton* const glic_button =
      GetGlicButtonForBrowser(browser());
  EXPECT_TRUE(glic_button->GetVisible());

  // Unpin the glic button
  chrome::ExecuteCommand(browser(), IDC_GLIC_TOGGLE_PIN);
  EXPECT_FALSE(pref_service->GetBoolean(glic::prefs::kGlicPinnedToTabstrip));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(glic_button->GetVisible());
  EXPECT_FALSE(glic_button->GetIsShowingNudge());

  // Pin the glic button.
  chrome::ExecuteCommand(browser(), IDC_GLIC_TOGGLE_PIN);
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

    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.NudgeInteraction",
        contextual_cueing::NudgeInteraction::kShown, 1);
  }

  {
    base::HistogramTester histogram_tester;

    // Open the Contextual Tasks Side Panel.
    auto* controller =
        contextual_tasks::ContextualTasksPanelController::From(browser());
    controller->Show();

    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.NudgeInteraction",
        contextual_cueing::NudgeInteraction::
            kIgnoredOpenedContextualTasksSidePanel,
        1);
    EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());
  }
}

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
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      contextual_cueing::NudgeDecision::kClientConditionsUnmet, 1);
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
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(nudge_delegate.GetIsShowingGlicNudge());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeDecision.GlicContextualCueing",
      contextual_cueing::NudgeDecision::kSuccess, 1);
}

// Test fixture to verify that auto-open for PDF bypasses nudge caps.
class ContextualCueingBypassNudgeCapsTest
    : public glic::test::InteractiveGlicTest {
 public:
  ContextualCueingBypassNudgeCapsTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0.0"},
           {"NudgeCapTime", "0h"},
           {"NudgeCapCount", "10"},
           {"MinPageCountBetweenNudges", "0"},
           {"UseDynamicCues", "true"}}},
         {contextual_cueing::kEnableAutoOpenGlicSidePanel, {}},
         {features::kAutoOpenGlicForPdf, {}},
         {page_content_annotations::features::kAnnotatedPageContentExtraction,
          {}},
         {contextual_tasks::kContextualTasks, {}}},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    glic::test::InteractiveGlicTest::SetUp();
  }

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
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
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddHintForTesting(
            https_server_.GetURL("autoopen.com",
                                 "/optimization_guide/hello.html"),
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

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("autoopen.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // kAutoOpenGlicForPdf + auto_open_eligible should open the panel.
  auto* glic_service = glic::GlicKeyedService::Get(browser()->profile());
  ASSERT_TRUE(glic_service);
  EXPECT_TRUE(glic_service->IsWindowShowing());
}
