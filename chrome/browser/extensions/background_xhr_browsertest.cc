// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace extensions {

namespace {

constexpr const char kWebstoreDomain[] = "cws.com";

std::unique_ptr<net::ClientCertStore> CreateNullCertStore() {
  return nullptr;
}

}  // namespace

class BackgroundXhrTest : public ExtensionBrowserTest {
 protected:
  void RunTest(const std::string& path, const GURL& url) {
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("background_xhr"));
    ASSERT_TRUE(extension);

    ResultCatcher catcher;
    GURL test_url = net::AppendQueryParameter(extension->GetResourceURL(path),
                                              "url", url.spec());
    ui_test_utils::NavigateToURL(browser(), test_url);
    content::BrowserContext::GetDefaultStoragePartition(profile())
        ->FlushNetworkInterfaceForTesting();
    constexpr char kSendXHRScript[] = R"(
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '%s');
      xhr.send();
      domAutomationController.send('');
    )";
    browsertest_util::ExecuteScriptInBackgroundPage(
        profile(), extension->id(),
        base::StringPrintf(kSendXHRScript, url.spec().c_str()));
    ASSERT_TRUE(catcher.GetNextResult());
  }
};

// Test that fetching a URL using TLS client auth doesn't crash, hang, or
// prompt.
IN_PROC_BROWSER_TEST_F(BackgroundXhrTest, TlsClientAuth) {
  // Install a null ClientCertStore so the client auth prompt isn't bypassed due
  // to the system certificate store returning no certificates.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateNullCertStore));

  // Launch HTTPS server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server.Start());

  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_tls_client_auth.html", https_server.GetURL("/")));
}

// Test that fetching a URL using HTTP auth doesn't crash, hang, or prompt.
IN_PROC_BROWSER_TEST_F(BackgroundXhrTest, HttpAuth) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(RunTest(
      "test_http_auth.html", embedded_test_server()->GetURL("/auth-basic")));
}

class BackgroundXhrWebstoreTest : public ExtensionApiTestWithManagementPolicy {
 public:
  BackgroundXhrWebstoreTest() = default;
  ~BackgroundXhrWebstoreTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // TODO(devlin): For some reason, trying to fetch an HTTPS url in this test
    // fails (even when using an HTTPS EmbeddedTestServer). For this reason, we
    // need to fake the webstore URLs as http versions.
    command_line->AppendSwitchASCII(
        ::switches::kAppsGalleryURL,
        base::StringPrintf("http://%s", kWebstoreDomain));
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::string ExecuteFetch(const Extension* extension, const GURL& url) {
    content::DOMMessageQueue message_queue;
    browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        profile(), extension->id(),
        content::JsReplace("executeFetch($1);", url));
    std::string json;
    EXPECT_TRUE(message_queue.WaitForMessage(&json));
    base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
    std::unique_ptr<base::Value> value = reader.ReadToValueDeprecated(json);
    std::string result;
    EXPECT_TRUE(value->GetAsString(&result));
    std::string trimmed_result;
    base::TrimWhitespaceASCII(result, base::TRIM_ALL, &trimmed_result);
    return trimmed_result;
  }

  const Extension* LoadXhrExtension(const std::string& host) {
    ExtensionTestMessageListener listener("ready", false);
    TestExtensionDir test_dir;
    test_dir.WriteManifest(R"(
    {
      "name": "XHR Test",
      "manifest_version": 2,
      "version": "0.1",
      "background": {"scripts": ["background.js"]},
      "permissions": [")" + host + R"("]
    })");
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

  DISALLOW_COPY_AND_ASSIGN(BackgroundXhrWebstoreTest);
};

// Extensions should not be able to XHR to the webstore.
IN_PROC_BROWSER_TEST_F(BackgroundXhrWebstoreTest, XHRToWebstore) {
  const Extension* extension = LoadXhrExtension("<all_urls>");

  GURL webstore_launch_url = extension_urls::GetWebstoreLaunchURL();
  GURL webstore_url_to_fetch = embedded_test_server()->GetURL(
      webstore_launch_url.host(), "/simple.html");

  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, webstore_url_to_fetch));

  // Sanity check: the extension should be able to fetch google.com.
  GURL google_url =
      embedded_test_server()->GetURL("google.com", "/simple.html");
  EXPECT_THAT(ExecuteFetch(extension, google_url),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
}

// Extensions should not be able to XHR to the webstore regardless of policy.
IN_PROC_BROWSER_TEST_F(BackgroundXhrWebstoreTest, XHRToWebstorePolicy) {
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyAllowedHost(
        "*", "*://" + extension_urls::GetWebstoreLaunchURL().host());
  }

  const Extension* extension = LoadXhrExtension("<all_urls>");

  GURL webstore_launch_url = extension_urls::GetWebstoreLaunchURL();
  GURL webstore_url_to_fetch = embedded_test_server()->GetURL(
      webstore_launch_url.host(), "/simple.html");

  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, webstore_url_to_fetch));

  // Sanity check: the extension should be able to fetch google.com.
  GURL google_url =
      embedded_test_server()->GetURL("google.com", "/simple.html");
  EXPECT_THAT(ExecuteFetch(extension, google_url),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
}

// Extensions should not be able to bypass same-origin despite declaring
// <all_urls> for hosts restricted by enterprise policy.
IN_PROC_BROWSER_TEST_F(BackgroundXhrWebstoreTest, PolicyBlockedXHR) {
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*.example.com");
    pref.AddPolicyAllowedHost("*", "*://public.example.com");
  }

  const Extension* extension = LoadXhrExtension("<all_urls>");

  // Should block due to "runtime_blocked_hosts" section of policy.
  GURL protected_url_to_fetch =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, protected_url_to_fetch));

  // Should allow due to "runtime_allowed_hosts" section of policy.
  GURL exempted_url_to_fetch =
      embedded_test_server()->GetURL("public.example.com", "/simple.html");
  EXPECT_THAT(ExecuteFetch(extension, exempted_url_to_fetch),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
}

// Verify that policy blocklists apply to XHRs done from injected scripts.
IN_PROC_BROWSER_TEST_F(BackgroundXhrWebstoreTest, PolicyContentScriptXHR) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"(
    {
      "name": "XHR Content Script Test",
      "manifest_version": 2,
      "version": "0.1",
      "permissions": ["<all_urls>", "tabs"],
      "background": {"scripts": ["background.js"]}
    })");

  constexpr char kBackgroundScript[] =
      R"(function executeFetch(url) {
           chrome.tabs.executeScript({code: `
             fetch("${url}")
             .then(response => response.text())
             .then(text => domAutomationController.send(text))
             .catch(err => domAutomationController.send('ERROR: ' + err));
           `});
         }
      )";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundScript);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a foo.com page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL page_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), page_url);
  EXPECT_EQ(page_url, web_contents->GetMainFrame()->GetLastCommittedURL());

  // Using "/non-corb.octet-stream" resource (instead of "/simple.html" as in
  // most other tests here) because XHRs/fetches from content scripts are
  // subject to CORB (which is already covered by
  // CrossOriginReadBlockingExtensionTest) and we want to focus the test below
  // on policy behavior (which should be independent from whether or not CORB
  // blocks the response).
  GURL example_url =
      embedded_test_server()->GetURL("example.com", "/non-corb.octet-stream");
  GURL public_example_url = embedded_test_server()->GetURL(
      "public.example.com", "/non-corb.octet-stream");

  // Sanity Check: Should be able to fetch cross origin.
  EXPECT_EQ("octet-stream-body", ExecuteFetch(extension, example_url));
  EXPECT_EQ("octet-stream-body", ExecuteFetch(extension, public_example_url));

  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*.example.com");
    pref.AddPolicyAllowedHost("*", "*://public.example.com");
  }

  // Policies apply to XHR from a content script.
  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, example_url));
  EXPECT_EQ("octet-stream-body", ExecuteFetch(extension, public_example_url));
}

// Make sure the blocklist and allowlist update for both Default and Individual
// scope policies. Testing with all host permissions granted (<all_urls>).
IN_PROC_BROWSER_TEST_F(BackgroundXhrWebstoreTest, PolicyUpdateXHR) {
  const Extension* extension = LoadXhrExtension("<all_urls>");

  GURL example_url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  GURL public_example_url =
      embedded_test_server()->GetURL("public.example.com", "/simple.html");

  // Sanity check: Without restrictions all fetches should work.
  EXPECT_THAT(ExecuteFetch(extension, public_example_url),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
  EXPECT_THAT(ExecuteFetch(extension, example_url),
              ::testing::HasSubstr("<head><title>OK</title></head>"));

  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*.example.com");
    pref.AddPolicyAllowedHost("*", "*://public.example.com");
  }

  // Default policies propagate.
  EXPECT_THAT(ExecuteFetch(extension, public_example_url),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, example_url));
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost(extension->id(), "*://*.example2.com");
    pref.AddPolicyAllowedHost(extension->id(), "*://public.example2.com");
  }

  // Default policies overridden when individual scope policies applied.
  EXPECT_THAT(ExecuteFetch(extension, public_example_url),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
  EXPECT_THAT(ExecuteFetch(extension, example_url),
              ::testing::HasSubstr("<head><title>OK</title></head>"));

  GURL example2_url =
      embedded_test_server()->GetURL("example2.com", "/simple.html");
  GURL public_example2_url =
      embedded_test_server()->GetURL("public.example2.com", "/simple.html");

  // Individual scope policies propagate.
  EXPECT_THAT(ExecuteFetch(extension, public_example2_url),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, example2_url));
}

// Make sure the allowlist entries added due to host permissions are removed
// when a more generic blocklist policy is updated and contains them.
// This tests the default policy scope update.
IN_PROC_BROWSER_TEST_F(BackgroundXhrWebstoreTest, PolicyUpdateDefaultXHR) {
  const Extension* extension = LoadXhrExtension("*://public.example.com/*");

  GURL example_url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  GURL public_example_url =
      embedded_test_server()->GetURL("public.example.com", "/simple.html");

  // Sanity check: Without restrictions only public.example.com should work.
  EXPECT_THAT(ExecuteFetch(extension, public_example_url),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, example_url));

  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*.example.com");
  }

  // The blocklist of example.com overrides allowlist of public.example.com.
  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, example_url));
  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, public_example_url));
}

// Make sure the allowlist entries added due to host permissions are removed
// when a more generic blocklist policy is updated and contains them.
// This tests an individual policy scope update.
IN_PROC_BROWSER_TEST_F(BackgroundXhrWebstoreTest, PolicyUpdateIndividualXHR) {
  const Extension* extension = LoadXhrExtension("*://public.example.com/*");

  GURL example_url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  GURL public_example_url =
      embedded_test_server()->GetURL("public.example.com", "/simple.html");

  // Sanity check: Without restrictions only public.example.com should work.
  EXPECT_THAT(ExecuteFetch(extension, public_example_url),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, example_url));

  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost(extension->id(), "*://*.example.com");
  }

  // The blocklist of example.com overrides allowlist of public.example.com.
  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, example_url));
  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, public_example_url));
}

IN_PROC_BROWSER_TEST_F(BackgroundXhrWebstoreTest, XHRAnyPortPermission) {
  const Extension* extension = LoadXhrExtension("http://example.com:*/*");

  GURL permitted_url_to_fetch =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  EXPECT_THAT(ExecuteFetch(extension, permitted_url_to_fetch),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
}

IN_PROC_BROWSER_TEST_F(BackgroundXhrWebstoreTest,
                       XHRPortSpecificPermissionAllow) {
  const Extension* extension = LoadXhrExtension(
      "http://example.com:" +
      base::NumberToString(embedded_test_server()->port()) + "/*");

  GURL permitted_url_to_fetch =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  EXPECT_THAT(ExecuteFetch(extension, permitted_url_to_fetch),
              ::testing::HasSubstr("<head><title>OK</title></head>"));
}

IN_PROC_BROWSER_TEST_F(BackgroundXhrWebstoreTest,
                       XHRPortSpecificPermissionBlock) {
  const Extension* extension = LoadXhrExtension(
      "http://example.com:" +
      base::NumberToString(embedded_test_server()->port() + 1) + "/*");

  GURL not_permitted_url_to_fetch =
      embedded_test_server()->GetURL("example.com", "/simple.html");

  EXPECT_EQ("ERROR: TypeError: Failed to fetch",
            ExecuteFetch(extension, not_permitted_url_to_fetch));
}

}  // namespace extensions
