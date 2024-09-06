// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
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
const char kSubFrameTestURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "iframe.html";
const char kRedirectURL[] =
    "/safe_browsing/download_protection/navigation_observer/redirect.html";
const char kEmptyURL[] = "/empty.html";
const char kBasicIframeURL[] = "/iframe.html";
// This is the src of the iframe in iframe.html above.
const char kInitialSubframeUrl[] = "/title1.html";
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

  DownloadItemCreatedObserver(const DownloadItemCreatedObserver&) = delete;
  DownloadItemCreatedObserver& operator=(const DownloadItemCreatedObserver&) =
      delete;

  ~DownloadItemCreatedObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  // Wait for the first download item created after object creation.
  void WaitForDownloadItem(
      std::vector<raw_ptr<DownloadItem, VectorExperimental>>* items_seen) {
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
      quit_waiting_callback_ = base::OnceClosure();
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
      std::move(quit_waiting_callback_).Run();
  }

  void ManagerGoingDown(DownloadManager* manager) override {
    manager_->RemoveObserver(this);
    manager_ = nullptr;
    if (!quit_waiting_callback_.is_null())
      std::move(quit_waiting_callback_).Run();
  }

  base::OnceClosure quit_waiting_callback_;
  raw_ptr<DownloadManager> manager_;
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> items_seen_;
};

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

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool InitialSetup() {
    if (!browser())
      return false;

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 false);
    content::DownloadManager* manager =
        browser()->profile()->GetDownloadManager();
    DownloadPrefs::FromDownloadManager(manager)->ResetAutoOpenByUser();

    return true;
  }

  void TearDownOnMainThread() override {
    observer_manager_.reset();
    // Cancel unfinished download if any.
    CancelDownloads();
    CHECK_EQ(DownloadCoreService::BlockingShutdownCountAllProfiles(), 0);
  }

  // Most test cases will trigger downloads, though we don't really care if
  // download completed or not. So we cancel downloads as soon as we record
  // all the navigation events we need.
  void CancelDownloads() {
    std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
    content::DownloadManager* manager =
        browser()->profile()->GetDownloadManager();
    manager->GetAllDownloads(&download_items);
    for (download::DownloadItem* item : download_items) {
      if (!item->IsDone())
        item->Cancel(true);
    }
  }

  DownloadItem* GetDownload() {
    std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
    content::DownloadManager* manager =
        browser()->profile()->GetDownloadManager();
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
          current_web_contents->GetPrimaryMainFrame();
      if (subframe_index != -1) {
        script_executing_frame =
            ChildFrameAt(script_executing_frame, subframe_index);
      }
      ASSERT_TRUE(content::ExecJs(script_executing_frame, script));
    }
    // Wait for navigations on current tab and new tab (if any) to finish.
    navigation_observer.Wait();
    navigation_observer.StopWatchingNewWebContents();

    // Since this test uses javascript to mimic clicking on a link (no actual
    // user gesture), and DidGetUserInteraction() does not respond to
    // ExecJs(), navigation_transition field in resulting
    // NavigationEvents will always be RENDERER_INITIATED_WITHOUT_USER_GESTURE.
    // Therefore, we need to make some adjustment to relevant NavigationEvent.
    for (std::size_t i = 0U;
         i < navigation_event_list()->NavigationEventsSize(); i++) {
      auto* nav_event = navigation_event_list()->GetNavigationEvent(i);
      if (nav_event->source_url == page_url) {
        nav_event->navigation_initiation =
            ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
        return;
      }
    }
  }

  // Same functionality as |ClickTestLink|, but doesn't wait for the navigation
  // to complete.
  void ClickTestLinkPending(const char* element_id,
                            int number_of_navigations,
                            const GURL& page_url,
                            int subframe_index = -1) {
    TabStripModel* tab_strip = browser()->tab_strip_model();
    content::WebContents* current_web_contents =
        tab_strip->GetActiveWebContents();
    ASSERT_TRUE(content::WaitForLoadStop(current_web_contents));
    // Execute test.
    {
      std::string script = base::StringPrintf("clickLink('%s');", element_id);
      content::RenderFrameHost* script_executing_frame =
          current_web_contents->GetPrimaryMainFrame();
      if (subframe_index != -1) {
        script_executing_frame =
            ChildFrameAt(script_executing_frame, subframe_index);
      }
      ASSERT_TRUE(content::ExecJs(script_executing_frame, script));
    }

    // Since this test uses javascript to mimic clicking on a link (no actual
    // user gesture), and DidGetUserInteraction() does not respond to
    // ExecJs(), navigation_transition field in resulting
    // NavigationEvents will always be RENDERER_INITIATED_WITHOUT_USER_GESTURE.
    // Therefore, we need to make some adjustment to relevant
    // PendingNavigationEvent.
    for (auto& it : navigation_event_list()->pending_navigation_events()) {
      if (it.second->source_url == page_url) {
        it.second->navigation_initiation =
            ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE;
        return;
      }
    }
  }

  void TriggerDownloadViaHtml5FileApi() {
    std::vector<raw_ptr<DownloadItem, VectorExperimental>> items;
    content::DownloadManager* manager =
        browser()->profile()->GetDownloadManager();
    content::WebContents* current_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(content::ExecJs(current_web_contents, "downloadViaFileApi()"));
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
    SessionID download_tab_id = sessions::SessionTabHelper::IdForTab(
        content::DownloadItemUtils::GetWebContents(download));
    content::RenderFrameHost* rfh =
        content::DownloadItemUtils::GetRenderFrameHost(download);
    content::GlobalRenderFrameHostId outermost_main_frame_id;
    if (rfh)
      outermost_main_frame_id = rfh->GetOutermostMainFrame()->GetGlobalId();

    auto result = observer_manager_->IdentifyReferrerChainByEventURL(
        download->GetURL(), download_tab_id, outermost_main_frame_id,
        2,  // kDownloadAttributionUserGestureLimit
        referrer_chain);
    if (result ==
        SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND) {
      DCHECK_EQ(0, referrer_chain->size());
      observer_manager_->IdentifyReferrerChainByRenderFrameHost(
          rfh,
          2,  // kDownloadAttributionUserGestureLimit
          referrer_chain);
    }
  }

  void IdentifyReferrerChainForWebContents(content::WebContents* web_contents,
                                           ReferrerChain* referrer_chain) {
    observer_manager_->IdentifyReferrerChainByRenderFrameHost(
        // We will assume the primary main frame here.
        web_contents->GetPrimaryMainFrame(),
        2,  // kDownloadAttributionUserGestureLimit
        referrer_chain);
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

  // Identify referrer chain of a PPAPI download and populate |referrer_chain|.
  void IdentifyReferrerChainForPPAPIDownload(
      const GURL& initiating_frame_url,
      content::WebContents* web_contents,
      ReferrerChain* referrer_chain) {
    SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
    bool has_user_gesture = observer_manager_->HasUserGesture(web_contents);
    observer_manager_->OnUserGestureConsumed(web_contents);
    EXPECT_LE(observer_manager_->IdentifyReferrerChainByHostingPage(
                  initiating_frame_url, web_contents->GetLastCommittedURL(),
                  web_contents->GetPrimaryMainFrame()->GetGlobalId(), tab_id,
                  has_user_gesture,
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

  void SimulateUserGesture() {
    observer_manager_->RecordUserGestureForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  NavigationEventList* navigation_event_list() {
    return observer_manager_->navigation_event_list();
  }

  SafeBrowsingNavigationObserverManager::HostToIpMap* host_to_ip_map() {
    return observer_manager_->host_to_ip_map();
  }

  int CountOfRecentNavigationsToAppend(
      bool enhanced_protection_enabled,
      bool is_incognito,
      SafeBrowsingNavigationObserverManager::AttributionResult result) {
    SetEnhancedProtectionPrefForTests(browser()->profile()->GetPrefs(),
                                      enhanced_protection_enabled);
    auto* maybe_otr_profile = is_incognito
                                  ? browser()->profile()->GetPrimaryOTRProfile(
                                        /*create_if_needed=*/true)
                                  : browser()->profile();
    return SafeBrowsingNavigationObserverManager::
        CountOfRecentNavigationsToAppend(maybe_otr_profile,
                                         maybe_otr_profile->GetPrefs(), result);
  }

  void AppendRecentNavigations(int recent_navigation_count,
                               ReferrerChain* out_referrer_chain) {
    observer_manager_->AppendRecentNavigations(recent_navigation_count,
                                               out_referrer_chain);
  }

  NavigationEvent* GetNavigationEvent(size_t index) {
    return observer_manager_->navigation_event_list()
        ->navigation_events()[index]
        .get();
  }

  std::optional<size_t> FindNavigationEventIndex(
      const GURL& target_url,
      content::GlobalRenderFrameHostId outermost_main_frame_id) {
    return observer_manager_->navigation_event_list()->FindNavigationEvent(
        base::Time::Now(), target_url, GURL(), SessionID::InvalidValue(),
        outermost_main_frame_id,
        (observer_manager_->navigation_event_list()->NavigationEventsSize() -
         1));
  }

  void FindAndAddNavigationToReferrerChain(ReferrerChain* referrer_chain,
                                           const GURL& target_url) {
    auto nav_event_index = FindNavigationEventIndex(
        target_url, content::GlobalRenderFrameHostId());
    if (nav_event_index) {
      auto* nav_event =
          observer_manager_->navigation_event_list()->GetNavigationEvent(
              *nav_event_index);
      observer_manager_->MaybeAddToReferrerChain(
          referrer_chain, nav_event, GURL(), ReferrerChainEntry::EVENT_URL);
    }
  }

 protected:
  std::unique_ptr<TestSafeBrowsingNavigationObserverManager> observer_manager_;

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::test::FencedFrameTestHelper& fenced_frame_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

// Type download URL into address bar and start download on the same page.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, TypeInURLDownload) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kDownloadItemURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("direct_download", 1, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));
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
  EXPECT_FALSE(referrer_chain.Get(0).is_url_removed_by_policy());
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
  EXPECT_FALSE(referrer_chain.Get(1).is_url_removed_by_policy());
}

// Click on a link with rel="noreferrer" attribute, and start download on the
// same page.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       DirectDownloadNoReferrer) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("direct_download_noreferrer", 1, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("direct_download_noreferrer_target_blank", 1, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  // The next NavigationEvent opens a new tab.
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  // This one is the actual navigation which triggers download.
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(2));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("single_meta_refresh_redirect", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  // Since unlike server redirects client redirects commit and then generate a
  // second navigation, our observer records two NavigationEvents for this test.
  ASSERT_EQ(3U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(2));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("single_meta_refresh_redirect_target_blank", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(4U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(2));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(3));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
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
  ASSERT_EQ(4U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,         // source_url
                        initial_url,         // source_main_frame_url
                        first_redirect_url,  // original_request_url
                        first_redirect_url,  // destination_url
                        true,                // is_user_initiated,
                        true,                // has_committed
                        false,               // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  VerifyNavigationEvent(first_redirect_url,   // source_url
                        first_redirect_url,   // source_main_frame_url
                        second_redirect_url,  // original_request_url
                        second_redirect_url,  // destination_url
                        false,                // is_user_initiated,
                        true,                 // has_committed
                        false,                // has_server_redirect
                        nav_list->GetNavigationEvent(2));
  VerifyNavigationEvent(second_redirect_url,  // source_url
                        second_redirect_url,  // source_main_frame_url
                        download_url,         // original_request_url
                        download_url,         // destination_url
                        false,                // is_user_initiated,
                        false,                // has_committed
                        false,                // has_server_redirect
                        nav_list->GetNavigationEvent(3));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("window_location_redirection", 1, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("mix_redirects", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(2));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("new_tab_download", 2, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL blank_url = GURL(url::kAboutBlankURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(4U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,  // source_url
                        initial_url,  // source_main_frame_url
                        blank_url,    // original_request_url
                        blank_url,    // destination_url
                        true,         // is_user_initiated,
                        false,        // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  // Source and target are at different tabs.
  EXPECT_NE(nav_list->GetNavigationEvent(1)->source_tab_id,
            nav_list->GetNavigationEvent(1)->target_tab_id);
  VerifyNavigationEvent(GURL(),     // source_url
                        GURL(),     // source_main_frame_url
                        blank_url,  // original_request_url
                        blank_url,  // destination_url
                        false,      // is_user_initiated,
                        true,       // has_committed
                        false,      // has_server_redirect
                        nav_list->GetNavigationEvent(2));
  EXPECT_EQ(nav_list->GetNavigationEvent(2)->source_tab_id,
            nav_list->GetNavigationEvent(2)->target_tab_id);
  VerifyNavigationEvent(blank_url,     // source_url
                        blank_url,     // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(3));
  EXPECT_EQ(nav_list->GetNavigationEvent(3)->source_tab_id,
            nav_list->GetNavigationEvent(3)->target_tab_id);
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("new_tab_download_with_data_url", 2, initial_url);
  GURL download_url = GURL(kDownloadDataURL);
  GURL short_download_url = GURL(kShortDataURL);
  GURL blank_url = GURL(url::kAboutBlankURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(4U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  // The second navigation event is a retargeting navigation.
  VerifyNavigationEvent(initial_url,  // source_url
                        initial_url,  // source_main_frame_url
                        blank_url,    // original_request_url
                        blank_url,    // destination_url
                        true,         // is_user_initiated,
                        false,        // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  // Source and target are at different tabs.
  EXPECT_FALSE(nav_list->GetNavigationEvent(1)->source_tab_id ==
               nav_list->GetNavigationEvent(1)->target_tab_id);
  VerifyNavigationEvent(GURL(),     // source_url
                        GURL(),     // source_main_frame_url
                        blank_url,  // original_request_url
                        blank_url,  // destination_url
                        false,      // is_user_initiated,
                        true,       // has_committed
                        false,      // has_server_redirect
                        nav_list->GetNavigationEvent(2));
  EXPECT_EQ(nav_list->GetNavigationEvent(2)->source_tab_id,
            nav_list->GetNavigationEvent(2)->target_tab_id);
  VerifyNavigationEvent(blank_url,     // source_url
                        blank_url,     // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(3));
  EXPECT_EQ(nav_list->GetNavigationEvent(3)->source_tab_id,
            nav_list->GetNavigationEvent(3)->target_tab_id);
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
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
  ASSERT_EQ(5U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,           // source_url
                        initial_url,           // source_main_frame_url
                        multi_frame_test_url,  // original_request_url
                        multi_frame_test_url,  // destination_url
                        true,                  // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  // The order of the next two navigation events may vary. We check for both
  // possibilities. Their order doesn't impact referrer chain attribution logic.
  if (nav_list->GetNavigationEvent(2)->original_request_url == iframe_url) {
    VerifyNavigationEvent(GURL(),                // source_url
                          multi_frame_test_url,  // source_main_frame_url
                          iframe_url,            // original_request_url
                          iframe_url,            // destination_url
                          false,                 // is_user_initiated,
                          true,                  // has_committed
                          false,                 // has_server_redirect
                          nav_list->GetNavigationEvent(2));
    VerifyNavigationEvent(GURL(),                  // source_url
                          multi_frame_test_url,    // source_main_frame_url
                          iframe_retargeting_url,  // original_request_url
                          iframe_retargeting_url,  // destination_url
                          false,                   // is_user_initiated,
                          true,                    // has_committed
                          false,                   // has_server_redirect
                          nav_list->GetNavigationEvent(3));
  } else {
    VerifyNavigationEvent(GURL(),                  // source_url
                          multi_frame_test_url,    // source_main_frame_url
                          iframe_retargeting_url,  // original_request_url
                          iframe_retargeting_url,  // destination_url
                          false,                   // is_user_initiated,
                          true,                    // has_committed
                          false,                   // has_server_redirect
                          nav_list->GetNavigationEvent(2));
    VerifyNavigationEvent(GURL(),                // source_url
                          multi_frame_test_url,  // source_main_frame_url
                          iframe_url,            // original_request_url
                          iframe_url,            // destination_url
                          false,                 // is_user_initiated,
                          true,                  // has_committed
                          false,                 // has_server_redirect
                          nav_list->GetNavigationEvent(3));
  }
  VerifyNavigationEvent(iframe_url,            // source_url
                        multi_frame_test_url,  // source_main_frame_url
                        download_url,          // original_request_url
                        download_url,          // destination_url
                        true,                  // is_user_initiated,
                        false,                 // has_committed
                        false,                 // has_server_redirect
                        nav_list->GetNavigationEvent(4));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
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
  ASSERT_EQ(7U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,           // source_url
                        initial_url,           // source_main_frame_url
                        multi_frame_test_url,  // original_request_url
                        multi_frame_test_url,  // destination_url
                        true,                  // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  // The order of the next two navigation events may vary. We check for both
  // possibilities. Their order doesn't impact referrer chain attribution logic.
  if (nav_list->GetNavigationEvent(2)->original_request_url == iframe_url) {
    VerifyNavigationEvent(GURL(),                // source_url
                          multi_frame_test_url,  // source_main_frame_url
                          iframe_url,            // original_request_url
                          iframe_url,            // destination_url
                          false,                 // is_user_initiated,
                          true,                  // has_committed
                          false,                 // has_server_redirect
                          nav_list->GetNavigationEvent(2));
    VerifyNavigationEvent(GURL(),                  // source_url
                          multi_frame_test_url,    // source_main_frame_url
                          iframe_retargeting_url,  // original_request_url
                          iframe_retargeting_url,  // destination_url
                          false,                   // is_user_initiated,
                          true,                    // has_committed
                          false,                   // has_server_redirect
                          nav_list->GetNavigationEvent(3));
  } else {
    VerifyNavigationEvent(GURL(),                  // source_url
                          multi_frame_test_url,    // source_main_frame_url
                          iframe_retargeting_url,  // original_request_url
                          iframe_retargeting_url,  // destination_url
                          false,                   // is_user_initiated,
                          true,                    // has_committed
                          false,                   // has_server_redirect
                          nav_list->GetNavigationEvent(2));
    VerifyNavigationEvent(GURL(),                // source_url
                          multi_frame_test_url,  // source_main_frame_url
                          iframe_url,            // original_request_url
                          iframe_url,            // destination_url
                          false,                 // is_user_initiated,
                          true,                  // has_committed
                          false,                 // has_server_redirect
                          nav_list->GetNavigationEvent(3));
  }
  VerifyNavigationEvent(iframe_retargeting_url,  // source_url
                        multi_frame_test_url,    // source_main_frame_url
                        blank_url,               // original_request_url
                        blank_url,               // destination_url
                        true,                    // is_user_initiated,
                        false,                   // has_committed
                        false,                   // has_server_redirect
                        nav_list->GetNavigationEvent(4));
  VerifyNavigationEvent(GURL(),     // source_url
                        GURL(),     // source_main_frame_url
                        blank_url,  // original_request_url
                        blank_url,  // destination_url
                        false,      // is_user_initiated,
                        true,       // has_committed
                        false,      // has_server_redirect
                        nav_list->GetNavigationEvent(5));
  VerifyNavigationEvent(blank_url,     // source_url
                        blank_url,     // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(6));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectToLandingURL);
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  ClickTestLink("download_on_landing_page", 1, landing_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(4U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        landing_url,   // original_request_url
                        landing_url,   // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(2));
  VerifyNavigationEvent(landing_url,   // source_url
                        landing_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(3));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
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
  ASSERT_EQ(5U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,  // source_url
                        initial_url,  // source_main_frame_url
                        page_before_landing_referrer_url,  // original_request
                        page_before_landing_referrer_url,  // destination_url
                        true,                              // is_user_initiated,
                        true,                              // has_committed
                        false,  // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  VerifyNavigationEvent(page_before_landing_referrer_url,  // source_url
                        page_before_landing_referrer_url,  // source_main_frame
                        landing_referrer_url,  // original_request_url
                        landing_referrer_url,  // destination_url
                        true,                  // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_list->GetNavigationEvent(2));
  VerifyNavigationEvent(landing_referrer_url,  // source_url
                        landing_referrer_url,  // source_main_frame
                        landing_url,           // original_request_url
                        landing_url,           // destination_url
                        true,                  // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_list->GetNavigationEvent(3));
  VerifyNavigationEvent(landing_url,   // source_url
                        landing_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(4));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectToLandingURL);
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  // Simulate a user gesture on landing page.
  SimulateUserGesture();
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        landing_url,   // original_request_url
                        landing_url,   // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(2));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL landing_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, landing_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectToLandingURL);
  GURL hosting_url = embedded_test_server()->GetURL(kLandingURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        landing_url,  // original_request_url
                        landing_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(landing_url,   // source_url
                        landing_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        true,          // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        hosting_url,   // original_request_url
                        hosting_url,   // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(2));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL request_url =
      embedded_test_server()->GetURL("/server-redirect?" + download_url.spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), request_url));
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        request_url,   // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        true,          // has_server_redirect
                        nav_list->GetNavigationEvent(1));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL destination_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL redirect_url = embedded_test_server()->GetURL("/server-redirect?" +
                                                     destination_url.spec());
  GURL request_url =
      embedded_test_server()->GetURL("/server-redirect?" + redirect_url.spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), request_url));
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(GURL(),           // source_url
                        GURL(),           // source_main_frame_url
                        request_url,      // original_request_url
                        destination_url,  // destination_url
                        true,             // is_user_initiated,
                        false,            // has_committed
                        true,             // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  const auto redirect_vector =
      nav_list->GetNavigationEvent(1)->server_redirect_urls;
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL request_url =
      embedded_test_server()->GetURL("/server-redirect?" + download_url.spec());
  ClickTestLink("new_tab_download_with_server_redirect", 1, initial_url);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,  // source_url
                        initial_url,  // source_main_frame_url
                        request_url,  // original_request_url
                        request_url,  // destination_url
                        true,         // is_user_initiated,
                        false,        // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        request_url,   // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        true,          // has_server_redirect
                        nav_list->GetNavigationEvent(2));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  auto* ip_map = host_to_ip_map();
  ip_map->clear();
  std::string test_server_host(embedded_test_server()->base_url().host());
  ip_map->insert(
      std::make_pair(test_server_host, std::vector<ResolvedIPAddress>()));
  base::Time yesterday(base::Time::Now() - base::Days(1));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL hosting_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  TriggerDownloadViaHtml5FileApi();
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(1U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        hosting_url,  // original_request_url
                        hosting_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
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
  ASSERT_EQ(4U, nav_list->NavigationEventsSize());

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL test_server_origin =
      embedded_test_server()->base_url().DeprecatedGetOriginAsURL();
  ClickTestLink("direct_download", 1, initial_url);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(2U, nav_list->NavigationEventsSize());
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
  EXPECT_EQ(0,
            CountOfRecentNavigationsToAppend(
                /*enhanced_protection_enabled=*/false, /*is_incognito=*/false,
                SafeBrowsingNavigationObserverManager::SUCCESS));
  EXPECT_EQ(0, CountOfRecentNavigationsToAppend(
                   /*enhanced_protection_enabled=*/false, /*is_incognito=*/true,
                   SafeBrowsingNavigationObserverManager::SUCCESS));
  EXPECT_EQ(0,
            CountOfRecentNavigationsToAppend(
                /*enhanced_protection_enabled=*/false, /*is_incognito=*/false,
                SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_PAGE));
  EXPECT_EQ(0,
            CountOfRecentNavigationsToAppend(
                /*enhanced_protection_enabled=*/false, /*is_incognito=*/true,
                SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_PAGE));
  EXPECT_EQ(
      0, CountOfRecentNavigationsToAppend(
             /*enhanced_protection_enabled=*/false, /*is_incognito=*/false,
             SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER));
  EXPECT_EQ(
      0, CountOfRecentNavigationsToAppend(
             /*enhanced_protection_enabled=*/false, /*is_incognito=*/true,
             SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER));
  EXPECT_EQ(0,
            CountOfRecentNavigationsToAppend(
                /*enhanced_protection_enabled=*/false, /*is_incognito=*/false,
                SafeBrowsingNavigationObserverManager::INVALID_URL));
  EXPECT_EQ(0, CountOfRecentNavigationsToAppend(
                   /*enhanced_protection_enabled=*/false, /*is_incognito=*/true,
                   SafeBrowsingNavigationObserverManager::INVALID_URL));
  EXPECT_EQ(
      0,
      CountOfRecentNavigationsToAppend(
          /*enhanced_protection_enabled=*/false, /*is_incognito=*/false,
          SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND));
  EXPECT_EQ(
      0,
      CountOfRecentNavigationsToAppend(
          /*enhanced_protection_enabled=*/false, /*is_incognito=*/true,
          SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND));

  EXPECT_EQ(5, CountOfRecentNavigationsToAppend(
                   /*enhanced_protection_enabled=*/true, /*is_incognito=*/false,
                   SafeBrowsingNavigationObserverManager::SUCCESS));
  EXPECT_EQ(0, CountOfRecentNavigationsToAppend(
                   /*enhanced_protection_enabled=*/true, /*is_incognito=*/true,
                   SafeBrowsingNavigationObserverManager::SUCCESS));
  EXPECT_EQ(5,
            CountOfRecentNavigationsToAppend(
                /*enhanced_protection_enabled=*/true, /*is_incognito=*/false,
                SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_PAGE));
  EXPECT_EQ(0,
            CountOfRecentNavigationsToAppend(
                /*enhanced_protection_enabled=*/true, /*is_incognito=*/true,
                SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_PAGE));
  EXPECT_EQ(
      0, CountOfRecentNavigationsToAppend(
             /*enhanced_protection_enabled=*/true, /*is_incognito=*/false,
             SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER));
  EXPECT_EQ(
      0, CountOfRecentNavigationsToAppend(
             /*enhanced_protection_enabled=*/true, /*is_incognito=*/true,
             SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER));
  EXPECT_EQ(5, CountOfRecentNavigationsToAppend(
                   /*enhanced_protection_enabled=*/true, /*is_incognito=*/false,
                   SafeBrowsingNavigationObserverManager::INVALID_URL));
  EXPECT_EQ(0, CountOfRecentNavigationsToAppend(
                   /*enhanced_protection_enabled=*/true, /*is_incognito=*/true,
                   SafeBrowsingNavigationObserverManager::INVALID_URL));
  EXPECT_EQ(
      5,
      CountOfRecentNavigationsToAppend(
          /*enhanced_protection_enabled=*/true, /*is_incognito=*/false,
          SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND));
  EXPECT_EQ(
      0,
      CountOfRecentNavigationsToAppend(
          /*enhanced_protection_enabled=*/true, /*is_incognito=*/true,
          SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND));
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       AppendRecentNavigationsToIncompleteReferrerChain) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
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
                       NewWindowAndNavigateSubframe) {
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto main_frame_url = embedded_test_server()->GetURL(kMultiFrameTestURL);
  auto new_window_url = embedded_test_server()->GetURL(kBasicIframeURL);
  auto new_window_subframe_url = embedded_test_server()->GetURL(kEmptyURL);
  auto initial_subframe_url =
      embedded_test_server()->GetURL(kInitialSubframeUrl);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  auto initiator_outermost_main_frame_id =
      web_contents()->GetPrimaryMainFrame()->GetGlobalId();

  auto* initial_web_contents = web_contents();

  ui_test_utils::UrlLoadObserver url_observer(new_window_url);
  ASSERT_TRUE(
      ExecJs(web_contents()->GetPrimaryMainFrame(),
             content::JsReplace("var w = window.open($1, 'New Window');",
                                new_window_url)));
  url_observer.Wait();
  const int kExpectedNumberOfNavigations = 1;

  content::TestNavigationObserver navigation_observer(
      web_contents(), kExpectedNumberOfNavigations);
  ASSERT_TRUE(
      ExecJs(initial_web_contents->GetPrimaryMainFrame(),
             content::JsReplace("w.document.querySelector('IFRAME').src = $1;",
                                new_window_subframe_url)));
  navigation_observer.Wait();
  EXPECT_EQ(new_window_subframe_url, navigation_observer.last_navigation_url());

  auto target_main_frame_id =
      web_contents()->GetPrimaryMainFrame()->GetGlobalId();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainByEventURL(
      new_window_url, sessions::SessionTabHelper::IdForTab(web_contents()),
      content::GlobalRenderFrameHostId(), &referrer_chain);

  auto index = FindNavigationEventIndex(new_window_url,
                                        content::GlobalRenderFrameHostId());
  ASSERT_TRUE(index);
  auto* nav_event = GetNavigationEvent(*index);
  EXPECT_EQ(initiator_outermost_main_frame_id,
            nav_event->initiator_outermost_main_frame_id);
  EXPECT_EQ(target_main_frame_id, nav_event->outermost_main_frame_id);

  EXPECT_EQ(2, referrer_chain.size());

  referrer_chain.Clear();
  IdentifyReferrerChainByEventURL(
      new_window_subframe_url,
      sessions::SessionTabHelper::IdForTab(web_contents()),
      content::GlobalRenderFrameHostId(), &referrer_chain);
  EXPECT_EQ(4, referrer_chain.size());
  VerifyReferrerChainEntry(
      new_window_subframe_url,  // url
      // TODO(crbug.com/40823953): this should be |new_window_url|.
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      initial_subframe_url,           // referrer_url
      new_window_url,                 // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));

  VerifyReferrerChainEntry(
      initial_subframe_url,                 // url
      new_window_url,                       // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      GURL(),                               // referrer_url
      new_window_url,                       // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));

  VerifyReferrerChainEntry(
      new_window_url,                       // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      main_frame_url,                       // referrer_url
      GURL(),                               // referrer_main_frame_url
      true,                                 // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(2));

  VerifyReferrerChainEntry(main_frame_url,  // url
                           GURL(),          // main_frame_url
                           ReferrerChainEntry::CLIENT_REDIRECT,  // type
                           test_server_ip,                       // ip_address
                           GURL(),                               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(3));

  index = FindNavigationEventIndex(new_window_subframe_url,
                                   content::GlobalRenderFrameHostId());
  ASSERT_TRUE(index);
  nav_event = GetNavigationEvent(*index);
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
            nav_event->outermost_main_frame_id);

  // If the initial main frame runs JS to initiate this subframe navigation, the
  // the navigation request's initiator frame is in the current frame tree,
  // not the frame in which the JS ran.
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
            nav_event->initiator_outermost_main_frame_id);
}

// This is similar to PrerenderReferrerChains, but navigates rather than
// initiating the prerender to ensure that the referrer chains match.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       ReferrerChainsMatchPrerender) {
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto main_frame_url = embedded_test_server()->GetURL(kMultiFrameTestURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  auto next_url = embedded_test_server()->GetURL(kSubFrameTestURL);
  content::TestNavigationObserver observer(web_contents(), 1);
  ASSERT_TRUE(ExecJs(web_contents(),
                     content::JsReplace("window.location = $1;", next_url)));
  observer.Wait();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainByEventURL(
      next_url, sessions::SessionTabHelper::IdForTab(web_contents()),
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), &referrer_chain);

  EXPECT_EQ(2, referrer_chain.size());

  VerifyReferrerChainEntry(
      next_url,                       // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      main_frame_url,                 // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));

  VerifyReferrerChainEntry(main_frame_url,  // url
                           GURL(),          // main_frame_url
                           ReferrerChainEntry::CLIENT_REDIRECT,  // type
                           test_server_ip,                       // ip_address
                           GURL(),                               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       PrerenderReferrerChains) {
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto main_frame_url = embedded_test_server()->GetURL(kMultiFrameTestURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  auto prerendered_url = embedded_test_server()->GetURL(kSubFrameTestURL);
  auto host_id = prerender_helper().AddPrerender(prerendered_url);
  prerender_helper().WaitForPrerenderLoadCompletion(prerendered_url);

  // The prerendered URL (iframe.html) will now be loaded twice within the
  // WebContents. Once as the main frame of the prerendered page and once as a
  // subframe of the primary page. Since the prerender was initiated after
  // loading the primary page, it will be the most recent navigation event.

  // Should find the event corresponding to the prerendered page. By passing an
  // invalid RFH id, FindNavigationEvent does not attempt to match within a
  // given outermost main frame but will instead match the most recent matching
  // navigation event.
  auto index_a = FindNavigationEventIndex(prerendered_url,
                                          content::GlobalRenderFrameHostId());

  // Since we're supplying the id for the primary page's outermost main frame,
  // we should find the event for the primary page.
  auto index_b = FindNavigationEventIndex(
      prerendered_url, web_contents()->GetPrimaryMainFrame()->GetGlobalId());

  // Ensure that these indices are valid and not equal.
  EXPECT_TRUE(index_a && index_b);
  EXPECT_NE(*index_a, *index_b);

  auto* prerendered_main_frame_host =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);
  auto index_c = FindNavigationEventIndex(
      prerendered_url,
      prerendered_main_frame_host->GetOutermostMainFrame()->GetGlobalId());

  // By explicitly supplying the outermost frame id for the prerendered page, we
  // should also get the event corresponding to the prerendered page.
  EXPECT_TRUE(index_c);
  EXPECT_EQ(*index_a, *index_c);

  // Build a full referrer chain for the prerendered page.
  ReferrerChain referrer_chain;
  IdentifyReferrerChainByEventURL(
      prerendered_url, sessions::SessionTabHelper::IdForTab(web_contents()),
      prerendered_main_frame_host->GetOutermostMainFrame()->GetGlobalId(),
      &referrer_chain);

  EXPECT_EQ(2, referrer_chain.size());

  VerifyReferrerChainEntry(
      prerendered_url,                // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      main_frame_url,                 // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));

  VerifyReferrerChainEntry(main_frame_url,  // url
                           GURL(),          // main_frame_url
                           ReferrerChainEntry::CLIENT_REDIRECT,  // type
                           test_server_ip,                       // ip_address
                           GURL(),                               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  prerender_helper().NavigatePrimaryPage(prerendered_url);
  host_observer.WaitForActivation();

  // Build a full referrer chain for the prerendered page following activation.
  referrer_chain.Clear();
  IdentifyReferrerChainByEventURL(
      prerendered_url, sessions::SessionTabHelper::IdForTab(web_contents()),
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), &referrer_chain);

  EXPECT_EQ(2, referrer_chain.size());

  VerifyReferrerChainEntry(
      prerendered_url,                // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      main_frame_url,                 // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));

  VerifyReferrerChainEntry(main_frame_url,  // url
                           GURL(),          // main_frame_url
                           ReferrerChainEntry::CLIENT_REDIRECT,  // type
                           test_server_ip,                       // ip_address
                           GURL(),                               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       FencedFrameNavigationEventsAndReferrerChain) {
  const auto test_server_ip(embedded_test_server()->host_port_pair().host());
  const auto main_frame_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  const auto outermost_rfh_id =
      web_contents()->GetPrimaryMainFrame()->GetGlobalId();

  const auto fenced_frame_gurl =
      embedded_test_server()->GetURL("b.test", "/fenced_frames/title1.html");
  auto* ff_rfh = fenced_frame_helper().CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), fenced_frame_gurl);
  ASSERT_TRUE(ff_rfh);

  // Expects the same non-null index.
  auto index_a = FindNavigationEventIndex(main_frame_url,
                                          content::GlobalRenderFrameHostId());
  auto index_b = FindNavigationEventIndex(main_frame_url, outermost_rfh_id);
  ASSERT_TRUE(index_a.has_value());
  ASSERT_EQ(index_a, index_b);

  // Expects the same non-null index.
  auto index_c = FindNavigationEventIndex(fenced_frame_gurl,
                                          content::GlobalRenderFrameHostId());
  auto index_d = FindNavigationEventIndex(fenced_frame_gurl, outermost_rfh_id);
  ASSERT_TRUE(index_c.has_value());
  ASSERT_EQ(index_c, index_d);

  // Main frame initial navigation and fenced frame initial navigation.
  ASSERT_EQ(navigation_event_list()->NavigationEventsSize(), 2U);
  // Main frame.
  VerifyNavigationEvent(GURL(),          // source_url
                        GURL(),          // source_main_frame_url
                        main_frame_url,  // original_request_url
                        main_frame_url,  // destination_url
                        true,            // is_user_initiated,
                        true,            // has_committed
                        false,           // has_server_redirect
                        navigation_event_list()->GetNavigationEvent(0));
  // Fenced frame initial navigation. The FencedFrame navigation is not
  // user-initiated by setting the `src` attribute.
  VerifyNavigationEvent(GURL(),             // source_url
                        main_frame_url,     // source_main_frame_url
                        fenced_frame_gurl,  // original_request_url
                        fenced_frame_gurl,  // destination_url
                        false,              // is_user_initiated,
                        true,               // has_committed
                        false,              // has_server_redirect
                        navigation_event_list()->GetNavigationEvent(1));

  ReferrerChain referrer_chain;
  IdentifyReferrerChainByEventURL(
      fenced_frame_gurl, sessions::SessionTabHelper::IdForTab(web_contents()),
      outermost_rfh_id, &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());

  // The fenced frame's `NavigationEvent` will not have `source_url` because the
  // last committed URL is empty (coming from `about:blank`), thus the entry's
  // `referrer_url` is empty.
  // The `source_main_frame_url` of the event is retrieved from the embedder
  // page, and since it is different from the `source_url` of the event, it is
  // stored at the entry's `referrer_main_frame_url`.
  VerifyReferrerChainEntry(
      fenced_frame_gurl,              // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      GURL(),                         // referrer_url
      main_frame_url,                 // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));

  VerifyReferrerChainEntry(main_frame_url,  // url
                           GURL(),          // main_frame_url
                           ReferrerChainEntry::CLIENT_REDIRECT,  // type
                           test_server_ip,                       // ip_address
                           GURL(),                               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));

  // Navigates the fenced frame.
  const auto fenced_frame_gurl2 =
      embedded_test_server()->GetURL("c.test", "/fenced_frames/title1.html");
  ff_rfh = fenced_frame_helper().NavigateFrameInFencedFrameTree(
      ff_rfh, fenced_frame_gurl2);
  ASSERT_TRUE(ff_rfh);

  // Main frame initial navigation, fenced frame initial navigation and fenced
  // frame second navigation.
  ASSERT_EQ(navigation_event_list()->NavigationEventsSize(), 3U);
  // Main frame.
  VerifyNavigationEvent(GURL(),          // source_url
                        GURL(),          // source_main_frame_url
                        main_frame_url,  // original_request_url
                        main_frame_url,  // destination_url
                        true,            // is_user_initiated,
                        true,            // has_committed
                        false,           // has_server_redirect
                        navigation_event_list()->GetNavigationEvent(0));
  // Fenced frame initial navigation.
  VerifyNavigationEvent(GURL(),             // source_url
                        main_frame_url,     // source_main_frame_url
                        fenced_frame_gurl,  // original_request_url
                        fenced_frame_gurl,  // destination_url
                        false,              // is_user_initiated,
                        true,               // has_committed
                        false,              // has_server_redirect
                        navigation_event_list()->GetNavigationEvent(1));
  // Fenced frame second navigation.
  VerifyNavigationEvent(fenced_frame_gurl,   // source_url
                        main_frame_url,      // source_main_frame_url
                        fenced_frame_gurl2,  // original_request_url
                        fenced_frame_gurl2,  // destination_url
                        false,               // is_user_initiated,
                        true,                // has_committed
                        false,               // has_server_redirect
                        navigation_event_list()->GetNavigationEvent(2));

  // Three entries.
  referrer_chain.Clear();
  IdentifyReferrerChainByEventURL(
      fenced_frame_gurl2, sessions::SessionTabHelper::IdForTab(web_contents()),
      outermost_rfh_id, &referrer_chain);
  ASSERT_EQ(3, referrer_chain.size());

  // For the second fenced frame navigation, we have a valid `referrer_url`.
  VerifyReferrerChainEntry(
      fenced_frame_gurl2,             // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      fenced_frame_gurl,              // referrer_url
      main_frame_url,                 // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(0));

  // Almost identical to the initial fenced frame navigation entry, except for
  // that the `type` is `CLIENT_REDIRECT` instead of `EVENT_URL`. Because of the
  // new `type` we also have a non-empty `main_frame_url`.
  VerifyReferrerChainEntry(
      fenced_frame_gurl,                    // url
      main_frame_url,                       // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      GURL(),                               // referrer_url
      main_frame_url,                       // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));

  // Same as the main frame entry prior to the second fenced frame navigation.
  VerifyReferrerChainEntry(main_frame_url,  // url
                           GURL(),          // main_frame_url
                           ReferrerChainEntry::CLIENT_REDIRECT,  // type
                           test_server_ip,                       // ip_address
                           GURL(),                               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(2));
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       NavigateBackwardForward) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, initial_url);
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  EXPECT_EQ(3U, nav_list->NavigationEventsSize());

  // Simulates back.
  ASSERT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.history.back();"));
  base::RunLoop().RunUntilIdle();

  // Simulates forward.
  ASSERT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.history.forward();"));
  base::RunLoop().RunUntilIdle();

  nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  // Verifies navigations caused by back/forward are ignored.
  EXPECT_EQ(3U, nav_list->NavigationEventsSize());
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, ReloadNotRecorded) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  EXPECT_EQ(1U, nav_list->NavigationEventsSize());

  // Simulates reload.
  ASSERT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "location.reload();"));
  base::RunLoop().RunUntilIdle();

  nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  // Verifies navigations caused by reload are ignored.
  EXPECT_EQ(1U, nav_list->NavigationEventsSize());
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       CreateIframeElementGetsReferrerChain) {
  GURL initial_url = embedded_test_server()->GetURL(kCreateIframeElementURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  ClickTestLink("do_download", 1, initial_url);

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       SetWindowLocationGetsReferrerChain) {
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  ASSERT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.location='../signed.exe'"));
  base::RunLoop().RunUntilIdle();

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(2, referrer_chain.size());
}

// Open a new tab to some arbitrary URL, then have the opener navigate the new
// tab to the actual landing page where the user then clicks a link to start a
// download.
// TODO(drubery, mcnee): The "source" information captured in a Safe Browsing
// |NavigationEvent| does not necessarily reflect the initiator of a navigation.
// This test illustrates this behaviour. Note that |initial_popup_url| appears
// to navigate itself to the landing page, even though it was navigated by its
// opener. Investigate whether the initiator of a navigation should be reflected
// in the referrer chain.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       NewTabClientRedirectByOpener) {
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL initial_popup_url = embedded_test_server()->GetURL("/title1.html");
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::TabAddedWaiter tab_added(browser());
  content::TestNavigationObserver new_tab_nav(initial_popup_url);
  new_tab_nav.StartWatchingNewWebContents();
  SimulateUserGesture();
  ASSERT_TRUE(content::ExecJs(
      opener_contents, content::JsReplace("window.newWindow = window.open($1);",
                                          initial_popup_url)));
  new_tab_nav.Wait();
  tab_added.Wait();

  content::TestNavigationObserver landing_nav_observer(landing_url);
  landing_nav_observer.WatchExistingWebContents();
  ASSERT_TRUE(content::ExecJs(
      opener_contents,
      content::JsReplace("window.newWindow.location = $1;", landing_url)));
  landing_nav_observer.Wait();

  ClickTestLink("download_on_landing_page", 1, landing_url);

  std::string test_server_ip(embedded_test_server()->host_port_pair().host());
  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(5U, nav_list->NavigationEventsSize());
  VerifyNavigationEvent(GURL(),       // source_url
                        GURL(),       // source_main_frame_url
                        initial_url,  // original_request_url
                        initial_url,  // destination_url
                        true,         // is_user_initiated,
                        true,         // has_committed
                        false,        // has_server_redirect
                        nav_list->GetNavigationEvent(0));
  VerifyNavigationEvent(initial_url,        // source_url
                        initial_url,        // source_main_frame_url
                        initial_popup_url,  // original_request_url
                        initial_popup_url,  // destination_url
                        true,               // is_user_initiated,
                        false,              // has_committed
                        false,              // has_server_redirect
                        nav_list->GetNavigationEvent(1));
  VerifyNavigationEvent(GURL(),             // source_url
                        GURL(),             // source_main_frame_url
                        initial_popup_url,  // original_request_url
                        initial_popup_url,  // destination_url
                        false,              // is_user_initiated,
                        true,               // has_committed
                        false,              // has_server_redirect
                        nav_list->GetNavigationEvent(2));
  VerifyNavigationEvent(initial_popup_url,  // source_url
                        initial_popup_url,  // source_main_frame_url
                        landing_url,        // original_request_url
                        landing_url,        // destination_url
                        false,              // is_user_initiated,
                        true,               // has_committed
                        false,              // has_server_redirect
                        nav_list->GetNavigationEvent(3));
  VerifyNavigationEvent(landing_url,   // source_url
                        landing_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_list->GetNavigationEvent(4));
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
      initial_popup_url,                 // referrer_url
      GURL(),                            // referrer_main_frame_url
      false,                             // is_retargeting
      std::vector<GURL>(),               // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      initial_popup_url,                    // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      initial_url,                          // referrer_url
      GURL(),                               // referrer_main_frame_url
      true,                                 // is_retargeting
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

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       IdentifyReferrerChainByPendingEventURL_TwoUserGestures) {
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL page_before_landing_referrer_url =
      embedded_test_server()->GetURL(kPageBeforeLandingReferrerURL);
  GURL landing_referrer_url =
      embedded_test_server()->GetURL(kLandingReferrerURL);
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  ClickTestLink("attribution_within_two_user_gestures", 1, initial_url);

  ClickTestLink("link_to_landing_referrer", 1,
                page_before_landing_referrer_url);

  // Navigate to landing_url. Keep the navigation in pending state so we can
  // test on the pending event API.
  content::TestNavigationManager navigation_manager(
      browser()->tab_strip_model()->GetActiveWebContents(), landing_url);
  ClickTestLinkPending("link_to_landing", 1, landing_referrer_url);
  EXPECT_TRUE(navigation_manager.WaitForResponse());

  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(3U, nav_list->NavigationEventsSize());
  ASSERT_EQ(1U, nav_list->PendingNavigationEventsSize());

  ReferrerChain referrer_chain;
  auto result = observer_manager_->IdentifyReferrerChainByPendingEventURL(
      landing_url,
      /*user_gesture_count_limit=*/2, &referrer_chain);
  ASSERT_EQ(SafeBrowsingNavigationObserverManager::SUCCESS_LANDING_REFERRER,
            result);
  EXPECT_EQ(3, referrer_chain.size());
  VerifyReferrerChainEntry(
      landing_url,                    // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      landing_referrer_url,           // referrer_url
      GURL(),                         // referrer_main_frame_url
      true,                           // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  VerifyReferrerChainEntry(
      landing_referrer_url,              // url
      GURL(),                            // main_frame_url
      ReferrerChainEntry::LANDING_PAGE,  // type
      test_server_ip,                    // ip_address
      page_before_landing_referrer_url,  // referrer_url
      GURL(),                            // referrer_main_frame_url
      false,                             // is_retargeting
      std::vector<GURL>(),               // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(1));
  VerifyReferrerChainEntry(
      page_before_landing_referrer_url,      // url
      GURL(),                                // main_frame_url
      ReferrerChainEntry::LANDING_REFERRER,  // type
      test_server_ip,                        // ip_address
      GURL(),                                // referrer_url
      GURL(),                                // referrer_main_frame_url
      false,                                 // is_retargeting
      std::vector<GURL>(),                   // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(2));

  navigation_manager.ResumeNavigation();
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_EQ(0U, nav_list->PendingNavigationEventsSize());
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       IdentifyReferrerChainByPendingEventURL_ServerRedirect) {
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  GURL request_url =
      embedded_test_server()->GetURL("/server-redirect?" + landing_url.spec());
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  content::TestNavigationManager navigation_manager(
      browser()->tab_strip_model()->GetActiveWebContents(), request_url);

  // Navigate to request_url. Keep the navigation in pending state so we can
  // test on the pending event API.
  browser()->tab_strip_model()->GetActiveWebContents()->GetController().LoadURL(
      request_url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
      std::string());
  EXPECT_TRUE(navigation_manager.WaitForResponse());

  auto* nav_list = navigation_event_list();
  ASSERT_TRUE(nav_list);
  ASSERT_EQ(1U, nav_list->PendingNavigationEventsSize());

  ReferrerChain referrer_chain;
  auto result = observer_manager_->IdentifyReferrerChainByPendingEventURL(
      request_url,
      /*user_gesture_count_limit=*/2, &referrer_chain);
  // The navigation event is not found because the destination URL has already
  // changed to landing_url.
  ASSERT_EQ(SafeBrowsingNavigationObserverManager::NAVIGATION_EVENT_NOT_FOUND,
            result);

  result = observer_manager_->IdentifyReferrerChainByPendingEventURL(
      landing_url,
      /*user_gesture_count_limit=*/2, &referrer_chain);
  ASSERT_EQ(SafeBrowsingNavigationObserverManager::SUCCESS, result);
  EXPECT_EQ(1, referrer_chain.size());
  VerifyReferrerChainEntry(landing_url,                    // url
                           GURL(),                         // main_frame_url
                           ReferrerChainEntry::EVENT_URL,  // type
                           test_server_ip,                 // ip_address
                           GURL(),                         // referrer_url
                           GURL(),  // referrer_main_frame_url
                           true,    // is_retargeting
                           {request_url, landing_url},  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(0));

  navigation_manager.ResumeNavigation();
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_EQ(0U, nav_list->PendingNavigationEventsSize());
}

// Verify URLs that match the Safe Browsing allowlist domains (a.k.a
// prefs::kSafeBrowsingAllowlistDomains) are removed from the referrer chain.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       AllowlistDomainsRemoved_ReferrerChain) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
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

  // Add URLs to the Safe Browsing allowlist.
  base::Value::List allowlist;
  allowlist.Append(initial_url.host());
  allowlist.Append(multi_frame_test_url.host());
  allowlist.Append(iframe_url.host());
  allowlist.Append(iframe_retargeting_url.host());
  allowlist.Append(download_url.host());
  browser()->profile()->GetPrefs()->SetList(
      prefs::kSafeBrowsingAllowlistDomains, std::move(allowlist));

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(4, referrer_chain.size());
  // All URL fields should be empty because they all match the allowlist.
  VerifyReferrerChainEntry(
      GURL(),                         // url
      GURL(),                         // main_frame_url
      ReferrerChainEntry::EVENT_URL,  // type
      test_server_ip,                 // ip_address
      GURL(),                         // referrer_url
      GURL(),                         // referrer_main_frame_url
      false,                          // is_retargeting
      std::vector<GURL>(),            // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  EXPECT_TRUE(referrer_chain.Get(0).is_url_removed_by_policy());
  VerifyReferrerChainEntry(
      GURL(),                            // url
      GURL(),                            // main_frame_url
      ReferrerChainEntry::LANDING_PAGE,  // type
      test_server_ip,                    // ip_address
      GURL(),                            // referrer_url
      GURL(),                            // referrer_main_frame_url
      false,                             // is_retargeting
      std::vector<GURL>(),               // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE,
      referrer_chain.Get(1));
  EXPECT_TRUE(referrer_chain.Get(1).is_url_removed_by_policy());
  VerifyReferrerChainEntry(
      GURL(),                               // url
      GURL(),                               // main_frame_url
      ReferrerChainEntry::CLIENT_REDIRECT,  // type
      test_server_ip,                       // ip_address
      GURL(),                               // referrer_url
      GURL(),                               // referrer_main_frame_url
      false,                                // is_retargeting
      std::vector<GURL>(),                  // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(2));
  EXPECT_TRUE(referrer_chain.Get(2).is_url_removed_by_policy());
  VerifyReferrerChainEntry(GURL(),  // url
                           GURL(),  // main_frame_url
                           ReferrerChainEntry::LANDING_REFERRER,  // type
                           test_server_ip,                        // ip_address
                           GURL(),               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(3));
  EXPECT_TRUE(referrer_chain.Get(3).is_url_removed_by_policy());
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       AllowlistDomainsRemoved_ServerRedirect) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL request_url =
      embedded_test_server()->GetURL("/server-redirect?" + download_url.spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), request_url));
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  // Add URLs to the Safe Browsing allowlist.
  base::Value::List allowlist;
  allowlist.Append(initial_url.host());
  allowlist.Append(download_url.host());
  allowlist.Append(request_url.host());
  browser()->profile()->GetPrefs()->SetList(
      prefs::kSafeBrowsingAllowlistDomains, std::move(allowlist));

  ReferrerChain referrer_chain;
  IdentifyReferrerChainForDownload(GetDownload(), &referrer_chain);
  ASSERT_EQ(1, referrer_chain.size());
  EXPECT_TRUE(referrer_chain.Get(0).is_url_removed_by_policy());
  // All URL fields should be empty because they all match the allowlist.
  VerifyReferrerChainEntry(GURL(),                         // url
                           GURL(),                         // main_frame_url
                           ReferrerChainEntry::EVENT_URL,  // type
                           test_server_ip,                 // ip_address
                           GURL(),                         // referrer_url
                           GURL(),            // referrer_main_frame_url
                           false,             // is_retargeting
                           {GURL(), GURL()},  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(0));
}

IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       AllowlistDomainsRemoved_RecentNavigation) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("direct_download", 1, initial_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  // Add URLs to the Safe Browsing allowlist.
  base::Value::List allowlist;
  allowlist.Append(initial_url.host());
  allowlist.Append(download_url.host());
  browser()->profile()->GetPrefs()->SetList(
      prefs::kSafeBrowsingAllowlistDomains, std::move(allowlist));

  ReferrerChain referrer_chain;
  AppendRecentNavigations(/*recent_navigation_count=*/2, &referrer_chain);
  EXPECT_EQ(2, referrer_chain.size());
  // All URL fields should be empty because they all match the allowlist.
  VerifyReferrerChainEntry(
      GURL(),                                 // url
      GURL(),                                 // main_frame_url
      ReferrerChainEntry::RECENT_NAVIGATION,  // type
      test_server_ip,                         // ip_address
      GURL(),                                 // referrer_url
      GURL(),                                 // referrer_main_frame_url
      false,                                  // is_retargeting
      std::vector<GURL>(),                    // server redirects
      ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE,
      referrer_chain.Get(0));
  EXPECT_TRUE(referrer_chain.Get(0).is_url_removed_by_policy());
  VerifyReferrerChainEntry(GURL(),  // url
                           GURL(),  // main_frame_url
                           ReferrerChainEntry::RECENT_NAVIGATION,  // type
                           test_server_ip,                         // ip_address
                           GURL(),               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::BROWSER_INITIATED,
                           referrer_chain.Get(1));
  EXPECT_TRUE(referrer_chain.Get(1).is_url_removed_by_policy());
}

// Test failure on macOS: crbug.com/1287901
#if BUILDFLAG(IS_MAC)
#define MAYBE_AppendRecentNavigationsToEmptyReferrerChain \
  DISABLED_AppendRecentNavigationsToEmptyReferrerChain
#else
#define MAYBE_AppendRecentNavigationsToEmptyReferrerChain \
  AppendRecentNavigationsToEmptyReferrerChain
#endif
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       MAYBE_AppendRecentNavigationsToEmptyReferrerChain) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kSingleFrameTestURL)));
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  ClickTestLink("complete_referrer_chain", 2, initial_url);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectToLandingURL);
  GURL landing_url = embedded_test_server()->GetURL(kLandingURL);
  ClickTestLink("download_on_landing_page", 1, landing_url);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  std::string test_server_ip(embedded_test_server()->host_port_pair().host());

  ReferrerChain referrer_chain;
  AppendRecentNavigations(/*recent_navigation_count=*/3, &referrer_chain);
  EXPECT_EQ(4, referrer_chain.size());
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
  // This entry is empty, because it is omitted to make space for entries with
  // user gesture.
  VerifyReferrerChainEntry(GURL(),  // url
                           GURL(),  // main_frame_url
                           ReferrerChainEntry::CLIENT_REDIRECT,  // type
                           "",                                   // ip_address
                           GURL(),                               // referrer_url
                           GURL(),               // referrer_main_frame_url
                           false,                // is_retargeting
                           std::vector<GURL>(),  // server redirects
                           ReferrerChainEntry::UNDEFINED,
                           referrer_chain.Get(1));
  // The is_url_removed_by_policy field should not be set on empty entry.
  EXPECT_FALSE(referrer_chain.Get(1).has_is_url_removed_by_policy());
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

}  // namespace safe_browsing
