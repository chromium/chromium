// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/url_canon.h"

using content::DownloadManager;
using download::DownloadItem;

namespace safe_browsing {

const char kSingleFrameTestURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "navigation_observer_tests.html";
const char kMultiFrameTestURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "navigation_observer_multi_frame_tests.html";
const char kRedirectURL[] =
    "/safe_browsing/download_protection/navigation_observer/redirect.html";
// Please update |kShortDataURL| too if you're changing |kDownloadDataURL|.
const char kDownloadDataURL[] =
    "data:application/octet-stream;base64,a2poYWxrc2hkbGtoYXNka2xoYXNsa2RoYWxra"
    "GtoYWxza2hka2xzamFoZGxramhhc2xka2hhc2xrZGgKYXNrZGpoa2FzZGpoYWtzaGRrYXNoZGt"
    "oYXNrZGhhc2tkaGthc2hka2Foc2RraGFrc2hka2FzaGRraGFzCmFza2pkaGFrc2hkbSxjbmtza"
    "mFoZGtoYXNrZGhhc2tka2hrYXNkCjg3MzQ2ODEyNzQ2OGtqc2hka2FoZHNrZGhraApha3NqZGt"
    "hc2Roa3NkaGthc2hka2FzaGtkaAohISomXkAqJl4qYWhpZGFzeWRpeWlhc1xcb1wKa2Fqc2Roa"
    "2FzaGRrYXNoZGsKYWtzamRoc2tkaAplbmQK";
// Short data url is computed by keeping the prefix of |kDownloadDataURL| up to
// the first ",", and appending the hash (SHA256) of entire |kDownloadDataURL|.
// e.g.,
// $echo -n <kDownloadDataURL> | sha256sum |
//  awk '{print "data:application/octet-stream;base64,"toupper($1)}'
const char kShortDataURL[] =
    "data:application/octet-stream;base64,4A19A03B1EF9D2C3061C5B87BF7D0BE05998D"
    "A5F6BA693B6759B47EEA211D246";
const char kIframeDirectDownloadURL[] =
    "/safe_browsing/download_protection/navigation_observer/iframe.html";
const char kIframeRetargetingURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "iframe_retargeting.html";
const char kDownloadItemURL[] = "/safe_browsing/download_protection/signed.exe";
const char kRedirectToLandingURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "redirect_to_landing.html";
const char kLandingURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "landing.html";
const char kLandingReferrerURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "landing_referrer.html";
const char kLandingReferrerURLWithQuery[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "landing_referrer.html?bar=foo";
const char kPageBeforeLandingReferrerURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "page_before_landing_referrer.html";
const char kCreateIframeElementURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "create_iframe_element.html";

class DownloadItemCreatedObserver : public DownloadManager::Observer {
 public:
  explicit DownloadItemCreatedObserver(DownloadManager* manager)
      : manager_(manager) {
    manager->AddObserver(this);
  }

  ~DownloadItemCreatedObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  // Wait for the first download item created after object creation.
  void WaitForDownloadItem(std::vector<DownloadItem*>* items_seen) {
    if (!manager_) {
      // The manager went away before we were asked to wait; return
      // what we have, even if it's null.
      *items_seen = items_seen_;
      return;
    }

    if (items_seen_.empty()) {
      base::RunLoop run_loop;
      quit_waiting_callback_ = run_loop.QuitClosure();
      run_loop.Run();
      quit_waiting_callback_ = base::Closure();
    }

    *items_seen = items_seen_;
    return;
  }

 private:
  // DownloadManager::Observer
  void OnDownloadCreated(DownloadManager* manager,
                         DownloadItem* item) override {
    DCHECK_EQ(manager, manager_);
    items_seen_.push_back(item);

    if (!quit_waiting_callback_.is_null())
      quit_waiting_callback_.Run();
  }

  void ManagerGoingDown(DownloadManager* manager) override {
    manager_->RemoveObserver(this);
    manager_ = NULL;
    if (!quit_waiting_callback_.is_null())
      quit_waiting_callback_.Run();
  }

  base::Closure quit_waiting_callback_;
  DownloadManager* manager_;
  std::vector<DownloadItem*> items_seen_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemCreatedObserver);
};

// Test class to help create SafeBrowsingNavigationObservers for each
// WebContents before they are actually installed through AttachTabHelper.
class TestNavigationObserverManager
    : public SafeBrowsingNavigationObserverManager,
      public TabStripModelObserver {
 public:
  explicit TestNavigationObserverManager(Browser* browser) {
    browser->tab_strip_model()->AddObserver(this);
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kInserted)
      return;

    for (const TabStripModelChange::ContentsWithIndex& tab :
         change.GetInsert()->contents) {
      content::WebContents* dest_content = tab.contents;
      DCHECK(dest_content);
      observer_list_.push_back(
          std::make_unique<SafeBrowsingNavigationObserver>(dest_content, this));
      DCHECK(observer_list_.back());
    }
  }

 protected:
  ~TestNavigationObserverManager() override = default;

 private:
  std::vector<std::unique_ptr<SafeBrowsingNavigationObserver>> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationObserverManager);
};

class SBNavigationObserverBrowserTest : public InProcessBrowserTest {
 public:
  SBNavigationObserverBrowserTest() {}

  void SetUpOnMainThread() override {
    // Disable Safe Browsing service so we can directly control when
    // SafeBrowsingNavigationObserverManager and SafeBrowsingNavigationObserver
    // are instantiated.
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                 false);
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    observer_manager_ = new TestNavigationObserverManager(browser());
    observer_ = new SafeBrowsingNavigationObserver(
        browser()->tab_strip_model()->GetActiveWebContents(),
        observer_manager_);
    ASSERT_TRUE(observer_);
    ASSERT_TRUE(InitialSetup());
  }

  bool InitialSetup() {
    if (!browser())
      return false;

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 false);
    content::DownloadManager* manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    DownloadPrefs::FromDownloadManager(manager)->ResetAutoOpen();

    return true;
  }

  void TearDownOnMainThread() override {
    // Cancel unfinished download if any.
    CancelDownloads();
    delete observer_;
  }

  // Most test cases will trigger downloads, though we don't really care if
  // download completed or not. So we cancel downloads as soon as we record
  // all the navigation events we need.
  void CancelDownloads() {
    std::vector<DownloadItem*> download_items;
    content::DownloadManager* manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    manager->GetAllDownloads(&download_items);
    for (auto* item : download_items) {
      if (!item->IsDone())
        item->Cancel(true);
    }
  }

  DownloadItem* GetDownload() {
    std::vector<DownloadItem*> download_items;
    content::DownloadManager* manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    manager->GetAllDownloads(&download_items);
    if (download_items.empty())
      DownloadItemCreatedObserver(manager).WaitForDownloadItem(&download_items);
    EXPECT_EQ(1U, download_items.size());
    return download_items[0];
  }

  // This function needs javascript support from the test page hosted at
  // |page_url|. It calls "clickLink(..)" javascript function to "click" on the
  // html element with ID specified by |element_id|, and waits for
  // |number_of_navigations| to complete.  If a |subframe_index| is specified,
  // |element_id| is assumed to be in corresponding subframe of the test page,
  // and the javascript function is executed that subframe.
  void ClickTestLink(const char* element_id,
                     int number_of_navigations,
                     const GURL& page_url,
                     int subframe_index = -1) {
    TabStripModel* tab_strip = browser()->tab_strip_model();
    content::WebContents* current_web_contents =
        tab_strip->GetActiveWebContents();
    ASSERT_TRUE(content::WaitForLoadStop(current_web_contents));
    content::TestNavigationObserver navigation_observer(
        current_web_contents, number_of_navigations,
        content::MessageLoopRunner::QuitMode::DEFERRED);
    navigation_observer.StartWatchingNewWebContents();
    // Execute test.
    {
      std::string script = base::StringPrintf("clickLink('%s');", element_id);
      content::RenderFrameHost* script_executing_frame =
          current_web_contents->GetMainFrame();
      if (subframe_index != -1) {
        script_executing_frame =
            ChildFrameAt(script_executing_frame, subframe_index);
      }
      ASSERT_TRUE(content::ExecuteScript(script_executing_frame, script));
    }
    // Wait for navigations on current tab and new tab (if any) to finish.
    navigation_observer.Wait();
    navigation_observer.StopWatchingNewWebContents();

    // Since this test uses javascript to mimic clicking on a link (no actual
    // user gesture), and DidGetUserInteraction() does not respond to
    // ExecuteScript(), navigation_transition field in resulting
    // NavigationEvents will always be RENDERER_INITIATED_WITHOUT_USER_GESTURE.
    // Therefore, we need to make some adjustment to relevant NavigationEvent.
    for (std::size_t i = 0U; i < navigation_event_list()->Size(); i++) {
      auto* nav_event = navigation_event_list()->Get(i);
      if (nav_event->source_url == page_url) {
        nav_event->navigation_initiation =
            ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
        return;
      }
    }
  }

  void TriggerDownloadViaHtml5FileApi() {
    std::vector<DownloadItem*> items;
    content::DownloadManager* manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    content::WebContents* current_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(
        content::ExecuteScript(current_web_contents, "downloadViaFileApi()"));
    manager->GetAllDownloads(&items);
    ASSERT_EQ(0U, items.size());
  }

  void VerifyNavigationEvent(const GURL& expected_source_url,
                             const GURL& expected_source_main_frame_url,
                             const GURL& expected_original_request_url,
                             const GURL& expected_destination_url,
                             bool expected_is_user_initiated,
                             bool expected_has_committed,
                             bool expected_has_server_redirect,
                             NavigationEvent* actual_nav_event) {
    EXPECT_EQ(expected_source_url, actual_nav_event->source_url);
    EXPECT_EQ(expected_source_main_frame_url,
              actual_nav_event->source_main_frame_url);
    EXPECT_EQ(expected_original_request_url,
              actual_nav_event->original_request_url);
    EXPECT_EQ(expected_destination_url, actual_nav_event->GetDestinationUrl());
    EXPECT_EQ(expected_is_user_initiated, actual_nav_event->IsUserInitiated());
    EXPECT_EQ(expected_has_committed, actual_nav_event->has_committed);
    EXPECT_EQ(expected_has_server_redirect,
              !actual_nav_event->server_redirect_urls.empty());
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

  // Identify referrer chain of a DownloadItem and populate |referrer_chain|.
  void IdentifyReferrerChainForDownload(
      DownloadItem* download,
      ReferrerChain* referrer_chain) {
    SessionID download_tab_id = SessionTabHelper::IdForTab(
        content::DownloadItemUtils::GetWebContents(download));
    auto result = observer_manager_->IdentifyReferrerChainByEventURL(
        download->GetURL(), download_tab_id,
        2,  // kDownloadAttributionUserGestureLimit
        referrer_chain);
    if (result ==
        SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND) {
      DCHECK_EQ(0, referrer_chain->size());
      observer_manager_->IdentifyReferrerChainByWebContents(
          content::DownloadItemUtils::GetWebContents(download),
          2,  // kDownloadAttributionUserGestureLimit
          referrer_chain);
    }
  }

  void IdentifyReferrerChainForWebContents(content::WebContents* web_contents,
                                           ReferrerChain* referrer_chain) {
    observer_manager_->IdentifyReferrerChainByWebContents(
        web_contents,
        2,  // kDownloadAttributionUserGestureLimit
        referrer_chain);
  }

  // Identify referrer chain of a PPAPI download and populate |referrer_chain|.
  void IdentifyReferrerChainForPPAPIDownload(
      const GURL& initiating_frame_url,
      content::WebContents* web_contents,
      ReferrerChain* referrer_chain) {
    SessionID tab_id = SessionTabHelper::IdForTab(web_contents);
    bool has_user_gesture = observer_manager_->HasUserGesture(web_contents);
    observer_manager_->OnUserGestureConsumed(web_contents, base::Time::Now());
    EXPECT_LE(observer_manager_->IdentifyReferrerChainByHostingPage(
                  initiating_frame_url, web_contents->GetLastCommittedURL(),
                  tab_id, has_user_gesture,
                  2,  // kDownloadAttributionUserGestureLimit
                  referrer_chain),
              SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER);
  }

  void VerifyHostToIpMap() {
    // Since all testing pages have the same host, there is only one entry in
    // host_to_ip_map_.
    SafeBrowsingNavigationObserverManager::HostToIpMap* actual_host_ip_map =
        host_to_ip_map();
    ASSERT_EQ(1U, actual_host_ip_map->size());
    auto ip_list =
        (*actual_host_ip_map)[embedded_test_server()->base_url().host()];
    ASSERT_EQ(1U, ip_list.size());
    EXPECT_EQ(embedded_test_server()->host_port_pair().host(),
              ip_list.back().ip);
  }

  void SimulateUserGesture(){
    observer_manager_->RecordUserGestureForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::Time::Now());
  }

  NavigationEventList* navigation_event_list() {
    return observer_manager_->navigation_event_list();
  }

  SafeBrowsingNavigationObserverManager::HostToIpMap* host_to_ip_map() {
    return observer_manager_->host_to_ip_map();
  }

  int CountOfRecentNavigationsToAppend(
      bool extended_reporting_enabled,
      bool is_incognito,
      SafeBrowsingNavigationObserverManager::AttributionResult result) {
    SetExtendedReportingPref(browser()->profile()->GetPrefs(),
                             extended_reporting_enabled);
    return SafeBrowsingNavigationObserverManager::
        CountOfRecentNavigationsToAppend(
            is_incognito ? *browser()->profile()->GetOffTheRecordProfile()
                         : *browser()->profile(),
            result);
  }

  void AppendRecentNavigations(int recent_navigation_count,
                               ReferrerChain* out_referrer_chain) {
    observer_manager_->AppendRecentNavigations(recent_navigation_count,
                                               out_referrer_chain);
  }

  void FindAndAddNavigationToReferrerChain(ReferrerChain* referrer_chain,
                                           const GURL& target_url) {
    NavigationEvent* nav_event =
        observer_manager_->navigation_event_list()->FindNavigationEvent(
            base::Time::Now(), target_url, GURL(), SessionID::InvalidValue());
    if (nav_event) {
      observer_manager_->AddToReferrerChain(referrer_chain, nav_event, GURL(),
                                            ReferrerChainEntry::EVENT_URL);
    }
  }

 protected:
  SafeBrowsingNavigationObserverManager* observer_manager_;
  SafeBrowsingNavigationObserver* observer_;
};

// Type download URL into address bar and start download on the same page.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, TypeInURLDownload) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kDownloadItemURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(1, referrer_chain.size());
  VerifyReferrerChainEntry(download_url,                   // url
                           GURL(),                         // main_frame_url
                           ReferrerChainEntry::EVENT_URL,  // type
                           test_server_ip,                 // ip_address
                           GURL(),                         // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(0));
}
// Click on a link and start download on the same page.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, DirectDownload) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("direct_download", 1, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      initial_url,                    // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));
}

// Click on a link with rel="noreferrer" attribute, and start download on the
// same page.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       DirectDownloadNoReferrer) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("direct_download_noreferrer", 1, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      initial_url,                    // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));
}

// Click on a link with rel="noreferrer" attribute, and start download in a
// new tab using target=_blank.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       DirectDownloadNoReferrerTargetBlank) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("direct_download_noreferrer_target_blank", 1, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  // The next NavigationEvent opens a new tab.
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));
  // This one is the actual navigation which triggers download.
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(2));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      initial_url,                    // referrer_url
      GURL(),                         // referrer_main_frame_url
      true,                           // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));
}

// Click on a link which navigates to a page then redirects to a download using
// META HTTP-EQUIV="refresh". All transitions happen in the same tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       SingleMetaRefreshRedirect) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("single_meta_refresh_redirect", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  // Since unlike server redirects client redirects commit and then generate a
  // second navigation, our observer records two NavigationEvents for this test.
  ASSERT_EQ(3U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(2));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(3, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      redirect_url,                   // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      redirect_url,                         // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(2));
}

// Click on a link which navigates to a page then redirects to a download using
// META HTTP-EQUIV="refresh". First navigation happens in target blank.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       SingleMetaRefreshRedirectTargetBlank) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("single_meta_refresh_redirect_target_blank", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(4U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(2));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(3));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(3, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      redirect_url,                   // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      redirect_url,                         // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      true,                                 // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(2));
}

// Click on a link which redirects twice before reaching download using
// META HTTP-EQUIV="refresh". All transitions happen in the same tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       MultiMetaRefreshRedirects) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("multiple_meta_refresh_redirects", 3, initial_url);
  GURL first_redirect_url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/navigation_observer/"
      "double_redirect.html");
  GURL second_redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(4U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,         // source_url
                        initial_url,         // source_main_frame_url
                        first_redirect_url,  // original_request_url
                        first_redirect_url,  // destination_url
                        true,                // is_user_initiated,
                        true,                // has_committed
                        false,               // has_server_redirect
                        nav_list->Get(1));
  VerifyNavigationEvent(first_redirect_url,   // source_url
                        first_redirect_url,   // source_main_frame_url
                        second_redirect_url,  // original_request_url
                        second_redirect_url,  // destination_url
                        false,                // is_user_initiated,
                        true,                 // has_committed
                        false,                // has_server_redirect
                        nav_list->Get(2));
  VerifyNavigationEvent(second_redirect_url,  // source_url
                        second_redirect_url,  // source_main_frame_url
                        download_url,         // original_request_url
                        download_url,         // destination_url
                        false,                // is_user_initiated,
                        false,                // has_committed
                        false,                // has_server_redirect
                        nav_list->Get(3));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(4, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      second_redirect_url,            // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      second_redirect_url,                  // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      first_redirect_url,                   // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      first_redirect_url,                   // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(2));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(3));
}

// Click on a link which redirects to download using window.location.
// All transitions happen in the same tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       WindowLocationRedirect) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("window_location_redirection", 1, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      initial_url,                    // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));
}

// Click on a link which redirects twice until it reaches download using a
// mixture of meta refresh and window.location. All transitions happen in the
// same tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, MixRedirects) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("mix_redirects", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(2));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(3, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      redirect_url,                   // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      redirect_url,                         // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(2));
}

// Use javascript to open download in a new tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, NewTabDownload) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("new_tab_download", 2, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL blank_url = GURL(url::kAboutBlankURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(4U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,  // source_url
                        initial_url,  // source_main_frame_url
                        blank_url,    // original_request_url
                        blank_url,    // destination_url
                        true,         // is_user_initiated,
                        false,        // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(1));
  // Source and target are at different tabs.
  EXPECT_NE(nav_list->Get(1)->source_tab_id, nav_list->Get(1)->target_tab_id);
  VerifyNavigationEvent(GURL(),     // source_url
                        GURL(),     // source_main_frame_url
                        blank_url,  // original_request_url
                        blank_url,  // destination_url
                        false,      // is_user_initiated,
                        false,      // has_committed
                        false,      // has_server_redirect
                        nav_list->Get(2));
  EXPECT_EQ(nav_list->Get(2)->source_tab_id, nav_list->Get(2)->target_tab_id);
  VerifyNavigationEvent(blank_url,     // source_url
                        blank_url,     // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(3));
  EXPECT_EQ(nav_list->Get(3)->source_tab_id, nav_list->Get(3)->target_tab_id);
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(3, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      blank_url,                      // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      blank_url,                            // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      "",                                   // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      true,                                 // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(2));
}

// Use javascript to open download in a new tab and download has a data url.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       NewTabDownloadWithDataURL) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("new_tab_download_with_data_url", 2, initial_url);
  GURL download_url = GURL(kDownloadDataURL);
  GURL short_download_url = GURL(kShortDataURL);
  GURL blank_url = GURL(url::kAboutBlankURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(4U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  // The second navigation event is a retargeting navigation.
  VerifyNavigationEvent(initial_url,  // source_url
                        initial_url,  // source_main_frame_url
                        blank_url,    // original_request_url
                        blank_url,    // destination_url
                        true,         // is_user_initiated,
                        false,        // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(1));
  // Source and target are at different tabs.
  EXPECT_FALSE(nav_list->Get(1)->source_tab_id ==
               nav_list->Get(1)->target_tab_id);
  VerifyNavigationEvent(GURL(),     // source_url
                        GURL(),     // source_main_frame_url
                        blank_url,  // original_request_url
                        blank_url,  // destination_url
                        false,      // is_user_initiated,
                        false,      // has_committed
                        false,      // has_server_redirect
                        nav_list->Get(2));
  EXPECT_EQ(nav_list->Get(2)->source_tab_id, nav_list->Get(2)->target_tab_id);
  VerifyNavigationEvent(blank_url,     // source_url
                        blank_url,     // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(3));
  EXPECT_TRUE(nav_list->Get(3)->source_tab_id ==
              nav_list->Get(3)->target_tab_id);
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(3, referrer_chain.size());
  VerifyReferrerChainEntry(
      short_download_url,             // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      "",                             // ip_address
      blank_url,                      // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      blank_url,                            // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      "",                                   // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      true,                                 // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(2));
}

// Click a link in a subframe and start download.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       SubFrameDirectDownload) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("sub_frame_download_attribution", 1, initial_url);
  GURL multi_frame_test_url =
      embedded_test_server()->GetURL(kMultiFrameTestURL);
  GURL iframe_url = embedded_test_server()->GetURL(kIframeDirectDownloadURL);
  ClickTestLink("iframe_direct_download", 1, iframe_url, 0);
  GURL iframe_retargeting_url =
      embedded_test_server()->GetURL(kIframeRetargetingURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(5U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,           // source_url
                        initial_url,           // source_main_frame_url
                        multi_frame_test_url,  // original_request_url
                        multi_frame_test_url,  // destination_url
                        true,                  // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_list->Get(1));
  // The order of the next two navigation events may vary. We check for both
  // possibilities. Their order doesn't impact referrer chain attribution logic.
  if (nav_list->Get(2)->original_request_url == iframe_url) {
    VerifyNavigationEvent(GURL(),                // source_url
                          multi_frame_test_url,  // source_main_frame_url
                          iframe_url,            // original_request_url
                          iframe_url,            // destination_url
                          false,                 // is_user_initiated,
                          true,                  // has_committed
                          false,                 // has_server_redirect
                          nav_list->Get(2));
    VerifyNavigationEvent(GURL(),                  // source_url
                          multi_frame_test_url,    // source_main_frame_url
                          iframe_retargeting_url,  // original_request_url
                          iframe_retargeting_url,  // destination_url
                          false,                   // is_user_initiated,
                          true,                    // has_committed
                          false,                   // has_server_redirect
                          nav_list->Get(3));
  } else {
    VerifyNavigationEvent(GURL(),                  // source_url
                          multi_frame_test_url,    // source_main_frame_url
                          iframe_retargeting_url,  // original_request_url
                          iframe_retargeting_url,  // destination_url
                          false,                   // is_user_initiated,
                          true,                    // has_committed
                          false,                   // has_server_redirect
                          nav_list->Get(2));
    VerifyNavigationEvent(GURL(),                // source_url
                          multi_frame_test_url,  // source_main_frame_url
                          iframe_url,            // original_request_url
                          iframe_url,            // destination_url
                          false,                 // is_user_initiated,
                          true,                  // has_committed
                          false,                 // has_server_redirect
                          nav_list->Get(3));
  }
  VerifyNavigationEvent(iframe_url,            // source_url
                        multi_frame_test_url,  // source_main_frame_url
                        download_url,          // original_request_url
                        download_url,          // destination_url
                        true,                  // is_user_initiated,
                        false,                 // has_committed
                        false,                 // has_server_redirect
                        nav_list->Get(4));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(4, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      iframe_url,                     // referrer_url
      multi_frame_test_url,           // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      iframe_url,                        // url
      multi_frame_test_url,              // main_frame_url
      ReferrerChainEntry::LANDING_PAGE,  // type
      test_server_ip,                    // ip_address
      GURL(),                            // referrer_url
      multi_frame_test_url,              // referrer_main_frame_url
      false,                             // is_retargeting
      std::vector<GURL>(),               // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      multi_frame_test_url,                 // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(2));
  VerifyReferrerChainEntry(initial_url,  // url
                           GURL(),       // main_frame_url
                           ReferrerChainEntry::LANDING_REFERRER,  // type
                           test_server_ip,                        // ip_address
                           GURL(),               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(3));
}

// Click a link in a subframe and open download in a new tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       SubFrameNewTabDownload) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("sub_frame_download_attribution", 1, initial_url);
  GURL multi_frame_test_url =
      embedded_test_server()->GetURL(kMultiFrameTestURL);
  GURL iframe_url = embedded_test_server()->GetURL(kIframeDirectDownloadURL);
  GURL iframe_retargeting_url =
      embedded_test_server()->GetURL(kIframeRetargetingURL);
  ClickTestLink("iframe_new_tab_download", 2, iframe_retargeting_url, 1);
  GURL blank_url = GURL(url::kAboutBlankURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(7U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,           // source_url
                        initial_url,           // source_main_frame_url
                        multi_frame_test_url,  // original_request_url
                        multi_frame_test_url,  // destination_url
                        true,                  // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_list->Get(1));
  // The order of the next two navigation events may vary. We check for both
  // possibilities. Their order doesn't impact referrer chain attribution logic.
  if (nav_list->Get(2)->original_request_url == iframe_url) {
    VerifyNavigationEvent(GURL(),                // source_url
                          multi_frame_test_url,  // source_main_frame_url
                          iframe_url,            // original_request_url
                          iframe_url,            // destination_url
                          false,                 // is_user_initiated,
                          true,                  // has_committed
                          false,                 // has_server_redirect
                          nav_list->Get(2));
    VerifyNavigationEvent(GURL(),                  // source_url
                          multi_frame_test_url,    // source_main_frame_url
                          iframe_retargeting_url,  // original_request_url
                          iframe_retargeting_url,  // destination_url
                          false,                   // is_user_initiated,
                          true,                    // has_committed
                          false,                   // has_server_redirect
                          nav_list->Get(3));
  } else {
    VerifyNavigationEvent(GURL(),                  // source_url
                          multi_frame_test_url,    // source_main_frame_url
                          iframe_retargeting_url,  // original_request_url
                          iframe_retargeting_url,  // destination_url
                          false,                   // is_user_initiated,
                          true,                    // has_committed
                          false,                   // has_server_redirect
                          nav_list->Get(2));
    VerifyNavigationEvent(GURL(),                // source_url
                          multi_frame_test_url,  // source_main_frame_url
                          iframe_url,            // original_request_url
                          iframe_url,            // destination_url
                          false,                 // is_user_initiated,
                          true,                  // has_committed
                          false,                 // has_server_redirect
                          nav_list->Get(3));
  }
  VerifyNavigationEvent(iframe_retargeting_url,  // source_url
                        multi_frame_test_url,    // source_main_frame_url
                        blank_url,               // original_request_url
                        blank_url,               // destination_url
                        true,                    // is_user_initiated,
                        false,                   // has_committed
                        false,                   // has_server_redirect
                        nav_list->Get(4));
  VerifyNavigationEvent(GURL(),     // source_url
                        GURL(),     // source_main_frame_url
                        blank_url,  // original_request_url
                        blank_url,  // destination_url
                        false,      // is_user_initiated,
                        false,      // has_committed
                        false,      // has_server_redirect
                        nav_list->Get(5));
  VerifyNavigationEvent(blank_url,     // source_url
                        blank_url,     // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(6));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  EXPECT_EQ(5, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      blank_url,                      // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      blank_url,                            // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      "",                                   // ip_address
      iframe_retargeting_url,               // referrer_url
      multi_frame_test_url,                 // referrer_main_frame_url
      true,                                 // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      iframe_retargeting_url,            // url
      multi_frame_test_url,              // main_frame_url
      ReferrerChainEntry::LANDING_PAGE,  // type
      test_server_ip,                    // ip_address
      GURL(),                            // referrer_url
      multi_frame_test_url,              // referrer_main_frame_url
      false,                             // is_retargeting
      std::vector<GURL>(),               // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(2));
  VerifyReferrerChainEntry(
      multi_frame_test_url,                 // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(3));
  VerifyReferrerChainEntry(initial_url,  // url
                           GURL(),       // main_frame_url
                           ReferrerChainEntry::LANDING_REFERRER,  // type
                           test_server_ip,                        // ip_address
                           GURL(),               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(4));
}

// Click a link which redirects to the landing page, and then click on the
// landing page to trigger download.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, CompleteReferrerChain) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectToLandingURL);
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  ClickTestLink("download_on_landing_page", 1, landing_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(4U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        landing_url,   // original_request_url
                        landing_url,   // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(2));
  VerifyNavigationEvent(landing_url,   // source_url
                        landing_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(3));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  EXPECT_EQ(4, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      landing_url,                    // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      landing_url,                       // url
      GURL(),                            // main_frame_url
      ReferrerChainEntry::LANDING_PAGE,  // type
      test_server_ip,                    // ip_address
      redirect_url,                      // referrer_url
      GURL(),                            // referrer_main_frame_url
      false,                             // is_retargeting
      std::vector<GURL>(),               // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      redirect_url,                         // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(2));
  VerifyReferrerChainEntry(
      initial_url,                           // url
      GURL(),                                // main_frame_url
      ReferrerChainEntry::LANDING_REFERRER,  // type
      test_server_ip,                        // ip_address
      GURL(),  // referrer_url is empty since this beyonds 2 clicks.
      GURL(),  // referrer_main_frame_url is empty for the same reason.
      false,   // is_retargeting
      std::vector<GURL>(),  // server redirects
      ReferrerChainEntry::BROWSER_INITIATED, referrer_chain.Get(3));
}

// Click three links before reaching download.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       ReferrerAttributionWithinTwoUserGestures) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("attribution_within_two_user_gestures", 1, initial_url);
  GURL page_before_landing_referrer_url =
      embedded_test_server()->GetURL(kPageBeforeLandingReferrerURL);
  ClickTestLink("link_to_landing_referrer", 1,
                page_before_landing_referrer_url);
  GURL landing_referrer_url =
      embedded_test_server()->GetURL(kLandingReferrerURL);
  ClickTestLink("link_to_landing", 1, landing_referrer_url);
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  ClickTestLink("download_on_landing_page", 1, landing_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(5U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,  // source_url
                        initial_url,  // source_main_frame_url
                        page_before_landing_referrer_url,  // original_request
                        page_before_landing_referrer_url,  // destination_url
                        true,                              // is_user_initiated,
                        true,                              // has_committed
                        false,  // has_server_redirect
                        nav_list->Get(1));
  VerifyNavigationEvent(page_before_landing_referrer_url,  // source_url
                        page_before_landing_referrer_url,  // source_main_frame
                        landing_referrer_url,  // original_request_url
                        landing_referrer_url,  // destination_url
                        true,                  // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_list->Get(2));
  VerifyNavigationEvent(landing_referrer_url,  // source_url
                        landing_referrer_url,  // source_main_frame
                        landing_url,           // original_request_url
                        landing_url,           // destination_url
                        true,                  // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_list->Get(3));
  VerifyNavigationEvent(landing_url,   // source_url
                        landing_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(4));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  EXPECT_EQ(3, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      landing_url,                    // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      landing_url,                       // url
      GURL(),                            // main_frame_url
      ReferrerChainEntry::LANDING_PAGE,  // type
      test_server_ip,                    // ip_address
      landing_referrer_url,              // referrer_url
      GURL(),                            // referrer_main_frame_url
      false,                             // is_retargeting
      std::vector<GURL>(),               // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      landing_referrer_url,                  // url
      GURL(),                                // main_frame_url
      ReferrerChainEntry::LANDING_REFERRER,  // type
      test_server_ip,                        // ip_address
      GURL(),  // referrer_url is empty since this beyonds 2 clicks.
      GURL(),  // referrer_main_frame_url is empty for the same reason.
      false,   // is_retargeting
      std::vector<GURL>(),  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(2));
  // page_before_landing_referrer_url is not in referrer chain.
}

// Click a link which redirects to a PPAPI landing page.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       PPAPIDownloadWithUserGestureOnHostingFrame) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectToLandingURL);
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  // Simulate a user gesture on landing page.
  SimulateUserGesture();
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        landing_url,   // original_request_url
                        landing_url,   // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(2));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForPPAPIDownload(
      landing_url,
      browser()->tab_strip_model()->GetActiveWebContents(),
      &referrer_chain);
  EXPECT_EQ(3, referrer_chain.size());
  VerifyReferrerChainEntry(
      landing_url,                       // url
      GURL(),                            // main_frame_url
      ReferrerChainEntry::LANDING_PAGE,  // type
      test_server_ip,                    // ip_address
      redirect_url,                      // referrer_url
      GURL(),                            // referrer_main_frame_url
      false,                             // is_retargeting
      std::vector<GURL>(),               // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      redirect_url,                         // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      initial_url,                           // url
      GURL(),                                // main_frame_url
      ReferrerChainEntry::LANDING_REFERRER,  // type
      test_server_ip,                        // ip_address
      GURL(),  // referrer_url is empty since this beyonds 2 clicks.
      GURL(),  // referrer_main_frame_url is empty for the same reason.
      false,   // is_retargeting
      std::vector<GURL>(),  // server redirects
      ReferrerChainEntry::BROWSER_INITIATED, referrer_chain.Get(2));
}

// Click a link which redirects to a page that triggers PPAPI download without
// user gesture (a.k.a not a landing page).
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       PPAPIDownloadWithoutUserGestureOnHostingFrame) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL landing_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, landing_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectToLandingURL);
  GURL hosting_url = embedded_test_server()->GetURL(kLandingURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        landing_url,  // original_request_url
                        landing_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(landing_url,   // source_url
                        landing_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        hosting_url,   // original_request_url
                        hosting_url,   // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->Get(2));
  VerifyHostToIpMap();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForPPAPIDownload(
      hosting_url,
      browser()->tab_strip_model()->GetActiveWebContents(),
      &referrer_chain);
  EXPECT_EQ(3, referrer_chain.size());
  VerifyReferrerChainEntry(
      hosting_url,                          // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      redirect_url,                         // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      redirect_url,                         // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      landing_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(landing_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),  // no more referrer before landing_url
                           GURL(),
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(2));
}

// Server-side redirect.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, ServerRedirect) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL request_url =
      embedded_test_server()->GetURL("/server-redirect?" + download_url.spec());
  ui_test_utils::NavigateToURL(browser(), request_url);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        request_url,   // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        true,          // has_server_redirect
                        nav_list->Get(1));

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(1, referrer_chain.size());
  VerifyReferrerChainEntry(download_url,                   // url
                           GURL(),                         // main_frame_url
                           ReferrerChainEntry::EVENT_URL,  // type
                           test_server_ip,                 // ip_address
                           GURL(),                         // referrer_url
                           GURL(),  // referrer_main_frame_url
                           false,   // is_retargeting
                           {request_url, download_url},  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(0));
}

// 2 consecutive server-side redirects.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, TwoServerRedirects) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL destination_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL redirect_url = embedded_test_server()->GetURL("/server-redirect?" +
                                                     destination_url.spec());
  GURL request_url =
      embedded_test_server()->GetURL("/server-redirect?" + redirect_url.spec());
  ui_test_utils::NavigateToURL(browser(), request_url);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(GURL(),           // source_url
                        GURL(),           // source_main_frame_url
                        request_url,      // original_request_url
                        destination_url,  // destination_url
                        true,             // is_user_initiated,
                        false,            // has_committed
                        true,             // has_server_redirect
                        nav_list->Get(1));
  const auto redirect_vector = nav_list->Get(1)->server_redirect_urls;
  ASSERT_EQ(2U, redirect_vector.size());
  EXPECT_EQ(redirect_url, redirect_vector[0]);
  EXPECT_EQ(destination_url, redirect_vector[1]);

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(1, referrer_chain.size());
  VerifyReferrerChainEntry(destination_url,                // url
                           GURL(),                         // main_frame_url
                           ReferrerChainEntry::EVENT_URL,  // type
                           test_server_ip,                 // ip_address
                           GURL(),                         // referrer_url
                           GURL(),  // referrer_main_frame_url
                           false,   // is_retargeting
                           {request_url, redirect_url, destination_url},
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(0));
}

// Retargeting immediately followed by server-side redirect.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       RetargetingAndServerRedirect) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL request_url =
      embedded_test_server()->GetURL("/server-redirect?" + download_url.spec());
  ClickTestLink("new_tab_download_with_server_redirect", 1, initial_url);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyNavigationEvent(initial_url,  // source_url
                        initial_url,  // source_main_frame_url
                        request_url,  // original_request_url
                        request_url,  // destination_url
                        true,         // is_user_initiated,
                        false,        // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(1));
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        request_url,   // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        true,          // has_server_redirect
                        nav_list->Get(2));

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      initial_url,                    // referrer_url
      GURL(),                         // referrer_main_frame_url
      true,                           // is_retargeting
      {request_url, download_url},    // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(initial_url,                       // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));
}

// host_to_ip_map_ size should increase by one after a new navigation.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, AddIPMapping) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  auto* ip_map = host_to_ip_map();
  std::string test_server_host(embedded_test_server()->base_url().host());
  ip_map->clear();
  ip_map->insert(
      std::make_pair(test_server_host, std::vector<ResolvedIPAddress>()));
  ASSERT_EQ(std::size_t(0), (*ip_map)[test_server_host].size());
  ClickTestLink("direct_download", 1, initial_url);
  EXPECT_EQ(1U, (*ip_map)[test_server_host].size());
  EXPECT_EQ(embedded_test_server()->host_port_pair().host(),
            (*ip_map)[test_server_host].back().ip);
}

// If we have already seen an IP associated with a host, update its timestamp.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, IPListDedup) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  auto* ip_map = host_to_ip_map();
  ip_map->clear();
  std::string test_server_host(embedded_test_server()->base_url().host());
  ip_map->insert(
      std::make_pair(test_server_host, std::vector<ResolvedIPAddress>()));
  base::Time yesterday(base::Time::Now() - base::TimeDelta::FromDays(1));
  (*ip_map)[test_server_host].push_back(ResolvedIPAddress(
      yesterday, embedded_test_server()->host_port_pair().host()));
  ASSERT_EQ(1U, (*ip_map)[test_server_host].size());
  ClickTestLink("direct_download", 1, initial_url);
  EXPECT_EQ(1U, (*ip_map)[test_server_host].size());
  EXPECT_EQ(embedded_test_server()->host_port_pair().host(),
            (*ip_map)[test_server_host].back().ip);
  EXPECT_NE(yesterday, (*ip_map)[test_server_host].front().timestamp);
}

// Download via html5 file API.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       DownloadViaHTML5FileApi) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL hosting_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  TriggerDownloadViaHtml5FileApi();
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(1U, nav_list->Size());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        hosting_url,  // original_request_url
                        hosting_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->Get(0));
  VerifyHostToIpMap();
  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(1, referrer_chain.size());

  VerifyReferrerChainEntry(hosting_url,  // url
                           GURL(),       // main_frame_url
                           ReferrerChainEntry::CLIENT_REDIRECT,  // type
                           test_server_ip,                       // ip_address
                           GURL(),                               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(0));
}

// Verify referrer chain when there are URL fragments.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       DownloadAttributionWithURLFragment) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  // Clicks on link and navigates to ".../page_before_landing_referrer.html".
  ClickTestLink("attribution_should_ignore_url_fragments", 1, initial_url);
  GURL expected_page_before_landing_referrer_url =
      embedded_test_server()->GetURL(kPageBeforeLandingReferrerURL);
  // Clicks on link and navigates to ".../landing_referrer.html?bar=foo#baz".
  ClickTestLink("link_to_landing_referrer_with_query_and_fragment", 1,
                expected_page_before_landing_referrer_url);
  GURL expected_landing_referrer_url_with_query =
      embedded_test_server()->GetURL(kLandingReferrerURLWithQuery);
  // Clicks on link and navigates to ".../landing.html#".
  ClickTestLink("link_to_landing_with_empty_fragment", 1,
                expected_landing_referrer_url_with_query);
  GURL expected_landing_url = embedded_test_server()->GetURL(kLandingURL);

  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_EQ(4U, nav_list->Size());

  ReferrerChain referrer_chain;
  SimulateUserGesture();
  IdentifyReferrerChainForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());

  // Verify url fragment is cleared in referrer chain.
  VerifyReferrerChainEntry(
      expected_landing_url,              // url
      GURL(),                            // main_frame_url
      ReferrerChainEntry::LANDING_PAGE,  // type
      test_server_ip,                    // ip_address
      expected_landing_referrer_url_with_query,
      GURL(),               // referrer_main_frame_url
      false,                // is_retargeting
      std::vector<GURL>(),  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      expected_landing_referrer_url_with_query,  // url
      GURL(),                                    // main_frame_url
      ReferrerChainEntry::LANDING_REFERRER,      // type
      test_server_ip,                            // ip_address
      GURL(),  // referrer_url is empty since this beyonds 2 clicks.
      GURL(),  // referrer_main_frame_url is empty for the same reason.
      false,   // is_retargeting
      std::vector<GURL>(),  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       VerifySanitizeReferrerChain) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL test_server_origin = embedded_test_server()->base_url().GetOrigin();
  ClickTestLink("direct_download", 1, initial_url);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->Size());
  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  SafeBrowsingNavigationObserverManager::SanitizeReferrerChain(&referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
  VerifyReferrerChainEntry(
      test_server_origin,             // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      test_server_origin,             // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(test_server_origin,                // url
                           GURL(),                            // main_frame_url
                           ReferrerChainEntry::LANDING_PAGE,  // type
                           test_server_ip,                    // ip_address
                           GURL(),                            // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       VerifyNumberOfRecentNavigationsToCollect) {
  EXPECT_EQ(0, CountOfRecentNavigationsToAppend(
                   /*sber=*/false, /*incognito=*/false,
                   SafeBrowsingNavigationObserverManager::SUCCESS));
  EXPECT_EQ(0, CountOfRecentNavigationsToAppend(
                   /*sber=*/false, /*incognito=*/true,
                   SafeBrowsingNavigationObserverManager::SUCCESS));
  EXPECT_EQ(0,
            CountOfRecentNavigationsToAppend(
                /*sber=*/false, /*incognito=*/false,
                SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_PAGE));
  EXPECT_EQ(0,
            CountOfRecentNavigationsToAppend(
                /*sber=*/false, /*incognito=*/true,
                SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_PAGE));
  EXPECT_EQ(
      0, CountOfRecentNavigationsToAppend(
             /*sber=*/false, /*incognito=*/false,
             SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER));
  EXPECT_EQ(
      0, CountOfRecentNavigationsToAppend(
             /*sber=*/false, /*incognito=*/true,
             SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER));
  EXPECT_EQ(0, CountOfRecentNavigationsToAppend(
                   /*sber=*/false, /*incognito=*/false,
                   SafeBrowsingNavigationObserverManager::INVALID_URL));
  EXPECT_EQ(0, CountOfRecentNavigationsToAppend(
                   /*sber=*/false, /*incognito=*/true,
                   SafeBrowsingNavigationObserverManager::INVALID_URL));
  EXPECT_EQ(
      0,
      CountOfRecentNavigationsToAppend(
          /*sber=*/false, /*incognito=*/false,
          SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND));
  EXPECT_EQ(
      0,
      CountOfRecentNavigationsToAppend(
          /*sber=*/false, /*incognito=*/true,
          SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND));

  EXPECT_EQ(5, CountOfRecentNavigationsToAppend(
                   /*sber=*/true, /*incognito=*/false,
                   SafeBrowsingNavigationObserverManager::SUCCESS));
  EXPECT_EQ(0, CountOfRecentNavigationsToAppend(
                   /*sber=*/true, /*incognito=*/true,
                   SafeBrowsingNavigationObserverManager::SUCCESS));
  EXPECT_EQ(5,
            CountOfRecentNavigationsToAppend(
                /*sber=*/true, /*incognito=*/false,
                SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_PAGE));
  EXPECT_EQ(0,
            CountOfRecentNavigationsToAppend(
                /*sber=*/true, /*incognito=*/true,
                SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_PAGE));
  EXPECT_EQ(
      0, CountOfRecentNavigationsToAppend(
             /*sber=*/true, /*incognito=*/false,
             SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER));
  EXPECT_EQ(
      0, CountOfRecentNavigationsToAppend(
             /*sber=*/true, /*incognito=*/true,
             SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER));
  EXPECT_EQ(5, CountOfRecentNavigationsToAppend(
                   /*sber=*/true, /*incognito=*/false,
                   SafeBrowsingNavigationObserverManager::INVALID_URL));
  EXPECT_EQ(0, CountOfRecentNavigationsToAppend(
                   /*sber=*/true, /*incognito=*/true,
                   SafeBrowsingNavigationObserverManager::INVALID_URL));
  EXPECT_EQ(
      5,
      CountOfRecentNavigationsToAppend(
          /*sber=*/true, /*incognito=*/false,
          SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND));
  EXPECT_EQ(
      0,
      CountOfRecentNavigationsToAppend(
          /*sber=*/true, /*incognito=*/true,
          SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND));
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       AppendRecentNavigationsToEmptyReferrerChain) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectToLandingURL);
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  ClickTestLink("download_on_landing_page", 1, landing_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  ReferrerChain referrer_chain;
  AppendRecentNavigations(/*recent_navigation_count=*/3, &referrer_chain);
  EXPECT_EQ(3, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                           // url
      GURL(),                                 // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,  // type
      test_server_ip,                         // ip_address
      landing_url,                            // referrer_url
      GURL(),                                 // referrer_main_frame_url
      false,                                  // is_retargeting
      std::vector<GURL>(),                    // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      landing_url,                            // url
      GURL(),                                 // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,  // type
      test_server_ip,                         // ip_address
      redirect_url,                           // referrer_url
      GURL(),                                 // referrer_main_frame_url
      false,                                  // is_retargeting
      std::vector<GURL>(),                    // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      redirect_url,                           // url
      GURL(),                                 // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,  // type
      test_server_ip,                         // ip_address
      initial_url,                            // referrer_url
      GURL(),                                 // referrer_main_frame_url
      false,                                  // is_retargeting
      std::vector<GURL>(),                    // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(2));
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       AppendRecentNavigationsToIncompleteReferrerChain) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectToLandingURL);
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  ClickTestLink("download_on_landing_page", 1, landing_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  ReferrerChain referrer_chain;
  // Add the download_url to referrer chain.
  FindAndAddNavigationToReferrerChain(&referrer_chain, download_url);
  AppendRecentNavigations(/*recent_navigation_count=*/3, &referrer_chain);
  // Now the resulting referrer chain should contain the download url entry, and
  // 3 recent navigaitons happened after the download url navigations.
  EXPECT_EQ(4, referrer_chain.size());
  VerifyReferrerChainEntry(
      download_url,                   // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      landing_url,                    // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      landing_url,                            // url
      GURL(),                                 // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,  // type
      test_server_ip,                         // ip_address
      redirect_url,                           // referrer_url
      GURL(),                                 // referrer_main_frame_url
      false,                                  // is_retargeting
      std::vector<GURL>(),                    // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      redirect_url,                           // url
      GURL(),                                 // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,  // type
      test_server_ip,                         // ip_address
      initial_url,                            // referrer_url
      GURL(),                                 // referrer_main_frame_url
      false,                                  // is_retargeting
      std::vector<GURL>(),                    // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(2));
  VerifyReferrerChainEntry(initial_url,  // url
                           GURL(),       // main_frame_url
                           ReferrerChainEntry::RECENT_NAVIGATION,  // type
                           test_server_ip,                         // ip_address
                           GURL(),               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(3));
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       NavigateBackwardForward) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, initial_url);
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  EXPECT_EQ(3U, nav_list->Size());

  // Simulates back.
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.history.back();"));
  base::RunLoop().RunUntilIdle();

  // Simulates forward.
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.history.forward();"));
  base::RunLoop().RunUntilIdle();

  nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  // Verifies navigations caused by back/forward are ignored.
  EXPECT_EQ(3U, nav_list->Size());
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, ReloadNotRecorded) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  EXPECT_EQ(1U, nav_list->Size());

  // Simulates reload.
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "location.reload();"));
  base::RunLoop().RunUntilIdle();

  nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  // Verifies navigations caused by reload are ignored.
  EXPECT_EQ(1U, nav_list->Size());
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       CreateIframeElementGetsReferrerChain) {
  GURL initial_url = embedded_test_server()->GetURL(kCreateIframeElementURL);
  ui_test_utils::NavigateToURL(browser(), initial_url);
  ClickTestLink("do_download", 1, initial_url);

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       SetWindowLocationGetsReferrerChain) {
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ui_test_utils::NavigateToURL(browser(), initial_url);

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.location='../signed.exe'"));
  base::RunLoop().RunUntilIdle();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
}

}  // namespace safe_browsing
