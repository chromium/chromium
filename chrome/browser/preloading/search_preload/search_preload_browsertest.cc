// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_preload_test_response_utils.h"
#include "chrome/browser/preloading/search_preload/search_preload_features.h"
#include "chrome/browser/preloading/search_preload/search_preload_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox.mojom.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prefetch_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Holds //content data to avoid disallowed import.
namespace alternative_content {

// Minimal copy of content/browser/preloading/prerender/prerender_final_status.h
enum class PrerenderFinalStatus {
  kActivated = 0,
};

}  // namespace alternative_content

// Collects requests to `EmbeddedTestServer` via RequestMonitor.
class EmbeddedTestServerRequestCollector {
 public:
  EmbeddedTestServerRequestCollector() = default;
  ~EmbeddedTestServerRequestCollector() = default;

  base::RepeatingCallback<void(const net::test_server::HttpRequest&)>
  GetOnResourceRequest() {
    return base::BindRepeating(
        &EmbeddedTestServerRequestCollector::OnResourceRequest,
        base::Unretained(this));
  }

  int CountByPath(const GURL& url) {
    base::AutoLock auto_lock(lock_);

    int i = 0;

    for (const auto& request : requests_) {
      if (request.GetURL().PathForRequest() == url.PathForRequest()) {
        i += 1;
      }
    }

    return i;
  }

 private:
  void OnResourceRequest(const net::test_server::HttpRequest& request) {
    // Called from a thread for EmbeddedTestServer.
    CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) &&
          !content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    // So, we guard the field with lock.
    base::AutoLock auto_lock(lock_);

    requests_.push_back(request);
  }

  base::Lock lock_;
  std::vector<net::test_server::HttpRequest> requests_ GUARDED_BY(lock_);
};

// Sets up testing context for the search preloading features: search prefetch
// and search prerender.
// These features are able to coordinate with the other: A prefetched result
// might be upgraded to prerender when needed (usually when service suggests
// clients to do so), and they share the prefetched response and other
// resources, so it is a unified test designed to test the interaction between
// these two features.
class SearchPreloadBrowserTest : public PlatformBrowserTest,
                                 public SearchPreloadResponseController {
 public:
  SearchPreloadBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kPrefetchPrerenderIntegration, {}},
            {
                features::kDsePreload2,
                {},
            },
            {
                features::kDsePreload2OnPress,
                {
                    {"kDsePreload2OnPressMouseDown", "true"},
                    {"kDsePreload2OnPressUpOrDownArrowButton", "true"},
                    {"kDsePreload2OnPressTouchDown", "true"},
                },
            },
        },
        /*disabled_features=*/{});

    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(
            [](SearchPreloadBrowserTest* that) {
              return &that->GetWebContents();
            },
            base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    request_collector_ = std::make_unique<EmbeddedTestServerRequestCollector>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up a generic server.
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Set up server for search engine.
    https_server_->SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->RegisterRequestMonitor(
        request_collector_->GetOnResourceRequest());
    https_server_->RegisterRequestHandler(
        base::BindRepeating(&SearchPreloadBrowserTest::HandleSearchRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpTemplateURLService(bool prefetch_likely_navigations = true) {
    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(&GetProfile());
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());

    TemplateURLData data;
    data.SetShortName(kSearchDomain16);
    data.SetKeyword(data.short_name());
    data.SetURL(https_server_
                    ->GetURL(kSearchDomain,
                             "/search_page.html?q={searchTerms}&{google:"
                             "assistedQueryStats}{google:prefetchSource}"
                             "type=test")
                    .spec());
    data.suggestions_url =
        https_server_->GetURL(kSearchDomain, "/?q={searchTerms}").spec();
    data.prefetch_likely_navigations = prefetch_likely_navigations;

    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSearchRequest(
      const net::test_server::HttpRequest& request) {
    net::HttpStatusCode code = net::HttpStatusCode::HTTP_OK;
    std::string content = R"(
      <html><body>
      PRERENDER: HI PREFETCH! \o/
      </body></html>
    )";
    base::StringPairs headers = {
        {"Content-Length", base::NumberToString(content.length())},
        {"Content-Type", "text/html"},
        {"No-Vary-Search", R"(key-order, params, except=("q"))"},
    };
    std::unique_ptr<net::test_server::HttpResponse> response =
        CreateDeferrableResponse(code, headers, content);
    return response;
  }

  enum class PrefetchHint { kEnabled, kDisabled };
  enum class PrerenderHint { kEnabled, kDisabled };
  enum class UrlType {
    // For URLs that will be used for a real navigation.
    kReal,
    // For URLs that will be used for prefetch requests.
    kPrefetch,
    // For URLs that will be used for prefetch requests for
    // `OnNavigationLikely()`.
    kPrefetchOnNavigationLikely,
    // For URLs that will be used for prerender requests.
    kPrerender
  };

  GURL GetSearchUrl(const std::string& search_terms, UrlType url_type) {
    const char* pf;
    switch (url_type) {
      case UrlType::kReal:
      case UrlType::kPrerender:
        pf = "";
        break;
      case UrlType::kPrefetch:
        pf = "&pf=cs";
        break;
      case UrlType::kPrefetchOnNavigationLikely:
        pf = "&pf=op";
        break;
    }
    std::string path = base::StrCat({
        "/search_page.html",
        "?q=",
        search_terms,
        pf,
        "&type=test",
    });
    return https_server_->GetURL(kSearchDomain, path);
  }

  void ChangeAutocompleteResult(const std::string& original_query,
                                const std::string& search_terms,
                                PrefetchHint prefetch_hint,
                                PrerenderHint prerender_hint) {
    AutocompleteInput input(base::ASCIIToUTF16(original_query),
                            metrics::OmniboxEventProto::BLANK,
                            ChromeAutocompleteSchemeClassifier(&GetProfile()));

    AutocompleteMatch autocomplete_match = CreateSearchSuggestionMatch(
        original_query, search_terms, prefetch_hint, prerender_hint);
    AutocompleteResult autocomplete_result;
    autocomplete_result.AppendMatches({autocomplete_match});

    GetSearchPreloadService().OnAutocompleteResultChanged(&GetWebContents(),
                                                          autocomplete_result);
  }

  AutocompleteMatch CreateSearchSuggestionMatch(
      const std::string& original_query,
      const std::string& search_terms,
      PrefetchHint prefetch_hint,
      PrerenderHint prerender_hint) {
    AutocompleteMatch match;
    match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
        base::UTF8ToUTF16(search_terms));
    match.search_terms_args->original_query = base::UTF8ToUTF16(original_query);
    match.destination_url = GetSearchUrl(search_terms, UrlType::kReal);
    match.keyword = base::UTF8ToUTF16(original_query);
    match.allowed_to_be_default_match = true;

    if (prefetch_hint == PrefetchHint::kEnabled) {
      match.RecordAdditionalInfo("should_prefetch", "true");
    }
    if (prerender_hint == PrerenderHint::kEnabled) {
      match.RecordAdditionalInfo("should_prerender", "true");
    }

    return match;
  }

  // `WaitEvent::kLoadStopped` is the default value for a
  // TestNavigationObserver. Pass another event type to not wait until it
  // finishes loading.
  void NavigateToPrerenderedResult(
      const GURL& url,
      content::TestNavigationObserver::WaitEvent wait_event =
          content::TestNavigationObserver::WaitEvent::kLoadStopped) {
    content::TestNavigationObserver observer(&GetWebContents());
    observer.set_wait_event(wait_event);
    GetWebContents().OpenURL(
        content::OpenURLParams(
            url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
            ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                      ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
            /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
    observer.Wait();
  }

  Profile& GetProfile() { return *chrome_test_utils::GetProfile(this); }
  SearchPreloadService& GetSearchPreloadService() {
    return *SearchPreloadService::GetForProfile(&GetProfile());
  }
  content::WebContents& GetWebContents() {
    return *chrome_test_utils::GetActiveWebContents(this);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return *prerender_helper_.get();
  }
  EmbeddedTestServerRequestCollector& request_collector() {
    return *request_collector_.get();
  }
  base::HistogramTester& histogram_tester() { return *histogram_tester_.get(); }

 private:
  constexpr static char kSearchDomain[] = "a.test";
  constexpr static char16_t kSearchDomain16[] = u"a.test";

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<net::test_server::EmbeddedTestServer> https_server_;
  std::unique_ptr<EmbeddedTestServerRequestCollector> request_collector_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

// Scenario:
//
// - A user inputs "he".
// - Autocomplete suggests to prefetch "hello".
// - `SearchPreloadService` starts prefetch with query "?q=hello&pf=cs...".
// - A user navigates to a page with query "?q=hello&..."
// - Prefetch is used.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest,
                       OnAutocompleteResultChanged_TriggersPrefetch) {
  SetUpTemplateURLService();

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";
  GURL prefetch_url = GetSearchUrl(search_terms, UrlType::kPrefetch);
  GURL prerender_url = GetSearchUrl(search_terms, UrlType::kPrerender);
  GURL navigation_url = GetSearchUrl(search_terms, UrlType::kReal);
  ASSERT_EQ(prerender_url, navigation_url);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt, prefetch_url);
  }

  EXPECT_EQ(1, request_collector().CountByPath(prefetch_url));
  EXPECT_EQ(0, request_collector().CountByPath(prerender_url));

  // Navigate.
  ASSERT_TRUE(content::NavigateToURL(&GetWebContents(), navigation_url));

  // Prefetch is used.
  EXPECT_EQ(1, request_collector().CountByPath(prefetch_url));
  EXPECT_EQ(0, request_collector().CountByPath(prerender_url));
  EXPECT_EQ(0, request_collector().CountByPath(navigation_url));

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      alternative_content::PrerenderFinalStatus::kActivated, 0);
}

// Scenario:
//
// - A user inputs "he".
// - Autocomplete suggests to prefetch "hello".
// - `SearchPreloadService` starts prefetch with query "?q=hello&pf=cs...".
// - A user inputs "hel".
// - Autocomplete suggests to prefetch "hello" (unchanged).
// - `SearchPreloadService` does nothing as prefetch for "hello" is already
//   triggered.
// - A user navigates to a page with query "?q=hello&..."
// - Prefetch is used.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest,
                       OnAutocompleteResultChanged_TriggeredPrefetchIsHeld) {
  SetUpTemplateURLService();

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string original_query2 = "hel";
  std::string search_terms = "hello";
  GURL prefetch_url = GetSearchUrl(search_terms, UrlType::kPrefetch);
  GURL prerender_url = GetSearchUrl(search_terms, UrlType::kPrerender);
  GURL navigation_url = GetSearchUrl(search_terms, UrlType::kReal);
  ASSERT_EQ(prerender_url, navigation_url);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt, prefetch_url);
  }

  // A user inputs anothor character and `OnAutocompleteResultChanged()` is
  // called with "hel". Prefetch is already triggered and it doesn't trigger
  // another one.
  ChangeAutocompleteResult(original_query2, search_terms,
                           PrefetchHint::kEnabled, PrerenderHint::kDisabled);

  EXPECT_EQ(1, request_collector().CountByPath(prefetch_url));
  EXPECT_EQ(0, request_collector().CountByPath(prerender_url));

  // Navigate.
  ASSERT_TRUE(content::NavigateToURL(&GetWebContents(), navigation_url));

  // Prefetch is used.
  EXPECT_EQ(1, request_collector().CountByPath(prefetch_url));
  EXPECT_EQ(0, request_collector().CountByPath(prerender_url));
  EXPECT_EQ(0, request_collector().CountByPath(navigation_url));

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      alternative_content::PrerenderFinalStatus::kActivated, 0);
}

// Scenario:
//
// - A user inputs "he".
// - Autocomplete suggests to prerender "hello".
// - `SearchPreloadService` starts prefetch with query "?q=hello&pf=cs...".
// - `SearchPreloadService` starts prerender with query "?q=hello...".
// - A user navigates to a page with query "?q=hello&..."
// - Prerender is used.
IN_PROC_BROWSER_TEST_F(
    SearchPreloadBrowserTest,
    OnAutocompleteResultChanged_TriggersPrefetchAndPrerender) {
  SetUpTemplateURLService();

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";
  GURL prefetch_url = GetSearchUrl(search_terms, UrlType::kPrefetch);
  GURL prerender_url = GetSearchUrl(search_terms, UrlType::kPrerender);
  GURL navigation_url = GetSearchUrl(search_terms, UrlType::kReal);
  ASSERT_EQ(prerender_url, navigation_url);

  {
    content::test::TestPrefetchWatcher watcher;
    content::test::PrerenderHostRegistryObserver registry_observer(
        GetWebContents());

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kEnabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt, prefetch_url);

    registry_observer.WaitForTrigger(prerender_url);
    prerender_helper().WaitForPrerenderLoadCompletion(GetWebContents(),
                                                      prerender_url);
  }

  // Only prefetch request went through network and prerender used the
  // prefetched response.
  EXPECT_EQ(1, request_collector().CountByPath(prefetch_url));
  EXPECT_EQ(0, request_collector().CountByPath(prerender_url));

  // Navigate.
  content::test::PrerenderHostObserver prerender_observer(GetWebContents(),
                                                          prerender_url);
  NavigateToPrerenderedResult(navigation_url);
  prerender_observer.WaitForActivation();

  // Prerender is used.
  EXPECT_EQ(1, request_collector().CountByPath(prefetch_url));
  EXPECT_EQ(0, request_collector().CountByPath(prerender_url));
  EXPECT_EQ(0, request_collector().CountByPath(navigation_url));

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      alternative_content::PrerenderFinalStatus::kActivated, 1);
}

// Scenario:
//
// - A user inputs "he".
// - Autocomplete suggests to prefetch "hello".
// - `SearchPreloadService` starts prefetch with query "?q=hello&pf=cs...".
// - A user inputs "hel".
// - Autocomplete suggests to prerender "hello".
// - `SearchPreloadService` starts prerender with query "?q=hello...".
// - A user navigates to a page with query "?q=hello&..."
// - Prerender is used.
IN_PROC_BROWSER_TEST_F(
    SearchPreloadBrowserTest,
    OnAutocompleteResultChanged_TriggersPrefetchThenPrerender) {
  SetUpTemplateURLService();

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string original_query2 = "hel";
  std::string search_terms = "hello";
  GURL prefetch_url = GetSearchUrl(search_terms, UrlType::kPrefetch);
  GURL prerender_url = GetSearchUrl(search_terms, UrlType::kPrerender);
  GURL navigation_url = GetSearchUrl(search_terms, UrlType::kReal);
  ASSERT_EQ(prerender_url, navigation_url);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt, prefetch_url);
  }

  {
    content::test::PrerenderHostRegistryObserver registry_observer(
        GetWebContents());

    ChangeAutocompleteResult(original_query2, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kEnabled);

    prerender_helper().WaitForPrerenderLoadCompletion(GetWebContents(),
                                                      prerender_url);
  }

  // Only prefetch request went through network and prerender used the
  // prefetched response.
  EXPECT_EQ(1, request_collector().CountByPath(prefetch_url));
  EXPECT_EQ(0, request_collector().CountByPath(prerender_url));

  // Navigate.
  content::test::PrerenderHostObserver prerender_observer(GetWebContents(),
                                                          prerender_url);
  NavigateToPrerenderedResult(navigation_url);
  prerender_observer.WaitForActivation();

  // Prerender is used.
  EXPECT_EQ(1, request_collector().CountByPath(prefetch_url));
  EXPECT_EQ(0, request_collector().CountByPath(prerender_url));
  EXPECT_EQ(0, request_collector().CountByPath(navigation_url));

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      alternative_content::PrerenderFinalStatus::kActivated, 1);
}

// Scenario:
//
// - A user inputs "he".
// - A user clicks a suggestion "hello".
// - `SearchPreloadService` starts prefetch with query "?q=hello&pf=op...".
// - A user navigates to a page with query "?q=hello&..."
// - Prefetch is used.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest,
                       OnNavigationLikely_TriggersPrefetch) {
  SetUpTemplateURLService(/*prefetch_likely_navigations=*/true);

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";
  GURL prefetch_url_on_navigation_likely =
      GetSearchUrl(search_terms, UrlType::kPrefetchOnNavigationLikely);
  GURL navigation_url = GetSearchUrl(search_terms, UrlType::kReal);

  {
    content::test::TestPrefetchWatcher watcher;

    AutocompleteMatch autocomplete_match = CreateSearchSuggestionMatch(
        original_query, search_terms, PrefetchHint::kEnabled,
        PrerenderHint::kDisabled);

    const bool is_triggered_prefetch =
        GetSearchPreloadService().OnNavigationLikely(
            1, autocomplete_match,
            omnibox::mojom::NavigationPredictor::kMouseDown, &GetWebContents());
    ASSERT_TRUE(is_triggered_prefetch);

    watcher.WaitUntilPrefetchResponseCompleted(
        std::nullopt, prefetch_url_on_navigation_likely);
  }

  EXPECT_EQ(1,
            request_collector().CountByPath(prefetch_url_on_navigation_likely));

  // Navigate.
  ASSERT_TRUE(content::NavigateToURL(&GetWebContents(), navigation_url));

  // Prefetch is used.
  EXPECT_EQ(1,
            request_collector().CountByPath(prefetch_url_on_navigation_likely));
  EXPECT_EQ(0, request_collector().CountByPath(navigation_url));
}

// `OnNavigationLikely()` doesn't trigger prefetch if default search provider
// doesn't opt in.
IN_PROC_BROWSER_TEST_F(
    SearchPreloadBrowserTest,
    OnNavigationLikely_DoesntTriggerPrefetchIfDefaultSearchProviderDoesntOptIn) {
  SetUpTemplateURLService(/*prefetch_likely_navigations=*/false);

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";

  AutocompleteMatch autocomplete_match = CreateSearchSuggestionMatch(
      original_query, search_terms, PrefetchHint::kEnabled,
      PrerenderHint::kDisabled);

  const bool is_triggered_prefetch =
      GetSearchPreloadService().OnNavigationLikely(
          1, autocomplete_match,
          omnibox::mojom::NavigationPredictor::kMouseDown, &GetWebContents());
  ASSERT_FALSE(is_triggered_prefetch);
}

// Scenario:
//
// - A user inputs "he".
// - Autocomplete suggests to prefetch "hello".
// - `SearchPreloadService` starts prefetch with query "?q=hello&pf=cs...".
// - A user clicks a suggestion "hello".
// - Prefetch is not triggered with query "?q=hello&pf=op..." as prefetch is
// already triggered.
// - A user navigates to a page with query "?q=hello&..."
// - Prefetch is used.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest,
                       OnAutocompleteResultChanged_Then_OnNavigationLikely) {
  SetUpTemplateURLService(/*prefetch_likely_navigations=*/true);

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";
  GURL prefetch_url = GetSearchUrl(search_terms, UrlType::kPrefetch);
  GURL prefetch_url_on_navigation_likely =
      GetSearchUrl(search_terms, UrlType::kPrefetchOnNavigationLikely);
  GURL navigation_url = GetSearchUrl(search_terms, UrlType::kReal);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt, prefetch_url);
  }

  EXPECT_EQ(1, request_collector().CountByPath(prefetch_url));

  AutocompleteMatch autocomplete_match = CreateSearchSuggestionMatch(
      original_query, search_terms, PrefetchHint::kEnabled,
      PrerenderHint::kDisabled);

  const bool is_triggered_prefetch =
      GetSearchPreloadService().OnNavigationLikely(
          1, autocomplete_match,
          omnibox::mojom::NavigationPredictor::kMouseDown, &GetWebContents());
  ASSERT_FALSE(is_triggered_prefetch);

  // Navigate.
  ASSERT_TRUE(content::NavigateToURL(&GetWebContents(), navigation_url));

  // Prefetch is used.
  EXPECT_EQ(1, request_collector().CountByPath(prefetch_url));
  EXPECT_EQ(0,
            request_collector().CountByPath(prefetch_url_on_navigation_likely));
  EXPECT_EQ(0, request_collector().CountByPath(navigation_url));
}

}  // namespace
