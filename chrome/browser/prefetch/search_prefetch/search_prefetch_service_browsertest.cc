// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace {
constexpr char kSuggestDomain[] = "suggest.com";
constexpr char kSearchDomain[] = "search.com";
constexpr char kOmniboxSuggestPrefetchQuery[] = "porgs";
constexpr char kOmniboxSuggestPrefetchSecondItemQuery[] = "porgsandwich";
constexpr char kOmniboxSuggestNonPrefetchQuery[] = "puffins";
}  // namespace

class SearchPrefetchBaseBrowserTest : public InProcessBrowserTest {
 public:
  SearchPrefetchBaseBrowserTest() {
    search_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    search_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    search_server_->RegisterRequestHandler(
        base::BindRepeating(&SearchPrefetchBaseBrowserTest::HandleSearchRequest,
                            base::Unretained(this)));
    EXPECT_TRUE(search_server_->Start());

    search_suggest_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    search_suggest_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    search_suggest_server_->RegisterRequestHandler(base::BindRepeating(
        &SearchPrefetchBaseBrowserTest::HandleSearchSuggestRequest,
        base::Unretained(this)));
    EXPECT_TRUE(search_suggest_server_->Start());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule(kSearchDomain, "127.0.0.1");
    host_resolver()->AddRule(kSuggestDomain, "127.0.0.1");

    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());

    SetDSEWithURL(GetSearchServerQueryURL("{searchTerms}"));
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    InProcessBrowserTest::SetUpCommandLine(cmd);
    // For the proxy.
    cmd->AppendSwitch("ignore-certificate-errors");
    cmd->AppendSwitch("force-enable-metrics-reporting");
  }

  size_t search_server_request_count() const {
    return search_server_request_count_;
  }

  size_t search_server_prefetch_request_count() const {
    return search_server_prefetch_request_count_;
  }

  const std::vector<net::test_server::HttpRequest>& search_server_requests()
      const {
    return search_server_requests_;
  }

  GURL GetSearchServerQueryURL(const std::string& path) const {
    return search_server_->GetURL(kSearchDomain, "/search_page.html?q=" + path);
  }

  GURL GetSearchServerQueryURLWithNoQuery(const std::string& path) const {
    return search_server_->GetURL(kSearchDomain, path);
  }

  GURL GetSuggestServerURL(const std::string& path) const {
    return search_suggest_server_->GetURL(kSuggestDomain, path);
  }

  void WaitUntilStatusChanges(base::string16 search_terms) {
    auto* search_prefetch_service =
        SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
    auto status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
        search_terms);
    while (status == search_prefetch_service->GetSearchPrefetchStatusForTesting(
                         search_terms)) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  void WaitUntilStatusChangesTo(base::string16 search_terms,
                                SearchPrefetchStatus status) {
    auto* search_prefetch_service =
        SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
    while (!search_prefetch_service
                ->GetSearchPrefetchStatusForTesting(search_terms)
                .has_value() ||
           status != search_prefetch_service
                         ->GetSearchPrefetchStatusForTesting(search_terms)
                         .value()) {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::string GetDocumentInnerHTML() const {
    return content::EvalJs(GetWebContents(),
                           "document.documentElement.innerHTML")
        .ExtractString();
  }

  void set_should_hang_requests(bool should_hang_requests) {
    should_hang_requests_ = should_hang_requests;
  }

  void WaitForDuration(base::TimeDelta duration) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

  void set_phi_is_one(bool phi_is_one) { phi_is_one_ = phi_is_one; }

  void ClearBrowsingCacheData(base::Optional<GURL> url_origin) {
    auto filter = content::BrowsingDataFilterBuilder::Create(
        url_origin ? content::BrowsingDataFilterBuilder::Mode::kDelete
                   : content::BrowsingDataFilterBuilder::Mode::kPreserve);
    if (url_origin)
      filter->AddOrigin(url::Origin::Create(url_origin.value()));
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_CACHE,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter), &completion_observer);
  }

  void SetDSEWithURL(const GURL& url) {
    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(kSearchDomain));
    data.SetKeyword(data.short_name());
    data.SetURL(url.spec());
    data.suggestions_url =
        search_suggest_server_->GetURL(kSuggestDomain, "/?q={searchTerms}")
            .spec();

    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  // This is sufficient to cause observer calls about updated template URL, but
  // doesn't change DSE at all.
  void UpdateButChangeNothingInDSE() {
    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16(kSuggestDomain));
    data.SetKeyword(data.short_name());
    data.SetURL(
        search_suggest_server_->GetURL(kSuggestDomain, "/?q={searchTerms}")
            .spec());
    data.suggestions_url =
        search_suggest_server_->GetURL(kSuggestDomain, "/?q={searchTerms}")
            .spec();

    model->Add(std::make_unique<TemplateURL>(data));
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleSearchRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("favicon") != std::string::npos)
      return nullptr;

    if (should_hang_requests_)
      return std::make_unique<net::test_server::HungResponse>();

    bool is_prefetch =
        request.headers.find("Purpose") != request.headers.end() &&
        request.headers.find("Purpose")->second == "prefetch";

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SearchPrefetchBaseBrowserTest::
                           MonitorSearchResourceRequestOnUIThread,
                       base::Unretained(this), request, is_prefetch));

    if (request.GetURL().spec().find("502_on_prefetch") != std::string::npos) {
      std::unique_ptr<net::test_server::BasicHttpResponse> resp =
          std::make_unique<net::test_server::BasicHttpResponse>();
      resp->set_code(is_prefetch ? net::HTTP_BAD_GATEWAY : net::HTTP_OK);
      resp->set_content_type("text/html");
      resp->set_content("<html><body></body></html>");
      return resp;
    }

    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    resp->set_content_type("text/html");
    std::string content = "<html><body> ";
    content.append(is_prefetch ? "prefetch" : "regular");
    content.append(" </body></html>");
    resp->set_content(content);
    return resp;
  }

  void MonitorSearchResourceRequestOnUIThread(
      net::test_server::HttpRequest request,
      bool has_prefetch_header) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    search_server_request_count_++;
    search_server_requests_.push_back(request);
    if (has_prefetch_header) {
      search_server_prefetch_request_count_++;
    }
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSearchSuggestRequest(
      const net::test_server::HttpRequest& request) {
    // |content| is a json request that contains the search suggest response.
    // The first item is the query (not used), the second is the results list,
    // the third is descriptions, fifth is an extra data dictionary. The
    // google:clientdata contains "phi" which is the prefetch index (i.e., which
    // suggest can be prefetched).
    std::string content = R"([
      "empty",
      ["empty", "porgs"],
      ["", ""],
      [],
      {}])";

    if (request.GetURL().spec().find(kOmniboxSuggestPrefetchQuery) !=
        std::string::npos) {
      if (phi_is_one_) {
        content = R"([
      "porgs",
      ["porgs","porgsandwich"],
      ["", ""],
      [],
      {
        "google:clientdata": {
          "phi": 1
        }
      }])";
      } else {
        content = R"([
      "porgs",
      ["porgs","porgsandwich"],
      ["", ""],
      [],
      {
        "google:clientdata": {
          "phi": 0
        }
      }])";
      }
    }

    if (request.GetURL().spec().find(kOmniboxSuggestNonPrefetchQuery) !=
        std::string::npos) {
      content = R"([
      "puffins",
      ["puffins","puffinsalad"],
      ["", ""],
      [],
      {}])";
    }

    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    resp->set_content_type("application/json");
    resp->set_content(content);
    return resp;
  }

  std::vector<net::test_server::HttpRequest> search_server_requests_;
  std::unique_ptr<net::EmbeddedTestServer> search_server_;

  std::unique_ptr<net::EmbeddedTestServer> search_suggest_server_;

  bool should_hang_requests_ = false;

  size_t search_server_request_count_ = 0;
  size_t search_server_prefetch_request_count_ = 0;

  // Sets the prefetch index to be 1 instead of 0, making the second result
  // prefetchable, but marking the first result as not prefetchable (must be
  // used with |kkOmniboxSuggestPrefetchQuery|).
  bool phi_is_one_ = false;
};

class SearchPrefetchServiceDisabledBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(kSearchPrefetchService);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceDisabledBrowserTest,
                       ServiceNotCreatedWhenDisabled) {
  EXPECT_EQ(nullptr,
            SearchPrefetchServiceFactory::GetForProfile(browser()->profile()));
}

class SearchPrefetchServiceEnabledWithoutPrefetchingBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceEnabledWithoutPrefetchingBrowserTest() {
    feature_list_.InitWithFeatures({kSearchPrefetchService},
                                   {kSearchPrefetchServicePrefetching});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SearchPrefetchServiceEnabledWithoutPrefetchingBrowserTest,
    ServiceNotCreatedWhenIncognito) {
  EXPECT_EQ(nullptr, SearchPrefetchServiceFactory::GetForProfile(
                         browser()->profile()->GetPrimaryOTRProfile()));
}

IN_PROC_BROWSER_TEST_F(
    SearchPrefetchServiceEnabledWithoutPrefetchingBrowserTest,
    ServiceCreatedWhenFeatureEnabled) {
  EXPECT_NE(nullptr,
            SearchPrefetchServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(
    SearchPrefetchServiceEnabledWithoutPrefetchingBrowserTest,
    NoFetchWhenPrefetchDisabled) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));

  EXPECT_FALSE(prefetch_status.has_value());
}

class SearchPrefetchServiceEnabledBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceEnabledBrowserTest() {
    feature_list_.InitWithFeatures(
        {kSearchPrefetchService, kSearchPrefetchServicePrefetching}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ServiceNotCreatedWhenIncognito) {
  EXPECT_EQ(nullptr, SearchPrefetchServiceFactory::GetForProfile(
                         browser()->profile()->GetPrimaryOTRProfile()));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ServiceCreatedWhenFeatureEnabled) {
  EXPECT_NE(nullptr,
            SearchPrefetchServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       BasicPrefetchFunctionality) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kInFlight, prefetch_status.value());

  WaitUntilStatusChanges(base::ASCIIToUTF16(search_terms));

  EXPECT_EQ(1u, search_server_requests().size());
  EXPECT_NE(std::string::npos,
            search_server_requests()[0].GetURL().spec().find(search_terms));
  EXPECT_EQ(1u, search_server_request_count());
  EXPECT_EQ(1u, search_server_prefetch_request_count());

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kSuccessfullyCompleted,
            prefetch_status.value());
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       PrefetchRateLimiting) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_1")));
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_2")));
  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_3")));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16("prefetch_1"));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kInFlight, prefetch_status.value());

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16("prefetch_2"));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kInFlight, prefetch_status.value());

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16("prefetch_3"));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       502PrefetchFunctionality) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "502_on_prefetch";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kInFlight, prefetch_status.value());

  WaitUntilStatusChanges(base::ASCIIToUTF16(search_terms));

  EXPECT_EQ(1u, search_server_requests().size());
  EXPECT_NE(std::string::npos,
            search_server_requests()[0].GetURL().spec().find(search_terms));
  EXPECT_EQ(1u, search_server_request_count());
  EXPECT_EQ(1u, search_server_prefetch_request_count());

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestFailed, prefetch_status.value());
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       FetchSameTermsOnlyOnce) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  WaitUntilStatusChanges(base::ASCIIToUTF16(search_terms));

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest, BadURL) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_path = "/bad_path";

  GURL prefetch_url = GetSearchServerQueryURLWithNoQuery(search_path);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       PreloadDisabled) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kNetworkPredictionOptions,
      chrome_browser_net::NETWORK_PREDICTION_NEVER);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       BasicPrefetchServed) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  WaitUntilStatusChanges(base::ASCIIToUTF16(search_terms));

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kSuccessfullyCompleted,
            prefetch_status.value());

  ui_test_utils::NavigateToURL(browser(), prefetch_url);

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       RegularSearchQueryWhenNoPrefetch) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL search_url = GetSearchServerQueryURL(search_terms);

  ui_test_utils::NavigateToURL(browser(), search_url);

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       NonMatchingPrefetchURL) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";
  std::string search_terms_other = "other";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  WaitUntilStatusChanges(base::ASCIIToUTF16(search_terms));

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kSuccessfullyCompleted,
            prefetch_status.value());

  ui_test_utils::NavigateToURL(browser(),
                               GetSearchServerQueryURL(search_terms_other));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ErrorCausesNoFetch) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "502_on_prefetch";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  WaitUntilStatusChanges(base::ASCIIToUTF16(search_terms));

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestFailed, prefetch_status.value());

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("other_query")));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       OmniboxEditTriggersPrefetch) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::TimeDelta::FromSeconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kSuccessfullyCompleted);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kSuccessfullyCompleted,
            prefetch_status.value());

  ui_test_utils::NavigateToURL(browser(),
                               GetSearchServerQueryURL(search_terms));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       OmniboxEditDoesNotTriggersPrefetch) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestNonPrefetchQuery;

  // Trigger an omnibox suggest fetch that does not have a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::TimeDelta::FromSeconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitForDuration(base::TimeDelta::FromMilliseconds(100));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
  ui_test_utils::NavigateToURL(browser(),
                               GetSearchServerQueryURL(search_terms));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       OmniboxEditTriggersPrefetchForSecondMatch) {
  // phi being set to one causes the order of prefetch suggest to be different.
  // This should still prefetch a result for the |kOmniboxSuggestPrefetchQuery|.
  set_phi_is_one(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::TimeDelta::FromSeconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(
      base::ASCIIToUTF16(kOmniboxSuggestPrefetchSecondItemQuery),
      SearchPrefetchStatus::kSuccessfullyCompleted);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(kOmniboxSuggestPrefetchSecondItemQuery));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kSuccessfullyCompleted,
            prefetch_status.value());

  ui_test_utils::NavigateToURL(
      browser(),
      GetSearchServerQueryURL(kOmniboxSuggestPrefetchSecondItemQuery));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       RemovingMatchCancelsInFlight) {
  set_should_hang_requests(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::TimeDelta::FromSeconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kInFlight);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kInFlight, prefetch_status.value());

  // Change the autocomplete to demote "porgs", but keep it as a match by using
  // the default returned suggest list.
  AutocompleteInput empty_input(
      base::ASCIIToUTF16("empty"), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  autocomplete_controller->Start(empty_input);
  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitForDuration(base::TimeDelta::FromMilliseconds(100));
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kInFlight, prefetch_status.value());

  // Change the autocomplete to remove "porgs" entirely.
  AutocompleteInput other_input(
      base::ASCIIToUTF16(kOmniboxSuggestNonPrefetchQuery),
      metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  autocomplete_controller->Start(other_input);
  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kRequestCancelled);
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestCancelled, prefetch_status.value());
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ClearCacheRemovesPrefetch) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  ClearBrowsingCacheData(base::nullopt);
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ClearCacheSearchRemovesPrefetch) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  ClearBrowsingCacheData(GURL(GetSearchServerQueryURLWithNoQuery("/")));
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ClearCacheOtherSavesCache) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  ClearBrowsingCacheData(GetSuggestServerURL("/"));
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ChangeDSESameOriginClearsPrefetches) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  SetDSEWithURL(GetSearchServerQueryURL("/q={searchTerms}&extra_stuff"));

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ChangeDSECrossOriginClearsPrefetches) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  SetDSEWithURL(GetSuggestServerURL("/q={searchTerms}"));

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ChangeDSESameDoesntClearPrefetches) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  UpdateButChangeNothingInDSE();

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());
}

class SearchPrefetchServiceZeroCacheTimeBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceZeroCacheTimeBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kSearchPrefetchServicePrefetching,
          {{"prefetch_caching_limit_ms", "10"}}},
         {{kSearchPrefetchService}, {}}},
        {});

    // Hang responses so the status will stay as InFlight until the entry is
    // removed.
    set_should_hang_requests(true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceZeroCacheTimeBrowserTest,
                       ExpireAfterDuration) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  // Make sure a new fetch doesn't happen before expiry.
  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  WaitUntilStatusChanges(base::ASCIIToUTF16(search_terms));
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));

  // Prefetch should be gone now.
  EXPECT_FALSE(prefetch_status.has_value());
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceZeroCacheTimeBrowserTest,
                       PrefetchRateLimitingClearsAfterRemoval) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_1")));
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_2")));
  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_3")));

  WaitUntilStatusChanges(base::ASCIIToUTF16("prefetch_1"));

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_4")));
}

class SearchPrefetchServiceZeroErrorTimeBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceZeroErrorTimeBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kSearchPrefetchServicePrefetching,
          {{"error_backoff_duration_ms", "10"}}},
         {{kSearchPrefetchService}, {}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceZeroErrorTimeBrowserTest,
                       ErrorClearedAfterDuration) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "502_on_prefetch";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  WaitUntilStatusChanges(base::ASCIIToUTF16(search_terms));

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestFailed, prefetch_status.value());

  WaitForDuration(base::TimeDelta::FromMilliseconds(30));

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("other_query")));
}

class SearchPrefetchServiceDefaultMatchOnlyBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceDefaultMatchOnlyBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kSearchPrefetchServicePrefetching,
          {{"only_prefetch_default_match", "true"}}},
         {{kSearchPrefetchService}, {}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceDefaultMatchOnlyBrowserTest,
                       OmniboxEditDoesNotTriggerPrefetchForSecondMatch) {
  // phi being set to one causes the order of prefetch suggest to be different.
  // This should still prefetch a result for the |kOmniboxSuggestPrefetchQuery|.
  set_phi_is_one(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that does not have a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::TimeDelta::FromSeconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitForDuration(base::TimeDelta::FromMilliseconds(100));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(kOmniboxSuggestPrefetchSecondItemQuery));
  EXPECT_FALSE(prefetch_status.has_value());
  ui_test_utils::NavigateToURL(
      browser(),
      GetSearchServerQueryURL(kOmniboxSuggestPrefetchSecondItemQuery));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
}
