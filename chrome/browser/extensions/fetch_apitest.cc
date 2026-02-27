// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/user_scripts_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"

namespace extensions {

namespace {

// Returns a response whose body is request's origin.
std::unique_ptr<net::test_server::HttpResponse> HandleEchoOrigin(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/echo-origin") {
    return nullptr;
  }

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/plain");
  auto it = request.headers.find("origin");
  if (it != request.headers.end()) {
    response->set_content(it->second);
  } else {
    response->set_content("<no origin attached>");
  }
  response->AddCustomHeader("access-control-allow-origin", "*");

  return response;
}

// JavaScript snippet which performs a fetch given a URL expression to be
// substituted as %s, then sends back the fetched content using
// chrome.test.sendScriptResult.
constexpr char kFetchScript[] = R"(
  fetch(%s).then(function(result) {
    return result.text();
  }).then(function(text) {
    chrome.test.sendScriptResult(text);
  }).catch(function(err) {
    chrome.test.sendScriptResult(String(err));
  });
)";

// JavaScript snippet which performs a fetch given a URL expression to be
// substituted as %s.
constexpr char kDOMFetchScript[] = R"(
  fetch(%s).then(function(result) {
    return result.text();
  }).catch(function(err) {
    return String(err);
  });
)";

constexpr char kFetchPostScript[] = R"(
  fetch($1, {method: 'POST'}).then((result) => {
    return result.text();
  }).then((text) => {
    window.domAutomationController.send(text);
  }).catch((error) => {
    window.domAutomationController.send(String(err));
  });
)";

class ExtensionFetchTest : public ExtensionApiTest {
 protected:
  // Writes an empty background page and a text file called "text" with content
  // "text content", then loads and returns the extension. |dir| must already
  // have a manifest.
  const Extension* WriteFilesAndLoadTestExtension(TestExtensionDir* dir) {
    dir->WriteFile(FILE_PATH_LITERAL("text"), "text content");
    dir->WriteFile(FILE_PATH_LITERAL("bg.js"), "");
    return LoadExtension(dir->UnpackedPath());
  }

  // Returns |kFetchScript| with |url_expression| substituted as its test URL.
  std::string GetFetchScript(const std::string& url_expression) {
    return base::StringPrintf(kFetchScript, url_expression.c_str());
  }

  // Returns |kDOMFetchScript| with |url_expression| substituted as its test
  // URL.
  std::string GetDOMFetchScript(const std::string& url_expression) {
    return base::StringPrintf(kDOMFetchScript, url_expression.c_str());
  }

  // Returns |url| as a string surrounded by single quotes, for passing to
  // JavaScript as a string literal.
  std::string GetQuotedURL(const GURL& url) {
    return base::StringPrintf("'%s'", url.spec().c_str());
  }

  // Like GetQuotedURL(), but fetching the URL from the test server's |host|
  // and |path|.
  std::string GetQuotedTestServerURL(const std::string& host,
                                     const std::string& path) {
    return GetQuotedURL(embedded_test_server()->GetURL(host, path));
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(HandleEchoOrigin));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionFetchTest, ExtensionCanFetchExtensionResource) {
  TestExtensionDir dir;
  static constexpr char kManifest[] =
      R"({
           "background": {"scripts": ["bg.js"]},
           "manifest_version": 2,
           "name": "ExtensionCanFetchExtensionResource",
           "version": "1"
         })";
  dir.WriteManifest(kManifest);
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  EXPECT_EQ(
      "text content",
      ExecuteScriptInBackgroundPage(
          extension->id(), GetFetchScript("chrome.runtime.getURL('text')")));
}

IN_PROC_BROWSER_TEST_F(ExtensionFetchTest,
                       ExtensionCanFetchHostedResourceWithHostPermissions) {
  TestExtensionDir dir;
  static constexpr char kManifest[] =
      R"({
           "background": {"scripts": ["bg.js"]},
           "manifest_version": 2,
           "name": "ExtensionCanFetchHostedResourceWithHostPermissions",
           "permissions": ["http://example.com/*"],
           "version": "1"
         })";
  dir.WriteManifest(kManifest);
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  EXPECT_EQ("Hello!", ExecuteScriptInBackgroundPage(
                          extension->id(),
                          GetFetchScript(GetQuotedTestServerURL(
                              "example.com", "/extensions/test_file.txt"))));
}

IN_PROC_BROWSER_TEST_F(
    ExtensionFetchTest,
    ExtensionCannotFetchHostedResourceWithoutHostPermissions) {
  TestExtensionDir dir;
  static constexpr char kManifest[] =
      R"({
           "background": {"scripts": ["bg.js"]},
           "manifest_version": 2,
           "name": "ExtensionCannotFetchHostedResourceWithoutHostPermissions",
           "version": "1"
         })";
  dir.WriteManifest(kManifest);
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  // TODO(kalman): Another test would be to configure the test server to work
  // with CORS, and test that the fetch succeeds.
  EXPECT_EQ(
      "TypeError: Failed to fetch",
      ExecuteScriptInBackgroundPage(
          extension->id(), GetFetchScript(GetQuotedTestServerURL(
                               "example.com", "/extensions/test_file.txt"))));
}

IN_PROC_BROWSER_TEST_F(ExtensionFetchTest,
                       HostCanFetchWebAccessibleExtensionResource) {
  TestExtensionDir dir;
  static constexpr char kManifest[] =
      R"({
           "background": {"scripts": ["bg.js"]},
           "manifest_version": 2,
           "name": "HostCanFetchWebAccessibleExtensionResource",
           "version": "1",
           "web_accessible_resources": ["text"]
         })";
  dir.WriteManifest(kManifest);
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  NavigateToURLInNewTab(
      embedded_test_server()->GetURL("example.com", "/empty.html"));

  // TODO(kalman): Test this from a content script too.
  EXPECT_EQ(
      "text content",
      content::EvalJs(
          GetActiveWebContents(),
          GetDOMFetchScript(GetQuotedURL(extension->GetResourceURL("text")))));
}

// Calling fetch() from a http(s) service worker context to a
// chrome-extensions:// URL since the loading path in a service worker is
// different from pages.
// This is a regression test for https://crbug.com/40600798.
IN_PROC_BROWSER_TEST_F(
    ExtensionFetchTest,
    HostCanFetchWebAccessibleExtensionResource_FetchFromServiceWorker) {
  TestExtensionDir dir;
  static constexpr char kManifest[] =
      R"({
           "background": {"scripts": ["bg.js"]},
           "manifest_version": 2,
           "name": "FetchFromServiceWorker",
           "version": "1",
           "web_accessible_resources": ["text"]
         })";
  dir.WriteManifest(kManifest);
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  NavigateToURLInNewTab(embedded_test_server()->GetURL(
      "/workers/fetch_from_service_worker.html"));
  auto* tab = GetActiveWebContents();
  EXPECT_EQ("ready", content::EvalJs(tab, "setup();"));
  EXPECT_EQ("text content",
            content::EvalJs(
                tab, base::StringPrintf(
                         "fetch_from_service_worker('%s');",
                         extension->GetResourceURL("text").spec().c_str())));
}

IN_PROC_BROWSER_TEST_F(ExtensionFetchTest,
                       HostCannotFetchNonWebAccessibleExtensionResource) {
  TestExtensionDir dir;
  static constexpr char kManifest[] =
      R"({
           "background": {"scripts": ["bg.js"]},
           "manifest_version": 2,
           "name": "HostCannotFetchNonWebAccessibleExtensionResource",
           "version": "1"
         })";
  dir.WriteManifest(kManifest);
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  NavigateToURLInNewTab(
      embedded_test_server()->GetURL("example.com", "/empty.html"));

  // TODO(kalman): Test this from a content script too.
  EXPECT_EQ(
      "TypeError: Failed to fetch",
      content::EvalJs(
          GetActiveWebContents(),
          GetDOMFetchScript(GetQuotedURL(extension->GetResourceURL("text")))));
}

IN_PROC_BROWSER_TEST_F(ExtensionFetchTest, FetchResponseType) {
  const std::string script = base::StringPrintf(
      R"(fetch(%s).then((response) => {
           chrome.test.sendScriptResult(response.type);
         }).catch((err) => {
           chrome.test.sendScriptResult(String(err));
         });)",
      GetQuotedTestServerURL("example.com", "/extensions/test_file.txt")
          .data());
  TestExtensionDir dir;
  static constexpr char kManifest[] =
      R"({
           "background": {"scripts": ["bg.js"]},
           "manifest_version": 2,
           "name": "FetchResponseType",
           "permissions": ["http://example.com/*"],
           "version": "1"
         })";
  dir.WriteManifest(kManifest);
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  EXPECT_EQ("basic", ExecuteScriptInBackgroundPage(extension->id(), script));
}

IN_PROC_BROWSER_TEST_F(ExtensionFetchTest, OriginOnPostWithPermissions) {
  TestExtensionDir dir;
  dir.WriteManifest(R"JSON(
     {
      "background": {"scripts": ["bg.js"]},
      "manifest_version": 2,
      "name": "FetchResponseType",
      "permissions": ["http://example.com/*"],
      "version": "1"
     })JSON");
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  GURL destination_url =
      embedded_test_server()->GetURL("example.com", "/echo-origin");
  std::string script = content::JsReplace(kFetchPostScript, destination_url);
  std::string origin_string = url::Origin::Create(extension->url()).Serialize();
  EXPECT_EQ(origin_string,
            ExecuteScriptInBackgroundPageDeprecated(extension->id(), script));
}

IN_PROC_BROWSER_TEST_F(ExtensionFetchTest, OriginOnPostWithoutPermissions) {
  TestExtensionDir dir;
  dir.WriteManifest(R"JSON(
     {
      "background": {"scripts": ["bg.js"]},
      "manifest_version": 2,
      "name": "FetchResponseType",
      "permissions": [],
      "version": "1"
     })JSON");
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  const std::string script = content::JsReplace(
      kFetchPostScript,
      embedded_test_server()->GetURL("example.com", "/echo-origin"));
  EXPECT_EQ(url::Origin::Create(extension->url()).Serialize(),
            ExecuteScriptInBackgroundPageDeprecated(extension->id(), script));
}

// An extension background script should be able to fetch resources contained in
// the extension, and those resources should not be opaque.
IN_PROC_BROWSER_TEST_F(ExtensionFetchTest, ExtensionResourceShouldNotBeOpaque) {
  // We use a script to test this feature. Ideally testing with fetch() and
  // response type is better, but some logic in blink (see the manual
  // response type handling in blink::FetchManager) would hide potential
  // breakages, which is why we are using a script.
  const std::string script = base::StringPrintf(R"(
      const script = document.createElement('script');
      window.onerror = (message) => {
        chrome.test.sendScriptResult('onerror: ' + message);
      }
      script.src = 'error.js'
      document.body.appendChild(script);)");
  TestExtensionDir dir;
  dir.WriteManifest(R"JSON(
     {
      "background": {"scripts": ["bg.js"]},
      "manifest_version": 2,
      "name": "FetchResponseType",
      "permissions": [],
      "version": "1"
     })JSON");
  dir.WriteFile(FILE_PATH_LITERAL("error.js"), "throw TypeError('hi!')");
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  // We expect that we can read the content of the error here. Otherwise
  // "onerror: Script error." will be seen.
  EXPECT_EQ("onerror: Uncaught TypeError: hi!",
            ExecuteScriptInBackgroundPage(extension->id(), script));
}

class ExtensionFetchHeadersTest : public ExtensionApiTest,
                                  public base::test::WithFeatureOverride {
 public:
  ExtensionFetchHeadersTest()
      : WithFeatureOverride(
            blink::features::kBypassRequestForbiddenHeadersCheck) {}

  ExtensionFetchHeadersTest(const ExtensionFetchHeadersTest&) = delete;
  ExtensionFetchHeadersTest& operator=(const ExtensionFetchHeadersTest&) =
      delete;
  ~ExtensionFetchHeadersTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    ASSERT_TRUE(InitializeEmbeddedTestServer());
    // All requests to embedded_test_server() will be passed to RecordRequest().
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &ExtensionFetchHeadersTest::RecordRequest, base::Unretained(this)));

    // Serve embedded_test_server() requests from a specific directory.
    base::FilePath http_server_root_path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &http_server_root_path);
    http_server_root_path = http_server_root_path.AppendASCII(
        "extensions/api_test/fetch/fetch_headers/fetch_html/");
    embedded_test_server()->ServeFilesFromDirectory(http_server_root_path);
    EmbeddedTestServerAcceptConnections();
  }

  // Records requests that are sent to embedded_test_server() during the test.
  void RecordRequest(const net::test_server::HttpRequest& request) {
    base::AutoLock lock(requests_to_server_lock_);
    requests_to_server_[request.GetURL()] = request;
    if (url_to_wait_for_ != request.GetURL()) {
      return;
    }
    ASSERT_TRUE(wait_for_request_run_loop_);
    url_to_wait_for_ = GURL();
    wait_for_request_run_loop_->Quit();
  }

  // Waits for `url_to_wait_for` request to be seen by the test and then
  // confirms that the value of header with `header_name` matches
  // `expected_header_value`.
  bool WaitForRequestAndCheckHeaderValue(
      const GURL& url_to_wait_for,
      const char* header_name,
      const std::string& expected_header_value) {
    {
      SCOPED_TRACE(
          base::StringPrintf("waiting for url request: %s to be captured",
                             url_to_wait_for.spec()));
      WaitForRequest(url_to_wait_for);
    }

    std::string header_value =
        GetHeaderValueFromRequest(url_to_wait_for, header_name);
    if (expected_header_value == header_value) {
      return true;
    }
    ADD_FAILURE() << "header name: " << header_name
                  << " for request: " << url_to_wait_for.spec()
                  << " had value: " << header_value
                  << " instead of expected value: " << expected_header_value;
    return false;
  }

  // Waits for `url_to_wait_for` to be requested from the embedded_test_server()
  // during the test.
  void WaitForRequest(const GURL& url_to_wait_for) {
    {
      base::AutoLock lock(requests_to_server_lock_);

      DCHECK(url_to_wait_for_.is_empty());
      DCHECK(!wait_for_request_run_loop_);

      if (requests_to_server_.count(url_to_wait_for)) {
        return;
      }
      url_to_wait_for_ = url_to_wait_for;
      wait_for_request_run_loop_ = std::make_unique<base::RunLoop>();
    }

    wait_for_request_run_loop_->Run();
    wait_for_request_run_loop_.reset();
  }

  // Gets the request headers for `url_request` that was seen during the test.
  // If the request wasn't recorded, or the header isn't present on the request
  // then return an empty string.
  std::string GetHeaderValueFromRequest(const GURL& url_request,
                                        const char* header_name) {
    base::AutoLock lock(requests_to_server_lock_);
    const auto url_request_search = requests_to_server_.find(url_request);
    if (url_request_search == requests_to_server_.end()) {
      ADD_FAILURE() << "url_request: " << url_request.spec()
                    << " wasn't seen during the test";
      return "";
    }
    const auto headers_for_request = url_request_search->second.headers;
    auto header = headers_for_request.find(header_name);
    if (header == headers_for_request.end()) {
      ADD_FAILURE() << "header_name: " << header_name
                    << " wasn't set on the request during the test";
      return "";
    }
    return header->second;
  }

 private:
  // Requests observed by the EmbeddedTestServer. This is accessed on both the
  // UI and the EmbeddedTestServer's IO thread. Access is protected by
  // `requests_to_server_lock_`.
  std::map<GURL, net::test_server::HttpRequest> requests_to_server_
      GUARDED_BY(requests_to_server_lock_);
  // URL that `wait_for_request_run_loop_` is currently waiting to observe.
  GURL url_to_wait_for_ GUARDED_BY(requests_to_server_lock_);
  // RunLoop to quit when a request for `url_to_wait_for_` is observed.
  std::unique_ptr<base::RunLoop> wait_for_request_run_loop_;
  base::Lock requests_to_server_lock_;
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/418811955): The SetFetchHeaders* tests are confirming that the
// renderer can set forbidden headers, but they don't confirm that the browser
// will actually send the forbidden headers outbound on the wire. Let's create
// test cases for that when the browser side component is completed.

// Tests the behavior of a privileged (background) context when it
// attempts to set forbidden and non-forbidden headers on fetch() requests to a
// URL for which the extension has host_permissions.
IN_PROC_BROWSER_TEST_P(ExtensionFetchHeadersTest,
                       SetFetchHeadersFromExtensionBackground) {
  SetCustomArg("run_background_tests");
  // Run fetch() header setting tests from the (privileged) background context.
  ASSERT_TRUE(
      RunExtensionTest("fetch/fetch_headers/set_headers_test_extension"));

  // Confirm that headers that are not forbidden are allowed to be set on a
  // fetch() request by an extension background script.
  EXPECT_TRUE(WaitForRequestAndCheckHeaderValue(
      embedded_test_server()->GetURL("/fetch_allowed.html"),
      /*header_name=*/"Content-Type",
      /*expected_header_value=*/"text/testing"));
  // Confirm that headers that are forbidden are not allowed to be set on a
  // fetch() request by an extension background script (they're overridden).
  EXPECT_TRUE(WaitForRequestAndCheckHeaderValue(
      embedded_test_server()->GetURL("/fetch_forbidden.html"),
      /*header_name=*/"Accept-Encoding",
      /*expected_header_value=*/
      GetParam() ? "fakeencoding, fakeencoding2" : "gzip, deflate, br, zstd"));
}

// Tests the behavior of a privileged (extension resource) context when it
// attempts to set forbidden and non-forbidden headers on fetch() requests to a
// URL for which the extension has host_permissions.
IN_PROC_BROWSER_TEST_P(ExtensionFetchHeadersTest,
                       SetFetchHeadersFromExtensionResource) {
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "fetch/fetch_headers/set_headers_test_extension"));
  ASSERT_TRUE(extension);

  // Opening extension popup causes popup script to run the fetch() header
  // setting tests.
  {
    SCOPED_TRACE("waiting for extension popup to open");
    // Open popup and test allowed and forbidden header setting.
    extensions::ExtensionHostTestHelper popup_waiter(profile(),
                                                     extension->id());
    popup_waiter.RestrictToType(extensions::mojom::ViewType::kExtensionPopup);
    ExtensionActionTestHelper::Create(browser())->Press(extension->id());
    popup_waiter.WaitForHostCompletedFirstLoad();
  }

  // Confirm that headers that are not forbidden are allowed to be set on a
  // fetch() request by an extension resource (popup) script.
  EXPECT_TRUE(WaitForRequestAndCheckHeaderValue(
      embedded_test_server()->GetURL("/fetch_allowed.html"),
      /*header_name=*/"Content-Type",
      /*expected_header_value=*/"text/testing"));
  // Confirm that headers that are forbidden are not allowed to be set on a
  // fetch() request by an extension resource (popup) (they're overridden).
  EXPECT_TRUE(WaitForRequestAndCheckHeaderValue(
      embedded_test_server()->GetURL("/fetch_forbidden.html"),
      /*header_name=*/"Accept-Encoding",
      /*expected_header_value=*/
      GetParam() ? "fakeencoding, fakeencoding2" : "gzip, deflate, br, zstd"));
}

// Tests the behavior of an unprivileged (content script) context when it
// attempts to set forbidden and non-forbidden headers on fetch() requests to a
// URL for which the extension has host_permissions.
IN_PROC_BROWSER_TEST_P(ExtensionFetchHeadersTest,
                       SetFetchHeadersFromExtensionContentScript) {
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "fetch/fetch_headers/set_headers_test_extension"));
  ASSERT_TRUE(extension);

  // Navigating to URL causes content script to run the fetch() header setting
  // tests.
  {
    SCOPED_TRACE(
        "waiting for page to load and content script to finish running");
    content::WebContents* web_contents = GetActiveWebContents();
    ResultCatcher content_script_catcher;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("/fetch_from_content_script.html")));
    ASSERT_TRUE(content::WaitForLoadStop(web_contents));
    EXPECT_TRUE(content_script_catcher.GetNextResult())
        << content_script_catcher.message();
  }

  // Confirm that headers that are not forbidden are allowed to be set on a
  // fetch() request by a content script.
  EXPECT_TRUE(WaitForRequestAndCheckHeaderValue(
      embedded_test_server()->GetURL("/fetch_allowed.html"),
      /*header_name=*/"Content-Type",
      /*expected_header_value=*/"text/testing"));
  // Confirm that headers that are forbidden are not allowed to be set on a
  // fetch() request by a content script since it's not a privileged extension
  // context (they're overridden).
  EXPECT_TRUE(WaitForRequestAndCheckHeaderValue(
      embedded_test_server()->GetURL("/fetch_forbidden.html"),
      /*header_name=*/"Accept-Encoding",
      /*expected_header_value=*/"gzip, deflate, br, zstd"));
}

// Toggle `blink::features::kBypassRequestForbiddenHeadersCheck`.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(ExtensionFetchHeadersTest);

}  // namespace

}  // namespace extensions
