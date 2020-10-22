// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

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
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("search.test", "127.0.0.1");

    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());

    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("search.test"));
    data.SetKeyword(data.short_name());
    data.SetURL(GetSearchServerQueryURL("{searchTerms}").spec());

    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
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
    return search_server_->GetURL("search.test", "/search_page.html?q=" + path);
  }

  GURL GetSearchServerQueryURLWithNoQuery(const std::string& path) const {
    return search_server_->GetURL("search.test", path);
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

  std::vector<net::test_server::HttpRequest> search_server_requests_;
  std::unique_ptr<net::EmbeddedTestServer> search_server_;

  bool should_hang_requests_ = false;

  size_t search_server_request_count_ = 0;
  size_t search_server_prefetch_request_count_ = 0;
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
