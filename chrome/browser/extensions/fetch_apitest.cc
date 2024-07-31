// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"

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

  // Opens a tab, puts it in the foreground, navigates it to |url| then returns
  // its WebContents.
  content::WebContents* CreateAndNavigateTab(const GURL& url) {
    NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    ui_test_utils::NavigateToURL(&params);
    return browser()->tab_strip_model()->GetActiveWebContents();
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

  content::WebContents* empty_tab = CreateAndNavigateTab(
      embedded_test_server()->GetURL("example.com", "/empty.html"));

  // TODO(kalman): Test this from a content script too.
  EXPECT_EQ(
      "text content",
      content::EvalJs(empty_tab, GetDOMFetchScript(GetQuotedURL(
                                     extension->GetResourceURL("text")))));
}

// Calling fetch() from a http(s) service worker context to a
// chrome-extensions:// URL since the loading path in a service worker is
// different from pages.
// This is a regression test for https://crbug.com/901443.
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

  content::WebContents* tab =
      CreateAndNavigateTab(embedded_test_server()->GetURL(
          "/workers/fetch_from_service_worker.html"));
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

  content::WebContents* empty_tab = CreateAndNavigateTab(
      embedded_test_server()->GetURL("example.com", "/empty.html"));

  // TODO(kalman): Test this from a content script too.
  EXPECT_EQ(
      "TypeError: Failed to fetch",
      content::EvalJs(empty_tab, GetDOMFetchScript(GetQuotedURL(
                                     extension->GetResourceURL("text")))));
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

}  // namespace

}  // namespace extensions
