// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

namespace {

// JavaScript snippet which performs a fetch given a URL expression to be
// substituted as %s, then sends back the fetched content using the
// domAutomationController.
const char* kFetchScript =
    "fetch(%s).then(function(result) {\n"
    "  return result.text();\n"
    "}).then(function(text) {\n"
    "  window.domAutomationController.send(text);\n"
    "}).catch(function(err) {\n"
    "  window.domAutomationController.send(String(err));\n"
    "});\n";

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

 private:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionFetchTest, ExtensionCanFetchExtensionResource) {
  TestExtensionDir dir;
  dir.WriteManifestWithSingleQuotes(
      "{"
      "'background': {'scripts': ['bg.js']},"
      "'manifest_version': 2,"
      "'name': 'ExtensionCanFetchExtensionResource',"
      "'version': '1'"
      "}");
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
  dir.WriteManifestWithSingleQuotes(
      "{"
      "'background': {'scripts': ['bg.js']},"
      "'manifest_version': 2,"
      "'name': 'ExtensionCanFetchHostedResourceWithHostPermissions',"
      "'permissions': ['http://example.com/*'],"
      "'version': '1'"
      "}");
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
  dir.WriteManifestWithSingleQuotes(
      "{"
      "'background': {'scripts': ['bg.js']},"
      "'manifest_version': 2,"
      "'name': 'ExtensionCannotFetchHostedResourceWithoutHostPermissions',"
      "'version': '1'"
      "}");
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
  dir.WriteManifestWithSingleQuotes(
      "{"
      "'background': {'scripts': ['bg.js']},"
      "'manifest_version': 2,"
      "'name': 'HostCanFetchWebAccessibleExtensionResource',"
      "'version': '1',"
      "'web_accessible_resources': ['text']"
      "}");
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  content::WebContents* empty_tab = CreateAndNavigateTab(
      embedded_test_server()->GetURL("example.com", "/empty.html"));

  // TODO(kalman): Test this from a content script too.
  std::string fetch_result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      empty_tab,
      GetFetchScript(GetQuotedURL(extension->GetResourceURL("text"))),
      &fetch_result));
  EXPECT_EQ("text content", fetch_result);
}

// Calling fetch() from a http(s) service worker context to a
// chrome-extensions:// URL since the loading path in a service worker is
// different from pages.
// This is a regression test for https://crbug.com/901443.
IN_PROC_BROWSER_TEST_F(
    ExtensionFetchTest,
    HostCanFetchWebAccessibleExtensionResource_FetchFromServiceWorker) {
  TestExtensionDir dir;
  dir.WriteManifestWithSingleQuotes(
      "{"
      "'background': {'scripts': ['bg.js']},"
      "'manifest_version': 2,"
      "'name': 'HostCanFetchWebAccessibleExtensionResource_"
      "FetchFromServiceWorker',"
      "'version': '1',"
      "'web_accessible_resources': ['text']"
      "}");
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
  dir.WriteManifestWithSingleQuotes(
      "{"
      "'background': {'scripts': ['bg.js']},"
      "'manifest_version': 2,"
      "'name': 'HostCannotFetchNonWebAccessibleExtensionResource',"
      "'version': '1'"
      "}");
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  content::WebContents* empty_tab = CreateAndNavigateTab(
      embedded_test_server()->GetURL("example.com", "/empty.html"));

  // TODO(kalman): Test this from a content script too.
  std::string fetch_result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      empty_tab,
      GetFetchScript(GetQuotedURL(extension->GetResourceURL("text"))),
      &fetch_result));
  EXPECT_EQ("TypeError: Failed to fetch", fetch_result);
}

IN_PROC_BROWSER_TEST_F(ExtensionFetchTest, FetchResponseType) {
  const std::string script = base::StringPrintf(
      "fetch(%s).then(function(response) {\n"
      "  window.domAutomationController.send(response.type);\n"
      "}).catch(function(err) {\n"
      "  window.domAutomationController.send(String(err));\n"
      "});\n",
      GetQuotedTestServerURL("example.com", "/extensions/test_file.txt")
          .data());
  TestExtensionDir dir;
  dir.WriteManifestWithSingleQuotes(
      "{"
      "'background': {'scripts': ['bg.js']},"
      "'manifest_version': 2,"
      "'name': 'FetchResponseType',"
      "'permissions': ['http://example.com/*'],"
      "'version': '1'"
      "}");
  const Extension* extension = WriteFilesAndLoadTestExtension(&dir);
  ASSERT_TRUE(extension);

  EXPECT_EQ("basic", ExecuteScriptInBackgroundPage(extension->id(), script));
}

}  // namespace

}  // namespace extensions
