// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

constexpr static char kSearchDomain[] = "a.test";
constexpr static char kSuggestionDomain[] = "a.test";
constexpr static char16_t kSearchDomain16[] = u"a.test";

// Copy of the feature here to test the actual feature string.
BASE_FEATURE(kNavigationLatencyAblation,
             "NavigationLatencyAblation",
             base::FEATURE_DISABLED_BY_DEFAULT);

std::unique_ptr<HttpResponse> ReturnOKResponseForAllRequests(
    const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/plain");
  http_response->set_content(request.GetURL().spec());
  return http_response;
}

}  // namespace

class LatencyAblationBrowserTest : public InProcessBrowserTest {
 public:
  LatencyAblationBrowserTest() = default;
  ~LatencyAblationBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    prerender_helper_->RegisterServerRequestMonitor(https_server_.get());
    https_server_->RegisterRequestHandler(
        base::BindRepeating(&ReturnOKResponseForAllRequests));
    ASSERT_TRUE(https_server_->Start());

    TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());
    TemplateURLData data;
    data.SetShortName(kSearchDomain16);
    data.SetKeyword(data.short_name());
    data.SetURL(
        https_server_->GetURL(kSearchDomain, "/title1.html?q={searchTerms}")
            .spec());
    data.suggestions_url =
        https_server_->GetURL(kSuggestionDomain, "/?q={searchTerms}").spec();
    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);

    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch("ignore-certificate-errors");

    mock_cert_verifier_.SetUpCommandLine(cmd);
    InProcessBrowserTest::SetUpCommandLine(cmd);
  }

  virtual void SetUpFieldTrial() {}

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Convenient for sub-classes to not have to define this.
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

 private:
  void SetUp() override {
    SetUpFieldTrial();

    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(&LatencyAblationBrowserTest::GetWebContents,
                            base::Unretained(this)));
    InProcessBrowserTest::SetUp();
  }

  content::ContentMockCertVerifier mock_cert_verifier_;
};

// Check that the default behavior is that there is no ablation.
IN_PROC_BROWSER_TEST_F(LatencyAblationBrowserTest, DenyIPAddress) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL("/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    0u);
}

class LatencyAblationDisabledBrowserTest : public LatencyAblationBrowserTest {
 public:
  LatencyAblationDisabledBrowserTest() = default;
  ~LatencyAblationDisabledBrowserTest() override = default;

 private:
  void SetUpFieldTrial() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {}, {kNavigationLatencyAblation});
  }
};

// Check that when the feature is off, there is no ablation.
IN_PROC_BROWSER_TEST_F(LatencyAblationDisabledBrowserTest, DenyIPAddress) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL("/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    0u);
}

class LatencyAblationEnabledBrowserTest : public LatencyAblationBrowserTest {
 public:
  LatencyAblationEnabledBrowserTest() = default;

 private:
  void SetUpFieldTrial() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kNavigationLatencyAblation, {{"duration", "5ms"}}}}, {});
  }
};

// Check that IP navigations are not ablated when there is a duration
// configured. Use the histogram as a proxy for actual ablation.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledBrowserTest, DontAblateIPAddress) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL("/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    0u);
}

// Check that prerender navigations and activations are not ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledBrowserTest, DontAblatePrerender) {
  EXPECT_TRUE(content::NavigateToURL(
      GetWebContents(), https_server_->GetURL(kSearchDomain, "/title2.html")));

  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSearchDomain, "/title1.html"));
  content::FrameTreeNodeId host_id = prerender_helper_->AddPrerender(url);
  EXPECT_TRUE(host_id);
  prerender_helper_->NavigatePrimaryPage(url);

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    0u);
}

// Search query navigations should be ablated by default.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledBrowserTest, AblateNonSearch) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL("anysite.com", "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

// Search navigations should be ablated by default.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledBrowserTest, AblateSearchHost) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSearchDomain, "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

// Search hosts that are not search queries should be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledBrowserTest,
                       AblateSearchRealtedHost) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSuggestionDomain, "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

// Search queries should be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledBrowserTest, AblateSearchQuery) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSearchDomain, "/title1.html?q=cat"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

class LatencyAblationEnabledSearchQueryDisabledBrowserTest
    : public LatencyAblationBrowserTest {
 public:
  LatencyAblationEnabledSearchQueryDisabledBrowserTest() = default;

 private:
  void SetUpFieldTrial() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kNavigationLatencyAblation,
          {{"duration", "5ms"}, {"ablate_default_search_queries", "false"}}}},
        {});
  }
};

// Non search queries should still be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledSearchQueryDisabledBrowserTest,
                       AblateNonSearch) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL("anysite.com", "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

// Search hosts that aren't queries should still be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledSearchQueryDisabledBrowserTest,
                       AblateSearchHost) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSearchDomain, "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

// Search related hosts should still be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledSearchQueryDisabledBrowserTest,
                       AblateSearchRealtedHost) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSuggestionDomain, "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

// With "ablate_default_search_queries" set to "false" search queries should not
// be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledSearchQueryDisabledBrowserTest,
                       DoNotAblateSearchQuery) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSearchDomain, "/title1.html?q=cat"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    0u);
}

class LatencyAblationEnabledSearchHostDisabledBrowserTest
    : public LatencyAblationBrowserTest {
 public:
  LatencyAblationEnabledSearchHostDisabledBrowserTest() = default;

 private:
  void SetUpFieldTrial() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kNavigationLatencyAblation,
          {{"duration", "5ms"}, {"ablate_default_search_host", "false"}}}},
        {});
  }
};

// Non search navigations should still be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledSearchHostDisabledBrowserTest,
                       AblateNonSearch) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL("anysite.com", "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

// With "ablate_default_search_host" set to "false", search hosts that aren't
// queries should be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledSearchHostDisabledBrowserTest,
                       DoNotAblateSearchHost) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSearchDomain, "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    0u);
}

// With "ablate_default_search_host" set to "false", search related hosts that
// aren't queries should be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledSearchHostDisabledBrowserTest,
                       DoNotAblateSearchRealtedHost) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSuggestionDomain, "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    0u);
}

// With "ablate_default_search_host" set to "false", search queries should still
// be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledSearchHostDisabledBrowserTest,
                       AblateSearchQuery) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSearchDomain, "/title1.html?q=cat"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

class LatencyAblationEnabledNonSearchDisabledBrowserTest
    : public LatencyAblationBrowserTest {
 public:
  LatencyAblationEnabledNonSearchDisabledBrowserTest() = default;

 private:
  void SetUpFieldTrial() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kNavigationLatencyAblation,
          {{"duration", "5ms"}, {"ablate_non_default_search_host", "false"}}}},
        {});
  }
};

// With "ablate_non_default_search_host" set to "false", non search should not
// be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledNonSearchDisabledBrowserTest,
                       DoNotAblateNonSearch) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL("anysite.com", "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    0u);
}

// Search host should still be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledNonSearchDisabledBrowserTest,
                       DoNotAblateSearchHost) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSearchDomain, "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

// Search related hosts should still be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledNonSearchDisabledBrowserTest,
                       AblateSearchRealtedHost) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSuggestionDomain, "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

// Search queries should still be ablated.
IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledNonSearchDisabledBrowserTest,
                       AblateSearchQuery) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL(kSearchDomain, "/title1.html?q=cat"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

// Test Latency Ablation based on pattern.
class LatencyAblationEnabledPatternBrowserTest
    : public LatencyAblationBrowserTest {
 public:
  LatencyAblationEnabledPatternBrowserTest() = default;

 private:
  void SetUpFieldTrial() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kNavigationLatencyAblation,
          {{"duration", "5ms"},
           {"pattern", "*foo.test*/maps*"},  // we need * after .test to match
                                             // `foo.test:[port_num]/maps/...`
           {"ablate_default_search_queries", "false"},
           {"ablate_default_search_host", "false"},
           {"ablate_non_default_search_host", "false"}}}},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledPatternBrowserTest,
                       AblatePattern) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL("www.foo.test", "/maps/places/12345"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    1u);
}

IN_PROC_BROWSER_TEST_F(LatencyAblationEnabledPatternBrowserTest,
                       DoNotAblateIfNotMatch) {
  base::HistogramTester histogram_tester;

  GURL url(https_server_->GetURL("anysite.com", "/title1.html"));

  EXPECT_TRUE(content::NavigateToURL(GetWebContents(), url));

  histogram_tester.ExpectTotalCount("Navigation.LatencyAblation.ExcessWaitTime",
                                    0u);
}
