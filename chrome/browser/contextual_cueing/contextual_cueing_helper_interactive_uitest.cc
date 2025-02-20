// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/browser/ui/tabs/glic_nudge_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/optimization_guide/proto/icon_view_metadata.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"

#if BUILDFLAG(ENABLE_GLIC)

class FakeGlicNudgeObserver : public GlicNudgeObserver {
 public:
  void OnTriggerGlicNudgeUI(std::string label) override {
    last_nudge_label_ = label;
    if (!last_nudge_label_.empty()) {
      future_.SetValue();
    }
  }
  void WaitUntilValidNudge() { future_.Get(); }
  std::string last_nudge_label_;
  base::test::TestFuture<void> future_;
};

class ContextualCueingHelperBrowserTest : public InProcessBrowserTest {
 public:
  ContextualCueingHelperBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        // Disable feature engagement logic.
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0.0"},
           {"NudgeCapTime", "0h"},
           {"NudgeCapCount", "10"},
           {"MinPageCountBetweenNudges", "0"}}},
         {page_content_annotations::features::kAnnotatedPageContentExtraction,
          {}},
         {features::kGlic, {}},
         {features::kTabstripComboButton, {}}},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ContextualCueingHelperBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
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
    metadata.SetAnyMetadataForTesting(cueing_metadata);
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddHintForTesting(
            https_server_.GetURL("enabled.com",
                                 "/optimization_guide/hello.html"),
            optimization_guide::proto::GLIC_CONTEXTUAL_CUEING, metadata);
  }

  void EnableSignIn() {
    auto account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@gmail.com",
                                          signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
  }

  tabs::GlicNudgeController* glic_nudge_controller() {
    return browser()->browser_window_features()->glic_nudge_controller();
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  // Identity test support.
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelDisplayed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EnableSignIn();
  SetUpEnabledHints();

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ("test label", nudge_observer.last_nudge_label_);

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

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest, TestCueNotAvailable) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EnableSignIn();

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ("", nudge_observer.last_nudge_label_);

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

  EnableSignIn();
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(optimization_guide::proto::Any());
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(
          https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
          optimization_guide::proto::GLIC_CONTEXTUAL_CUEING, metadata);

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ("", nudge_observer.last_nudge_label_);

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

  EnableSignIn();
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

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ("", nudge_observer.last_nudge_label_);

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
  EnableSignIn();
  SetUpEnabledHints();

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://disabled.com/"),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(nudge_observer.last_nudge_label_.empty());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelClearedOnTabChange) {
  EnableSignIn();
  SetUpEnabledHints();

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ("test label", nudge_observer.last_nudge_label_);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://disabled.com/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(nudge_observer.last_nudge_label_.empty());

  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(nudge_observer.last_nudge_label_.empty());

  browser()->tab_strip_model()->ActivateTabAt(2);
  EXPECT_TRUE(nudge_observer.last_nudge_label_.empty());
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueShownHistogram) {
  base::HistogramTester histogram_tester;

  EnableSignIn();
  SetUpEnabledHints();

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction",
      contextual_cueing::NudgeInteraction::kShown, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestNudgeDismissedTabChangeHistogramShown) {
  base::HistogramTester histogram_tester;

  EnableSignIn();
  SetUpEnabledHints();

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction",
      contextual_cueing::NudgeInteraction::kShown, 1);

  base::HistogramTester histogram_tester_2;
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://disabled.com/"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_TRUE(nudge_observer.last_nudge_label_.empty());
  histogram_tester_2.ExpectUniqueSample(
      "ContextualCueing.NudgeInteraction",
      contextual_cueing::NudgeInteraction::kIgnoredTabChange, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingHelperBrowserTest,
                       TestCueLabelDisplayedForWordCount) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  EnableSignIn();
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

  FakeGlicNudgeObserver nudge_observer;
  glic_nudge_controller()->AddObserver(&nudge_observer);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      https_server_.GetURL("enabled.com", "/optimization_guide/hello.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  nudge_observer.WaitUntilValidNudge();
  EXPECT_EQ("cue label", nudge_observer.last_nudge_label_);

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

#endif  // BUILDFLAG(ENABLE_GLIC)
