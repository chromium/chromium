// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/prerender_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using chrome_browser_interstitials::IsShowingSSLInterstitial;
using content::EvalJs;
using content::ExecJs;
using content::RenderFrameHost;
using content::SSLHostStateDelegate;
using content::TestNavigationManager;
using content::URLLoaderInterceptor;
using content::WebContents;
using content::test::PrerenderHostObserver;
using net::EmbeddedTestServer;
using ui_test_utils::NavigateToURL;

namespace {

std::unique_ptr<net::EmbeddedTestServer> CreateExpiredCertServer(
    const base::FilePath& data_dir) {
  auto server =
      std::make_unique<EmbeddedTestServer>(EmbeddedTestServer::TYPE_HTTPS);
  server->AddDefaultHandlers(data_dir);
  server->SetSSLConfig(EmbeddedTestServer::CERT_EXPIRED);
  return server;
}

std::unique_ptr<net::EmbeddedTestServer> CreateHTTPSServer(
    const base::FilePath& data_dir) {
  auto server =
      std::make_unique<EmbeddedTestServer>(EmbeddedTestServer::TYPE_HTTPS);
  server->AddDefaultHandlers(data_dir);
  server->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  return server;
}

std::string GetFilePathWithHostAndPortReplacement(
    const std::string& original_file_path,
    const net::HostPortPair& host_port_pair) {
  base::StringPairs replacement_text;
  replacement_text.push_back(
      make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
  return net::test_server::GetFilePathWithReplacements(original_file_path,
                                                       replacement_text);
}

}  // namespace

class SSLPrerenderTest : public InProcessBrowserTest {
 public:
  SSLPrerenderTest()
      : prerender_helper_(base::BindRepeating(&SSLPrerenderTest::web_contents,
                                              base::Unretained(this))) {}
  ~SSLPrerenderTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper prerender_helper_;
};

class SecurityVisibleStateObserver : public content::WebContentsObserver {
 public:
  explicit SecurityVisibleStateObserver(WebContents& web_contents)
      : WebContentsObserver(&web_contents) {}
  void DidChangeVisibleSecurityState() override {
    is_visible_state_changed_ = true;
  }
  bool is_visible_state_changed() const { return is_visible_state_changed_; }

 private:
  bool is_visible_state_changed_ = false;
};

// Verifies that a certificate error in a prerendered page causes cancelation
// of prerendering without showing an interstitial.
// TODO(bokan): In the future, when prerendering supports cross origin
// triggering, this test can be more straightforward by using one server for
// the initial page and another, with bad certs, for the prerendering page.
IN_PROC_BROWSER_TEST_F(SSLPrerenderTest, TestNoInterstitialInPrerender) {
  auto server = CreateExpiredCertServer(GetChromeTestDataDir());
  ASSERT_TRUE(server->Start());

  const GURL kPrerenderUrl = server->GetURL("/empty.html?prerender");
  const GURL kInitialUrl = server->GetURL("/empty.html");

  // Use an interceptor to load the initial page. This is done because the
  // server has certificate errors. If the initial URL is loaded from the test
  // server, this will trigger an interstitial before the prerender can be
  // triggered. Since the prerender must be same origin with the initial page,
  // proceeding through that interstitial would add an exception for the URL,
  // and so the error won't be visible to the prerender load. Since this test
  // is trying to make sure that interstitials on prerender loads abort the
  // prerender, this interceptor ensures the initial load won't have an
  // interstitial, but the prerender will.
  {
    auto url_loader_interceptor =
        content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
            GetChromeTestDataDir().MaybeAsASCII(),
            kInitialUrl.DeprecatedGetOriginAsURL());

    // Navigate to the initial page.
    ASSERT_TRUE(NavigateToURL(browser(), kInitialUrl));
    ASSERT_FALSE(IsShowingSSLInterstitial(web_contents()));

    // Make sure there is no exception for the prerendering URL, so that an SSL
    // error will not be ignored.
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
    ASSERT_FALSE(state->HasAllowException(
        kPrerenderUrl.host(),
        web_contents()->GetPrimaryMainFrame()->GetStoragePartition()));
  }

  // Trigger a prerender. Unlike the initial navigation, this will hit the
  // server, so it'll respond with a bad certificate. If this request was a
  // normal navigation, an interstitial would be shown, but because it is a
  // prerender request, the prerender should be canceled and no interstitial
  // shown.
  {
    TestNavigationManager observer(web_contents(), kPrerenderUrl);

    // Trigger the prerender. The PrerenderHost starts the request when it is
    // created so it should be available after WaitForRequestStart.
    prerender_helper_.AddPrerenderAsync(kPrerenderUrl);
    ASSERT_TRUE(observer.WaitForRequestStart());
    ASSERT_TRUE(prerender_helper_.GetHostForUrl(kPrerenderUrl));

    // The prerender navigation should be canceled as part of the response.
    // Ensure the prerender host is destroyed and no interstitial is showing.
    EXPECT_FALSE(observer.WaitForResponse());
    EXPECT_TRUE(prerender_helper_.GetHostForUrl(kPrerenderUrl).is_null());
    EXPECT_FALSE(IsShowingSSLInterstitial(web_contents()));
  }
}

// Verifies that a certificate error in a prerendered page fetched via service
// worker causes cancelation of prerendering without showing an interstitial.
// TODO(bokan): In the future, when prerendering supports cross origin
// triggering, this test can be more straightforward by using one server for
// the initial page and another, with bad certs, for the prerendering page.
// TODO(crbug.com/40923072): the test has been flaky across platforms.
IN_PROC_BROWSER_TEST_F(SSLPrerenderTest,
                       DISABLED_TestNoInterstitialInPrerenderSW) {
  auto server = CreateExpiredCertServer(GetChromeTestDataDir());
  ASSERT_TRUE(server->Start());

  const GURL kPrerenderUrl = server->GetURL("/service_worker/blank.html");
  const GURL kInitialUrl =
      server->GetURL("/service_worker/create_service_worker.html");

  // Use an interceptor to load the initial URL. This is done because the
  // server has certificate errors. If the initial URL is loaded from the test
  // server, this will trigger an interstitial before the prerender can be
  // triggered. Since the prerender must be same origin with the initial page,
  // proceeding through that interstitial would add an exception for the URL,
  // and so the error won't be visible to the prerender load. Since this test
  // is trying to make sure that interstitials on prerender loads abort the
  // prerender, this interceptor ensures the initial load won't have an
  // interstitial, but the prerender will.
  {
    auto url_loader_interceptor =
        content::URLLoaderInterceptor::ServeFilesFromDirectoryAtOrigin(
            GetChromeTestDataDir().MaybeAsASCII(),
            kInitialUrl.DeprecatedGetOriginAsURL());

    // Navigate to the initial page and register a service worker that will
    // relay the fetch.
    ASSERT_TRUE(NavigateToURL(browser(), kInitialUrl));
    ASSERT_EQ("DONE", EvalJs(web_contents(),
                             "register('fetch_event_respond_with_fetch.js');"));
    ASSERT_FALSE(IsShowingSSLInterstitial(web_contents()));

    // Make sure there is no exception for the prerendering URL, so that an SSL
    // error will not be ignored.
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
    ASSERT_FALSE(state->HasAllowException(
        kPrerenderUrl.host(),
        web_contents()->GetPrimaryMainFrame()->GetStoragePartition()));
  }

  // Trigger a prerender. Unlike the initial navigation, this will hit the
  // server, so it'll respond with a bad certificate. If this request was a
  // normal navigation, an interstitial would be shown, but because it is a
  // prerender request, the prerender should be canceled and no interstitial
  // shown.
  {
    TestNavigationManager observer(web_contents(), kPrerenderUrl);

    // Trigger the prerender. The PrerenderHost starts the request when it is
    // created so it should be available after WaitForRequestStart.
    prerender_helper_.AddPrerenderAsync(kPrerenderUrl);
    ASSERT_TRUE(observer.WaitForRequestStart());
    ASSERT_TRUE(prerender_helper_.GetHostForUrl(kPrerenderUrl));

    // The prerender navigation should be canceled as part of the response.
    // Ensure the prerender host is destroyed and no interstitial is showing.
    EXPECT_FALSE(observer.WaitForResponse());
    EXPECT_TRUE(prerender_helper_.GetHostForUrl(kPrerenderUrl).is_null());
    EXPECT_FALSE(IsShowingSSLInterstitial(web_contents()));
  }
}

// Prerenders a page that tries to submit an insecure form and checks that this
// cancels the prerender instead.
IN_PROC_BROWSER_TEST_F(SSLPrerenderTest,
                       InsecureFormSubmissionCancelsPrerender) {
  base::HistogramTester histograms;
  const std::string kHistogramName =
      "Security.MixedForm.InterstitialTriggerState";

  // Histogram should start off empty.
  histograms.ExpectTotalCount(kHistogramName, 0);

  auto https_server = CreateHTTPSServer(GetChromeTestDataDir());
  ASSERT_TRUE(https_server->Start());

  // Add a "replace_text=" query param that the test server will use to replace
  // the string "REPLACE_WITH_HOST_AND_PORT" in the destination page.
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server->GetURL("a.test", "/"));
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_form_targeting_http_url.html", host_port_pair);

  // Use "a.test" since the default host is 127.0.0.1 and that's considered a
  // "potentially trustworthy origin" by the throttle so navigation won't
  // trigger the throttle.
  const GURL kPrerenderUrl = https_server->GetURL("a.test", replacement_path);
  const GURL kInitialUrl = https_server->GetURL("a.test", "/empty.html");

  // Test steps
  {
    ASSERT_TRUE(NavigateToURL(browser(), kInitialUrl));

    // Trigger the prerender.
    const content::FrameTreeNodeId kPrerenderHostId =
        prerender_helper_.AddPrerender(kPrerenderUrl);
    ASSERT_TRUE(kPrerenderHostId);
    ASSERT_EQ(prerender_helper_.GetHostForUrl(kPrerenderUrl), kPrerenderHostId);

    // Submit a form targeting an insecure URL. The prerender should be
    // destroyed.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    PrerenderHostObserver host_observer(*tab, kPrerenderHostId);
    ASSERT_TRUE(
        ExecJs(prerender_helper_.GetPrerenderedMainFrameHost(kPrerenderHostId),
               "document.getElementById('submit').click();"));
    host_observer.WaitForDestroyed();

    // The prerender navigation should be canceled as part of the response.
    // Ensure the prerender host is destroyed, no interstitial is showing, and
    // we didn't affect the relevant metric.
    EXPECT_TRUE(prerender_helper_.GetHostForUrl(kPrerenderUrl).is_null());
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            tab);
    EXPECT_FALSE(helper);
    histograms.ExpectTotalCount(kHistogramName, 0);
  }
}

// Prerenders a page that tries to submit an insecure form and checks that this
// cancels the prerender even if the primary page is proceeding on an insecure
// form.
IN_PROC_BROWSER_TEST_F(SSLPrerenderTest,
                       InsecureFormSubmissionCancelsPrerenderEvenIfProceeding) {
  base::HistogramTester histograms;
  const std::string kHistogramName =
      "Security.MixedForm.InterstitialTriggerState";

  // Histogram should start off empty.
  histograms.ExpectTotalCount(kHistogramName, 0);

  auto https_server = CreateHTTPSServer(GetChromeTestDataDir());
  ASSERT_TRUE(https_server->Start());

  // Add a "replace_text=" query param that the test server will use to replace
  // the string "REPLACE_WITH_HOST_AND_PORT" in the destination page.
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server->GetURL("a.test", "/"));
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form.html",
      embedded_test_server()->host_port_pair());

  // Use "a.test" since the default host is 127.0.0.1 and that's considered a
  // "potentially trustworthy origin" by the throttle so navigation won't
  // trigger the throttle.
  const GURL kUrl = https_server->GetURL("a.test", replacement_path);

  // Test steps
  {
    ASSERT_TRUE(NavigateToURL(browser(), kUrl));

    // Submit a form targeting an insecure URL.
    content::TestNavigationObserver nav_observer(web_contents(), 1);
    ASSERT_TRUE(ExecJs(web_contents(), "submitForm();"));
    nav_observer.Wait();
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            web_contents());
    ASSERT_TRUE(helper);
    EXPECT_TRUE(helper->IsDisplayingInterstitial());
    histograms.ExpectTotalCount(kHistogramName, 1);

    // Prerender the same insecure form.
    std::unique_ptr<content::PrerenderHandle> prerender_handle =
        web_contents()->StartPrerendering(
            kUrl, content::PreloadingTriggerType::kEmbedder,
            prerender_utils::kDirectUrlInputMetricSuffix,
            ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                      ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
            /*should_warm_up_compositor=*/false,
            content::PreloadingHoldbackStatus::kUnspecified,
            /*preloading_attempt=*/nullptr, /*url_match_predicate=*/{},
            /*prerender_navigation_handle_callback=*/{});
    ASSERT_TRUE(prerender_handle);
    const content::FrameTreeNodeId kPrerenderHostId =
        prerender_helper_.GetHostForUrl(kUrl);
    ASSERT_TRUE(kPrerenderHostId);
    prerender_helper_.WaitForPrerenderLoadCompletion(kPrerenderHostId);

    // Proceed with the interstitial page in the primary page.
    content::TestNavigationObserver nav_observer2(web_contents(), 1);
    helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            web_contents());
    helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
        ->CommandReceived(
            base::NumberToString(security_interstitials::CMD_PROCEED));
    nav_observer2.Wait();
    ASSERT_TRUE(helper);
    EXPECT_FALSE(helper->IsDisplayingInterstitial());

    // Submit the prerendered form. The prerender should be destroyed.
    PrerenderHostObserver host_observer(*web_contents(), kPrerenderHostId);
    ASSERT_TRUE(
        ExecJs(prerender_helper_.GetPrerenderedMainFrameHost(kPrerenderHostId),
               "submitForm();"));
    host_observer.WaitForDestroyed();

    // The prerender navigation should be canceled as part of the response.
    // Ensure the prerender host is destroyed, no interstitial is showing, and
    // we didn't affect the relevant metric.
    EXPECT_TRUE(prerender_helper_.GetHostForUrl(kUrl).is_null());
    helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            web_contents());
    ASSERT_TRUE(helper);
    EXPECT_FALSE(helper->IsDisplayingInterstitial());
    histograms.ExpectTotalCount(kHistogramName, 1);
  }
}

IN_PROC_BROWSER_TEST_F(SSLPrerenderTest,
                       TestNoVisibleStateChangedOnInitialPrerendering) {
  auto https_server = CreateHTTPSServer(GetChromeTestDataDir());
  ASSERT_TRUE(https_server->Start());

  const GURL kPrerenderUrl =
      https_server->GetURL("a.test", "/empty.html?prerender");
  const GURL kInitialUrl = https_server->GetURL("a.test", "/empty.html");

  // Test steps
  {
    ASSERT_TRUE(NavigateToURL(browser(), kInitialUrl));

    // Trigger the prerender.
    content::TestActivationManager activation_manager(web_contents(),
                                                      kPrerenderUrl);
    SecurityVisibleStateObserver visible_state_observer(*web_contents());
    const content::FrameTreeNodeId kPrerenderHostId =
        prerender_helper_.AddPrerender(kPrerenderUrl);
    ASSERT_TRUE(kPrerenderHostId);
    ASSERT_EQ(prerender_helper_.GetHostForUrl(kPrerenderUrl), kPrerenderHostId);
    ASSERT_FALSE(visible_state_observer.is_visible_state_changed());

    // Activate.
    ASSERT_TRUE(
        content::ExecJs(web_contents()->GetPrimaryMainFrame(),
                        content::JsReplace("location = $1", kPrerenderUrl)));
    activation_manager.WaitForNavigationFinished();
    EXPECT_TRUE(activation_manager.was_activated());
    EXPECT_TRUE(visible_state_observer.is_visible_state_changed());
  }
}
