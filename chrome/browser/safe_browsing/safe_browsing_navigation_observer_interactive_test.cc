// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

namespace safe_browsing {

// Infrastructure copied from these
// src\chrome\browser\safe_browsing\safe_browsing_navigation_observer_browsertest.cc
// The tests in this file are for files that needs to be interactive tests.
// Each test should have a comment why it needs to be interactive.

// Copied as an example page from:
// chrome/browser/safe_browsing/safe_browsing_navigation_observer_browsertest.cc
const char kLandingURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "landing.html";
const char kCopyReferrerUrl[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "copy_referrer.html";
const char kCopyReferrerInnerUrl[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "copy_referrer_inner.html";

class SBNavigationObserverBrowserTest : public InProcessBrowserTest {
 public:
  SBNavigationObserverBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&SBNavigationObserverBrowserTest::web_contents,
                                base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Disable Safe Browsing service so we can directly control when
    // SafeBrowsingNavigationObserverManager and SafeBrowsingNavigationObserver
    // are instantiated.
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                 false);
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    observer_manager_ =
        std::make_unique<TestSafeBrowsingNavigationObserverManager>(browser());
    observer_manager_->ObserveContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(InitialSetup());
  }

  void TearDownOnMainThread() override { observer_manager_.reset(); }

  bool InitialSetup() {
    if (!browser()) {
      return false;
    }

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 false);
    return true;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void AppendRecentNavigations(int recent_navigation_count,
                               ReferrerChain* out_referrer_chain) {
    observer_manager_->AppendRecentNavigations(recent_navigation_count,
                                               out_referrer_chain);
  }

  NavigationEventList* navigation_event_list() {
    return observer_manager_->navigation_event_list();
  }

  void CopyUrlToWebClipboard(std::string urlToCopy,
                             std::optional<int> subframe_index = std::nullopt) {
    TabStripModel* tab_strip = browser()->tab_strip_model();
    content::WebContents* current_web_contents =
        tab_strip->GetActiveWebContents();
    content::RenderFrameHost* script_executing_frame =
        current_web_contents->GetPrimaryMainFrame();
    if (subframe_index.has_value()) {
      script_executing_frame =
          ChildFrameAt(script_executing_frame, subframe_index.value());
    }
    std::string script = base::StringPrintf(
        "navigator.clipboard.writeText('%s');", urlToCopy.c_str());
    ASSERT_TRUE(content::ExecJs(script_executing_frame, script));
  }

  void IdentifyReferrerChainByEventURL(
      const GURL& event_url,
      SessionID event_tab_id,  // Invalid if tab id is unknown or not available.
      const content::GlobalRenderFrameHostId&
          event_outermost_main_frame_id,  // Can also be Invalid.
      ReferrerChain* out_referrer_chain) {
    observer_manager_->IdentifyReferrerChainByEventURL(
        event_url, event_tab_id, event_outermost_main_frame_id,
        2,  // user_gesture_count_limit
        out_referrer_chain);
  }

  void VerifyReferrerChainEntry(
      const GURL& expected_url,
      const GURL& expected_main_frame_url,
      ReferrerChainEntry::URLType expected_type,
      const std::string& expected_ip_address,
      const GURL& expected_referrer_url,
      const GURL& expected_referrer_main_frame_url,
      bool expected_is_retargeting,
      const std::vector<GURL>& expected_server_redirects,
      ReferrerChainEntry::NavigationInitiation expected_navigation_initiation,
      const ReferrerChainEntry& actual_entry) {
    EXPECT_EQ(expected_url.spec(), actual_entry.url());
    if (expected_main_frame_url.is_empty()) {
      EXPECT_FALSE(actual_entry.has_main_frame_url());
    } else {
      // main_frame_url only set if it is different from url.
      EXPECT_EQ(expected_main_frame_url.spec(), actual_entry.main_frame_url());
      EXPECT_NE(expected_main_frame_url.spec(), actual_entry.url());
    }
    EXPECT_EQ(expected_type, actual_entry.type());
    if (expected_ip_address.empty()) {
      ASSERT_EQ(0, actual_entry.ip_addresses_size());
    } else {
      ASSERT_EQ(1, actual_entry.ip_addresses_size());
      EXPECT_EQ(expected_ip_address, actual_entry.ip_addresses(0));
    }
    EXPECT_EQ(expected_referrer_url.spec(), actual_entry.referrer_url());
    if (expected_referrer_main_frame_url.is_empty()) {
      EXPECT_FALSE(actual_entry.has_referrer_main_frame_url());
    } else {
      // referrer_main_frame_url only set if it is different from referrer_url.
      EXPECT_EQ(expected_referrer_main_frame_url.spec(),
                actual_entry.referrer_main_frame_url());
      EXPECT_NE(expected_referrer_main_frame_url.spec(),
                actual_entry.referrer_url());
    }
    EXPECT_EQ(expected_is_retargeting, actual_entry.is_retargeting());
    if (expected_server_redirects.empty()) {
      EXPECT_EQ(0, actual_entry.server_redirect_chain_size());
    } else {
      ASSERT_EQ(static_cast<int>(expected_server_redirects.size()),
                actual_entry.server_redirect_chain_size());
      for (int i = 0; i < actual_entry.server_redirect_chain_size(); i++) {
        EXPECT_EQ(expected_server_redirects[i].spec(),
                  actual_entry.server_redirect_chain(i).url());
      }
    }
    EXPECT_EQ(expected_navigation_initiation,
              actual_entry.navigation_initiation());
  }

 protected:
  std::unique_ptr<TestSafeBrowsingNavigationObserverManager> observer_manager_;

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// This test needs to be interactive as JS copy to clipboard only works for the
// focused window and frame. With regular tests they run in parallel so they
// will fail if the window loose focus before it finished.
// The test use kLandingURL as the target, not using any functionality from
// this page.
// TODO(crbug.com/41487061): Test is flaky on Mac ARM64 builders.
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
#define MAYBE_VerifyCopiedUrlReferrerChain DISABLED_VerifyCopiedUrlReferrerChain
#else
#define MAYBE_VerifyCopiedUrlReferrerChain VerifyCopiedUrlReferrerChain
#endif
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       MAYBE_VerifyCopiedUrlReferrerChain) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kCopyReferrerUrl)));
  CopyUrlToWebClipboard(embedded_test_server()->GetURL(kLandingURL).spec());
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->NavigationEventsSize());

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_TRUE(content::WaitForLoadStop(tab_strip->GetActiveWebContents()));
  ui_test_utils::SendToOmniboxAndSubmit(
      browser(), embedded_test_server()->GetURL(kLandingURL).spec());
  ASSERT_TRUE(content::WaitForLoadStop(tab_strip->GetActiveWebContents()));
  ASSERT_EQ(3U, navigation_event_list()->NavigationEventsSize());

  ReferrerChain referrer_chain;
  AppendRecentNavigations(10, &referrer_chain);
  ASSERT_EQ(3, referrer_chain.size());

  VerifyReferrerChainEntry(
      embedded_test_server()->GetURL(kLandingURL),       // url
      GURL(),                                            // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,             // type
      test_server_ip,                                    // ip_address
      embedded_test_server()->GetURL(kCopyReferrerUrl),  // referrer_url
      GURL(),               // referrer_main_frame_url
      false,                // is_retargeting
      std::vector<GURL>(),  // server redirects
      ReferrerChainEntry::COPY_PASTE_USER_INITIATED, referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      embedded_test_server()->GetURL(kCopyReferrerInnerUrl),  // url
      GURL(),                                                 // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,                  // type
      test_server_ip,                                         // ip_address
      GURL(),                                                 // referrer_url
      embedded_test_server()->GetURL(
          kCopyReferrerUrl),  // referrer_main_frame_url
      false,                  // is_retargeting
      std::vector<GURL>(),    // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      embedded_test_server()->GetURL(kCopyReferrerUrl),  // url
      GURL(),                                            // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,             // type
      test_server_ip,                                    // ip_address
      GURL(),                                            // referrer_url
      GURL(),               // referrer_main_frame_url
      false,                // is_retargeting
      std::vector<GURL>(),  // server redirects
      ReferrerChainEntry::BROWSER_INITIATED, referrer_chain.Get(2));
}

// This test needs to be interactive as JS copy to clipboard only works for the
// focused window and frame. With regular tests they run in parallel so they
// will fail if the window loose focus before it finished.
// The test use kLandingURL as the target, not using any functionality from
// this page.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       VerifyIdentifyReferrerChainByEventURL) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kCopyReferrerUrl)));
  CopyUrlToWebClipboard(embedded_test_server()->GetURL(kLandingURL).spec());
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->NavigationEventsSize());

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_TRUE(content::WaitForLoadStop(tab_strip->GetActiveWebContents()));
  ui_test_utils::SendToOmniboxAndSubmit(
      browser(), embedded_test_server()->GetURL(kLandingURL).spec());
  ASSERT_TRUE(content::WaitForLoadStop(tab_strip->GetActiveWebContents()));
  ASSERT_EQ(3U, navigation_event_list()->NavigationEventsSize());

  ReferrerChain referrer_chain;
  IdentifyReferrerChainByEventURL(
      embedded_test_server()->GetURL(kLandingURL),
      sessions::SessionTabHelper::IdForTab(web_contents()),
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());

  VerifyReferrerChainEntry(
      embedded_test_server()->GetURL(kLandingURL),       // url
      GURL(),                                            // main_frame_url
      ReferrerChainEntry::EVENT_URL,                     // type
      test_server_ip,                                    // ip_address
      embedded_test_server()->GetURL(kCopyReferrerUrl),  // referrer_url
      GURL(),               // referrer_main_frame_url
      false,                // is_retargeting
      std::vector<GURL>(),  // server redirects
      ReferrerChainEntry::COPY_PASTE_USER_INITIATED, referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      embedded_test_server()->GetURL(kCopyReferrerUrl),  // url
      GURL(),                                            // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,               // type
      test_server_ip,                                    // ip_address
      GURL(),                                            // referrer_url
      GURL(),               // referrer_main_frame_url
      false,                // is_retargeting
      std::vector<GURL>(),  // server redirects
      ReferrerChainEntry::BROWSER_INITIATED, referrer_chain.Get(1));
}

// This test needs to be interactive as JS copy to clipboard only works for the
// focused window and frame. With regular tests they run in parallel so they
// will fail if the window loose focus before it finished.
// The test use kLandingURL as the target, not using any functionality from
// this page.
// TODO(crbug.com/41487061): Test is flaky on Mac Arm64 builders.
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
#define MAYBE_VerifyCopiedInnerUrlReferrerChain \
  DISABLED_VerifyCopiedInnerUrlReferrerChain
#else
#define MAYBE_VerifyCopiedInnerUrlReferrerChain \
  VerifyCopiedInnerUrlReferrerChain
#endif
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       MAYBE_VerifyCopiedInnerUrlReferrerChain) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kCopyReferrerUrl)));
  CopyUrlToWebClipboard(embedded_test_server()->GetURL(kLandingURL).spec(),
                        /*subframe_index=*/0);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->NavigationEventsSize());

  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_TRUE(content::WaitForLoadStop(tab_strip->GetActiveWebContents()));
  ui_test_utils::SendToOmniboxAndSubmit(
      browser(), embedded_test_server()->GetURL(kLandingURL).spec());
  ASSERT_TRUE(content::WaitForLoadStop(tab_strip->GetActiveWebContents()));
  ASSERT_EQ(3U, navigation_event_list()->NavigationEventsSize());

  ReferrerChain referrer_chain;
  AppendRecentNavigations(/*recent_navigation_count=*/10, &referrer_chain);
  ASSERT_EQ(3, referrer_chain.size());

  VerifyReferrerChainEntry(
      embedded_test_server()->GetURL(kLandingURL),            // url
      GURL(),                                                 // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,                  // type
      test_server_ip,                                         // ip_address
      embedded_test_server()->GetURL(kCopyReferrerInnerUrl),  // referrer_url
      embedded_test_server()->GetURL(
          kCopyReferrerUrl),  // referrer_main_frame_url
      false,                  // is_retargeting
      std::vector<GURL>(),    // server redirects
      ReferrerChainEntry::COPY_PASTE_USER_INITIATED, referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      embedded_test_server()->GetURL(kCopyReferrerInnerUrl),  // url
      GURL(),                                                 // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,                  // type
      test_server_ip,                                         // ip_address
      GURL(),                                                 // referrer_url
      embedded_test_server()->GetURL(
          kCopyReferrerUrl),  // referrer_main_frame_url
      false,                  // is_retargeting
      std::vector<GURL>(),    // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      embedded_test_server()->GetURL(kCopyReferrerUrl),  // url
      GURL(),                                            // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,             // type
      test_server_ip,                                    // ip_address
      GURL(),                                            // referrer_url
      GURL(),               // referrer_main_frame_url
      false,                // is_retargeting
      std::vector<GURL>(),  // server redirects
      ReferrerChainEntry::BROWSER_INITIATED, referrer_chain.Get(2));
}

}  // namespace safe_browsing
