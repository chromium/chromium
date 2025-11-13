// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include <optional>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/extensions/keyed_services/browser_context_keyed_service_factories.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_pref_names.h"
#endif

namespace contextual_cueing {

using ::testing::_;
using ::testing::An;
using ::testing::WithArgs;

class ContextualCueingServiceBrowserTest : public InProcessBrowserTest {
 public:
  ContextualCueingServiceBrowserTest() = default;

  void SetUp() override {
    ContextualCueingServiceFactory::GetInstance();
    InProcessBrowserTest::SetUp();
  }
};

#if BUILDFLAG(ENABLE_GLIC)
class ContextualCueingServiceBrowserTestZSSFlag
    : public ContextualCueingServiceBrowserTest {
 public:
  ContextualCueingServiceBrowserTestZSSFlag() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kGlicZeroStateSuggestions,
             {{"ZSSAllowContextualSuggestionsForSearchResultsPages", "false"}}},
        },
        {});
    // Initialize `scoped_prewarm_feature_list_` after the
    // `scoped_feature_list_` that will be removed in the parent class's
    // destructor, so that these instances are destroyed in the reversed order.
    scoped_prewarm_feature_list_ =
        std::make_unique<test::ScopedPrewarmFeatureList>(
            test::ScopedPrewarmFeatureList::PrewarmState::kDisabled);
  }

  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetBoolean(
        glic::prefs::kGlicTabContextEnabled, true);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGlicDev);
  }

 private:
  glic::GlicTestEnvironment glic_test_environment_{{
      .force_signin_and_model_execution_capability = false,
  }};
  base::test::ScopedFeatureList scoped_feature_list_;

  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  std::unique_ptr<test::ScopedPrewarmFeatureList> scoped_prewarm_feature_list_;
};

// A WebContentsObserver that asks for zero state suggestions every
// DidFinishNavigation.
class ZeroStateSuggestionsFetcher : public content::WebContentsObserver {
 public:
  explicit ZeroStateSuggestionsFetcher(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~ZeroStateSuggestionsFetcher() override = default;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    ContextualCueingService* service =
        ContextualCueingServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    service->GetContextualGlicZeroStateSuggestionsForFocusedTab(
        web_contents(), /*is_fre=*/false, /*supported_tools=*/{},
        std::move(callback_));
  }

  void set_callback(GlicSuggestionsCallback callback) {
    callback_ = std::move(callback);
  }

 private:
  GlicSuggestionsCallback callback_;
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestZSSFlag,
                       ServiceSpawnsWithZSSFlag) {
  EXPECT_NE(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestZSSFlag,
                       PrepareFetchesPageContentButNoModelExecution) {
  base::HistogramTester histogram_tester;

  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->profile());

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  service->PrepareToFetchContextualGlicZeroStateSuggestions(web_contents);

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 1);

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "ZeroStateSuggestions",
      0);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestZSSFlag,
                       GetFetchesSuggestions) {
  base::HistogramTester histogram_tester;

  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->profile());

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));

  base::test::TestFuture<std::vector<std::string>> future;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  service->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      web_contents, /*is_fre=*/true,
      /*supported_tools=*/{}, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "ZeroStateSuggestions",
      1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestZSSFlag,
                       SuggestionsWaitForLoadedSignal) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // First page waits until after load before calling GetSuggestions.
  {
    base::HistogramTester histogram_tester;

    base::test::TestFuture<std::vector<std::string>> future;
    auto* service =
        ContextualCueingServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("/optimization_guide/zss_page.html")));
    service->GetContextualGlicZeroStateSuggestionsForFocusedTab(
        web_contents, /*is_fre=*/true, /*supported_tools=*/{},
        future.GetCallback());
    ASSERT_TRUE(future.Wait());

    histogram_tester.ExpectTotalCount(
        "ContextualCueing.ZeroStateSuggestions.ContentExtractionWait", 0);
    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
        "ZeroStateSuggestions",
        1);
  }

  // Pretend that there is now something observing every navigation and fetching
  // suggestions quickly.
  auto fetcher = std::make_unique<ZeroStateSuggestionsFetcher>(web_contents);

  // Simulate quick navigation in same web contents.
  {
    base::HistogramTester histogram_tester;

    GURL new_url(embedded_test_server()->GetURL("/links.html"));
    base::test::TestFuture<std::vector<std::string>> future;
    fetcher->set_callback(future.GetCallback());
    content::TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(ExecJs(web_contents, "location = '" + new_url.spec() + "';"));
    observer.Wait();
    EXPECT_EQ(observer.last_navigation_url(), new_url);
    ASSERT_TRUE(future.Wait());

    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.ZeroStateSuggestions.ContentExtractionWait", true, 1);
    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
        "ZeroStateSuggestions",
        1);
  }

  // Simulate fragment navigation. Should re-request.
  {
    base::HistogramTester histogram_tester;

    // Perform a same-document navigation to a fragment. `NavigateToURL()`
    // automatically detects that the navigation should be same-document, since
    // the URLs differ only by fragment.
    GURL new_url(embedded_test_server()->GetURL("/links.html#ref"));
    base::test::TestFuture<std::vector<std::string>> future;
    fetcher->set_callback(future.GetCallback());
    content::TestNavigationObserver observer(web_contents);
    EXPECT_TRUE(ExecJs(web_contents, "location = '" + new_url.spec() + "';"));
    observer.Wait();
    EXPECT_EQ(observer.last_navigation_url(), new_url);
    ASSERT_TRUE(future.Wait());

    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.ZeroStateSuggestions.ContentExtractionSameDocDelay",
        true, 1);
    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
        "ZeroStateSuggestions",
        1);
  }

  // Simulate same-doc navigation in same web contents.
  {
    base::HistogramTester histogram_tester;

    base::test::TestFuture<std::vector<std::string>> future;
    fetcher->set_callback(future.GetCallback());
    std::string same_document_script = R"(
      window.history.pushState({}, "", "form.html");
    )";
    ASSERT_THAT(content::EvalJs(web_contents, same_document_script),
                content::EvalJsResult::IsOk());
    ASSERT_TRUE(future.Wait());

    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.ZeroStateSuggestions.ContentExtractionSameDocDelay",
        true, 1);
    histogram_tester.ExpectUniqueSample(
        "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
        "ZeroStateSuggestions",
        1);
  }
}

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestZSSFlag,
                       IgnoresNewTabPage) {
  base::HistogramTester histogram_tester;

  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->profile());

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));

  base::test::TestFuture<std::vector<std::string>> future;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  service->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      web_contents, /*is_fre=*/false,
      /*supported_tools=*/{}, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "ZeroStateSuggestions",
      0);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.FocusedTabEligibleForSuggestions",
      false, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestZSSFlag,
                       IgnoresSearchResultsPage) {
  base::HistogramTester histogram_tester;

  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->profile());
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      template_url_service->GenerateSearchURLForDefaultSearchProvider(u"foo")));

  base::test::TestFuture<std::vector<std::string>> future;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  service->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      web_contents, /*is_fre=*/false,
      /*supported_tools=*/{}, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  histogram_tester.ExpectTotalCount(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "ZeroStateSuggestions",
      0);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.FocusedTabEligibleForSuggestions",
      false, 1);
}

class ContextualCueingServiceBrowserTestAllowZSSForSrp
    : public ContextualCueingServiceBrowserTest {
 public:
  ContextualCueingServiceBrowserTestAllowZSSForSrp() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kGlicZeroStateSuggestions,
          {{"ZSSAllowContextualSuggestionsForSearchResultsPages", "true"}}},
         {features::kGlic, {}},
         {features::kTabstripComboButton, {}}},
        {});
  }

  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetBoolean(
        glic::prefs::kGlicTabContextEnabled, true);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGlicDev);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestAllowZSSForSrp,
                       AllowsSearchResultsPage) {
  base::HistogramTester histogram_tester;

  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->profile());
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      template_url_service->GenerateSearchURLForDefaultSearchProvider(u"foo")));

  base::test::TestFuture<std::vector<std::string>> future;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  service->GetContextualGlicZeroStateSuggestionsForFocusedTab(
      web_contents, /*is_fre=*/false,
      /*supported_tools=*/{}, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "ZeroStateSuggestions",
      1);
  histogram_tester.ExpectUniqueSample(
      "ContextualCueing.GlicSuggestions.FocusedTabEligibleForSuggestions", true,
      1);
}

#endif  // ENABLE_GLIC

class ContextualCueingServiceBrowserTestCCFlag
    : public ContextualCueingServiceBrowserTest {
 public:
  ContextualCueingServiceBrowserTestCCFlag() {
    scoped_feature_list_.InitAndEnableFeature(kContextualCueing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestCCFlag,
                       ServiceSpawnsWithCCFlag) {
  EXPECT_NE(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

class ContextualCueingServiceBrowserTestDisabledFeatures
    : public ContextualCueingServiceBrowserTest {
 public:
  ContextualCueingServiceBrowserTestDisabledFeatures() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        {kContextualCueing, kGlicZeroStateSuggestions});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestDisabledFeatures,
                       NullServiceWithDisabledFeatures) {
  EXPECT_EQ(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

}  // namespace contextual_cueing
