// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_tab_reloader.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using captive_portal::CaptivePortalResult;

// Used for testing CaptivePortalTabReloader in isolation from the observer.
// Exposes a number of private functions and mocks out others.
class TestCaptivePortalTabReloader : public CaptivePortalTabReloader {
 public:
  explicit TestCaptivePortalTabReloader(content::WebContents* web_contents)
      : CaptivePortalTabReloader(NULL,
                                 web_contents,
                                 base::Callback<void(void)>()) {
  }

  ~TestCaptivePortalTabReloader() override {}

  bool TimerRunning() {
    return slow_ssl_load_timer_.IsRunning();
  }

  // The following methods are aliased so they can be publicly accessed by the
  // unit tests.

  State state() const {
    return CaptivePortalTabReloader::state();
  }

  void set_slow_ssl_load_time(base::TimeDelta slow_ssl_load_time) {
    EXPECT_FALSE(TimerRunning());
    CaptivePortalTabReloader::set_slow_ssl_load_time(slow_ssl_load_time);
  }

  // CaptivePortalTabReloader:
  MOCK_METHOD0(ReloadTab, void());
  MOCK_METHOD0(MaybeOpenCaptivePortalLoginTab, void());
  MOCK_METHOD0(CheckForCaptivePortal, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCaptivePortalTabReloader);
};

// Used to test behavior when a WebContents is showing an interstitial page.
class MockInterstitialPageDelegate : public content::InterstitialPageDelegate {
 public:
  // The newly created MockInterstitialPageDelegate will be owned by the
  // WebContents' InterstitialPage, and cleaned up when the WebContents
  // destroys it.
  explicit MockInterstitialPageDelegate(
      content::WebContents* web_contents) {
    content::InterstitialPage* interstitial_page =
        content::InterstitialPage::Create(
            web_contents, true, GURL("http://blah"), this);
    interstitial_page->DontCreateViewForTesting();
    interstitial_page->Show();
  }

  ~MockInterstitialPageDelegate() override {}

 private:
  // InterstitialPageDelegate implementation:
  std::string GetHTMLContents() override { return "HTML Contents"; }

  DISALLOW_COPY_AND_ASSIGN(MockInterstitialPageDelegate);
};

class CaptivePortalTabReloaderTest : public ChromeRenderViewHostTestHarness {
 public:
  // testing::Test:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_reloader_.reset(new testing::StrictMock<TestCaptivePortalTabReloader>(
        web_contents()));

    // Most tests don't run the message loop, so don't use a timer for them.
    tab_reloader_->set_slow_ssl_load_time(base::TimeDelta());
  }

  void TearDown() override {
    EXPECT_FALSE(tab_reloader().TimerRunning());
    tab_reloader_.reset(NULL);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestCaptivePortalTabReloader& tab_reloader() { return *tab_reloader_.get(); }

 private:
  std::unique_ptr<TestCaptivePortalTabReloader> tab_reloader_;
};

// Simulates a slow SSL load when the Internet is connected.
TEST_F(CaptivePortalTabReloaderTest, InternetConnected) {
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_INTERNET_CONNECTED);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  tab_reloader().OnLoadCommitted(net::OK);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulates a slow SSL load when the Internet is connected.  In this case,
// the timeout error occurs before the timer triggers.  Unlikely to happen
// in practice, but best if it still works.
TEST_F(CaptivePortalTabReloaderTest, InternetConnectedTimeout) {
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_INTERNET_CONNECTED);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulates a slow SSL load when captive portal checks return no response.
TEST_F(CaptivePortalTabReloaderTest, NoResponse) {
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  tab_reloader().OnCaptivePortalResults(captive_portal::RESULT_NO_RESPONSE,
                                        captive_portal::RESULT_NO_RESPONSE);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  tab_reloader().OnLoadCommitted(net::OK);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulates a slow HTTP load when behind a captive portal, that eventually.
// tiems out.  Since it's HTTP, the TabReloader should do nothing.
TEST_F(CaptivePortalTabReloaderTest, DoesNothingOnHttp) {
  tab_reloader().OnLoadStart(false);
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  // The user logs in.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  // The page times out.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the normal login process.  The user logs in before the error page
// in the original tab commits.
TEST_F(CaptivePortalTabReloaderTest, Login) {
  tab_reloader().OnLoadStart(true);

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal.  The TabReloader
  // should try and create a new login tab in response.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The user logs on from another tab, and a captive portal check is triggered.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the normal login process.  The user logs in after the tab finishes
// loading the error page.
TEST_F(CaptivePortalTabReloaderTest, LoginLate) {
  tab_reloader().OnLoadStart(true);

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal.  The TabReloader
  // should try and create a new login tab in response.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The error page commits.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The user logs on from another tab, and a captive portal check is triggered.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate a login after the tab times out unexpectedly quickly.
TEST_F(CaptivePortalTabReloaderTest, TimeoutFast) {
  tab_reloader().OnLoadStart(true);

  // The error page commits, which should trigger a captive portal check,
  // since the timer's still running.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal.  The TabReloader
  // should try and create a new login tab in response.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The user logs on from another tab, and a captive portal check is triggered.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// An SSL protocol error triggers a captive portal check behind a captive
// portal.  The user then logs in.
TEST_F(CaptivePortalTabReloaderTest, SSLProtocolError) {
  tab_reloader().OnLoadStart(true);

  // The error page commits, which should trigger a captive portal check,
  // since the timer's still running.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  tab_reloader().OnLoadCommitted(net::ERR_SSL_PROTOCOL_ERROR);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal.  The TabReloader
  // should try and create a new login tab in response.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The user logs on from another tab, and a captive portal check is triggered.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// An SSL protocol error triggers a captive portal check behind a captive
// portal.  The user logs in before the results from the captive portal check
// completes.
TEST_F(CaptivePortalTabReloaderTest, SSLProtocolErrorFastLogin) {
  tab_reloader().OnLoadStart(true);

  // The error page commits, which should trigger a captive portal check,
  // since the timer's still running.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  tab_reloader().OnLoadCommitted(net::ERR_SSL_PROTOCOL_ERROR);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The user has logged in from another tab.  The tab automatically reloads.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// An SSL protocol error triggers a captive portal check behind a captive
// portal.  The user logs in before the results from the captive portal check
// completes.  This case is probably not too likely, but should be handled.
TEST_F(CaptivePortalTabReloaderTest, SSLProtocolErrorAlreadyLoggedIn) {
  tab_reloader().OnLoadStart(true);

  // The user logs in from another tab before the tab errors out.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should trigger a reload.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnLoadCommitted(net::ERR_SSL_PROTOCOL_ERROR);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the case that a user has already logged in before the tab receives a
// captive portal result, but a RESULT_BEHIND_CAPTIVE_PORTAL was received
// before the tab started loading.
TEST_F(CaptivePortalTabReloaderTest, AlreadyLoggedIn) {
  tab_reloader().OnLoadStart(true);

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The user has already logged in.  Since the last result found a captive
  // portal, the tab will be reloaded if a timeout is committed.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Same as above, except the result is received even before the timer triggers,
// due to a captive portal test request from some external source, like a login
// tab.
TEST_F(CaptivePortalTabReloaderTest, AlreadyLoggedInBeforeTimerTriggers) {
  tab_reloader().OnLoadStart(true);

  // The user has already logged in.  Since the last result indicated there is
  // a captive portal, the tab will be reloaded if it times out.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the user logging in while the timer is still running.  May happen
// if the tab is reloaded just before logging in on another tab.
TEST_F(CaptivePortalTabReloaderTest, LoginWhileTimerRunning) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  // The user has already logged in.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate a captive portal being detected while the time is still running.
// The captive portal check triggered by the timer detects the captive portal
// again, and then the user logs in.
TEST_F(CaptivePortalTabReloaderTest, BehindPortalResultWhileTimerRunning) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  // The user is behind a captive portal, but since the tab hasn't timed out,
  // the message is ignored.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  // The rest proceeds as normal.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal, and this time the
  // tab tries to create a login tab.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The user logs on from another tab, and a captive portal check is triggered.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// The CaptivePortalService detects the user has logged in to a captive portal
// while the timer is still running, but the original load succeeds, so no
// reload is done.
TEST_F(CaptivePortalTabReloaderTest, LogInWhileTimerRunningNoError) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  // The user has already logged in.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The page successfully commits, so no reload is triggered.
  tab_reloader().OnLoadCommitted(net::OK);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the login process when there's an SSL certificate error.
TEST_F(CaptivePortalTabReloaderTest, SSLCertErrorLogin) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  // The load is interrupted by an interstitial page.  The interstitial page
  // is created after the TabReloader is notified.
  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal());
  net::SSLInfo ssl_info;
  ssl_info.cert_status |= net::CERT_STATUS_COMMON_NAME_INVALID;
  tab_reloader().OnSSLCertError(ssl_info);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());
  // The MockInterstitialPageDelegate will cleaned up by the WebContents.
  new MockInterstitialPageDelegate(web_contents());

  // Captive portal probe finds a captive portal.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);

  // The user logs in.  Since the interstitial is showing, the page should
  // be reloaded, despite still having a provisional load.
  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
}

// Simulate an HTTP redirect to HTTPS, when the Internet is connected.
TEST_F(CaptivePortalTabReloaderTest, HttpToHttpsRedirectInternetConnected) {
  tab_reloader().OnLoadStart(false);
  // There should be no captive portal check pending.
  base::RunLoop().RunUntilIdle();

  // HTTP to HTTPS redirect.
  tab_reloader().OnRedirect(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());
  EXPECT_TRUE(tab_reloader().TimerRunning());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_INTERNET_CONNECTED);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  tab_reloader().OnLoadCommitted(net::OK);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate an HTTP redirect to HTTPS and subsequent Login, when the user logs
// in before the original page commits.
TEST_F(CaptivePortalTabReloaderTest, HttpToHttpsRedirectLogin) {
  tab_reloader().OnLoadStart(false);
  // There should be no captive portal check pending.
  base::RunLoop().RunUntilIdle();

  // HTTP to HTTPS redirect.
  tab_reloader().OnRedirect(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), CheckForCaptivePortal()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_reloader().TimerRunning());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_MAYBE_BROKEN_BY_PORTAL,
            tab_reloader().state());

  // The captive portal service detects a captive portal.  The TabReloader
  // should try and create a new login tab in response.
  EXPECT_CALL(tab_reloader(), MaybeOpenCaptivePortalLoginTab()).Times(1);
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_INTERNET_CONNECTED,
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // The user logs on from another tab, and a captive portal check is triggered.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  // The error page commits, which should start an asynchronous reload.
  tab_reloader().OnLoadCommitted(net::ERR_CONNECTION_TIMED_OUT);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            tab_reloader().state());

  EXPECT_CALL(tab_reloader(), ReloadTab()).Times(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Simulate the case where an HTTPs page redirects to an HTTPS page, before
// the timer triggers.
TEST_F(CaptivePortalTabReloaderTest, HttpsToHttpRedirect) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  tab_reloader().OnRedirect(false);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // There should be no captive portal check pending after the redirect.
  base::RunLoop().RunUntilIdle();

  // Logging in shouldn't do anything.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}

// Check that an HTTPS to HTTPS redirect results in no timer running.
TEST_F(CaptivePortalTabReloaderTest, HttpsToHttpsRedirect) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  tab_reloader().OnRedirect(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());
  // Nothing should happen.
  base::RunLoop().RunUntilIdle();
}

// Check that an HTTPS to HTTP to HTTPS redirect results in no timer running.
TEST_F(CaptivePortalTabReloaderTest, HttpsToHttpToHttpsRedirect) {
  tab_reloader().OnLoadStart(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_TIMER_RUNNING,
            tab_reloader().state());

  tab_reloader().OnRedirect(false);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  tab_reloader().OnRedirect(true);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());
  // Nothing should happen.
  base::RunLoop().RunUntilIdle();
}

// Check that an HTTP to HTTP redirect results in the timer not running.
TEST_F(CaptivePortalTabReloaderTest, HttpToHttpRedirect) {
  tab_reloader().OnLoadStart(false);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());

  tab_reloader().OnRedirect(false);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
  EXPECT_FALSE(tab_reloader().TimerRunning());

  // There should be no captive portal check pending after the redirect.
  base::RunLoop().RunUntilIdle();

  // Logging in shouldn't do anything.
  tab_reloader().OnCaptivePortalResults(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
      captive_portal::RESULT_INTERNET_CONNECTED);
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE, tab_reloader().state());
}
