// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string>

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
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
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prefetch_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "extensions/common/extension_features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"

namespace {

// Holds //content data to avoid disallowed import.
namespace alternative_content {

// Minimal copy of content/browser/preloading/prefetch/prefetch_status.h
enum class PrefetchStatus {
  kPrefetchNotFinishedInTime = 10,
};

// Minimal copy of content/browser/preloading/prerender/prerender_final_status.h
enum class PrerenderFinalStatus {
  kActivated = 0,
  kPrerenderFailedDuringPrefetch = 86,
};

}  // namespace alternative_content

class HistogramTesterWrapper {
 public:
  HistogramTesterWrapper() = default;
  ~HistogramTesterWrapper() = default;

  template <typename T>
  void ExpectUma(std::string_view name,
                 std::vector<T> values,
                 const base::Location& location = FROM_HERE) {
    std::map<T, size_t> counts;
    for (auto& value : values) {
      counts[value]++;
    }

    histogram_tester_.ExpectTotalCount(name, values.size(), location);
    for (auto& [value, count] : counts) {
      histogram_tester_.ExpectBucketCount(name, value, count, location);
    }
  }

  template <typename T>
  void ExpectUma(std::string_view name,
                 std::initializer_list<T> values,
                 const base::Location& location = FROM_HERE) {
    ExpectUma(name, std::vector<T>(values), location);
  }

  // Special case for an empty initializer `{}`.
  void ExpectUma(std::string_view name,
                 void* values,
                 const base::Location& location = FROM_HERE) {
    ExpectUma(name, std::vector<int>({}), location);
  }

  void ExpectTotalCount(std::string_view name,
                        int count,
                        const base::Location& location = FROM_HERE) {
    histogram_tester_.ExpectTotalCount(name, count, location);
  }

 private:
  base::HistogramTester histogram_tester_;
};

constexpr static char kSearchTerms_502OnPrefetch[] = "502-on-prefetch";

std::optional<net::HttpNoVarySearchData> ParseNoVarySearchData(std::string s) {
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("No-Vary-Search", s);
  auto maybe_no_vary_search_data =
      net::HttpNoVarySearchData::ParseFromHeaders(*headers);
  if (!maybe_no_vary_search_data.has_value()) {
    return std::nullopt;
  }
  return maybe_no_vary_search_data.value();
}

// Collects requests to `EmbeddedTestServer` via RequestMonitor.
class EmbeddedTestServerRequestCollector {
 public:
  EmbeddedTestServerRequestCollector() = default;
  ~EmbeddedTestServerRequestCollector() = default;

  void Reset() {
    base::AutoLock auto_lock(lock_);

    requests_.clear();
  }

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

// Injects delay for each response of `EmbeddedTestServer` via RequestMonitor.
class EmbeddedTestServerDelayInjector {
 public:
  EmbeddedTestServerDelayInjector() = default;
  ~EmbeddedTestServerDelayInjector() = default;

  base::RepeatingCallback<void(const net::test_server::HttpRequest&)>
  GetOnResourceRequest() {
    return base::BindRepeating(
        &EmbeddedTestServerDelayInjector::OnResourceRequest,
        base::Unretained(this));
  }

  void SetResponseDelay(base::TimeDelta duration) {
    response_delay_ = duration;
  }

 private:
  void OnResourceRequest(const net::test_server::HttpRequest& request) {
    // Called from a thread for EmbeddedTestServer.
    CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) &&
          !content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    base::PlatformThread::Sleep(response_delay_);
  }

  base::TimeDelta response_delay_ = base::Seconds(0);
};

// Sets up testing context for the search preloading features: search prefetch
// and search prerender.
// These features are able to coordinate with the other: A prefetched result
// might be upgraded to prerender when needed (usually when service suggests
// clients to do so), and they share the prefetched response and other
// resources, so it is a unified test designed to test the interaction between
// these two features.
class SearchPreloadBrowserTestBase : public PlatformBrowserTest,
                                     public SearchPreloadResponseController {
 public:
  SearchPreloadBrowserTestBase() = default;

  virtual void InitFeatures(
      base::test::ScopedFeatureList& scoped_feature_list) = 0;

  void SetUp() override {
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(
            [](SearchPreloadBrowserTestBase* that) {
              return &that->GetWebContents();
            },
            base::Unretained(this)));

    InitFeatures(scoped_feature_list_);

    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    request_collector_ = std::make_unique<EmbeddedTestServerRequestCollector>();
    delay_injector_ = std::make_unique<EmbeddedTestServerDelayInjector>();
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
        base::BindRepeating(&SearchPreloadBrowserTestBase::HandleSearchRequest,
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

  struct SetUpSearchPreloadServiceArgs {
    std::optional<std::string> no_vary_search_data_cache;
  };

  void SetUpSearchPreloadService(SetUpSearchPreloadServiceArgs args) {
    auto no_vary_search_data_cache =
        [&]() -> std::optional<net::HttpNoVarySearchData> {
      if (!args.no_vary_search_data_cache.has_value()) {
        return std::nullopt;
      }

      return ParseNoVarySearchData(args.no_vary_search_data_cache.value());
    }();

    GetSearchPreloadService().SetNoVarySearchDataCacheForTesting(
        std::move(no_vary_search_data_cache));
  }

  void WaitForDuration(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSearchRequest(
      const net::test_server::HttpRequest& request) {
    const bool is_prefetch =
        request.GetURL().GetQuery().find("pf=cs") != std::string::npos ||
        request.GetURL().GetQuery().find("pf=op") != std::string::npos;
    CHECK_EQ(is_prefetch,
             request.headers.find(blink::kSecPurposeHeaderName) !=
                     request.headers.end() &&
                 request.headers.find(blink::kSecPurposeHeaderName)->second ==
                     blink::kSecPurposePrefetchPrerenderHeaderValue);

    if (request.GetURL().spec().find(kSearchTerms_502OnPrefetch) !=
            std::string::npos &&
        is_prefetch) {
      net::HttpStatusCode code = net::HTTP_BAD_GATEWAY;
      std::string content = "<html><body>preeftch</body></html>";
      base::StringPairs headers = {
          {"Content-Length", base::NumberToString(content.length())},
          {"Content-Type", "text/html"},
          {"No-Vary-Search", R"(key-order, params, except=("q"))"},
      };
      return CreateDeferrableResponse(code, headers, content);
    }

    net::HttpStatusCode code = net::HttpStatusCode::HTTP_OK;
    std::string content = "<html><body>prefetch</body></html>";
    base::StringPairs headers = {
        {"Content-Length", base::NumberToString(content.length())},
        {"Content-Type", "text/html"},
        {"No-Vary-Search", R"(key-order, params, except=("q"))"},
    };
    return CreateDeferrableResponse(code, headers, content);
  }

  enum class PrefetchHint { kEnabled, kDisabled };
  enum class PrerenderHint { kEnabled, kDisabled };
  enum class UrlType {
    // For URLs that will be used for a real navigation.
    kReal,
    // For URLs that will be used for prefetch requests for
    // `OnAutocompleteResultChanged()`.
    kPrefetchOnSuggest,
    // For URLs that will be used for prefetch requests for
    // `OnNavigationLikely()`.
    kPrefetchOnPress,
    // For URLs that will be used for prerender requests.
    kPrerender
  };
  struct SearchUrls {
    GURL navigation;
    GURL prefetch_on_suggest;
    GURL prefetch_on_press;
    GURL prerender;
  };

  SearchUrls GetSearchUrls(const std::string& search_terms) {
    SearchUrls urls = {
        .navigation = GetSearchUrl(search_terms, UrlType::kReal),
        .prefetch_on_suggest =
            GetSearchUrl(search_terms, UrlType::kPrefetchOnSuggest),
        .prefetch_on_press =
            GetSearchUrl(search_terms, UrlType::kPrefetchOnPress),
        .prerender = GetSearchUrl(search_terms, UrlType::kPrerender),
    };
    CHECK_EQ(urls.prerender, urls.navigation);
    return urls;
  }

  GURL GetSearchUrl(const std::string& search_terms, UrlType url_type) {
    const char* pf;
    switch (url_type) {
      case UrlType::kReal:
      case UrlType::kPrerender:
        pf = "";
        break;
      case UrlType::kPrefetchOnSuggest:
        pf = "&pf=cs";
        break;
      case UrlType::kPrefetchOnPress:
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
    match.destination_url = GetSearchUrls(search_terms).navigation;
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

  void NavigateAndWaitFCP(const GURL& url) {
    page_load_metrics::PageLoadMetricsTestWaiter waiter(&GetWebContents());
    waiter.AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                  TimingField::kFirstContentfulPaint);

    ASSERT_TRUE(content::NavigateToURL(&GetWebContents(), url));

    waiter.Wait();
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

  void NavigateAwayToRecordHistogram() {
    CHECK(content::NavigateToURL(&GetWebContents(), GURL(url::kAboutBlankURL)));
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
  EmbeddedTestServerDelayInjector& delay_injector() {
    return *delay_injector_.get();
  }
  base::HistogramTester& histogram_tester() { return *histogram_tester_.get(); }

 private:
  constexpr static char kSearchDomain[] = "a.test";
  constexpr static char16_t kSearchDomain16[] = u"a.test";

  std::unique_ptr<net::test_server::EmbeddedTestServer> https_server_;
  std::unique_ptr<EmbeddedTestServerRequestCollector> request_collector_;
  std::unique_ptr<EmbeddedTestServerDelayInjector> delay_injector_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class SearchPreloadBrowserTest : public SearchPreloadBrowserTestBase {
  void InitFeatures(
      base::test::ScopedFeatureList& scoped_feature_list) override {
    scoped_feature_list.InitWithFeaturesAndParameters(
        {
            // Check webRequest API.
            //
            // See http://crbug.com/438935264
            {
                extensions_features::kForceWebRequestProxyForTest,
                {},
            },
            {
                features::kPrefetchPrerenderIntegration,
                {},
            },
            {
                features::kDsePreload2,
                {
                    {"kDsePreload2UsePreloadServingMetrics", "true"},
                    {"kDsePreload2DeviceMemoryThresholdMiB", "0"},
                },
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
  }
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
  HistogramTesterWrapper uma_tester;
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";
  SearchUrls urls = GetSearchUrls(search_terms);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_suggest);
  }

  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));

  // Navigate.
  NavigateAndWaitFCP(urls.navigation);

  NavigateAwayToRecordHistogram();

  // Prefetch is used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));
  EXPECT_EQ(0, request_collector().CountByPath(urls.navigation));

  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
                       {SearchPreloadSignalResult::kPrefetchTriggered});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prerender",
                       {});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnPress.Prefetch", {});

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      alternative_content::PrerenderFinalStatus::kActivated, 0);

  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      0);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      1);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
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
  HistogramTesterWrapper uma_tester;
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string original_query2 = "hel";
  std::string search_terms = "hello";
  SearchUrls urls = GetSearchUrls(search_terms);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_suggest);
  }

  // A user inputs anothor character and `OnAutocompleteResultChanged()` is
  // called with "hel". Prefetch is already triggered and it doesn't trigger
  // another one.
  ChangeAutocompleteResult(original_query2, search_terms,
                           PrefetchHint::kEnabled, PrerenderHint::kDisabled);

  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));

  // Navigate.
  NavigateAndWaitFCP(urls.navigation);

  NavigateAwayToRecordHistogram();

  // Prefetch is used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));
  EXPECT_EQ(0, request_collector().CountByPath(urls.navigation));

  uma_tester.ExpectUma(
      "Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
      {SearchPreloadSignalResult::kPrefetchTriggered,
       SearchPreloadSignalResult::kNotTriggeredAlreadyTriggered});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prerender",
                       {});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnPress.Prefetch", {});

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      alternative_content::PrerenderFinalStatus::kActivated, 0);

  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      0);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      1);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
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
  HistogramTesterWrapper uma_tester;
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";
  SearchUrls urls = GetSearchUrls(search_terms);

  {
    content::test::TestPrefetchWatcher watcher;
    content::test::PrerenderHostRegistryObserver registry_observer(
        GetWebContents());

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kEnabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_suggest);

    registry_observer.WaitForTrigger(urls.prerender);
    prerender_helper().WaitForPrerenderLoadCompletion(GetWebContents(),
                                                      urls.prerender);
  }

  // Only prefetch request went through network and prerender used the
  // prefetched response.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));

  // Navigate.
  {
    content::test::PrerenderHostObserver prerender_observer(GetWebContents(),
                                                            urls.prerender);
    page_load_metrics::PageLoadMetricsTestWaiter waiter(&GetWebContents());
    waiter.AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                  TimingField::kFirstContentfulPaint);

    NavigateToPrerenderedResult(urls.navigation);

    prerender_observer.WaitForActivation();
    waiter.Wait();
  }

  NavigateAwayToRecordHistogram();

  // Prerender is used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));
  EXPECT_EQ(0, request_collector().CountByPath(urls.navigation));

  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
                       {SearchPreloadSignalResult::kPrefetchTriggered});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prerender",
                       {SearchPreloadSignalResult::kPrerenderTriggered});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnPress.Prefetch", {});

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      alternative_content::PrerenderFinalStatus::kActivated, 1);

  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      0);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      1);
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
  HistogramTesterWrapper uma_tester;
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string original_query2 = "hel";
  std::string search_terms = "hello";
  SearchUrls urls = GetSearchUrls(search_terms);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_suggest);
  }

  {
    content::test::PrerenderHostRegistryObserver registry_observer(
        GetWebContents());

    ChangeAutocompleteResult(original_query2, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kEnabled);

    prerender_helper().WaitForPrerenderLoadCompletion(GetWebContents(),
                                                      urls.prerender);
  }

  // Only prefetch request went through network and prerender used the
  // prefetched response.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));

  // Navigate.
  {
    content::test::PrerenderHostObserver prerender_observer(GetWebContents(),
                                                            urls.prerender);
    page_load_metrics::PageLoadMetricsTestWaiter waiter(&GetWebContents());
    waiter.AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                  TimingField::kFirstContentfulPaint);

    NavigateToPrerenderedResult(urls.navigation);

    prerender_observer.WaitForActivation();
    waiter.Wait();
  }

  NavigateAwayToRecordHistogram();

  // Prerender is used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));
  EXPECT_EQ(0, request_collector().CountByPath(urls.navigation));

  uma_tester.ExpectUma(
      "Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
      {SearchPreloadSignalResult::kPrefetchTriggered,
       SearchPreloadSignalResult::kNotTriggeredAlreadyTriggered});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prerender",
                       {SearchPreloadSignalResult::kPrerenderTriggered});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnPress.Prefetch", {});

  histogram_tester().ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      alternative_content::PrerenderFinalStatus::kActivated, 1);

  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      0);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      1);
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
  HistogramTesterWrapper uma_tester;
  SetUpTemplateURLService(/*prefetch_likely_navigations=*/true);
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";
  SearchUrls urls = GetSearchUrls(search_terms);

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

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_press);
  }

  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_press));

  // Navigate.
  NavigateAndWaitFCP(urls.navigation);

  NavigateAwayToRecordHistogram();

  // Prefetch is used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_press));
  EXPECT_EQ(0, request_collector().CountByPath(urls.navigation));

  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
                       {});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prerender",
                       {});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnPress.Prefetch",
                       {SearchPreloadSignalResult::kPrefetchTriggered});

  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      0);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      1);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
}

// `OnNavigationLikely()` doesn't trigger prefetch if default search provider
// doesn't opt in.
IN_PROC_BROWSER_TEST_F(
    SearchPreloadBrowserTest,
    OnNavigationLikely_DoesntTriggerPrefetchIfDefaultSearchProviderDoesntOptIn) {
  SetUpTemplateURLService(/*prefetch_likely_navigations=*/false);
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

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
//   already triggered.
// - A user navigates to a page with query "?q=hello&..."
// - Prefetch is used.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest,
                       OnAutocompleteResultChanged_Then_OnNavigationLikely) {
  HistogramTesterWrapper uma_tester;
  SetUpTemplateURLService(/*prefetch_likely_navigations=*/true);
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";
  SearchUrls urls = GetSearchUrls(search_terms);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_suggest);
  }

  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));

  AutocompleteMatch autocomplete_match = CreateSearchSuggestionMatch(
      original_query, search_terms, PrefetchHint::kEnabled,
      PrerenderHint::kDisabled);

  const bool is_triggered_prefetch =
      GetSearchPreloadService().OnNavigationLikely(
          1, autocomplete_match,
          omnibox::mojom::NavigationPredictor::kMouseDown, &GetWebContents());
  ASSERT_FALSE(is_triggered_prefetch);

  // Navigate.
  NavigateAndWaitFCP(urls.navigation);

  NavigateAwayToRecordHistogram();

  // Prefetch is used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prefetch_on_press));
  EXPECT_EQ(0, request_collector().CountByPath(urls.navigation));

  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
                       {SearchPreloadSignalResult::kPrefetchTriggered});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prerender",
                       {});
  uma_tester.ExpectUma(
      "Omnibox.DsePreload.SignalResult.OnPress.Prefetch",
      {SearchPreloadSignalResult::kNotTriggeredAlreadyTriggered});

  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      0);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      1);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
}

// Scenario:
//
// - A user inputs "he".
// - Autocomplete suggests to prefetch "hello".
// - `SearchPreloadService` starts prefetch with query "?q=hello&pf=cs..."
//   without No-Vary-Search hint.
// - A user navigates to a page with query "?q=hello&..."
//   - Prefetch matching fails due to lack of No-Vary-Search hint and "pf=cs"
//     param.
// - Prefetch is not used.
// TODO(crbug.com/434918482): Re-enable this test.
IN_PROC_BROWSER_TEST_F(
    SearchPreloadBrowserTest,
    DISABLED_TriggersPrefetchButMatchingFailedDueToNoVarySearchHint) {
  HistogramTesterWrapper uma_tester;
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = std::nullopt,
  });
  // Inject delay to keep `PrefetchContainer` waiting for a response header, so
  // that prefetch matching fail because the prefetch has query parameter
  // "pf=cs" but navigation doesn't and No-Vary-Search hint is not set. If we
  // don't do this, No-Vary-Search header is used and prefetch matching succeed.
  delay_injector().SetResponseDelay(base::Seconds(1));

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";
  SearchUrls urls = GetSearchUrls(search_terms);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    // Navigate.
    NavigateAndWaitFCP(urls.navigation);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_suggest);
  }

  NavigateAwayToRecordHistogram();

  // Prefetch isn't used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(1, request_collector().CountByPath(urls.navigation));

  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
                       {SearchPreloadSignalResult::kPrefetchTriggered});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prerender",
                       {});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnPress.Prefetch", {});

  // No-Vary-Search data cache is updated.
  histogram_tester().ExpectUniqueSample(
      "Omnibox.DsePreload.Prefetch.NoVarySearchDataCacheUpdate",
      SearchPreloadServiceNoVarySearchDataCacheUpdate::kNullToSome, 1);

  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      1);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);

  ASSERT_EQ(GetSearchPreloadService().GetNoVarySearchDataCacheForTesting(),
            ParseNoVarySearchData(R"(key-order, params, except=("q"))"));
}

// Scenario:
//
// - A user inputs "he".
// - Autocomplete suggests to prerender "hello".
// - `SearchPreloadService` starts prefetch with query "?q=hello&pf=cs..."
//   without No-Vary-Search hint.
// - `SearchPreloadService` starts prerender with query "?q=hello...".
//   - Prefetch matching fails due to lack of No-Vary-Search hint and "pf=cs"
//     param
//   - `PrerenderURLLoaderThrottle` cancels the prerender.
// - A user navigates to a page with query "?q=hello&..."
//   - Prefetch matching fails due to lack of No-Vary-Search hint and "pf=cs"
//     param.
// - Prefetch is not used.
// TODO(crbug.com/434918482): Re-enable this test on Mac, Linux, and Windows.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_TriggersPrefetchAndPrerenderButPrerenderFailsDueToNoVarySearchHint \
  DISABLED_TriggersPrefetchAndPrerenderButPrerenderFailsDueToNoVarySearchHint
#else
#define MAYBE_TriggersPrefetchAndPrerenderButPrerenderFailsDueToNoVarySearchHint \
  TriggersPrefetchAndPrerenderButPrerenderFailsDueToNoVarySearchHint
#endif
IN_PROC_BROWSER_TEST_F(
    SearchPreloadBrowserTest,
    MAYBE_TriggersPrefetchAndPrerenderButPrerenderFailsDueToNoVarySearchHint) {
  HistogramTesterWrapper uma_tester;
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = std::nullopt,
  });
  // Inject delay to keep `PrefetchContainer` waiting for a response header, so
  // that prefetch matching fail because the prefetch has query parameter
  // "pf=cs" but navigation doesn't and No-Vary-Search hint is not set. If we
  // don't do this, No-Vary-Search header is used and prefetch matching succeed.
  delay_injector().SetResponseDelay(base::Seconds(1));

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "he";
  std::string search_terms = "hello";
  SearchUrls urls = GetSearchUrls(search_terms);

  {
    content::test::TestPrefetchWatcher watcher;
    content::test::PrerenderHostObserver prerender_host_observer(
        GetWebContents(), urls.prerender);

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kEnabled);

    prerender_host_observer.WaitForDestroyed();

    // Navigate.
    NavigateAndWaitFCP(urls.navigation);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_suggest);
  }

  NavigateAwayToRecordHistogram();

  // Prefetch nor prerender aren't used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(1, request_collector().CountByPath(urls.navigation));

  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
                       {SearchPreloadSignalResult::kPrefetchTriggered});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prerender",
                       {SearchPreloadSignalResult::kPrerenderTriggered});
  uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnPress.Prefetch", {});

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      alternative_content::PrerenderFinalStatus::kPrerenderFailedDuringPrefetch,
      1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrefetchAheadOfPrerenderFailed.PrefetchStatus."
      "Embedder_DefaultSearchEngine",
      alternative_content::PrefetchStatus::kPrefetchNotFinishedInTime, 1);

  // No-Vary-Search data cache is updated.
  histogram_tester().ExpectUniqueSample(
      "Omnibox.DsePreload.Prefetch.NoVarySearchDataCacheUpdate",
      SearchPreloadServiceNoVarySearchDataCacheUpdate::kNullToSome, 1);

  ASSERT_EQ(GetSearchPreloadService().GetNoVarySearchDataCacheForTesting(),
            ParseNoVarySearchData(R"(key-order, params, except=("q"))"));

  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      1);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  uma_tester.ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
}

// A pipeline is consumed by navigation.
//
// Note that this is for aligning the behavior of `SearchPrefetchService`. It
// would be nice to discuss the ideal behavior.
//
// See also
// https://docs.google.com/document/d/1NjxwlOEoBwpXojG13M85XtS8nH-S4uc0F6VOrlwIAXE/edit?pli=1&tab=t.0#heading=h.5qv0ome418fo
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest,
                       PipelineIsConsumedByNavigation) {
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "hello";
  std::string search_terms = original_query;
  SearchUrls urls = GetSearchUrls(search_terms);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_suggest);
  }

  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));

  // Navigate.
  ASSERT_TRUE(content::NavigateToURL(&GetWebContents(), urls.navigation));

  // Prefetch is used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.navigation));

  // Prefetch was consumed by the navigation.
  ASSERT_FALSE(GetSearchPreloadService().InvalidatePipelineForTesting(
      GetWebContents(), urls.navigation));

  // Navigate.
  ASSERT_TRUE(content::NavigateToURL(&GetWebContents(), urls.navigation));

  // Prefetch is not available.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(1, request_collector().CountByPath(urls.navigation));
}

class SearchPreloadBrowserTest_ErrorBackoffDuration
    : public SearchPreloadBrowserTestBase {
  void InitFeatures(
      base::test::ScopedFeatureList& scoped_feature_list) override {
    scoped_feature_list.InitWithFeaturesAndParameters(
        {
            {
                features::kPrefetchPrerenderIntegration,
                {},
            },
            {
                features::kDsePreload2,
                {
                    {"kDsePreload2UsePreloadServingMetrics", "true"},
                    {"kDsePreload2ErrorBackoffDuration", "1000ms"},
                    {"kDsePreload2DeviceMemoryThresholdMiB", "0"},
                },
            },
        },
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(
    SearchPreloadBrowserTest_ErrorBackoffDuration,
    PreloadsAreNotTriggeredCertainPeriodAfterPrefetchFailed) {
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  auto check = [&](std::string original_query,
                   const bool is_triggered_expected) {
    HistogramTesterWrapper uma_tester;
    request_collector().Reset();

    std::string search_terms = original_query;
    SearchUrls urls = GetSearchUrls(search_terms);

    {
      content::test::TestPrefetchWatcher watcher;

      ChangeAutocompleteResult(original_query, search_terms,
                               PrefetchHint::kEnabled,
                               PrerenderHint::kDisabled);

      if (is_triggered_expected) {
        watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                                   urls.prefetch_on_suggest);
      }
    }

    EXPECT_EQ(is_triggered_expected,
              request_collector().CountByPath(urls.prefetch_on_suggest));
    EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));
  };

  check(kSearchTerms_502OnPrefetch, true);
  check("two", false);
  WaitForDuration(base::Milliseconds(1000));
  check("three", true);
}

class SearchPreloadBrowserTest_DeviceMemoryThreshold
    : public SearchPreloadBrowserTestBase {
  void InitFeatures(
      base::test::ScopedFeatureList& scoped_feature_list) override {
    scoped_feature_list.InitWithFeaturesAndParameters(
        {
            {
                features::kPrefetchPrerenderIntegration,
                {},
            },
            {
                features::kDsePreload2,
                {
                    {"kDsePreload2UsePreloadServingMetrics", "true"},
                    {"kDsePreload2DeviceMemoryThresholdMiB",
                     base::NumberToString(std::numeric_limits<int>::max())},
                },
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
  }
};

IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest_DeviceMemoryThreshold,
                       FeatureIsDisabledIfDeviceMemoryIsSmallerThanThreshold) {
  ASSERT_FALSE(features::IsDsePreload2Enabled());
}

class SearchPreloadBrowserTest_Limit : public SearchPreloadBrowserTestBase {
  void InitFeatures(
      base::test::ScopedFeatureList& scoped_feature_list) override {
    scoped_feature_list.InitWithFeaturesAndParameters(
        {
            {
                features::kPrefetchPrerenderIntegration,
                {},
            },
            {
                features::kDsePreload2,
                {
                    {"kDsePreload2UsePreloadServingMetrics", "true"},
                    {"kDsePreload2DeviceMemoryThresholdMiB", "0"},
                    {"kDsePreload2MaxPrefetch", "2"},
                },
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
  }
};

// The number of prefetches are bounded by `kDsePreload2MaxPrefetch`.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest_Limit,
                       OnAutocompleteResultChanged_PrefetchIsLimited) {
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  auto check = [&](std::string original_query,
                   const bool is_triggered_expected) {
    HistogramTesterWrapper uma_tester;
    request_collector().Reset();

    std::string search_terms = original_query;
    SearchUrls urls = GetSearchUrls(search_terms);

    {
      content::test::TestPrefetchWatcher watcher;

      ChangeAutocompleteResult(original_query, search_terms,
                               PrefetchHint::kEnabled,
                               PrerenderHint::kDisabled);

      if (is_triggered_expected) {
        watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                                   urls.prefetch_on_suggest);
      }
    }

    EXPECT_EQ(is_triggered_expected,
              request_collector().CountByPath(urls.prefetch_on_suggest));
    EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));

    if (is_triggered_expected) {
      uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
                           {SearchPreloadSignalResult::kPrefetchTriggered});
      uma_tester.ExpectUma(
          "Omnibox.DsePreload.SignalResult.OnSuggest.Prerender", {});
    } else {
      uma_tester.ExpectUma(
          "Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
          {SearchPreloadSignalResult::kNotTriggeredLimitExceeded});
      uma_tester.ExpectUma(
          "Omnibox.DsePreload.SignalResult.OnSuggest.Prerender", {});
    }
  };

  check("one", true);
  check("two", true);
  check("three", false);
  ASSERT_TRUE(GetSearchPreloadService().InvalidatePipelineForTesting(
      GetWebContents(), GetSearchUrls("one").navigation));
  check("four", true);
}

// The number of prefetches are bounded by `kDsePreload2MaxPrefetch`.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest_Limit,
                       OnNavigationLikely_PrefetchIsLimited) {
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  auto check = [&](std::string original_query,
                   const bool is_triggered_expected) {
    HistogramTesterWrapper uma_tester;
    request_collector().Reset();

    std::string search_terms = original_query;
    SearchUrls urls = GetSearchUrls(search_terms);

    {
      content::test::TestPrefetchWatcher watcher;

      AutocompleteMatch autocomplete_match = CreateSearchSuggestionMatch(
          original_query, search_terms, PrefetchHint::kEnabled,
          PrerenderHint::kDisabled);

      const bool is_triggered_prefetch =
          GetSearchPreloadService().OnNavigationLikely(
              1, autocomplete_match,
              omnibox::mojom::NavigationPredictor::kMouseDown,
              &GetWebContents());
      ASSERT_EQ(is_triggered_expected, is_triggered_prefetch);

      if (is_triggered_expected) {
        watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                                   urls.prefetch_on_press);
      }
    }

    EXPECT_EQ(is_triggered_expected,
              request_collector().CountByPath(urls.prefetch_on_press));
    EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));

    if (is_triggered_expected) {
      uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnPress.Prefetch",
                           {SearchPreloadSignalResult::kPrefetchTriggered});
    } else {
      uma_tester.ExpectUma(
          "Omnibox.DsePreload.SignalResult.OnPress.Prefetch",
          {SearchPreloadSignalResult::kNotTriggeredLimitExceeded});
    }
  };

  check("one", true);
  check("two", true);
  check("three", false);
  ASSERT_TRUE(GetSearchPreloadService().InvalidatePipelineForTesting(
      GetWebContents(), GetSearchUrls("one").navigation));
  check("four", true);
}

// The number of prerenders are bounded by 1; the last one wins.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest_Limit,
                       OnAutocompleteResultChanged_PrerenderIsLimited) {
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  auto check = [&](std::string original_query, const bool is_triggered_expected,
                   std::vector<std::string> queries_cancelled_prerender) {
    HistogramTesterWrapper uma_tester;
    request_collector().Reset();

    std::string search_terms = original_query;
    SearchUrls urls = GetSearchUrls(search_terms);

    {
      content::test::TestPrefetchWatcher watcher;
      content::test::PrerenderHostRegistryObserver registry_observer(
          GetWebContents());

      ChangeAutocompleteResult(original_query, search_terms,
                               PrefetchHint::kEnabled, PrerenderHint::kEnabled);

      if (is_triggered_expected) {
        watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                                   urls.prefetch_on_suggest);

        // Check prerender is triggered even if it reached the limit.
        registry_observer.WaitForTrigger(urls.prerender);
        prerender_helper().WaitForPrerenderLoadCompletion(GetWebContents(),
                                                          urls.prerender);

        // Check other prerenderes are cancelled.
        for (const auto& query_cancelled_prerender :
             queries_cancelled_prerender) {
          SearchUrls cancelled_urls = GetSearchUrls(query_cancelled_prerender);
          ASSERT_EQ(prerender_helper().GetHostForUrl(cancelled_urls.prerender),
                    content::FrameTreeNodeId());
        }
      }
    }

    EXPECT_EQ(is_triggered_expected,
              request_collector().CountByPath(urls.prefetch_on_suggest));
    EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));

    if (is_triggered_expected) {
      uma_tester.ExpectUma("Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
                           {SearchPreloadSignalResult::kPrefetchTriggered});
      uma_tester.ExpectUma(
          "Omnibox.DsePreload.SignalResult.OnSuggest.Prerender",
          {SearchPreloadSignalResult::kPrerenderTriggered});
    } else {
      uma_tester.ExpectUma(
          "Omnibox.DsePreload.SignalResult.OnSuggest.Prefetch",
          {SearchPreloadSignalResult::kNotTriggeredLimitExceeded});
      uma_tester.ExpectUma(
          "Omnibox.DsePreload.SignalResult.OnSuggest.Prerender", {});
    }
  };

  check("one", true, {});
  check("two", true, {"one"});
  check("three", false, {});
  ASSERT_TRUE(GetSearchPreloadService().InvalidatePipelineForTesting(
      GetWebContents(), GetSearchUrls("one").navigation));
  check("four", true, {"one", "two"});
}

class SearchPreloadBrowserTest_Ttl : public SearchPreloadBrowserTestBase {
  void InitFeatures(
      base::test::ScopedFeatureList& scoped_feature_list) override {
    scoped_feature_list.InitWithFeaturesAndParameters(
        {
            {
                features::kPrefetchPrerenderIntegration,
                {},
            },
            {
                features::kDsePreload2,
                {
                    {"kDsePreload2UsePreloadServingMetrics", "true"},
                    {"kDsePreload2DeviceMemoryThresholdMiB", "0"},
                    {"kDsePreload2MaxPrefetch", "2"},
                    {"kDsePreload2PrefetchTtl", "1000ms"},
                },
            },
        },
        /*disabled_features=*/{});
  }
};

// Prefetch expires after `kDsePreload2PrefetchTtl`.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest_Ttl, PrefetchExpiresAfterTtl) {
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "hello";
  std::string search_terms = original_query;
  SearchUrls urls = GetSearchUrls(search_terms);

  {
    content::test::TestPrefetchWatcher watcher;

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kDisabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_suggest);
  }

  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));

  WaitForDuration(base::Milliseconds(1001));

  // Navigate.
  NavigateAndWaitFCP(urls.navigation);

  NavigateAwayToRecordHistogram();

  // Prefetch is not used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(1, request_collector().CountByPath(urls.navigation));

  histogram_tester().ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      1);
  histogram_tester().ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  histogram_tester().ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      0);
}

// Scenario:
//
// - A user inputs "hello".
// - Autocomplete suggests to prerender "hello".
// - `SearchPreloadService` starts prefetch with query "?q=hello&pf=cs...".
// - `SearchPreloadService` starts prerender with query "?q=hello...".
// - Prefetch is expired.
//   - Prerender is still available because it already used the prefetch result.
// - A user navigates to a page with query "?q=hello&..."
// - Prerender is used.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest_Ttl,
                       PrerenderIsAvailableAfterPrefetchTtl) {
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  std::string original_query = "hello";
  std::string search_terms = original_query;
  SearchUrls urls = GetSearchUrls(search_terms);

  {
    content::test::TestPrefetchWatcher watcher;
    content::test::PrerenderHostRegistryObserver registry_observer(
        GetWebContents());

    ChangeAutocompleteResult(original_query, search_terms,
                             PrefetchHint::kEnabled, PrerenderHint::kEnabled);

    watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                               urls.prefetch_on_suggest);

    registry_observer.WaitForTrigger(urls.prerender);
    prerender_helper().WaitForPrerenderLoadCompletion(GetWebContents(),
                                                      urls.prerender);
  }

  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));

  WaitForDuration(base::Milliseconds(1001));

  // Navigate.
  {
    content::test::PrerenderHostObserver prerender_observer(GetWebContents(),
                                                            urls.prerender);
    page_load_metrics::PageLoadMetricsTestWaiter waiter(&GetWebContents());
    waiter.AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                  TimingField::kFirstContentfulPaint);

    NavigateToPrerenderedResult(urls.navigation);

    prerender_observer.WaitForActivation();
    waiter.Wait();
  }

  NavigateAwayToRecordHistogram();

  // Prerender is used.
  EXPECT_EQ(1, request_collector().CountByPath(urls.prefetch_on_suggest));
  EXPECT_EQ(0, request_collector().CountByPath(urls.navigation));

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
      "DefaultSearchEngine",
      alternative_content::PrerenderFinalStatus::kActivated, 1);

  histogram_tester().ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithoutPreload",
      0);
  histogram_tester().ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrefetch",
      0);
  histogram_tester().ExpectTotalCount(
      "PreloadServingMetrics.PageLoad.Clients.PaintTiming."
      "NavigationToFirstContentfulPaint.WithPrerender",
      1);
}

// Limit cares TTL; expired prefetch is not counted.
IN_PROC_BROWSER_TEST_F(SearchPreloadBrowserTest_Ttl, LimitCaresTtl) {
  SetUpTemplateURLService();
  SetUpSearchPreloadService({
      .no_vary_search_data_cache = R"(key-order, params, except=("q"))",
  });

  ASSERT_TRUE(content::NavigateToURL(
      &GetWebContents(), embedded_test_server()->GetURL("/empty.html")));

  auto check = [&](std::string original_query,
                   const bool is_triggered_expected) {
    HistogramTesterWrapper uma_tester;
    request_collector().Reset();

    std::string search_terms = original_query;
    SearchUrls urls = GetSearchUrls(search_terms);

    {
      content::test::TestPrefetchWatcher watcher;

      ChangeAutocompleteResult(original_query, search_terms,
                               PrefetchHint::kEnabled,
                               PrerenderHint::kDisabled);

      if (is_triggered_expected) {
        watcher.WaitUntilPrefetchResponseCompleted(std::nullopt,
                                                   urls.prefetch_on_suggest);
      }
    }

    EXPECT_EQ(is_triggered_expected,
              request_collector().CountByPath(urls.prefetch_on_suggest));
    EXPECT_EQ(0, request_collector().CountByPath(urls.prerender));
  };

  check("one", true);
  check("two", true);
  check("three", false);
  WaitForDuration(base::Milliseconds(1001));
  check("four", true);
}

}  // namespace
