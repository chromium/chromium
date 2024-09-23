// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language/core/common/language_experiments.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "components/translate/core/browser/translate_driver.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

class TestLocalSessionEventHandler
    : public sync_sessions::LocalSessionEventHandler {
 public:
  void OnSessionRestoreComplete() override {}

  void OnLocalTabModified(
      sync_sessions::SyncedTabDelegate* modified_tab) override {
    local_tab_updated_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  bool local_tab_updated() { return local_tab_updated_; }
  void reset_local_tab_updated() { local_tab_updated_ = false; }

 private:
  base::OnceClosure quit_closure_;
  bool local_tab_updated_ = false;
};

class TestTranslateDriverObserver
    : public translate::TranslateDriver::LanguageDetectionObserver {
 public:
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override {
    if (interested_url_ != details.url) {
      return;
    }
    language_determined_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void WaitForLanguageDetermined() {
    if (language_determined_) {
      return;
    }
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void SetInterestedURL(const GURL& url) { interested_url_ = url; }

 private:
  GURL interested_url_;
  base::OnceClosure quit_closure_;
  bool language_determined_ = false;
};

}  // namespace

class SyncSessionsRouterTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  SyncSessionsRouterTabHelperBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SyncSessionsRouterTabHelperBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~SyncSessionsRouterTabHelperBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void AddLanguageDetectionObserver(const GURL& url) {
    observer_.SetInterestedURL(url);
    ChromeTranslateClient* chrome_translate_client =
        ChromeTranslateClient::FromWebContents(web_contents());
    if (!chrome_translate_client) {
      return;
    }
    chrome_translate_client->GetTranslateDriver()->AddLanguageDetectionObserver(
        &observer_);
  }

  void RemoveLanguageDetectionObserver() {
    observer_.SetInterestedURL(GURL());
    ChromeTranslateClient* chrome_translate_client =
        ChromeTranslateClient::FromWebContents(web_contents());
    if (!chrome_translate_client) {
      return;
    }
    chrome_translate_client->GetTranslateDriver()
        ->RemoveLanguageDetectionObserver(&observer_);
  }

  content::WebContents* web_contents() { return web_contents_; }
  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }
  TestLocalSessionEventHandler* GetSessionEventHandler() { return &handler; }
  TestTranslateDriverObserver* GetTranslateDriverObserver() {
    return &observer_;
  }

 protected:
 private:
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;
  content::test::PrerenderTestHelper prerender_helper_;
  TestLocalSessionEventHandler handler;
  TestTranslateDriverObserver observer_;
};

// Tests if SyncSessionsRouterTabHelper doesn't notify of
// SyncSessionsWebContentsRouter while a page without a title is prerendered.
IN_PROC_BROWSER_TEST_F(SyncSessionsRouterTabHelperBrowserTest,
                       SyncSessionRouterWithoutTitleInPrerender) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  // Set LanguageDetectionObserver to make sure that it starts prerendering
  // after OnLanguageDetermined() on the current loading which could trigger a
  // session sync.
  AddLanguageDetectionObserver(url);

  // Navigate to |url|.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  sync_sessions::SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(browser()->profile())
      ->StartRoutingTo(GetSessionEventHandler());
  // Wait for OnLanguageDetermined().
  GetTranslateDriverObserver()->WaitForLanguageDetermined();

  // Reset to test if it's updated in prerendering.
  GetSessionEventHandler()->reset_local_tab_updated();

  // Prerender a page. It uses a page that doesn't have a title to avoid getting
  // OnLocalTabModified() by SyncSessionsRouterTabHelper::TitleWasSet(). Except
  // it, SyncSessionsRouterTabHelper doesn't trigger OnLocalTabModified() on
  // prerendering.
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  content::FrameTreeNodeId prerender_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(),
                                                     prerender_id);
  // Make sure that OnLocalTabModified() is not called.
  EXPECT_FALSE(GetSessionEventHandler()->local_tab_updated());

  // Navigate the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // Make sure that once the prerendered page is activated,
  // OnLocalTabModified() is called.
  EXPECT_TRUE(GetSessionEventHandler()->local_tab_updated());

  // Clear LanguageDetectionObserver.
  RemoveLanguageDetectionObserver();
  // Stop Routing.
  sync_sessions::SyncSessionsWebContentsRouterFactory::GetInstance()
      ->GetForProfile(browser()->profile())
      ->Stop();

  // Make sure that the prerender was activated when the main frame was
  // navigated to the prerender_url.
  ASSERT_TRUE(host_observer.was_activated());
}
