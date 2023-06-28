// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

namespace {

const std::string kTestDictionaryString = "A dictionary";

class SharedDictionaryAccessObserver : public content::WebContentsObserver {
 public:
  SharedDictionaryAccessObserver(content::WebContents* web_contents,
                                 base::RepeatingClosure on_accessed_callback)
      : WebContentsObserver(web_contents),
        on_accessed_callback_(std::move(on_accessed_callback)) {}

  const network::mojom::SharedDictionaryAccessDetailsPtr& details() const {
    return details_;
  }

 private:
  // WebContentsObserver overrides:
  void OnSharedDictionaryAccessed(
      content::NavigationHandle* navigation,
      const network::mojom::SharedDictionaryAccessDetails& details) override {
    details_ = details.Clone();
    on_accessed_callback_.Run();
  }
  void OnSharedDictionaryAccessed(
      content::RenderFrameHost* rfh,
      const network::mojom::SharedDictionaryAccessDetails& details) override {
    details_ = details.Clone();
    on_accessed_callback_.Run();
  }

  base::RepeatingClosure on_accessed_callback_;
  network::mojom::SharedDictionaryAccessDetailsPtr details_;
};

absl::optional<std::string> GetSecAvailableDictionary(
    const net::test_server::HttpRequest::HeaderMap& headers) {
  auto it = headers.find("sec-available-dictionary");
  if (it == headers.end()) {
    return absl::nullopt;
  }
  return it->second;
}

}  // namespace

// `ChromeSharedDictionaryBrowserTest` is required to test Chrome
// specific code such as Site Settings.
// See `SharedDictionaryBrowserTest` for content's version of tests.
class ChromeSharedDictionaryBrowserTest : public InProcessBrowserTest {
 public:
  ChromeSharedDictionaryBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::
                                  kCompressionDictionaryTransportBackend,
                              blink::features::kCompressionDictionaryTransport},
        /*disabled_features=*/{});

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&ChromeSharedDictionaryBrowserTest::RequestHandler,
                            base::Unretained(this)));
    EXPECT_TRUE(embedded_test_server()->Start());

    cross_origin_server_ = std::make_unique<net::EmbeddedTestServer>();
    cross_origin_server()->RegisterRequestHandler(
        base::BindRepeating(&ChromeSharedDictionaryBrowserTest::RequestHandler,
                            base::Unretained(this)));
    CHECK(cross_origin_server()->Start());
  }

  ChromeSharedDictionaryBrowserTest(const ChromeSharedDictionaryBrowserTest&) =
      delete;
  ChromeSharedDictionaryBrowserTest& operator=(
      const ChromeSharedDictionaryBrowserTest&) = delete;

 protected:
  net::EmbeddedTestServer* cross_origin_server() {
    return cross_origin_server_.get();
  }

  std::string FetchStringInActiveContents(const GURL& url) {
    return EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                  content::JsReplace(R"(
          (async () => {
            return await (await fetch($1)).text();
          })();
        )",
                                     url))
        .ExtractString();
  }

  bool TryRegisterDictionary(const net::EmbeddedTestServer& server) {
    base::RunLoop loop;
    SharedDictionaryAccessObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        loop.QuitClosure());

    EXPECT_EQ(kTestDictionaryString,
              FetchStringInActiveContents(server.GetURL("/dictionary")));
    loop.Run();
    CHECK(observer.details());
    EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kWrite,
              observer.details()->type);
    return !observer.details()->is_blocked;
  }

  bool CheckDictionaryHeader(const net::EmbeddedTestServer& server,
                             bool expect_blocked) {
    base::RunLoop loop;
    SharedDictionaryAccessObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        loop.QuitClosure());
    return CheckResultOfDictionaryHeaderAndObserver(
        FetchStringInActiveContents(server.GetURL("/path/check_header")), loop,
        observer, expect_blocked);
  }

  bool CheckDictionaryHeaderByNavigation(const GURL& url, bool expect_blocked) {
    base::RunLoop loop;
    SharedDictionaryAccessObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        loop.QuitClosure());

    CHECK(ui_test_utils::NavigateToURL(browser(), url));

    return CheckResultOfDictionaryHeaderAndObserver(
        EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
               "document.body.innerText")
            .ExtractString(),
        loop, observer, expect_blocked);
  }

  bool CheckResultOfDictionaryHeaderAndObserver(
      const std::string& check_result,
      base::RunLoop& loop,
      SharedDictionaryAccessObserver& observer,
      bool expect_blocked) {
    if (check_result == "Dictionary header available") {
      loop.Run();
      CHECK(observer.details());
      EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
                observer.details()->type);
      EXPECT_FALSE(observer.details()->is_blocked);
      EXPECT_FALSE(expect_blocked);
      return true;
    }
    if (expect_blocked) {
      loop.Run();
      CHECK(observer.details());
      EXPECT_EQ(network::mojom::SharedDictionaryAccessDetails::Type::kRead,
                observer.details()->type);
      EXPECT_TRUE(observer.details()->is_blocked);
    }
    return false;
  }

  void WaitForDictionaryReady(net::EmbeddedTestServer& server) {
    // Need the polling because writing the dictionary to the storage and the
    // database may take time.
    while (!CheckDictionaryHeader(server, /*expect_blocked=*/false)) {
      base::PlatformThread::Sleep(base::Milliseconds(5));
    }
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->AddCustomHeader("access-control-allow-origin", "*");
    if (request.relative_url == "/dictionary") {
      response->set_content_type("text/plain");
      response->AddCustomHeader("use-as-dictionary", "match=\"/path/*\"");
      response->set_content(kTestDictionaryString);
      return response;
    } else if (request.relative_url == "/path/check_header") {
      response->set_content_type("text/plain");
      absl::optional<std::string> dict_hash =
          GetSecAvailableDictionary(request.headers);
      response->set_content(dict_hash ? "Dictionary header available"
                                      : "Dictionary header not available");
      return response;
    } else if (request.relative_url == "/path/check_header1.html" ||
               request.relative_url == "/path/check_header2.html") {
      response->set_content_type("text/html");
      absl::optional<std::string> dict_hash =
          GetSecAvailableDictionary(request.headers);
      response->set_content(dict_hash ? "Dictionary header available"
                                      : "Dictionary header not available");
      return response;
    }
    return nullptr;
  }
  std::unique_ptr<net::EmbeddedTestServer> cross_origin_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest, BlockWriting) {
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(embedded_test_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_FALSE(TryRegisterDictionary(*embedded_test_server()));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       BlockWritingCrossOrigin) {
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(cross_origin_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_FALSE(TryRegisterDictionary(*cross_origin_server()));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest, BlockReading) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(embedded_test_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html?")));
  EXPECT_FALSE(
      CheckDictionaryHeader(*embedded_test_server(), /*expect_blocked=*/true));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       BlockReadingCrossOrigin) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*cross_origin_server()));
  WaitForDictionaryReady(*cross_origin_server());

  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(cross_origin_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html?")));
  EXPECT_FALSE(
      CheckDictionaryHeader(*cross_origin_server(), /*expect_blocked=*/true));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedDictionaryBrowserTest,
                       BlockReadingWhileNavigation) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(TryRegisterDictionary(*embedded_test_server()));
  WaitForDictionaryReady(*embedded_test_server());

  EXPECT_TRUE(CheckDictionaryHeaderByNavigation(
      embedded_test_server()->GetURL("/path/check_header1.html"),
      /*expect_blocked=*/false));

  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(embedded_test_server()->GetURL("/"),
                             CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(CheckDictionaryHeaderByNavigation(
      embedded_test_server()->GetURL("/path/check_header2.html"),
      /*expect_blocked=*/true));
}
