// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::NetworkChangeNotifier;
using prefetch::PreloadPagesState;

namespace {

const char kPrefetchPage[] = "/prerender/simple_prefetch.html";
const char kRedirectPrefetchPage[] = "/redirect_prefetch.html";
const char kRedirectPrefetchUrl[] = "/redirect";
const char kRedirectedPrefetchUrl[] = "/redirected";
const char kPrefetchCachingPeriodPage[] = "/prefetch_caching_period.html";
const char kPrefetchCachingPeriodUrl[] = "/prefetch_caching_period";

bool HasVariationsHeader(
    const net::test_server::HttpRequest::HeaderMap& headers) {
  return headers.find("X-Client-Data") != headers.end();
}

class MockNetworkChangeNotifierWIFI : public NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_WIFI;
  }
};

class MockNetworkChangeNotifier4G : public NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_4G;
  }
};

class PrefetchBrowserTest : public InProcessBrowserTest {
 public:
  PrefetchBrowserTest() {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set a dummy variation ID to send X-Client-Data header to Google hosts
    // in RedirectedPrefetch test.
    command_line->AppendSwitchASCII("force-variation-ids", "42");
    // Need to ignore cert errors to use a HTTPS server for the test domains.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetPreference(prefetch::PreloadPagesState value) {
    prefetch::SetPreloadPagesState(browser()->profile()->GetPrefs(), value);
  }

  bool RunPrefetchExperiment(bool expect_success, Browser* browser) {
    GURL url = embedded_test_server()->GetURL(kPrefetchPage);

    const std::u16string expected_title =
        expect_success ? u"link onload" : u"link onerror";
    content::TitleWatcher title_watcher(
        browser->tab_strip_model()->GetActiveWebContents(), expected_title);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));
    return expected_title == title_watcher.WaitAndGetTitle();
  }
};

// When initiated from the renderer, prefetch should be allowed regardless of
// the network type.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTest, PreferenceWorks) {
  // Set real NetworkChangeNotifier singleton aside.
  std::unique_ptr<NetworkChangeNotifier::DisableForTest> disable_for_test(
      new NetworkChangeNotifier::DisableForTest);

  // Preference defaults to ALWAYS.
  {
    std::unique_ptr<NetworkChangeNotifier> mock(
        new MockNetworkChangeNotifierWIFI);
    EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
  }
  {
    std::unique_ptr<NetworkChangeNotifier> mock(
        new MockNetworkChangeNotifier4G);
    EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
  }

  // Set preference to no preloading: prefetch should be unaffected.
  SetPreference(PreloadPagesState::kNoPreloading);
  {
    std::unique_ptr<NetworkChangeNotifier> mock(
        new MockNetworkChangeNotifierWIFI);
    EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
  }
  {
    std::unique_ptr<NetworkChangeNotifier> mock(
        new MockNetworkChangeNotifier4G);
    EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
  }
}

// Bug 339909: When in incognito mode the browser crashed due to an
// uninitialized preference member. Verify that it no longer does.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTest, IncognitoTest) {
  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  Browser* incognito_browser =
      Browser::Create(Browser::CreateParams(incognito_profile, true));

  // Navigate just to have a tab in this window, otherwise there is no
  // WebContents for the incognito browser.
  OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  EXPECT_TRUE(RunPrefetchExperiment(true, incognito_browser));
}

// https://crbug.com/922362: When the prefetched request is redirected, DCHECKs
// in PrefetchURLLoader::FollowRedirect() failed due to "X-Client-Data" in
// removed_headers. Verify that it no longer does, and the header is removed
// when redirected to non-Google host.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTest, RedirectedPrefetch) {
  std::vector<net::test_server::HttpRequest> requests;
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&requests](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        if (request.relative_url == std::string(kRedirectPrefetchPage)) {
          requests.push_back(request);
          response->set_content_type("text/html");
          response->set_content(
              base::StringPrintf("<link rel=\"prefetch\" href=\"%s\" "
                                 "onload=\"document.title='done'\">",
                                 kRedirectPrefetchUrl));
          return response;
        } else if (request.relative_url == std::string(kRedirectPrefetchUrl)) {
          requests.push_back(request);
          response->set_code(net::HTTP_MOVED_PERMANENTLY);
          response->AddCustomHeader(
              "Location", base::StringPrintf("https://example.com:%s%s",
                                             request.GetURL().port().c_str(),
                                             kRedirectedPrefetchUrl));
          return response;
        } else if (request.relative_url ==
                   std::string(kRedirectedPrefetchUrl)) {
          requests.push_back(request);
          return response;
        }
        return nullptr;
      }));

  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL("www.google.com", kRedirectPrefetchPage);
  const std::u16string expected_title = u"done";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  ASSERT_EQ(3U, requests.size());

  EXPECT_EQ(base::StringPrintf("www.google.com:%u", https_server.port()),
            requests[0].headers["Host"]);
  EXPECT_EQ(kRedirectPrefetchPage, requests[0].relative_url);
  // The navigation request to Google host must have X-Client-Data header.
  EXPECT_TRUE(HasVariationsHeader(requests[0].headers));

  EXPECT_EQ(base::StringPrintf("www.google.com:%u", https_server.port()),
            requests[1].headers["Host"]);
  EXPECT_EQ(kRedirectPrefetchUrl, requests[1].relative_url);
  // The prefetch request to Google host must have X-Client-Data header.
  EXPECT_TRUE(HasVariationsHeader(requests[1].headers));

  EXPECT_EQ(base::StringPrintf("example.com:%u", https_server.port()),
            requests[2].headers["Host"]);
  EXPECT_EQ(kRedirectedPrefetchUrl, requests[2].relative_url);
  // The redirected prefetch request to non-Google host must not have
  // X-Client-Data header.
  EXPECT_FALSE(HasVariationsHeader(requests[2].headers));
}

// Test that non-cacheable prefetched resources are not being re-requested.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTest, PrefetchCachingPeriod) {
  std::vector<net::test_server::HttpRequest> requests;
  // Cannot test HTTPS because caching is disabled when cert errors are present
  net::EmbeddedTestServer http_server(net::EmbeddedTestServer::TYPE_HTTP);
  http_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&requests](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        if (request.relative_url == std::string(kPrefetchCachingPeriodPage)) {
          requests.push_back(request);
          response->set_content_type("text/html");
          std::string onload = base::StringPrintf(
              "const script = document.createElement('script'); "
              "script.onload=done; script.src='%s'; "
              "document.body.appendChild(script);",
              kPrefetchCachingPeriodUrl);

          response->set_content(
              base::StringPrintf("<script>const done = () => { "
                                 "document.title='done'; };</script>\n"
                                 "<body><link rel=\"prefetch\" href=\"%s\" "
                                 "onload=\"%s\">",
                                 kPrefetchCachingPeriodUrl, onload.c_str()));
          return response;
        } else if (request.relative_url ==
                   std::string(kPrefetchCachingPeriodUrl)) {
          requests.push_back(request);
          response->set_content_type("text/javascript");
          response->AddCustomHeader("Cache-Control", "no-cache");
          response->set_content("console.log('script ran');");
          return response;
        }
        return nullptr;
      }));

  http_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(http_server.Start());

  GURL url = http_server.GetURL("localhost", kPrefetchCachingPeriodPage);
  const std::u16string expected_title = u"done";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  ASSERT_EQ(2U, requests.size());
}

// Test that non-cacheable prefetched resources are not being re-requested,
// even if they already spent time in an intermediary cache along the way.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTest, PrefetchCachingPeriodWithAge) {
  std::vector<net::test_server::HttpRequest> requests;
  // Cannot test HTTPS because caching is disabled when cert errors are present
  net::EmbeddedTestServer http_server(net::EmbeddedTestServer::TYPE_HTTP);
  http_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&requests](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        if (request.relative_url == std::string(kPrefetchCachingPeriodPage)) {
          requests.push_back(request);
          response->set_content_type("text/html");
          std::string onload = base::StringPrintf(
              "const script = document.createElement('script'); "
              "script.onload=done; script.src='%s'; "
              "document.body.appendChild(script);",
              kPrefetchCachingPeriodUrl);

          response->set_content(
              base::StringPrintf("<script>const done = () => { "
                                 "document.title='done'; };</script>\n"
                                 "<body><link rel=\"prefetch\" href=\"%s\" "
                                 "onload=\"%s\">",
                                 kPrefetchCachingPeriodUrl, onload.c_str()));
          return response;
        } else if (request.relative_url ==
                   std::string(kPrefetchCachingPeriodUrl)) {
          requests.push_back(request);
          response->set_content_type("text/javascript");
          response->AddCustomHeader("Cache-Control", "no-cache");
          response->AddCustomHeader("Age", "600");
          response->set_content("console.log('script ran');");
          return response;
        }
        return nullptr;
      }));

  http_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(http_server.Start());

  GURL url = http_server.GetURL("localhost", kPrefetchCachingPeriodPage);
  const std::u16string expected_title = u"done";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  ASSERT_EQ(2U, requests.size());
}

}  // namespace
