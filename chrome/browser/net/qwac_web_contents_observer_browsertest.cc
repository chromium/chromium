// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/qwac_web_contents_observer.h"

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class ResponseHandlerBase {
 public:
  ResponseHandlerBase(net::EmbeddedTestServer& server, std::string_view path)
      : expected_path_(path) {
    server.RegisterRequestHandler(base::BindRepeating(
        &ResponseHandlerBase::ServeResponse, base::Unretained(this)));
  }

  std::unique_ptr<net::test_server::HttpResponse> ServeResponse(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() != expected_path_) {
      return nullptr;
    }

    ++request_count_;
    return ServeResponseImpl(request);
  }

  virtual std::unique_ptr<net::test_server::HttpResponse> ServeResponseImpl(
      const net::test_server::HttpRequest& request) = 0;

  int request_count() const { return request_count_; }

 private:
  std::atomic_int request_count_ = 0;
  std::string expected_path_;
};

class ServePageWithQwacHeaderHandler : public ResponseHandlerBase {
 public:
  ServePageWithQwacHeaderHandler(net::EmbeddedTestServer& server,
                                 std::string_view path,
                                 std::string_view qwac_link_url)
      : ResponseHandlerBase(server, path), qwac_link_url_(qwac_link_url) {}

  std::unique_ptr<net::test_server::HttpResponse> ServeResponseImpl(
      const net::test_server::HttpRequest& request) override {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content("");
    http_response->AddCustomHeader(
        "Link", base::StringPrintf("<%s>;rel=\"tls-certificate-binding\"",
                                   qwac_link_url_));
    http_response->AddCustomHeader("Cache-Control", "max-age=3600");
    return std::move(http_response);
  }

 private:
  std::string qwac_link_url_;
};

class ServeResponseHandler : public ResponseHandlerBase {
 public:
  ServeResponseHandler(net::EmbeddedTestServer& server,
                       std::string_view path,
                       std::string_view content,
                       std::string_view content_type,
                       net::HttpStatusCode status_code = net::HTTP_OK)
      : ResponseHandlerBase(server, path),
        content_(content),
        content_type_(content_type),
        status_code_(status_code) {}

  std::unique_ptr<net::test_server::HttpResponse> ServeResponseImpl(
      const net::test_server::HttpRequest& request) override {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_content(content_);
    http_response->set_content_type(content_type_);
    http_response->set_code(status_code_);
    http_response->AddCustomHeader("Cache-Control", "max-age=3600");
    return http_response;
  }

 private:
  std::string content_;
  std::string content_type_;
  net::HttpStatusCode status_code_;
};

std::unique_ptr<net::test_server::HttpResponse> ServeRedirectWithQwacHeader(
    const std::string& expected_path,
    const std::string& redirect_url,
    const std::string& qwac_link_url,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() != expected_path) {
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url);
  http_response->AddCustomHeader(
      "Link", base::StringPrintf("<%s>;rel=\"tls-certificate-binding\"",
                                 qwac_link_url));
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> FailTestIfPathRequested(
    const std::string& expected_path,
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() != expected_path) {
    return nullptr;
  }

  ADD_FAILURE() << "FailTestIfPathRequested " << request.relative_url;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_BAD_REQUEST);
  return http_response;
}

bool WaitForStatusFinished(QwacWebContentsObserver::QwacStatus* status) {
  if (status->is_finished()) {
    return true;
  }

  base::test::TestFuture<void> future;
  auto subscription = status->RegisterCallback(future.GetCallback());
  return future.Wait();
}

class QwacWebContentsObserverTestBase : public PlatformBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  QwacWebContentsObserver::QwacStatus* GetCurrentPageQwacStatus() {
    return QwacWebContentsObserver::QwacStatus::GetForPage(
        web_contents()->GetPrimaryPage());
  }
};

class QwacWebContentsObserverDisabledBrowserTest
    : public QwacWebContentsObserverTestBase {
 public:
  QwacWebContentsObserverDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(net::features::kVerifyQWACs);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverDisabledBrowserTest,
                       TestFeatureDisabled) {
  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&FailTestIfPathRequested, "/qwac"));
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main")));

  EXPECT_FALSE(GetCurrentPageQwacStatus());
}

class QwacWebContentsObserverBrowserTest
    : public QwacWebContentsObserverTestBase {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      net::features::kVerifyQWACs};
};

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       TestSuccessAndCachedGoBack) {
  const std::string kFakeQwac = "fake qwac data";

  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                    kFakeQwac, "application/jose");
  ServeResponseHandler nonqwac_page_handler(embedded_https_test_server(),
                                            "/main_noqwac", "", "text/html");
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main")));

  {
    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    EXPECT_EQ(kFakeQwac, status->GetResponseBodyForTesting());
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());
  }

  // Navigate to a different page without a qwac.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main_noqwac")));
  EXPECT_FALSE(GetCurrentPageQwacStatus());

  // Now go back to the previous page. If BFCache is enabled, qwac status
  // should still be available and not need to be re-fetched, otherwise it
  // should be re-fetched successfully.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  {
    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    if (base::FeatureList::IsEnabled(features::kBackForwardCache)) {
      ASSERT_TRUE(status->is_finished());
    } else {
      ASSERT_TRUE(WaitForStatusFinished(status));
    }
    EXPECT_EQ(kFakeQwac, status->GetResponseBodyForTesting());
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());
  }
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       TestSuccessAndCachedNavigateSamePage) {
  const std::string kFakeQwac = "fake qwac data";

  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                    kFakeQwac, "application/jose");
  ServeResponseHandler nonqwac_page_handler(embedded_https_test_server(),
                                            "/main_noqwac", "", "text/html");
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main")));

  {
    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    EXPECT_EQ(kFakeQwac, status->GetResponseBodyForTesting());
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());
  }

  // Navigate to a different page without a qwac.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main_noqwac")));
  EXPECT_FALSE(GetCurrentPageQwacStatus());

  // Now navigate to the original page URL again.
  // The page and the 2-QWAC should be in HTTP cache, so the page and qwac
  // status should be loaded without reloading either from the http server.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main")));

  {
    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    EXPECT_EQ(kFakeQwac, status->GetResponseBodyForTesting());
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());
  }
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       TestReloadDifferentCertAndQwac) {
  const std::string kFakeQwac1 = "initial fake qwac data";
  const std::string kFakeQwac2 = "changed fake qwac data";

  int port;
  {
    ServePageWithQwacHeaderHandler main_page_handler(
        embedded_https_test_server(), "/main", "/qwac");
    ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                      kFakeQwac1, "application/jose");
    auto test_server_handle =
        embedded_https_test_server().StartAndReturnHandle();
    ASSERT_TRUE(test_server_handle.is_valid());

    EXPECT_TRUE(content::NavigateToURL(
        web_contents(), embedded_https_test_server().GetURL("/main")));

    {
      auto* status = GetCurrentPageQwacStatus();
      ASSERT_TRUE(status);
      ASSERT_TRUE(WaitForStatusFinished(status));
      EXPECT_EQ(kFakeQwac1, status->GetResponseBodyForTesting());
    }

    // Save the port number and release the `test_server_handle`, shutting down
    // the existing test server.
    port = embedded_https_test_server().port();
  }

  // Start a new test server on the same port with a different certificate and
  // different 2-QWAC.
  // This might be flaky if the test can't open the same port again, but
  // there's not really any other way to test this.
  net::EmbeddedTestServer https_test_server_2(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_2.SetCertHostnames({"example.com"});
  ServePageWithQwacHeaderHandler main_page_handler(https_test_server_2, "/main",
                                                   "/qwac2");
  ServeResponseHandler qwac_handler(https_test_server_2, "/qwac2", kFakeQwac2,
                                    "application/jose");
  auto test_server_handle = https_test_server_2.StartAndReturnHandle(port);
  ASSERT_TRUE(test_server_handle.is_valid());

  // Reload the page.
  content::TestNavigationObserver reload_observer(
      web_contents(),
      /*expected_number_of_navigations=*/1);
  web_contents()->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                         /*check_for_repost=*/false);
  reload_observer.Wait();

  {
    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    // Should have gotten the new 2-QWAC response.
    EXPECT_EQ(kFakeQwac2, status->GetResponseBodyForTesting());
  }
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       SameDocumentNavigation) {
  const std::string kFakeQwac = "fake qwac data";

  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                    kFakeQwac, "application/jose");
  ServeResponseHandler nonqwac_page_handler(embedded_https_test_server(),
                                            "/main_noqwac", "", "text/html");
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main")));

  {
    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(WaitForStatusFinished(status));
    EXPECT_EQ(kFakeQwac, status->GetResponseBodyForTesting());
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());
  }

  // Navigate to a different URL with the same page.
  // Since the page didn't change, the qwac status should still be available
  // and not need to be re-fetched.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main#anchor")));
  {
    auto* status = GetCurrentPageQwacStatus();
    ASSERT_TRUE(status);
    ASSERT_TRUE(status->is_finished());
    EXPECT_EQ(kFakeQwac, status->GetResponseBodyForTesting());
    EXPECT_EQ(1, main_page_handler.request_count());
    EXPECT_EQ(1, qwac_handler.request_count());
  }
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       SameOriginRedirectAllowed) {
  const std::string kFakeQwac = "fake qwac data";

  ServePageWithQwacHeaderHandler main_page_handler(
      embedded_https_test_server(), "/main", "/server-redirect?/qwac");
  ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                    kFakeQwac, "application/jose");
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main")));

  auto* status = GetCurrentPageQwacStatus();
  ASSERT_TRUE(status);
  ASSERT_TRUE(WaitForStatusFinished(status));
  EXPECT_EQ(kFakeQwac, status->GetResponseBodyForTesting());
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       LinkHeaderFromMainPageRedirectIgnored) {
  embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
      &ServeRedirectWithQwacHeader, "/main", /*redirect_url=*/"/realmain",
      /*qwac_link_url=*/"/qwac"));
  ServeResponseHandler page_handler(embedded_https_test_server(), "/realmain",
                                    "", "text/html");
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&FailTestIfPathRequested, "/qwac"));
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main"),
      /*expected_commit_url=*/
      embedded_https_test_server().GetURL("/realmain")));

  // The QWAC link header was present on a redirect, but not on the final page
  // load, so no QwacStatus should be created.
  EXPECT_FALSE(GetCurrentPageQwacStatus());
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       CrossOriginRedirectNotAllowed) {
  net::EmbeddedTestServer other_https_test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  other_https_test_server.RegisterRequestHandler(
      base::BindRepeating(&FailTestIfPathRequested, "/qwac"));
  auto other_test_server_handle =
      other_https_test_server.StartAndReturnHandle();
  ASSERT_TRUE(other_test_server_handle.is_valid());

  ServePageWithQwacHeaderHandler main_page_handler(
      embedded_https_test_server(), "/main",
      "/server-redirect?" + other_https_test_server.GetURL("/qwac").spec());
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main")));

  auto* status = GetCurrentPageQwacStatus();
  // The initial QWAC url is same-origin so a fetch request should be started
  // and a QwacStatus created, but the fetch should fail once it hits the
  // redirect to a different origin.
  ASSERT_TRUE(status);
  ASSERT_TRUE(WaitForStatusFinished(status));
  EXPECT_EQ(std::nullopt, status->GetResponseBodyForTesting());
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       CrossOriginFetchNotAllowed) {
  net::EmbeddedTestServer other_https_test_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  other_https_test_server.RegisterRequestHandler(
      base::BindRepeating(&FailTestIfPathRequested, "/qwac"));
  auto other_test_server_handle =
      other_https_test_server.StartAndReturnHandle();
  ASSERT_TRUE(other_test_server_handle.is_valid());

  ServePageWithQwacHeaderHandler main_page_handler(
      embedded_https_test_server(), "/main",
      other_https_test_server.GetURL("/qwac").spec());
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main")));

  // Since the initial QWAC url was cross-origin, the fetch is never started
  // and no QwacStatus should be created.
  EXPECT_FALSE(GetCurrentPageQwacStatus());
  EXPECT_EQ(1, main_page_handler.request_count());
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest,
                       TestQwacLoadHttpError) {
  ServePageWithQwacHeaderHandler main_page_handler(embedded_https_test_server(),
                                                   "/main", "/qwac");
  ServeResponseHandler qwac_handler(embedded_https_test_server(), "/qwac",
                                    "404 content", "text/html",
                                    net::HTTP_NOT_FOUND);
  auto test_server_handle = embedded_https_test_server().StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/main")));

  auto* status = GetCurrentPageQwacStatus();
  ASSERT_TRUE(status);
  ASSERT_TRUE(WaitForStatusFinished(status));
  EXPECT_EQ(std::nullopt, status->GetResponseBodyForTesting());
  EXPECT_EQ(1, qwac_handler.request_count());
}

IN_PROC_BROWSER_TEST_F(QwacWebContentsObserverBrowserTest, NotFetchedForHttp) {
  ServePageWithQwacHeaderHandler main_page_handler(*embedded_test_server(),
                                                   "/main", "/qwac");
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&FailTestIfPathRequested, "/qwac"));
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  ASSERT_TRUE(test_server_handle.is_valid());

  EXPECT_TRUE(content::NavigateToURL(web_contents(),
                                     embedded_test_server()->GetURL("/main")));

  EXPECT_FALSE(GetCurrentPageQwacStatus());
  EXPECT_EQ(1, main_page_handler.request_count());
}

// TODO(crbug.com/392931069): These tests all currently just check that the
// 2-QWAC was fetched or not but don't check the verification. Once 2-QWAC
// verification has been implemented, the tests should be updated to confirm
// the 2-QWAC verification result.
// TODO(crbug.com/392931069): Test that qwac is not fetched after clicking
// through HTTPS error.
// TODO(crbug.com/392931069): Test that qwac requests shows up in netlog?

}  // namespace
