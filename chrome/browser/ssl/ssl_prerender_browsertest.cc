// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using chrome_browser_interstitials::IsShowingSSLInterstitial;
using content::EvalJs;
using content::RenderFrameHost;
using content::SSLHostStateDelegate;
using content::TestNavigationManager;
using content::URLLoaderInterceptor;
using content::WebContents;
using net::EmbeddedTestServer;
using ui_test_utils::NavigateToURL;

namespace {

std::unique_ptr<net::EmbeddedTestServer> CreateExpiredCertServer(
    const base::FilePath& data_dir) {
  auto server =
      std::make_unique<EmbeddedTestServer>(EmbeddedTestServer::TYPE_HTTPS);
  server->SetSSLConfig(EmbeddedTestServer::CERT_EXPIRED);
  server->ServeFilesFromSourceDirectory(data_dir);
  return server;
}

}  // namespace

class SSLPrerenderTest : public InProcessBrowserTest {
 public:
  SSLPrerenderTest()
      : prerender_helper_(base::BindRepeating(&SSLPrerenderTest::web_contents,
                                              base::Unretained(this))) {}
  ~SSLPrerenderTest() override = default;

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper prerender_helper_;
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
            GetChromeTestDataDir().MaybeAsASCII(), kInitialUrl.GetOrigin());

    // Navigate to the initial page.
    ASSERT_TRUE(NavigateToURL(browser(), kInitialUrl));
    ASSERT_FALSE(IsShowingSSLInterstitial(web_contents()));

    // Make sure there is no exception for the prerendering URL, so that an SSL
    // error will not be ignored.
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    SSLHostStateDelegate* state = profile->GetSSLHostStateDelegate();
    ASSERT_FALSE(
        state->HasAllowException(kPrerenderUrl.host(), web_contents()));
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
    ASSERT_NE(prerender_helper_.GetHostForUrl(kPrerenderUrl),
              RenderFrameHost::kNoFrameTreeNodeId);

    // The prerender navigation should be canceled as part of the response.
    // Ensure the prerender host is destroyed and no interstitial is showing.
    EXPECT_FALSE(observer.WaitForResponse());
    EXPECT_EQ(prerender_helper_.GetHostForUrl(kPrerenderUrl),
              RenderFrameHost::kNoFrameTreeNodeId);
    EXPECT_FALSE(IsShowingSSLInterstitial(web_contents()));
  }
}

// Verifies that a certificate error in a prerendered page fetched via service
// worker causes cancelation of prerendering without showing an interstitial.
// TODO(bokan): In the future, when prerendering supports cross origin
// triggering, this test can be more straightforward by using one server for
// the initial page and another, with bad certs, for the prerendering page.
#if defined(OS_LINUX)
// TODO(https://crbug.com/1245117):
// SSLPrerenderTest.TestNoInterstitialInPrerenderSW fails on "Builder Network
// Service Linux".
#define MAYBE_TestNoInterstitialInPrerenderSW \
  DISABLED_TestNoInterstitialInPrerenderSW
#else
#define MAYBE_TestNoInterstitialInPrerenderSW TestNoInterstitialInPrerenderSW
#endif
IN_PROC_BROWSER_TEST_F(SSLPrerenderTest,
                       MAYBE_TestNoInterstitialInPrerenderSW) {
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
            GetChromeTestDataDir().MaybeAsASCII(), kInitialUrl.GetOrigin());

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
    ASSERT_FALSE(
        state->HasAllowException(kPrerenderUrl.host(), web_contents()));
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
    ASSERT_NE(prerender_helper_.GetHostForUrl(kPrerenderUrl),
              RenderFrameHost::kNoFrameTreeNodeId);

    // The prerender navigation should be canceled as part of the response.
    // Ensure the prerender host is destroyed and no interstitial is showing.
    EXPECT_FALSE(observer.WaitForResponse());
    EXPECT_EQ(prerender_helper_.GetHostForUrl(kPrerenderUrl),
              RenderFrameHost::kNoFrameTreeNodeId);
    EXPECT_FALSE(IsShowingSSLInterstitial(web_contents()));
  }
}
