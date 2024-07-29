// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

namespace extensions {

class BackgroundHeaderTest : public ExtensionBrowserTest {
 public:
  BackgroundHeaderTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  BackgroundHeaderTest(const BackgroundHeaderTest& other) = delete;
  BackgroundHeaderTest& operator=(const BackgroundHeaderTest& other) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  GURL GetSecFetchUrl(const std::string& hostname) {
    if (hostname.empty()) {
      return https_test_server_.GetURL("/echoheader?sec-fetch-site");
    }
    return https_test_server_.GetURL(hostname, "/echoheader?sec-fetch-site");
  }

  const base::FilePath GetTestDataFilePath() {
    return base::FilePath(FILE_PATH_LITERAL("chrome/test/data"));
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_test_server_.AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_test_server_.Start());
  }

  std::string ExecuteFetch(const Extension* extension, const GURL& url) {
    ExtensionHost* host =
        ProcessManager::Get(profile())->GetBackgroundHostForExtension(
            extension->id());
    if (!host) {
      ADD_FAILURE() << "No background page found.";
      return "";
    }
    content::DOMMessageQueue message_queue(host->host_contents());

    ExecuteScriptInBackgroundPageNoWait(
        extension->id(), content::JsReplace("executeFetch($1);", url));
    std::string json;
    EXPECT_TRUE(message_queue.WaitForMessage(&json));
    std::optional<base::Value> value =
        base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS);
    if (!value) {
      ADD_FAILURE() << "Received invalid response: " << json;
      return std::string();
    }
    EXPECT_TRUE(value->is_string());
    std::string trimmed_result;
    base::TrimWhitespaceASCII(value->GetString(), base::TRIM_ALL,
                              &trimmed_result);
    return trimmed_result;
  }

  const Extension* LoadFetchExtension(const std::string& host) {
    ExtensionTestMessageListener listener("ready");
    TestExtensionDir test_dir;
    constexpr char kManifestTemplate[] = R"(
    {
      "name": "XHR Test",
      "manifest_version": 2,
      "version": "0.1",
      "background": {"scripts": ["background.js"]},
      "permissions": ["%s"]
    })";
    test_dir.WriteManifest(base::StringPrintf(kManifestTemplate, host.c_str()));
    constexpr char kBackgroundScriptFile[] = R"(
    function executeFetch(url) {
      console.warn('Fetching: ' + url);
      fetch(url)
          .then(response => response.text())
          .then(text => domAutomationController.send(text))
          .catch(err => domAutomationController.send('ERROR: ' + err));
    }
    chrome.test.sendMessage('ready');)";

    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                       kBackgroundScriptFile);
    const Extension* extension = LoadExtension(test_dir.UnpackedPath());
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    return extension;
  }

 private:
  net::EmbeddedTestServer https_test_server_;
};

// Test the response headers of fetch a HTTPS request in extension background
// page.
IN_PROC_BROWSER_TEST_F(BackgroundHeaderTest, SecFetchSite) {
  const Extension* extension = LoadFetchExtension("<all_urls>");
  ASSERT_TRUE(extension);

  EXPECT_EQ("none", ExecuteFetch(extension, GetSecFetchUrl("example.com")));
}

// Test the response headers of fetch a HTTPS request with non-privileged host
// in extension background page.
IN_PROC_BROWSER_TEST_F(BackgroundHeaderTest,
                       SecFetchSiteFromPermissionBlockedHost) {
  const Extension* extension = LoadFetchExtension("*://example.com:*/*");
  ASSERT_TRUE(extension);

  EXPECT_EQ("cross-site",
            ExecuteFetch(extension, GetSecFetchUrl("example2.com")));
}

}  // namespace extensions
