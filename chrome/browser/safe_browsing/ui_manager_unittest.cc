// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ui_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/db/util.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

static const char* kGoodURL = "https://www.good.com";
static const char* kBadURL = "https://www.malware.com";
static const char* kBadURLWithPath = "https://www.malware.com/index.html";
static const char* kAnotherBadURL = "https://www.badware.com";
static const char* kLandingURL = "https://www.landing.com";

namespace safe_browsing {

class SafeBrowsingCallbackWaiter {
  public:
   SafeBrowsingCallbackWaiter() {}

   bool callback_called() const { return callback_called_; }
   bool proceed() const { return proceed_; }

   void OnBlockingPageDone(bool proceed, bool showed_interstitial) {
     DCHECK_CURRENTLY_ON(BrowserThread::UI);
     callback_called_ = true;
     proceed_ = proceed;
     loop_.Quit();
   }

   void OnBlockingPageDoneOnIO(bool proceed, bool showed_interstitial) {
     DCHECK_CURRENTLY_ON(BrowserThread::IO);
     content::GetUIThreadTaskRunner({})->PostTask(
         FROM_HERE,
         base::BindOnce(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                        base::Unretained(this), proceed, showed_interstitial));
   }

   void WaitForCallback() {
     DCHECK_CURRENTLY_ON(BrowserThread::UI);
     loop_.Run();
   }

  private:
   bool callback_called_ = false;
   bool proceed_ = false;
   base::RunLoop loop_;
};

class SafeBrowsingUIManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  SafeBrowsingUIManagerTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {
    ui_manager_ = new SafeBrowsingUIManager(nullptr);
  }

  ~SafeBrowsingUIManagerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SafeBrowsingUIManager::CreateWhitelistForTesting(web_contents());

    safe_browsing::TestSafeBrowsingServiceFactory sb_service_factory;
    auto* safe_browsing_service =
        sb_service_factory.CreateSafeBrowsingService();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        safe_browsing_service);
    g_browser_process->safe_browsing_service()->Initialize();
    // A profile was created already but SafeBrowsingService wasn't around to
    // get notified of it, so include that notification now.
    safe_browsing_service->OnProfileAdded(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    content::BrowserThread::RunAllPendingTasksOnThreadForTesting(
        content::BrowserThread::IO);
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);

    // Depends on LocalState from ChromeRenderViewHostTestHarness.
    if (SystemNetworkContextManager::GetInstance())
      SystemNetworkContextManager::DeleteInstance();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  bool IsWhitelisted(security_interstitials::UnsafeResource resource) {
    return ui_manager_->IsWhitelisted(resource);
  }

  void AddToWhitelist(security_interstitials::UnsafeResource resource) {
    ui_manager_->AddToWhitelistUrlSet(
        SafeBrowsingUIManager::GetMainFrameWhitelistUrlForResourceForTesting(
            resource),
        web_contents(), false, resource.threat_type);
  }

  security_interstitials::UnsafeResource MakeUnsafeResource(
      const char* url,
      bool is_subresource) {
    security_interstitials::UnsafeResource resource;
    resource.url = GURL(url);
    resource.is_subresource = is_subresource;
    resource.web_contents_getter = security_interstitials::GetWebContentsGetter(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID());
    resource.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    return resource;
  }

  security_interstitials::UnsafeResource MakeUnsafeResourceAndStartNavigation(
      const char* url) {
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(url, false /* is_subresource */);

    // The WC doesn't have a URL without a navigation. A main-frame malware
    // unsafe resource must be a pending navigation.
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL(url), web_contents());
    navigation->Start();
    return resource;
  }

  void SimulateBlockingPageDone(
      const std::vector<security_interstitials::UnsafeResource>& resources,
      bool proceed) {
    GURL main_frame_url;
    content::NavigationEntry* entry =
        web_contents()->GetController().GetVisibleEntry();
    if (entry)
      main_frame_url = entry->GetURL();

    ui_manager_->OnBlockingPageDone(resources, proceed, web_contents(),
                                    main_frame_url,
                                    true /* showed_interstitial */);
  }

 protected:
  SafeBrowsingUIManager* ui_manager() { return ui_manager_.get(); }

 private:
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  ScopedTestingLocalState scoped_testing_local_state_;
};

// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_Whitelist DISABLED_Whitelist
#else
#define MAYBE_Whitelist Whitelist
#endif
TEST_F(SafeBrowsingUIManagerTest, MAYBE_Whitelist) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));
}

// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_WhitelistIgnoresSitesNotAdded \
  DISABLED_WhitelistIgnoresSitesNotAdded
#else
#define MAYBE_WhitelistIgnoresSitesNotAdded WhitelistIgnoresSitesNotAdded
#endif
TEST_F(SafeBrowsingUIManagerTest, MAYBE_WhitelistIgnoresSitesNotAdded) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kGoodURL);
  EXPECT_FALSE(IsWhitelisted(resource));
}

// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_WhitelistRemembersThreatType DISABLED_WhitelistRemembersThreatType
#else
#define MAYBE_WhitelistRemembersThreatType WhitelistRemembersThreatType
#endif
TEST_F(SafeBrowsingUIManagerTest, MAYBE_WhitelistRemembersThreatType) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));
  SBThreatType threat_type;
  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(ui_manager()->IsUrlWhitelistedOrPendingForWebContents(
      resource.url, resource.is_subresource, entry,
      resource.web_contents_getter.Run(), true, &threat_type));
  EXPECT_EQ(resource.threat_type, threat_type);
}

// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_WhitelistIgnoresPath DISABLED_WhitelistIgnoresPath
#else
#define MAYBE_WhitelistIgnoresPath WhitelistIgnoresPath
#endif
TEST_F(SafeBrowsingUIManagerTest, MAYBE_WhitelistIgnoresPath) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));

  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

  security_interstitials::UnsafeResource resource_path =
      MakeUnsafeResourceAndStartNavigation(kBadURLWithPath);
  EXPECT_TRUE(IsWhitelisted(resource_path));
}

// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_WhitelistIgnoresThreatType DISABLED_WhitelistIgnoresThreatType
#else
#define MAYBE_WhitelistIgnoresThreatType WhitelistIgnoresThreatType
#endif
TEST_F(SafeBrowsingUIManagerTest, MAYBE_WhitelistIgnoresThreatType) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  AddToWhitelist(resource);
  EXPECT_TRUE(IsWhitelisted(resource));

  security_interstitials::UnsafeResource resource_phishing =
      MakeUnsafeResource(kBadURL, false /* is_subresource */);
  resource_phishing.threat_type = SB_THREAT_TYPE_URL_PHISHING;
  EXPECT_TRUE(IsWhitelisted(resource_phishing));
}

// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_WhitelistWithUnrelatedPendingLoad \
  DISABLED_WhitelistWithUnrelatedPendingLoad
#else
#define MAYBE_WhitelistWithUnrelatedPendingLoad \
  WhitelistWithUnrelatedPendingLoad
#endif
TEST_F(SafeBrowsingUIManagerTest, MAYBE_WhitelistWithUnrelatedPendingLoad) {
  // Commit load of landing page.
  NavigateAndCommit(GURL(kLandingURL));
  auto unrelated_navigation =
      content::NavigationSimulator::CreateBrowserInitiated(GURL(kGoodURL),
                                                           web_contents());
  {
    // Simulate subresource malware hit on the landing page.
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, true /* is_subresource */);

    // Start pending load to unrelated site.
    unrelated_navigation->Start();

    // Whitelist the resource on the landing page.
    AddToWhitelist(resource);
    EXPECT_TRUE(IsWhitelisted(resource));
  }

  // Commit the pending load of unrelated site.
  unrelated_navigation->Commit();
  {
    // The unrelated site is not on the whitelist, even if the same subresource
    // was on it.
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, true /* is_subresource */);
    EXPECT_FALSE(IsWhitelisted(resource));
  }

  // Navigate back to the original landing url.
  NavigateAndCommit(GURL(kLandingURL));
  {
    security_interstitials::UnsafeResource resource =
        MakeUnsafeResource(kBadURL, true /* is_subresource */);
    // Original resource url is whitelisted.
    EXPECT_TRUE(IsWhitelisted(resource));
  }
  {
    // A different malware subresource on the same page is also whitelisted.
    // (The whitelist is by the page url, not the resource url.)
    security_interstitials::UnsafeResource resource2 =
        MakeUnsafeResource(kAnotherBadURL, true /* is_subresource */);
    EXPECT_TRUE(IsWhitelisted(resource2));
  }
}

// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_UICallbackProceed DISABLED_UICallbackProceed
#else
#define MAYBE_UICallbackProceed UICallbackProceed
#endif
TEST_F(SafeBrowsingUIManagerTest, MAYBE_UICallbackProceed) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                 base::Unretained(&waiter));
  resource.callback_thread = content::GetUIThreadTaskRunner({});
  std::vector<security_interstitials::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, true);
  EXPECT_TRUE(IsWhitelisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_TRUE(waiter.proceed());
}

// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_UICallbackDontProceed DISABLED_UICallbackDontProceed
#else
#define MAYBE_UICallbackDontProceed UICallbackDontProceed
#endif
TEST_F(SafeBrowsingUIManagerTest, MAYBE_UICallbackDontProceed) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDone,
                 base::Unretained(&waiter));
  resource.callback_thread = content::GetUIThreadTaskRunner({});
  std::vector<security_interstitials::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, false);
  EXPECT_FALSE(IsWhitelisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_FALSE(waiter.proceed());
}

// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_IOCallbackProceed DISABLED_IOCallbackProceed
#else
#define MAYBE_IOCallbackProceed IOCallbackProceed
#endif
TEST_F(SafeBrowsingUIManagerTest, MAYBE_IOCallbackProceed) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                 base::Unretained(&waiter));
  resource.callback_thread = content::GetIOThreadTaskRunner({});
  std::vector<security_interstitials::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, true);
  EXPECT_TRUE(IsWhitelisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_TRUE(waiter.proceed());
}

// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_IOCallbackDontProceed DISABLED_IOCallbackDontProceed
#else
#define MAYBE_IOCallbackDontProceed IOCallbackDontProceed
#endif
TEST_F(SafeBrowsingUIManagerTest, MAYBE_IOCallbackDontProceed) {
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResourceAndStartNavigation(kBadURL);
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                 base::Unretained(&waiter));
  resource.callback_thread = content::GetIOThreadTaskRunner({});
  std::vector<security_interstitials::UnsafeResource> resources;
  resources.push_back(resource);
  SimulateBlockingPageDone(resources, false);
  EXPECT_FALSE(IsWhitelisted(resource));
  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_FALSE(waiter.proceed());
}

namespace {

// A WebContentsDelegate that records whether
// VisibleSecurityStateChanged() was called.
class SecurityStateWebContentsDelegate : public content::WebContentsDelegate {
 public:
  SecurityStateWebContentsDelegate() {}
  ~SecurityStateWebContentsDelegate() override {}

  bool visible_security_state_changed() const {
    return visible_security_state_changed_;
  }

  void ClearVisibleSecurityStateChanged() {
    visible_security_state_changed_ = false;
  }

  // WebContentsDelegate:
  void VisibleSecurityStateChanged(content::WebContents* source) override {
    visible_security_state_changed_ = true;
  }

 private:
  bool visible_security_state_changed_ = false;
  DISALLOW_COPY_AND_ASSIGN(SecurityStateWebContentsDelegate);
};

// A test blocking page that does not create windows.
class TestSafeBrowsingBlockingPage : public SafeBrowsingBlockingPage {
 public:
  TestSafeBrowsingBlockingPage(BaseUIManager* manager,
                               content::WebContents* web_contents,
                               const GURL& main_frame_url,
                               const UnsafeResourceList& unsafe_resources)
      : SafeBrowsingBlockingPage(
            manager,
            web_contents,
            main_frame_url,
            unsafe_resources,
            BaseSafeBrowsingErrorUI::SBErrorDisplayOptions(
                BaseBlockingPage::IsMainPageLoadBlocked(unsafe_resources),
                false,                 // is_extended_reporting_opt_in_allowed
                false,                 // is_off_the_record
                false,                 // is_extended_reporting_enabled
                false,                 // is_extended_reporting_policy_managed
                false,                 // is_enhanced_protection_enabled
                false,                 // is_proceed_anyway_disabled
                true,                  // should_open_links_in_new_tab
                true,                  // always_show_back_to_safety
                false,                 // is_enhanced_protection_message_enabled
                false,                 // is_safe_browsing_managed
                "cpn_safe_browsing"),  // help_center_article_link
            true) {                    // should_trigger_reporting
    // Don't delay details at all for the unittest.
    SetThreatDetailsProceedDelayForTesting(0);
    DontCreateViewForTesting();
  }
};

// A factory that creates TestSafeBrowsingBlockingPages.
class TestSafeBrowsingBlockingPageFactory
    : public SafeBrowsingBlockingPageFactory {
 public:
  TestSafeBrowsingBlockingPageFactory() {}
  ~TestSafeBrowsingBlockingPageFactory() override {}

  SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      BaseUIManager* delegate,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources,
      bool should_trigger_reporting) override {
    return new TestSafeBrowsingBlockingPage(delegate, web_contents,
                                            main_frame_url, unsafe_resources);
  }
};

}  // namespace

// Tests that the WebContentsDelegate is notified of a visible security
// state change when a blocking page is shown for a subresource.
// Leaks memory. https://crbug.com/755118
#if defined(LEAK_SANITIZER)
#define MAYBE_VisibleSecurityStateChangedForUnsafeSubresource \
  DISABLED_VisibleSecurityStateChangedForUnsafeSubresource
#else
#define MAYBE_VisibleSecurityStateChangedForUnsafeSubresource \
  VisibleSecurityStateChangedForUnsafeSubresource
#endif
TEST_F(SafeBrowsingUIManagerTest,
       MAYBE_VisibleSecurityStateChangedForUnsafeSubresource) {
  TestSafeBrowsingBlockingPageFactory factory;
  SafeBrowsingBlockingPage::RegisterFactory(&factory);
  SecurityStateWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  // Simulate a blocking page showing for an unsafe subresource.
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResource(kBadURL, true /* is_subresource */);
  // Needed for showing the blocking page.
  resource.threat_source = safe_browsing::ThreatSource::REMOTE;

  NavigateAndCommit(GURL("http://example.test"));

  delegate.ClearVisibleSecurityStateChanged();
  EXPECT_FALSE(delegate.visible_security_state_changed());
  ui_manager()->DisplayBlockingPage(resource);
  EXPECT_TRUE(delegate.visible_security_state_changed());

  // Simulate proceeding through the blocking page.
  SafeBrowsingCallbackWaiter waiter;
  resource.callback =
      base::Bind(&SafeBrowsingCallbackWaiter::OnBlockingPageDoneOnIO,
                 base::Unretained(&waiter));
  resource.callback_thread = content::GetIOThreadTaskRunner({});
  std::vector<security_interstitials::UnsafeResource> resources;
  resources.push_back(resource);

  delegate.ClearVisibleSecurityStateChanged();
  EXPECT_FALSE(delegate.visible_security_state_changed());
  SimulateBlockingPageDone(resources, true);
  EXPECT_TRUE(delegate.visible_security_state_changed());

  waiter.WaitForCallback();
  EXPECT_TRUE(waiter.callback_called());
  EXPECT_TRUE(waiter.proceed());
  EXPECT_TRUE(IsWhitelisted(resource));
}

TEST_F(SafeBrowsingUIManagerTest, ShowBlockPageNoCallback) {
  TestSafeBrowsingBlockingPageFactory factory;
  SafeBrowsingBlockingPage::RegisterFactory(&factory);
  SecurityStateWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  // Simulate a blocking page showing for an unsafe subresource.
  security_interstitials::UnsafeResource resource =
      MakeUnsafeResource(kBadURL, false /* is_subresource */);
  // Needed for showing the blocking page.
  resource.threat_source = safe_browsing::ThreatSource::REMOTE;

  // This call caused a crash in https://crbug.com/1058094. Just verify that we
  // don't crash anymore.
  ui_manager()->DisplayBlockingPage(resource);
}

}  // namespace safe_browsing
