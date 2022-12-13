// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace {

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using ukm::builders::Preloading_Attempt;

class OmniboxPrerenderBrowserTest : public PlatformBrowserTest {
 public:
  OmniboxPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &OmniboxPrerenderBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kOmniboxTriggerForPrerender2);
  }

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ukm_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            ToPreloadingPredictor(
                ChromePreloadingPredictor::kOmniboxDirectURLInput));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  Profile* GetProfile() {
#if BUILDFLAG(IS_ANDROID)
    return chrome_test_utils::GetProfile(this);
#else
    return browser()->profile();
#endif
  }

  predictors::AutocompleteActionPredictor* GetAutocompleteActionPredictor() {
    Profile* profile = GetProfile();
    return predictors::AutocompleteActionPredictorFactory::GetForProfile(
        profile);
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const content::test::PreloadingAttemptUkmEntryBuilder& ukm_entry_builder() {
    return *ukm_entry_builder_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      ukm_entry_builder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that Prerender2 cannot be triggered when preload setting is disabled.
IN_PROC_BROWSER_TEST_F(OmniboxPrerenderBrowserTest, DisableNetworkPrediction) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  content::WebContents* web_contents = GetActiveWebContents();

  // Disable network prediction.
  PrefService* prefs = GetProfile()->GetPrefs();
  prefetch::SetPreloadPagesState(prefs,
                                 prefetch::PreloadPagesState::kNoPreloading);
  ASSERT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kPreloadingDisabled);

  // Navigate to initial URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents, kInitialUrl));

  // Attempt to prerender a direct URL input.
  auto* predictor = GetAutocompleteActionPredictor();
  ASSERT_TRUE(predictor);
  GURL prerender_url = embedded_test_server()->GetURL("/simple.html");
  predictor->StartPrerendering(prerender_url, *web_contents, gfx::Size(50, 50));

  // Since preload setting is disabled, prerender shouldn't be triggered.
  base::RunLoop().RunUntilIdle();
  int host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_EQ(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  {
    // Navigate to a different URL other than the prerender_url to flush the
    // metrics.
    GURL kUrl_a = embedded_test_server()->GetURL("a.com", "/title1.html");
    ASSERT_TRUE(content::NavigateToURL(web_contents, kUrl_a));

    ukm::SourceId ukm_source_id =
        web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 1u);

    UkmEntry expected_entry = ukm_entry_builder().BuildEntry(
        ukm_source_id, content::PreloadingType::kPrerender,
        content::PreloadingEligibility::kPreloadingDisabled,
        content::PreloadingHoldbackStatus::kUnspecified,
        content::PreloadingTriggeringOutcome::kUnspecified,
        content::PreloadingFailureReason::kUnspecified,
        /*accurate=*/false);
    EXPECT_EQ(ukm_entries[0], expected_entry)
        << content::test::ActualVsExpectedUkmEntryToString(ukm_entries[0],
                                                           expected_entry);
  }

  // Re-enable the setting.
  prefetch::SetPreloadPagesState(
      prefs, prefetch::PreloadPagesState::kStandardPreloading);
  ASSERT_EQ(prefetch::IsSomePreloadingEnabled(*prefs),
            content::PreloadingEligibility::kEligible);

  content::test::PrerenderHostRegistryObserver registry_observer(*web_contents);
  // Attempt to trigger prerendering again.
  predictor->StartPrerendering(prerender_url, *web_contents, gfx::Size(50, 50));

  // Since preload setting is enabled, prerender should be triggered
  // successfully.
  registry_observer.WaitForTrigger(prerender_url);
  host_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);

  {
    // Navigate to prerender_url.
    content::NavigationHandleObserver activation_observer(web_contents,
                                                          prerender_url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, prerender_url));

    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);

    // There should be 2 entries after we attempt Prerender again.
    EXPECT_EQ(ukm_entries.size(), 2u);

    UkmEntry expected_entry = ukm_entry_builder().BuildEntry(
        ukm_source_id, content::PreloadingType::kPrerender,
        content::PreloadingEligibility::kEligible,
        content::PreloadingHoldbackStatus::kAllowed,
        content::PreloadingTriggeringOutcome::kSuccess,
        content::PreloadingFailureReason::kUnspecified,
        /*accurate=*/true);
    EXPECT_EQ(ukm_entries[1], expected_entry)
        << content::test::ActualVsExpectedUkmEntryToString(ukm_entries[1],
                                                           expected_entry);
  }
}

class PrerenderOmniboxSearchSuggestionBrowserTest
    : public OmniboxPrerenderBrowserTest {
 public:
  PrerenderOmniboxSearchSuggestionBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kSupportSearchSuggestionForPrerender2}, {});
  }

  void SetUp() override {
    prerender_helper().SetUp(&search_engine_server_);
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    OmniboxPrerenderBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    search_engine_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    search_engine_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(search_engine_server_.Start());
    SetUrlTemplate();
  }

  void SetNewUrlTemplate(const std::string& prerender_page_target) {
    prerender_page_target_ = prerender_page_target;
    SetUrlTemplate();
  }

 protected:
  int PrerenderQuery(const std::string& search_terms,
                     const GURL& expected_prerender_url) {
    AutocompleteMatch match = CreateSearchSuggestionMatch(search_terms);
    prerender_manager_->StartPrerenderSearchSuggestion(match);
    int host_id = prerender_helper().GetHostForUrl(expected_prerender_url);
    EXPECT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
    return host_id;
  }

  GURL GetSearchSuggestionUrl(const std::string& search_terms,
                              bool is_prerender) {
    std::string url_template = prerender_page_target_ + "?q=$1$2&type=test";
    return search_engine_server_.GetURL(
        kSearchDomain,
        base::ReplaceStringPlaceholders(
            url_template, {search_terms, is_prerender ? "&pf=cs" : ""},
            nullptr));
  }

  void InitializePrerenderManager() {
    PrerenderManager::CreateForWebContents(GetActiveWebContents());

    prerender_manager_ =
        PrerenderManager::FromWebContents(GetActiveWebContents());
    ASSERT_TRUE(prerender_manager_);
  }

  void NavigateToPrerenderedResult(const GURL& expected_prerender_url) {
    content::TestNavigationObserver observer(GetActiveWebContents());
    GetActiveWebContents()->OpenURL(content::OpenURLParams(
        expected_prerender_url, content::Referrer(),
        WindowOpenDisposition::CURRENT_TAB,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
        /*is_renderer_initiated=*/false));
    observer.Wait();
  }

  void PrerenderAndActivate(const std::string& search_terms,
                            bool update_history_before_activation) {
    GURL expected_prerender_url =
        GetSearchSuggestionUrl(search_terms, /*is_prerender=*/true);
    int host_id = PrerenderQuery(search_terms, expected_prerender_url);
    prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                      expected_prerender_url);

    std::string script = R"(
      const url = new URL(document.URL);
      url.searchParams.delete('pf');
      history.replaceState(null, "", url.toString());
    )";
    GURL expected_activated_url =
        GetSearchSuggestionUrl(search_terms, /*is_prerender=*/false);
    content::RenderFrameHost* prerender_frame_host =
        prerender_helper().GetPrerenderedMainFrameHost(host_id);
    if (update_history_before_activation) {
      ASSERT_EQ(true, content::ExecJs(prerender_frame_host, script));
    }
    NavigateToPrerenderedResult(expected_prerender_url);
    if (!update_history_before_activation) {
      ASSERT_EQ(true, content::ExecJs(prerender_frame_host, script));
    }
    EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(),
              expected_activated_url);
  }

  PrerenderManager* prerender_manager() { return prerender_manager_; }

 private:
  AutocompleteMatch CreateSearchSuggestionMatch(
      const std::string& search_terms) {
    AutocompleteMatch match;
    match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
        base::UTF8ToUTF16(search_terms));
    match.search_terms_args->original_query = base::UTF8ToUTF16(search_terms);
    match.destination_url =
        GetSearchSuggestionUrl(search_terms, /*is_prerender=*/false);
    match.keyword = base::UTF8ToUTF16(search_terms);
    match.RecordAdditionalInfo("should_prerender", "true");
    return match;
  }

  void SetUrlTemplate() {
    TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());
    TemplateURLData data;
    data.SetShortName(kSearchDomain16);
    data.SetKeyword(data.short_name());
    data.SetURL(
        search_engine_server_
            .GetURL(kSearchDomain,
                    prerender_page_target_ +
                        "?q={searchTerms}&{google:prefetchSource}type=test")
            .spec());
    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  constexpr static char kSearchDomain[] = "a.test";
  constexpr static char16_t kSearchDomain16[] = u"a.test";
  raw_ptr<PrerenderManager, DanglingUntriaged> prerender_manager_;
  net::test_server::EmbeddedTestServer search_engine_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
  std::string prerender_page_target_ = "/title1.html";
  base::test::ScopedFeatureList feature_list_;
};

class PrerenderOmniboxSearchSuggestionReloadBrowserTest
    : public testing::WithParamInterface<bool>,
      public PrerenderOmniboxSearchSuggestionBrowserTest {
 public:
  PrerenderOmniboxSearchSuggestionReloadBrowserTest() {
    // Disable BFCache, to test the HTTP Cache path.
    feature_list_.InitWithFeatures(
        {features::kSupportSearchSuggestionForPrerender2},
        {features::kBackForwardCache});
  }

 protected:
  bool UpdateHistoryBeforeActivation() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderOmniboxSearchSuggestionReloadBrowserTest,
                         testing::Bool());

// Test back or forward navigations can use the HTTP Cache.
IN_PROC_BROWSER_TEST_P(PrerenderOmniboxSearchSuggestionReloadBrowserTest,
                       BackNavigationHitsHttpCache) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  InitializePrerenderManager();

  // 1. Prerender the first page.
  std::string search_terms_1 = "prerender2222";
  GURL expected_prerender_url_1 =
      GetSearchSuggestionUrl(search_terms_1, /*is_prerender=*/true);
  GURL expected_activated_url_1 =
      GetSearchSuggestionUrl(search_terms_1, /*is_prerender=*/false);
  PrerenderAndActivate(search_terms_1, UpdateHistoryBeforeActivation());
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_activated_url_1));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prerender_url_1));

  // 2. Prerender and activate another page.
  std::string search_terms_2 = "prefetch233";
  GURL expected_prerender_url_2 =
      GetSearchSuggestionUrl(search_terms_2, /*is_prerender=*/true);
  GURL expected_activated_url_2 =
      GetSearchSuggestionUrl(search_terms_2, /*is_prerender=*/false);
  PrerenderAndActivate(search_terms_2, UpdateHistoryBeforeActivation());
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_activated_url_2));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prerender_url_2));

  // 3. Navigate back. Chrome is supposed to read the response from the cache,
  // instead of sending another request.
  content::TestNavigationObserver back_load_observer(GetActiveWebContents());
  GetActiveWebContents()->GetController().GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(expected_activated_url_1,
            GetActiveWebContents()->GetLastCommittedURL());
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_activated_url_1));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prerender_url_1));
}

class PrerenderOmniboxSearchSuggestionExpiryBrowserTest
    : public PrerenderOmniboxSearchSuggestionBrowserTest {
 public:
  PrerenderOmniboxSearchSuggestionExpiryBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSupportSearchSuggestionForPrerender2, {}},
         {kSearchPrefetchServicePrefetching,
          {
              {"prefetch_caching_limit_ms", "10"},
          }}},
        {});
  }

 protected:
  void PrerenderQueryAndWaitForExpiring(const std::string& search_terms,
                                        const GURL& expected_prerender_url) {
    int host_id = PrerenderQuery(search_terms, expected_prerender_url);

    content::test::PrerenderHostObserver prerender_observer(
        *GetActiveWebContents(), host_id);

    // The prerender will be destroyed automatically soon, since the duration is
    // set to 10ms.
    prerender_observer.WaitForDestroyed();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that an ongoing prerender which loads an SRP should be canceled
// automatically after the expiry duration.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxSearchSuggestionExpiryBrowserTest,
                       SearchPrerenderExpiry) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  InitializePrerenderManager();

  std::string search_query = "prerender2";
  GURL expected_prerender_url =
      GetSearchSuggestionUrl("prerender222", /*is_prerender=*/true);
  PrerenderQueryAndWaitForExpiring("prerender222", expected_prerender_url);

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      /*PrerenderFinalStatus::kTriggerDestroyed*/ 16, 1);

  // Select the prerender hint. The prerendered result has been deleted, so
  // browser loads the search result over again.
  content::TestNavigationObserver observer(GetActiveWebContents());
  GetActiveWebContents()->OpenURL(content::OpenURLParams(
      expected_prerender_url, content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
  observer.Wait();

  // The prediction is correct, so kHitFinished should be recorded.
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kHitFinished, 1);
  // Since the prerendered page ran out of time, the timing metric should
  // record `prefetch_caching_limit_ms`.
  histogram_tester.ExpectUniqueTimeSample(
      "Prerender.Experimental.Search."
      "FirstCorrectPrerenderHintReceivedToRealSearchNavigationStartedDuration",
      base::Milliseconds(10), 1);
}

// Tests that kCanceled is correctly recorded in the case that PrerenderManager
// receives a new suggestion. Note: kCancel should only recorded when
// PrerenderManager receives a new suggestion or on primary-page changed.
// Otherwise one prediction might be recorded twice.
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxSearchSuggestionExpiryBrowserTest,
                       DifferentSuggestionAfterPrerenderExpired) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  InitializePrerenderManager();

  GURL expected_prerender_url =
      GetSearchSuggestionUrl("prerender222", /*is_prerender=*/true);
  // Prerender the first query, and wait for it to be deleted.
  PrerenderQueryAndWaitForExpiring("prerender222", expected_prerender_url);

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      /*PrerenderFinalStatus::kTriggerDestroyed*/ 16, 1);

  // Nothing should be recorded. Because there is no new navigation nor new
  // search suggestion.
  histogram_tester.ExpectTotalCount(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine, 0);

  // Suggest to prerender another term.
  GURL prerender_url_2 =
      GetSearchSuggestionUrl("prerender233", /*is_prerender=*/true);
  PrerenderQuery("prerender233", prerender_url_2);

  // PrerenderPredictionStatus::kCancelled should be recorded for the prediction
  // of "prerender222".
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kCancelled, 1);

  content::TestNavigationObserver observer(GetActiveWebContents());
  GetActiveWebContents()->OpenURL(content::OpenURLParams(
      prerender_url_2, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      /*is_renderer_initiated=*/false));
  observer.Wait();

  // The prediction is correct, so kHitFinished should be recorded.
  histogram_tester.ExpectBucketCount(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kHitFinished, 1);

  // Two predictions, two samples.
  histogram_tester.ExpectTotalCount(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine, 2);
}

}  // namespace
