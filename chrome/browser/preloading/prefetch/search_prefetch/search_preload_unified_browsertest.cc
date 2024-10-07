// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/cache_alias_search_prefetch_url_loader.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_request.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_preload_test_response_utils.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/streaming_search_prefetch_url_loader.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using ukm::builders::Preloading_Attempt;
using ukm::builders::Preloading_Prediction;
using ukm::builders::PrerenderPageLoad;
static const auto kMockElapsedTime =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime;

// Sets up testing context for the search preloading features: search prefetch
// and search prerender.
// These features are able to coordinate with the other: A prefetched result
// might be upgraded to prerender when needed (usually when service suggests
// clients to do so), and they share the prefetched response and other
// resources, so it is a unified test designed to test the interaction between
// these two features.
class SearchPreloadUnifiedBrowserTest : public PlatformBrowserTest,
                                        public SearchPreloadResponseController {
 public:
  SearchPreloadUnifiedBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SearchPreloadUnifiedBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kSupportSearchSuggestionForPrerender2, {}},
            {kSearchPrefetchServicePrefetching,
             {{"max_attempts_per_caching_duration", "3"},
              {"cache_size", "1"},
              {"device_memory_threshold_MB", "0"}}},
        },
        /*disabled_features=*/{});
  }

  void SetUp() override {
    prerender_helper().RegisterServerRequestMonitor(&search_engine_server_);
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up a generic server.
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Set up server for search engine.
    search_engine_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    search_engine_server_.RegisterRequestHandler(base::BindRepeating(
        &SearchPreloadUnifiedBrowserTest::HandleSearchRequest,
        base::Unretained(this)));
    ASSERT_TRUE(search_engine_server_.Start());

    TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());
    TemplateURLData data;
    data.SetShortName(kSearchDomain16);
    data.SetKeyword(data.short_name());
    data.SetURL(search_engine_server_
                    .GetURL(kSearchDomain,
                            "/search_page.html?q={searchTerms}&{google:"
                            "assistedQueryStats}{google:prefetchSource}"
                            "type=test")
                    .spec());
    data.suggestions_url =
        search_engine_server_.GetURL(kSearchDomain, "/?q={searchTerms}").spec();
    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    attempt_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            chrome_preloading_predictor::kDefaultSearchEngine);
    prediction_entry_builder_ =
        std::make_unique<content::test::PreloadingPredictionUkmEntryBuilder>(
            chrome_preloading_predictor::kDefaultSearchEngine);
    scoped_test_timer_ =
        std::make_unique<base::ScopedMockElapsedTimersForTest>();

    // Reset pointer position to avoid the pointer hover on the back button
    // that unintentionally triggers `kBackButtonHover` preloading, which may
    // cause flaky tests due to UKM mismatch.
    ResetPointerPosition();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSearchRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("favicon") != std::string::npos) {
      return nullptr;
    }

    std::string content = R"(
      <html><body>
      PRERENDER: HI PREFETCH! \o/
      </body></html>
    )";
    base::StringPairs headers = {
        {"Content-Length", base::NumberToString(content.length())},
        {"content-type", "text/html"}};
    bool is_invalid_response_body =
        request.GetURL().spec().find("invalid_content") != std::string::npos;

    net::HttpStatusCode code = net::HttpStatusCode::HTTP_OK;
    if (request.GetURL().spec().find("failed_terms") != std::string::npos) {
      code = net::HttpStatusCode::HTTP_SERVICE_UNAVAILABLE;
    }

    std::unique_ptr<net::test_server::HttpResponse> resp =
        CreateDeferrableResponse(code, headers,
                                 is_invalid_response_body ? "" : content);

    return resp;
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(search_engine_server_.ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const content::test::PreloadingAttemptUkmEntryBuilder&
  attempt_entry_builder() {
    return *attempt_entry_builder_;
  }

  const content::test::PreloadingPredictionUkmEntryBuilder&
  prediction_entry_builder() {
    return *prediction_entry_builder_;
  }

  SearchPrefetchURLLoader::RequestHandler CreatePrefetchRequestHandler(
      const network::ResourceRequest& request) {
    return search_prefetch_service_->TakePrefetchResponseFromMemoryCache(
        request);
  }

  SearchPrefetchURLLoader::RequestHandler CreatePrerenderRequestHandler(
      const network::ResourceRequest& request) {
    return search_prefetch_service_->MaybeCreateResponseReader(request);
  }

  network::ResourceRequest CreateServingRequest(const GURL& url) {
    network::ResourceRequest serving_request;
    serving_request.url = url;
    serving_request.method = "GET";
    serving_request.transition_type =
        ui::PageTransition::PAGE_TRANSITION_GENERATED |
        ui ::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR;
    return serving_request;
  }

 protected:
  enum class PrerenderHint { kEnabled, kDisabled };
  enum class PrefetchHint { kEnabled, kDisabled };
  enum class UrlType {
    // For URLs that will be used for a real navigation.
    kReal,
    // For URLs that will be used for prefetch requests.
    kPrefetch,
    // For URLs that will be used for prerender requests.
    kPrerender
  };

  void SetUpContext() {
    // Have SearchPrefetchService and PrerenderManager prepared.
    PrerenderManager::CreateForWebContents(GetActiveWebContents());
    prerender_manager_ =
        PrerenderManager::FromWebContents(GetActiveWebContents());
    ASSERT_TRUE(prerender_manager_);
    search_prefetch_service_ = SearchPrefetchServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
    ASSERT_NE(nullptr, search_prefetch_service_);
  }

  GURL GetSearchUrl(const std::string& search_terms, UrlType url_type) {
    // $1: the search terms that will be retrieved.
    // $2: parameter for prefetch request. Should be &pf=cs if the url is
    // expected to declare itself as a prefetch request. Otherwise it should be
    // an empty string.
    std::string url_template = "/search_page.html?q=$1$2&type=test";
    bool attach_prefetch_flag;
    switch (url_type) {
      case UrlType::kReal:
      case UrlType::kPrerender:
        attach_prefetch_flag = false;
        break;
      case UrlType::kPrefetch:
        attach_prefetch_flag = true;
        break;
    }
    return search_engine_server_.GetURL(
        kSearchDomain,
        base::ReplaceStringPlaceholders(
            url_template, {search_terms, attach_prefetch_flag ? "&pf=cs" : ""},
            nullptr));
  }

  GURL GetCanonicalSearchURL(const GURL& prefetch_url) {
    GURL canonical_search_url;
    HasCanonicalPreloadingOmniboxSearchURL(prefetch_url,
                                           chrome_test_utils::GetProfile(this),
                                           &canonical_search_url);
    return canonical_search_url;
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

  void ChangeAutocompleteResult(const std::string& original_query,
                                const std::string& search_terms,
                                PrerenderHint prerender_hint,
                                PrefetchHint prefetch_hint) {
    AutocompleteInput input(base::ASCIIToUTF16(original_query),
                            metrics::OmniboxEventProto::BLANK,
                            ChromeAutocompleteSchemeClassifier(
                                chrome_test_utils::GetProfile(this)));

    AutocompleteMatch autocomplete_match = CreateSearchSuggestionMatch(
        original_query, search_terms, prerender_hint, prefetch_hint);
    AutocompleteResult autocomplete_result;
    autocomplete_result.AppendMatches({autocomplete_match});
    search_prefetch_service()->OnResultChanged(GetActiveWebContents(),
                                               autocomplete_result);
  }

  void WaitUntilStatusChangesTo(
      const GURL& canonical_search_url,
      std::vector<SearchPrefetchStatus> acceptable_status) {
    while (true) {
      std::optional<SearchPrefetchStatus> current_status =
          search_prefetch_service()->GetSearchPrefetchStatusForTesting(
              canonical_search_url);
      if (current_status &&
          base::Contains(acceptable_status, current_status.value())) {
        break;
      }
      if (!current_status && acceptable_status.empty()) {
        break;
      }

      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  // `WaitEvent::kLoadStopped` is the default value for a
  // TestNavigationObserver. Pass another event type to not wait until it
  // finishes loading.
  void NavigateToPrerenderedResult(
      const GURL& expected_prerender_url,
      content::TestNavigationObserver::WaitEvent wait_event =
          content::TestNavigationObserver::WaitEvent::kLoadStopped) {
    content::TestNavigationObserver observer(GetActiveWebContents());
    observer.set_wait_event(wait_event);
    GetActiveWebContents()->OpenURL(
        content::OpenURLParams(
            expected_prerender_url, content::Referrer(),
            WindowOpenDisposition::CURRENT_TAB,
            ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                      ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
            /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
    observer.Wait();
  }

  void WaitForActivatedPageLoaded() {
    // TODO(crbug.com/40256454):
    // `content::WaitForLoadStop(GetActiveWebContents())` would end before the
    // page actually finishes loading. This is the workaround to ensure that the
    // page is fully loaded.
    std::string script_string = R"(
      function get_inner_html () {
        if(document.documentElement){
          return document.documentElement.innerHTML;
        }
        return "";
      }
      get_inner_html();
    )";
    while (true) {
      std::string inner_html =
          content::EvalJs(GetActiveWebContents(), script_string)
              .ExtractString();
      if (base::Contains(inner_html, "PREFETCH")) {
        break;
      }
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  PrerenderManager* prerender_manager() { return prerender_manager_; }

  SearchPrefetchService* search_prefetch_service() {
    return search_prefetch_service_;
  }

  void ShutDownSearchServer() {
    ASSERT_TRUE(search_engine_server_.ShutdownAndWaitUntilComplete());
  }

  // Similar to UkmRecorder::GetMergedEntriesByName(), but returned map is keyed
  // by source URL.
  std::map<GURL, ukm::mojom::UkmEntryPtr> GetMergedUkmEntries(
      const std::string& entry_name) {
    auto entries = test_ukm_recorder()->GetMergedEntriesByName(entry_name);
    std::map<GURL, ukm::mojom::UkmEntryPtr> result;
    for (auto& kv : entries) {
      const ukm::mojom::UkmEntry* entry = kv.second.get();
      const ukm::UkmSource* source =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (!source) {
        continue;
      }
      EXPECT_TRUE(source->url().is_valid());
      result.emplace(source->url(), std::move(kv.second));
    }
    return result;
  }

 private:
  void ResetPointerPosition() {
#if !BUILDFLAG(IS_ANDROID)
    content::WebContents* contents = GetActiveWebContents();
    content::InputEventAckWaiter waiter(
        contents->GetPrimaryMainFrame()->GetRenderWidgetHost(),
        blink::WebInputEvent::Type::kMouseMove);
    SimulateMouseEvent(contents, blink::WebMouseEvent::Type::kMouseMove,
                       blink::WebMouseEvent::Button::kNoButton,
                       gfx::Point(0, 0));
    waiter.Wait();
#else
    // TODO(crbug.com/339718083): Simulate |WebGestureEvent| to make this
    // function work for Android.
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  AutocompleteMatch CreateSearchSuggestionMatch(
      const std::string& original_query,
      const std::string& search_terms,
      PrerenderHint prerender_hint,
      PrefetchHint prefetch_hint) {
    AutocompleteMatch match;
    match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
        base::UTF8ToUTF16(search_terms));
    match.search_terms_args->original_query = base::UTF8ToUTF16(original_query);
    match.destination_url = GetSearchUrl(search_terms, UrlType::kReal);
    match.keyword = base::UTF8ToUTF16(original_query);
    if (prerender_hint == PrerenderHint::kEnabled) {
      match.RecordAdditionalInfo("should_prerender", "true");
    }
    if (prefetch_hint == PrefetchHint::kEnabled) {
      match.RecordAdditionalInfo("should_prefetch", "true");
    }
    match.allowed_to_be_default_match = true;
    return match;
  }

  constexpr static char kSearchDomain[] = "a.test";
  constexpr static char16_t kSearchDomain16[] = u"a.test";
  raw_ptr<PrerenderManager, AcrossTasksDanglingUntriaged> prerender_manager_ =
      nullptr;
  raw_ptr<SearchPrefetchService, AcrossTasksDanglingUntriaged>
      search_prefetch_service_ = nullptr;
  net::test_server::EmbeddedTestServer search_engine_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      attempt_entry_builder_;
  std::unique_ptr<content::test::PreloadingPredictionUkmEntryBuilder>
      prediction_entry_builder_;

  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> scoped_test_timer_;
};

// Tests that the SearchSuggestionService can trigger prerendering after the
// corresponding prefetch request succeeds.
// TODO(crbug.com/40943413): enable the flaky test.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PrerenderHintReceivedBeforeSucceed \
  DISABLED_PrerenderHintReceivedBeforeSucceed
#else
#define MAYBE_PrerenderHintReceivedBeforeSucceed \
  PrerenderHintReceivedBeforeSucceed
#endif
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       MAYBE_PrerenderHintReceivedBeforeSucceed) {
  SearchPrefetchServiceFactory::GetForProfile(chrome_test_utils::GetProfile(this));
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  // Snapshot those samples recorded before the main test.
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.PrefetchServingReason2", 1);

  std::string search_query = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  ChangeAutocompleteResult(search_query, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // The suggestion service should hint expected_prerender_url, and prerendering
  // for this url should start.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason2.Prerender",
      SearchPrefetchServingReason::kServed, 1);

  // Prefetch should be triggered as well.
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kComplete);

  // No prerender requests went through network, so there should be only one
  // request and it is with the prefetch flag attached.
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        expected_prerender_url);
  NavigateToPrerenderedResult(expected_prerender_url);
  prerender_observer.WaitForActivation();
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kComplete, 1);

  // On prerender activation, `URLLoaderRequestInterceptor` would not be called,
  // so no more sample should be recorded.
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.PrefetchServingReason2", 1);
  {
    // Check that we store one entry corresponding to the prerender prediction
    // and attempt with prefetch hints.
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    auto prediction_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Prediction::kEntryName,
        content::test::kPreloadingPredictionUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 2u);
    EXPECT_EQ(prediction_ukm_entries.size(), 2u);

    // Prerender should succeed and should be used for the next navigation.
    std::vector<UkmEntry> expected_prediction_entries = {
        prediction_entry_builder().BuildEntry(ukm_source_id,
                                              /*confidence=*/80,
                                              /*accurate_prediction=*/true),
        std::make_unique<content::test::PreloadingPredictionUkmEntryBuilder>(
            chrome_preloading_predictor::kOmniboxSearchSuggestDefaultMatch)
            ->BuildEntry(ukm_source_id,
                         /*confidence=*/80,
                         /*accurate_prediction=*/true),
    };
    std::vector<UkmEntry> expected_attempt_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kReady,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kSuccess,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(attempt_ukm_entries,
                testing::UnorderedElementsAreArray(expected_attempt_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(
               attempt_ukm_entries, expected_attempt_entries);
    EXPECT_THAT(prediction_ukm_entries,
                testing::UnorderedElementsAreArray(expected_prediction_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(
               prediction_ukm_entries, expected_prediction_entries);
  }

  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_EQ(entries.size(), 1u);

  const ukm::mojom::UkmEntry* prerendered_page_entry =
      entries[expected_prerender_url].get();
  ASSERT_TRUE(prerendered_page_entry);
  test_ukm_recorder()->ExpectEntryMetric(
      prerendered_page_entry, PrerenderPageLoad::kWasPrerenderedName, 1);
  test_ukm_recorder()->ExpectEntryMetric(
      prerendered_page_entry, PrerenderPageLoad::kNavigation_PageTransitionName,
      ui::PAGE_TRANSITION_GENERATED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
}

// Tests that the SearchSuggestionService can trigger prerendering if it
// receives prerender hints after the previous prefetch request succeeds.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrerenderHintReceivedAfterSucceed) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);

  // Wait until prefetch request succeeds.
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(
      GetCanonicalSearchURL(expected_prefetch_url),
      {SearchPrefetchStatus::kCanBeServed, SearchPrefetchStatus::kComplete});
  std::string search_query_2 = "prer";
  ChangeAutocompleteResult(search_query_2, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // The suggestion service should hint `expected_prefetch_url`, and
  // prerendering for this url should start.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);

  // No prerender requests went through network, so there should be only one
  // request and it is with the prefetch flag attached.
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));

  // Activate.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        expected_prerender_url);
  NavigateToPrerenderedResult(expected_prerender_url);
  prerender_observer.WaitForActivation();
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kComplete, 1);
  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 3u);

    // Prerender should succeed and should be used for the next navigation.
    std::vector<UkmEntry> expected_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kReady,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kDuplicate,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kSuccess,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }

  // No prerender requests went through network.
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_EQ(entries.size(), 1u);

  const ukm::mojom::UkmEntry* prerendered_page_entry =
      entries[expected_prerender_url].get();
  ASSERT_TRUE(prerendered_page_entry);
  test_ukm_recorder()->ExpectEntryMetric(
      prerendered_page_entry, PrerenderPageLoad::kWasPrerenderedName, 1);
  test_ukm_recorder()->ExpectEntryMetric(
      prerendered_page_entry, PrerenderPageLoad::kNavigation_PageTransitionName,
      ui::PAGE_TRANSITION_GENERATED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
}

// Tests that the SearchSuggestionService will not trigger prerender if the
// prefetch failed.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       FailedPrefetchCannotBeUpgraded) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  std::string search_query = "fail";
  std::string prerender_query = "failed_terms";

  ChangeAutocompleteResult(search_query, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // Prefetch should be triggered, and the prefetch request should fail.
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(
              GetSearchUrl(prerender_query, UrlType::kPrerender)));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(
      GetCanonicalSearchURL(GetSearchUrl(prerender_query, UrlType::kPrerender)),
      {SearchPrefetchStatus::kRequestFailed});

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.FetchResult.SuggestionPrefetch", false, 1);
  EXPECT_FALSE(prerender_manager()->HasSearchResultPagePrerendered());

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_EQ(entries.size(), 0u);
}

// Tests that the SearchSuggestionService will not trigger prerender if the
// suggestions changes before SearchSuggestionService receives a servable
// response.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       SuggestionChangeBeforeStartPrerender) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  set_service_deferral_type(
      SearchPreloadTestResponseDeferralType::kDeferHeader);

  // 1. Type the first query.
  std::string search_query_1 = "hang";
  std::string prerender_query_1 = "hang_response";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query_1, UrlType::kPrefetch);
  ChangeAutocompleteResult(search_query_1, prerender_query_1,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 2. Prefetch should be triggered.
  auto prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url),
                           {SearchPrefetchStatus::kCanBeServed});

  // 3. Type a different query which results in different suggestions.
  std::string search_query_2 = "pre";
  ChangeAutocompleteResult(search_query_2, search_query_2,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);

  // 4. The old prefetch should not be cancelled.
  prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(
              GetSearchUrl(prerender_query_1, UrlType::kPrerender)));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(GetSearchUrl(
                               prerender_query_1, UrlType::kPrerender)),
                           {SearchPrefetchStatus::kCanBeServed});

  EXPECT_FALSE(prerender_manager()->HasSearchResultPagePrerendered());

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_EQ(entries.size(), 0u);
}

// Tests the activated prerendered page records navigation timings correctly.
// Though the prerender happens before the activation navigation, the timings
// should not be a negative value, so that the activated page can measure the
// timing correctly.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       SetLoaderTimeCorrectly) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);

  // 2. Wait until prefetch completed.
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url),
                           {SearchPrefetchStatus::kComplete});

  // 3. Type a longer one.
  std::string search_query_2 = "preren";
  ChangeAutocompleteResult(search_query_2, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kComplete);

  // 4. Activate.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  NavigateToPrerenderedResult(expected_prerender_url);
  prerender_observer.WaitForActivation();

  // Check the response time is non-negative.
  std::string script =
      "window.performance.timing."
      "responseEnd - window.performance.timing.responseStart";
  EXPECT_LE(0, content::EvalJs(GetActiveWebContents(), script));

  // Check the response start is after (or the same as) request start.
  script =
      "window.performance.timing."
      "responseStart - window.performance.timing.requestStart";
  EXPECT_LE(0, content::EvalJs(GetActiveWebContents(), script));

  // Check request start is after (or the same as) navigation start.
  script =
      "window.performance.timing."
      "requestStart - window.performance.timing.navigationStart";
  EXPECT_LE(0, content::EvalJs(GetActiveWebContents(), script));
}

// Tests that prerender fails as well if the prefetch response that prerender
// uses fails.
// TODO(crbug.com/351753962): Fix flakiness and re-enable.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_NavigationFailsAfterPrefetchServedTheResponse \
  DISABLED_NavigationFailsAfterPrefetchServedTheResponse
#else
#define MAYBE_NavigationFailsAfterPrefetchServedTheResponse \
  NavigationFailsAfterPrefetchServedTheResponse
#endif
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       MAYBE_NavigationFailsAfterPrefetchServedTheResponse) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");

  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  set_service_deferral_type(SearchPreloadTestResponseDeferralType::kDeferBody);
  SetUpContext();

  // 1. Type the first query.
  std::string search_query_1 = "invalid";
  std::string prerender_query_1 = "invalid_content";

  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query_1, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query_1, UrlType::kPrerender);

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  ChangeAutocompleteResult(search_query_1, prerender_query_1,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 2. Prefetch and prerender should be triggered.
  registry_observer.WaitForTrigger(expected_prerender_url);

  auto prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kCanBeServed, prefetch_status.value());

  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));

  // 3. Make the prerender fail to read the response body by sending "Finish"
  // signal before sending content body.
  DispatchDelayedResponseTask();
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);

  // 4. The prerender will be destroyed because of the failing request.
  prerender_observer.WaitForDestroyed();

  // Navigate away to flush the metrics.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kNavigatedUrl));

  {
    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 2u);

    // DispatchDelayedResponseTask will dispatch
    // StreamingSearchPrefetchURLLoader::OnComplete(), which in turn calls
    // SearchPrefetchRequest::ErrorEncountered() resulting in prerender
    // cancelling with status 1001 i.e., => PrerenderFinalStatus::kDestroyed.
    std::vector<UkmEntry> expected_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kFailure,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kFailure,
            static_cast<content::PreloadingFailureReason>(1001),
            /*accurate=*/false),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }

  // Prerender should not retry the request.
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));

  auto entries = GetMergedUkmEntries(PrerenderPageLoad::kEntryName);
  EXPECT_EQ(entries.size(), 0u);
}

// Tests the prerender works well when running in the
// `StreamingSearchPrefetchURLLoader::serving_from_data_` mode, even if the
// server is slow.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest, ChunkedResponseBody) {
  base::HistogramTester histogram_tester;
  set_service_deferral_type(
      SearchPreloadTestResponseDeferralType::kDeferChunkedResponseBody);

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");

  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  // 1. Type the first query.
  std::string search_query = "prer";
  std::string prerender_query = "prerender";

  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  ChangeAutocompleteResult(search_query.substr(0, search_query.size() - 1),
                           prerender_query, PrerenderHint::kDisabled,
                           PrefetchHint::kEnabled);

  // 2. Trigger prefetch and serve the first part of response body.
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url),
                           {SearchPrefetchStatus::kCanBeServed});
  DispatchDelayedResponseTask();

  // 3. Trigger prerender.
  ChangeAutocompleteResult(search_query, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  registry_observer.WaitForTrigger(expected_prerender_url);
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);

  // 4. Activate the prerendered page.
  NavigateToPrerenderedResult(
      expected_prerender_url,
      content::TestNavigationObserver::WaitEvent::kNavigationFinished);

  // To commit a navigation, we only need a valid header. So the prerendering
  // navigation is ready for activation.
  prerender_observer.WaitForActivation();

  DispatchDelayedResponseTask();
  content::WaitForLoadStop(GetActiveWebContents());

  // TODO(crbug.com/40256454):
  // `content::WaitForLoadStop(GetActiveWebContents())` would end before the
  // page actually finishes loading. This is the workaround to ensure that the
  // page is fully loaded.
  std::string script_string = R"(
      function get_inner_html () {
        if(document.documentElement){
          return document.documentElement.innerHTML;
        }
        return "";
      }
      get_inner_html();
    )";
  while (true) {
    std::string inner_html =
        content::EvalJs(GetActiveWebContents(), script_string).ExtractString();
    if (base::Contains(inner_html, "PREFETCH")) {
      break;
    }
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  // Prerender should not retry the request.
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
}

// Tests prerender is cancelled after SearchPrefetchService cancels prefetch
// requests.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest, DoNotRefetchSameTerms) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 2. Prefetch and prerender should be triggered, and chrome is waiting for
  // the body.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kComplete);

  // 3. Type a different query which results in the same suggestion.
  std::string search_query_2 = "prer";
  ChangeAutocompleteResult(search_query_2, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 4. Do not prefetch/prerender again.
  prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kComplete);

  // Navigate away to flush the metrics.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kNavigatedUrl));
  {
    // Check that we log the correct PreloadingEligibility metrics.
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 4u);

    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    std::vector<UkmEntry> expected_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kReady,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kReady,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kDuplicate,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kDuplicate,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }

  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason2.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kAttemptedQueryRecently, 1);
}

class HoldbackSearchPreloadUnifiedBrowserTest
    : public SearchPreloadUnifiedBrowserTest {
 public:
  void RunTest() {
    base::HistogramTester histogram_tester;
    const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
    ASSERT_TRUE(GetActiveWebContents());
    ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
    SetUpContext();

    std::string search_query_1 = "pre";
    std::string prerender_query = "prerender";
    GURL expected_prefetch_url =
        GetSearchUrl(prerender_query, UrlType::kPrefetch);
    GURL expected_prerender_url =
        GetSearchUrl(prerender_query, UrlType::kPrerender);
    content::test::PrerenderHostRegistryObserver registry_observer(
        *GetActiveWebContents());

    ChangeAutocompleteResult(search_query_1, prerender_query,
                             PrerenderHint::kDisabled, PrefetchHint::kEnabled);

    // Wait until prefetch request succeeds.
    std::optional<SearchPrefetchStatus> prefetch_status =
        search_prefetch_service()->GetSearchPrefetchStatusForTesting(
            GetCanonicalSearchURL(expected_prefetch_url));
    EXPECT_TRUE(prefetch_status.has_value());
    WaitUntilStatusChangesTo(
        GetCanonicalSearchURL(expected_prefetch_url),
        {SearchPrefetchStatus::kCanBeServed, SearchPrefetchStatus::kComplete});
    std::string search_query_2 = "prer";
    ChangeAutocompleteResult(search_query_2, prerender_query,
                             PrerenderHint::kEnabled, PrefetchHint::kEnabled);

    // The suggestion service should hint `expected_prefetch_url`, and
    // prerendering for this url should start.
    registry_observer.WaitForTrigger(expected_prerender_url);

    // Navigate to flush the metrics.
    content::NavigationHandleObserver activation_observer(
        GetActiveWebContents(), expected_prerender_url);
    ASSERT_TRUE(
        content::NavigateToURL(GetActiveWebContents(), expected_prerender_url));
    {
      ukm::SourceId ukm_source_id =
          activation_observer.next_page_ukm_source_id();
      auto ukm_entries = test_ukm_recorder()->GetEntries(
          Preloading_Attempt::kEntryName,
          content::test::kPreloadingAttemptUkmMetrics);
      EXPECT_EQ(ukm_entries.size(), 3u);

      // Prerender should be under holdback and not succeed.
      std::vector<UkmEntry> expected_entries = {
          attempt_entry_builder().BuildEntry(
              ukm_source_id, content::PreloadingType::kPrefetch,
              content::PreloadingEligibility::kEligible,
              content::PreloadingHoldbackStatus::kAllowed,
              content::PreloadingTriggeringOutcome::kSuccess,
              content::PreloadingFailureReason::kUnspecified,
              /*accurate=*/true,
              /*ready_time=*/kMockElapsedTime),
          attempt_entry_builder().BuildEntry(
              ukm_source_id, content::PreloadingType::kPrefetch,
              content::PreloadingEligibility::kEligible,
              content::PreloadingHoldbackStatus::kAllowed,
              content::PreloadingTriggeringOutcome::kDuplicate,
              content::PreloadingFailureReason::kUnspecified,
              /*accurate=*/true),
          attempt_entry_builder().BuildEntry(
              ukm_source_id, content::PreloadingType::kPrerender,
              content::PreloadingEligibility::kEligible,
              content::PreloadingHoldbackStatus::kHoldback,
              content::PreloadingTriggeringOutcome::kUnspecified,
              content::PreloadingFailureReason::kUnspecified,
              /*accurate=*/true),
      };
      EXPECT_THAT(ukm_entries,
                  testing::UnorderedElementsAreArray(expected_entries))
          << content::test::ActualVsExpectedUkmEntriesToString(
                 ukm_entries, expected_entries);
    }
  }
  ~HoldbackSearchPreloadUnifiedBrowserTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DSEPrerenderHoldbackSearchPreloadUnifiedBrowserTest
    : public HoldbackSearchPreloadUnifiedBrowserTest {
 public:
  DSEPrerenderHoldbackSearchPreloadUnifiedBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kSupportSearchSuggestionForPrerender2, {{}}},
            {kSearchPrefetchServicePrefetching,
             {{"max_attempts_per_caching_duration", "3"},
              {"cache_size", "4"},
              {"device_memory_threshold_MB", "0"}}},
            {features::kPrerenderDSEHoldback, {{}}},
        },
        /*disabled_features=*/{});
  }
};

// Tests that we log correct metrics for Prerender holdback in case of Search
// Prerender.
IN_PROC_BROWSER_TEST_F(DSEPrerenderHoldbackSearchPreloadUnifiedBrowserTest,
                       PrerenderDSEHoldbackTest) {
  RunTest();
}

class PreloadingConfigHoldbackSearchPreloadUnifiedBrowserTest
    : public HoldbackSearchPreloadUnifiedBrowserTest {
 public:
  PreloadingConfigHoldbackSearchPreloadUnifiedBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kSupportSearchSuggestionForPrerender2, {{}}},
            {kSearchPrefetchServicePrefetching,
             {{"max_attempts_per_caching_duration", "3"},
              {"cache_size", "4"},
              {"device_memory_threshold_MB", "0"}}},
            {features::kPrerenderDSEHoldback, {{}}},
        },
        {});
    preloading_config_override_.SetHoldback(
        content::PreloadingType::kPrerender,
        chrome_preloading_predictor::kDefaultSearchEngine, true);
  }

 private:
  content::test::PreloadingConfigOverride preloading_config_override_;
};

// Tests that we log correct metrics for Prerender holdback in case of Search
// Prerender.
IN_PROC_BROWSER_TEST_F(PreloadingConfigHoldbackSearchPreloadUnifiedBrowserTest,
                       PrerenderDSEHoldbackTest) {
  RunTest();
}

// Disables BFCache for testing back forward navigation can reuse the HTTP
// Cache.
class HTTPCacheSearchPreloadUnifiedBrowserTest
    : public SearchPreloadUnifiedBrowserTest {
 public:
  HTTPCacheSearchPreloadUnifiedBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kSupportSearchSuggestionForPrerender2, {{}}},
            {kSearchPrefetchServicePrefetching,
             {{"max_attempts_per_caching_duration", "3"},
              {"cache_size", "4"},
              {"device_memory_threshold_MB", "0"}}},
        },
        // Disable BackForwardCache to ensure that the page is not restored from
        // the cache.
        /*disabled_features=*/{features::kBackForwardCache});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test back or forward navigations can use the HTTP Cache.
IN_PROC_BROWSER_TEST_F(HTTPCacheSearchPreloadUnifiedBrowserTest,
                       BackwardHitHttpCache) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  std::string search_query_1 = "pre";
  std::string prerender_query_1 = "prerender";
  GURL expected_prefetch_url_1 =
      GetSearchUrl(prerender_query_1, UrlType::kPrefetch);
  GURL expected_prerender_url_1 =
      GetSearchUrl(prerender_query_1, UrlType::kPrerender);
  auto trigger_and_activate = [&](const std::string& search_query,
                                  const std::string& prerender_query) {
    GURL expected_prefetch_url =
        GetSearchUrl(prerender_query, UrlType::kPrefetch);
    GURL expected_prerender_url =
        GetSearchUrl(prerender_query, UrlType::kPrerender);
    ChangeAutocompleteResult(search_query, prerender_query,
                             PrerenderHint::kEnabled, PrefetchHint::kEnabled);
    registry_observer.WaitForTrigger(expected_prerender_url);
    WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prefetch_url),
                             {SearchPrefetchStatus::kCanBeServed});
    // No prerender requests went through network, so there should be only one
    // request and it is with the prefetch flag attached.
    EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
    EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));

    // Activate.
    content::test::PrerenderHostObserver prerender_observer(
        *GetActiveWebContents(), expected_prerender_url);
    NavigateToPrerenderedResult(expected_prerender_url);
    prerender_observer.WaitForActivation();

    // No prerender requests went through network.
    EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
    EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  };

  trigger_and_activate(search_query_1, prerender_query_1);
  // Trigger another preloading attempt and navigate to that page.
  trigger_and_activate("pref", "prefetch");

  // Navigate back. Chrome is supposed to read the response from the cache,
  // instead of sending another request.
  content::TestNavigationObserver back_load_observer(GetActiveWebContents());
  GetActiveWebContents()->GetController().GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(expected_prerender_url_1,
            GetActiveWebContents()->GetLastCommittedURL());
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url_1));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url_1));

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.CacheAliasFallbackReason",
      CacheAliasSearchPrefetchURLLoader::FallbackReason::kNoFallback, 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.CacheAliasElapsedTimeToFallback", 0);
}

// Tests the started prerender is destroyed after prefetch request expired.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrerenderGetDestroyedAfterPrefetchExpired) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  // Trigger prerender and prefetch.
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  std::string search_query = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  registry_observer.WaitForTrigger(expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  auto prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kCanBeServed, prefetch_status.value());

  // Fire the timer to make all prefetch requests expire.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  search_prefetch_service()->FireAllExpiryTimerForTesting();
  prerender_observer.WaitForDestroyed();
  prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_FALSE(prefetch_status.has_value());

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kCanBeServed, 1);
}

// TODO(https://cubug.com/1282624): This test should run on Android after we're
// able to interact with Android UI.
// TODO(crbug.com/40231021): On LacrOS, the window's bound changes
// unexpectedly, and it stops auto completing.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest, TriggerAndActivate) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);

  // 2. Prepare some context.
  AutocompleteInput input(
      base::ASCIIToUTF16(prerender_query), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->controller()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));

  // 3. Trigger prerender and prefetch.
  autocomplete_controller->Start(input);
  ui_test_utils::WaitForAutocompleteDone(browser());
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kComplete);
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason2.Prerender",
      SearchPrefetchServingReason::kServed, 1);

  // 4. Click and activate.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  omnibox->model()->OpenSelection();
  prerender_observer.WaitForActivation();
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kComplete, 1);
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
}

// Tests the metrics for analyzing the unideal scenario that prerender fails
// after taking response away. Without prerender, these prefetches could have
// helped improve the performance of loading SRPs, so it is necessary to
// understand the percentage of failing ones.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrerenderFailAfterResponseServed) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  GURL expected_real_url = GetSearchUrl(prerender_query, UrlType::kReal);

  // 2. Prepare some context.
  AutocompleteInput input(
      base::ASCIIToUTF16(prerender_query), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->controller()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));

  // 3. Trigger prerender and prefetch.
  autocomplete_controller->Start(input);
  ui_test_utils::WaitForAutocompleteDone(browser());
  ChangeAutocompleteResult(prerender_query, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kComplete);
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason2.Prerender",
      SearchPrefetchServingReason::kServed, 1);

  // 4. Fail the prerender.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(expected_prerender_url);
  ASSERT_TRUE(host_id);
  prerender_helper().CancelPrerenderedPage(host_id);
  prerender_observer.WaitForDestroyed();
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));

  // 5. Click the result.
  content::TestNavigationObserver navigation_observer(GetActiveWebContents(),
                                                      1);
  omnibox->model()->OpenSelection();
  navigation_observer.Wait();
  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPrefetch.PrefetchServingReason2",
      SearchPrefetchServingReason::kServed, 1);

  // 6. Fire the timer to make all prefetch requests expire
  search_prefetch_service()->FireAllExpiryTimerForTesting();
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kPrefetchServedForRealNavigation, 1);

  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests prerender is not cancelled after SearchPrefetchService cancels prefetch
// requests.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       SuggestionChangeAfterStartPrerender) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query_1 = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query_1, UrlType::kPrerender);
  GURL canonical_search_url = GetCanonicalSearchURL(expected_prerender_url);
  ChangeAutocompleteResult(search_query_1, prerender_query_1,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 2. Prefetch and prerender should be triggered.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          canonical_search_url);
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kComplete);
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason2.Prerender",
      SearchPrefetchServingReason::kServed, 1);

  // 3. Type a different query which results in different suggestions.
  std::string search_query_2 = "pre";
  ChangeAutocompleteResult(search_query_2, search_query_2,
                           PrerenderHint::kDisabled, PrefetchHint::kDisabled);

  // 4. Navigate to the initial prerender.
  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        expected_prerender_url);
  NavigateToPrerenderedResult(expected_prerender_url);
  prerender_observer.WaitForActivation();
  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 2u);

    // Prerender should be used for the next navigation as it won't be cancelled
    // when suggestions change.
    std::vector<UkmEntry> expected_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kReady,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kSuccess,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }
}

// Used by SearchPreloadUnifiedFallbackBrowserTest and
// NoCancelSearchPreloadUnifiedFallbackBrowserTest to check the streaming
// loader's status. Since the result can be set upon Mojo disconnection, we have
// to wait until it to be reported.
void CheckCorrectForwardingResultMetric(
    base::HistogramTester& histogram_tester,
    StreamingSearchPrefetchURLLoader::ForwardingResult result,
    int count) {
  while (true) {
    int num = histogram_tester.GetBucketCount(
        "Omnibox.SearchPreload.ForwardingResult.WasServedToPrerender", result);
    if (num >= count) {
      break;
    }
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPreload.ForwardingResult.WasServedToPrerender", result,
      count);
}

// Tests cancelling prerenders should not delete the prefetched responses.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrefetchSucceedAfterPrerenderFailed) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  WaitUntilStatusChangesTo(
      GetCanonicalSearchURL(expected_prerender_url),
      {SearchPrefetchStatus::kCanBeServed, SearchPrefetchStatus::kComplete});

  // 2. Prefetch and prerender should be triggered, and chrome is waiting for
  // the body.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);

  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kComplete);

  // 3. Cancel the prerenders
  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(expected_prerender_url);
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), host_id);
  // Ensure kCompleted is recorded.
  prerender_helper().WaitForPrerenderLoadCompletion(host_id);
  prerender_helper().CancelPrerenderedPage(host_id);
  prerender_observer.WaitForDestroyed();

  // 4. Prefetch should still exist.
  prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());

  base::RunLoop run_loop;
  search_prefetch_service()->SetLoaderDestructionCallbackForTesting(
      GetCanonicalSearchURL(expected_prerender_url), run_loop.QuitClosure());
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), expected_prerender_url));
  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kPrefetchServedForRealNavigation, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender",
      StreamingSearchPrefetchURLLoader::ResponseReader::
          ResponseDataReaderStatus::kCompleted,
      1);
  CheckCorrectForwardingResultMetric(
      histogram_tester,
      StreamingSearchPrefetchURLLoader::ForwardingResult::kCompleted, 1);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.Search.ResponseReuseCount",
      /*prerender_serving_times*/ 1, 1);
}

// Tests that prefetched response can be served to prerender client
// successfully.
// TODO(crbug.com/370067813): enable the flaky test.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_FetchPrerenderActivated DISABLED_FetchPrerenderActivated
#else
#define MAYBE_FetchPrerenderActivated FetchPrerenderActivated
#endif
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       MAYBE_FetchPrerenderActivated) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  WaitUntilStatusChangesTo(
      GetCanonicalSearchURL(expected_prerender_url),
      {SearchPrefetchStatus::kCanBeServed, SearchPrefetchStatus::kComplete});

  // 2. Prefetch and prerender should be triggered, and chrome is waiting for
  // the body.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);

  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kComplete);

  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  base::RunLoop run_loop;
  search_prefetch_service()->SetLoaderDestructionCallbackForTesting(
      GetCanonicalSearchURL(expected_prerender_url), run_loop.QuitClosure());
  NavigateToPrerenderedResult(expected_prerender_url);
  run_loop.Run();
  WaitForActivatedPageLoaded();
  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender",
      StreamingSearchPrefetchURLLoader::ResponseReader::
          ResponseDataReaderStatus::kCompleted,
      1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPreload.ForwardingResult.WasServedToPrerender", 0);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.Search.ResponseReuseCount", 1, 1);
}

// Tests that the SearchSuggestionService can trigger prerendering if it
// receives prerender hints after the previous prefetch request succeeds.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrerenderHintReceivedAfterCompletion) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);

  // Wait until prefetch request succeeds.
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prefetch_url),
                           {SearchPrefetchStatus::kComplete});
  std::string search_query_2 = "prer";
  ChangeAutocompleteResult(search_query_2, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // The suggestion service should hint `expected_prefetch_url`, and
  // prerendering for this url should start.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);

  // No prerender requests went through network, so there should be only one
  // request and it is with the prefetch flag attached.
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));

  // Activate.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        expected_prerender_url);
  NavigateToPrerenderedResult(expected_prerender_url);
  prerender_observer.WaitForActivation();
  WaitForActivatedPageLoaded();

  // No prerender requests went through network.
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender",
      StreamingSearchPrefetchURLLoader::ResponseReader::
          ResponseDataReaderStatus::kCompleted,
      1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPreload.ForwardingResult.WasServedToPrerender", 0);
}

// Tests that once prefetch encountered error, prerender would be canceled as
// well.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrefetchErrorCancelsPrerender) {
  base::HistogramTester histogram_tester;
  set_service_deferral_type(SearchPreloadTestResponseDeferralType::kDeferBody);

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);

  // Wait until prefetch request succeeds.
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prefetch_url),
                           {SearchPrefetchStatus::kCanBeServed});
  std::string search_query_2 = "prer";
  content::TestNavigationManager prerender_navigation_manager(
      GetActiveWebContents(), expected_prerender_url);
  ChangeAutocompleteResult(search_query_2, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // The suggestion service should hint `expected_prefetch_url`, and
  // prerendering for this url should start.
  registry_observer.WaitForTrigger(expected_prerender_url);
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);

  // Ensure prerender has started to read response body.
  ASSERT_TRUE(prerender_navigation_manager.WaitForResponse());
  prerender_navigation_manager.ResumeNavigation();

  ShutDownSearchServer();
  prerender_observer.WaitForDestroyed();
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prefetch_url),
                           {SearchPrefetchStatus::kRequestFailed});
  while (true) {
    int num_count = histogram_tester.GetBucketCount(
        "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender",
        StreamingSearchPrefetchURLLoader::ResponseReader::
            ResponseDataReaderStatus::kNetworkError);

    if (num_count) {
      EXPECT_EQ(num_count, 1);
      break;
    }
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
}

// Tests that if prerender is canceled by itself before the loader receives
// response body from the internet, the correct result can be recorded.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrerenderDiscardedBeforeServingData) {
  base::HistogramTester histogram_tester;
  set_service_deferral_type(SearchPreloadTestResponseDeferralType::kDeferBody);

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  content::TestNavigationManager prerender_navigation_manager(
      GetActiveWebContents(), expected_prerender_url);
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // Wait until prefetch request succeeds.
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prefetch_url),
                           {SearchPrefetchStatus::kCanBeServed});
  // The suggestion service should hint `expected_prefetch_url`, and
  // prerendering for this url should start.
  registry_observer.WaitForTrigger(expected_prerender_url);
  content::FrameTreeNodeId host_id =
      prerender_helper().GetHostForUrl(expected_prerender_url);

  // Ensure prerender has started to read response body.
  ASSERT_TRUE(prerender_navigation_manager.WaitForResponse());
  prerender_navigation_manager.ResumeNavigation();
  while (true) {
    if (prerender_navigation_manager.was_committed()) {
      break;
    }
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), host_id);
  prerender_helper().CancelPrerenderedPage(host_id);
  prerender_observer.WaitForDestroyed();
  prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  DispatchDelayedResponseTask();
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prefetch_url),
                           {SearchPrefetchStatus::kComplete});

  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), expected_prerender_url));

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kPrefetchServedForRealNavigation, 1);

  CheckCorrectForwardingResultMetric(
      histogram_tester,
      StreamingSearchPrefetchURLLoader::ForwardingResult::kCompleted, 1);

  // If the prerender is completely destroyed before the final state code
  // arrives, `kServingError` will be recorded, otherwise `kCompleted` will be
  // recorded. The timing issue is not controllable due to asynchronous Mojo
  // messages and asynchronous destruction tasks, so both state are expected.
  {
    while (true) {
      int num = histogram_tester.GetTotalSum(
          "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender");
      if (num >= 1) {
        break;
      }
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender", 1);
  EXPECT_EQ(
      1,
      histogram_tester.GetBucketCount(
          "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender",
          StreamingSearchPrefetchURLLoader::ResponseReader::
              ResponseDataReaderStatus::kServingError) +
          histogram_tester.GetBucketCount(
              "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender",
              StreamingSearchPrefetchURLLoader::ResponseReader::
                  ResponseDataReaderStatus::kCompleted));
}

// Edge case: when the prerendering navigation is still reading from the cache,
// the loader would not be deleted until finishing reading.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       ServingToPrerenderingUntilCompletion) {
  base::HistogramTester histogram_tester;
  set_service_deferral_type(
      SearchPreloadTestResponseDeferralType::kDeferChunkedResponseBody);

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");

  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  // 1. Type the first query.
  std::string search_query = "prer";
  std::string prerender_query = "prerender";

  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  ChangeAutocompleteResult(search_query.substr(0, search_query.size() - 1),
                           prerender_query, PrerenderHint::kDisabled,
                           PrefetchHint::kEnabled);

  // 2. Trigger prefetch and serve the first part of response body.
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url),
                           {SearchPrefetchStatus::kCanBeServed});
  DispatchDelayedResponseTask();

  // 3. Trigger prerender.
  ChangeAutocompleteResult(search_query, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  registry_observer.WaitForTrigger(expected_prerender_url);
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);

  // 4. Activate the prerendered page.
  NavigateToPrerenderedResult(expected_prerender_url);

  prerender_observer.WaitForActivation();

  // 5. After activation, the request would be deleted from the prefetched
  // request list.
  std::optional<SearchPrefetchStatus> status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  ASSERT_EQ(status, std::nullopt);

  // 6. And then we can dispatch the result.
  DispatchDelayedResponseTask();
  WaitForActivatedPageLoaded();

  // Flush metrics.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  histogram_tester.ExpectUniqueSample(
      "PageLoad.Internal.Prerender2.ActivatedPageLoaderStatus.Embedder_"
      "DefaultSearchEngine",
      std::abs(net::Error::OK), 1);
  // Prerender should not retry the request.
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender",
      StreamingSearchPrefetchURLLoader::ResponseReader::
          ResponseDataReaderStatus::kCompleted,
      1);
}

// It is possible that one prefetched response is served to multiple prerender
// in the current design.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       ServingToPrerenderNavigationTwice) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  WaitUntilStatusChangesTo(
      GetCanonicalSearchURL(expected_prerender_url),
      {SearchPrefetchStatus::kCanBeServed, SearchPrefetchStatus::kComplete});

  // 2. Prefetch and prerender should be triggered.
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);

  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  std::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kComplete);

  // 3. Turns to another prediction.
  {
    content::FrameTreeNodeId host_id =
        prerender_helper().GetHostForUrl(expected_prerender_url);
    content::test::PrerenderHostObserver prerender_observer(
        *GetActiveWebContents(), host_id);
    ChangeAutocompleteResult("pref", "prefetch", PrerenderHint::kEnabled,
                             PrefetchHint::kEnabled);
    prerender_observer.WaitForDestroyed();
    prerender_helper().WaitForPrerenderLoadCompletion(
        *GetActiveWebContents(), GetSearchUrl("prefetch", UrlType::kPrerender));
  }
  // 4. The previous prefetch response should still exist.
  prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  ASSERT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());

  // 5. Prerender the page again.
  {
    content::test::PrerenderHostRegistryObserver registry_observer(
        *GetActiveWebContents());
    ChangeAutocompleteResult(search_query_1, prerender_query,
                             PrerenderHint::kEnabled, PrefetchHint::kEnabled);
    registry_observer.WaitForTrigger(expected_prerender_url);
  }
  {
    prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                      expected_prerender_url);
    content::test::PrerenderHostObserver prerender_observer(
        *GetActiveWebContents(), expected_prerender_url);
    base::RunLoop run_loop;
    search_prefetch_service()->SetLoaderDestructionCallbackForTesting(
        GetCanonicalSearchURL(expected_prerender_url), run_loop.QuitClosure());
    NavigateToPrerenderedResult(expected_prerender_url);
    prerender_observer.WaitForActivation();
    search_prefetch_service()->FireAllExpiryTimerForTesting();
    run_loop.Run();
  }

  // This one was recorded by the loader fetching the "prerender" search term.
  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.Search.ResponseReuseCount",
      /*prerender_serving_times*/ 2, 1);
  // This one was recorded by the loader fetching the "prefetch" search term.
  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.Search.ResponseReuseCount",
      /*prerender_serving_times*/ 1, 1);
}

// Fake URLLoader that reads the prefetched response from memory cache.
class SearchPreloadServingTestURLLoader
    : public network::mojom::URLLoaderClient,
      public mojo::DataPipeDrainer::Client {
 public:
  SearchPreloadServingTestURLLoader() = default;
  ~SearchPreloadServingTestURLLoader() override = default;

  SearchPreloadServingTestURLLoader(const SearchPreloadServingTestURLLoader&) =
      delete;
  SearchPreloadServingTestURLLoader& operator=(
      const SearchPreloadServingTestURLLoader&) = delete;

  mojo::PendingReceiver<network::mojom::URLLoader>
  BindURLloaderAndGetReceiver() {
    return remote_.BindNewPipeAndPassReceiver();
  }
  mojo::PendingRemote<network::mojom::URLLoaderClient>
  BindURLLoaderClientAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }
  void DisconnectMojoPipes() {
    remote_.reset();
    receiver_.reset();
  }

 private:
  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    pipe_drainer_ =
        std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override { return; }
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    return;
  }

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(base::span<const uint8_t> data) override { return; }
  void OnDataComplete() override { return; }

  mojo::Remote<network::mojom::URLLoader> remote_;
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;
};

// Regression test for https://crbug.com/1493229.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrerenderHandlerExecutedAfterPrefetchHandler) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  // 1. Type the query to start  prefetch.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url),
                           {SearchPrefetchStatus::kComplete});

  // 2. Prepare network requests and handlers.
  network::ResourceRequest prerender_serving_request =
      CreateServingRequest(expected_prerender_url);
  network::ResourceRequest prefetch_serving_request =
      CreateServingRequest(expected_prerender_url);

  SearchPrefetchURLLoader::RequestHandler prerender_serving_handler =
      CreatePrerenderRequestHandler(prerender_serving_request);
  if (!prerender_serving_handler) {
    NOTREACHED_IN_MIGRATION()
        << "prerender handler should not be an empty callback!";
  }
  SearchPrefetchURLLoader::RequestHandler prefetch_serving_handler =
      CreatePrefetchRequestHandler(prefetch_serving_request);
  if (!prefetch_serving_handler) {
    NOTREACHED_IN_MIGRATION()
        << "prefetch handler should not be an empty callback!";
  }
  SearchPreloadServingTestURLLoader prefetch_serving_loader;
  SearchPreloadServingTestURLLoader prerender_serving_loader;

  // 3. Execute prefetch handler callback first, this should take the prefetched
  // result away.
  std::move(prefetch_serving_handler)
      .Run(prerender_serving_request,
           prefetch_serving_loader.BindURLloaderAndGetReceiver(),
           prefetch_serving_loader.BindURLLoaderClientAndGetRemote());

  // 4. Then executed the prerender one. The test should not crash.
  std::move(prerender_serving_handler)
      .Run(prerender_serving_request,
           prerender_serving_loader.BindURLloaderAndGetReceiver(),
           prerender_serving_loader.BindURLLoaderClientAndGetRemote());

  prefetch_serving_loader.DisconnectMojoPipes();
  prerender_serving_loader.DisconnectMojoPipes();
}

// We cannot open the result in another tab on Android.
#if !BUILDFLAG(IS_ANDROID)

// Tests that even when prerendering is not failed, users can open the
// prefetched result in another tab and activate the prefetched response
// successfully.
#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)) && \
    defined(ADDRESS_SANITIZER)
#define MAYBE_OpenPrefetchedResponseInBackgroundedTab \
  DISABLED_OpenPrefetchedResponseInBackgroundedTab
#else
#define MAYBE_OpenPrefetchedResponseInBackgroundedTab \
  OpenPrefetchedResponseInBackgroundedTab
#endif
// TODO(crbug.com/40272425): Flaky on chromiumos ASAN LSAN and Linux
// ASAN.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       MAYBE_OpenPrefetchedResponseInBackgroundedTab) {
  base::HistogramTester histogram_tester;
  set_service_deferral_type(
      SearchPreloadTestResponseDeferralType::kDeferChunkedResponseBody);

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");

  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  // 1. Type the first query.
  std::string search_query = "prer";
  std::string prerender_query = "prerender";

  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);

  ChangeAutocompleteResult(search_query.substr(0, search_query.size() - 1),
                           prerender_query, PrerenderHint::kDisabled,
                           PrefetchHint::kEnabled);

  // 2. Trigger prefetch and serve the first part of response body.
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url),
                           {SearchPrefetchStatus::kCanBeServed});
  DispatchDelayedResponseTask();

  {
    // 3. Trigger prerender.
    content::test::PrerenderHostRegistryObserver registry_observer(
        *GetActiveWebContents());
    ChangeAutocompleteResult(search_query, prerender_query,
                             PrerenderHint::kEnabled, PrefetchHint::kEnabled);
    registry_observer.WaitForTrigger(expected_prerender_url);

    // This ensures that the request handler from
    // SearchPrefetchURLLoaderInterceptor fully process the request.
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);

  // 4. Open the search in a background new tab. This is the default disposition
  // when users open a suggestion in another tab. Prerender will be canceled in
  // this case.
  content::WebContents* new_prefetch_tab = GetActiveWebContents()->OpenURL(
      content::OpenURLParams(
          expected_prerender_url, content::Referrer(),
          WindowOpenDisposition::NEW_BACKGROUND_TAB,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url), {});

  // TODO(crbug.com/40259971): Ideally we should open the tab with the
  // prerendered result.
  prerender_observer.WaitForDestroyed();

  // 5. And then we can dispatch the result. We no longer need to delay
  // responses.
  DispatchDelayedResponseTask();
  content::WaitForLoadStop(new_prefetch_tab);
  set_service_deferral_type(SearchPreloadTestResponseDeferralType::kNoDeferral);

  // 6. The popup autocomplete window in the current tab is still open, type
  // something more to trigger prerender again. Note the old prerender was
  // canceled at step 4.
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  {
    content::test::PrerenderHostRegistryObserver new_registry_observer(
        *GetActiveWebContents());
    ChangeAutocompleteResult(search_query + "e", prerender_query,
                             PrerenderHint::kEnabled, PrefetchHint::kEnabled);
    new_registry_observer.WaitForTrigger(expected_prerender_url);
  }

  // 7.  Navigate to the prerendered page in the same tab.
  NavigateToPrerenderedResult(expected_prerender_url);
  WaitForActivatedPageLoaded();

  // Both of them loaded full content.
  std::string inner_html = content::EvalJs(GetActiveWebContents(),
                                           "document.documentElement.innerHTML")
                               .ExtractString();
  EXPECT_TRUE(base::Contains(inner_html, "PREFETCH"));
  std::string prefetch_inner_html =
      content::EvalJs(new_prefetch_tab, "document.documentElement.innerHTML")
          .ExtractString();
  EXPECT_TRUE(base::Contains(prefetch_inner_html, "PREFETCH"));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  EXPECT_EQ(2, prerender_helper().GetRequestCount(expected_prefetch_url));

  // For the second response, `kCompleted` should be recorded. For the first
  // one, if the prerender is completely destroyed before the final state code
  // arrives, `kServingError` will be recorded, otherwise `kCompleted` will be
  // recorded. The timing issue is not controllable due to asynchronous Mojo
  // messages and asynchronous destruction tasks, so both state are expected.
  EXPECT_EQ(
      2,
      histogram_tester.GetBucketCount(
          "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender",
          StreamingSearchPrefetchURLLoader::ResponseReader::
              ResponseDataReaderStatus::kServingError) +
          histogram_tester.GetBucketCount(
              "Omnibox.SearchPreload.ResponseDataReaderFinalStatus.Prerender",
              StreamingSearchPrefetchURLLoader::ResponseReader::
                  ResponseDataReaderStatus::kCompleted));
  CheckCorrectForwardingResultMetric(
      histogram_tester,
      StreamingSearchPrefetchURLLoader::ForwardingResult::kCompleted, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
