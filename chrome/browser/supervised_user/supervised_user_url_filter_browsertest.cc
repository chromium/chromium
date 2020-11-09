// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/logged_in_user_mixin.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace {

class InterstitialPageObserver : public content::WebContentsObserver {
 public:
  InterstitialPageObserver(WebContents* web_contents,
                           const base::Closure& callback)
      : content::WebContentsObserver(web_contents), callback_(callback) {}
  ~InterstitialPageObserver() override {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // With committed interstitials, DidAttachInterstitialPage is not called, so
    // call the callback from here if there was an error.
    if (navigation_handle->IsErrorPage()) {
      callback_.Run();
    }
  }

 private:
  base::Closure callback_;
};

// Tests filtering for supervised users.
class SupervisedUserURLFilterTest : public MixinBasedInProcessBrowserTest {
 public:
  // Indicates whether the interstitial should proceed or not.
  enum InterstitialAction {
    INTERSTITIAL_PROCEED,
    INTERSTITIAL_DONTPROCEED,
  };

  SupervisedUserURLFilterTest() = default;
  ~SupervisedUserURLFilterTest() override = default;

  bool ShownPageIsInterstitial(Browser* browser) {
    WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_FALSE(tab->IsCrashed());
    base::string16 title;
    ui_test_utils::GetCurrentTabTitle(browser, &title);
    return tab->GetController().GetLastCommittedEntry()->GetPageType() ==
               content::PAGE_TYPE_ERROR &&
           title == base::ASCIIToUTF16("Site blocked");
  }

  void SendAccessRequest(WebContents* tab) {
    tab->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(
            "supervisedUserErrorPageController.requestPermission()"),
        base::NullCallback());
    return;
  }

  void GoBack(WebContents* tab) {
    tab->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("supervisedUserErrorPageController.goBack()"),
        base::NullCallback());
    return;
  }

  void GoBackAndWaitForNavigation(WebContents* tab) {
    content::TestNavigationObserver observer(tab);
    GoBack(tab);
    observer.Wait();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Remap all URLs to test server.
    ASSERT_TRUE(embedded_test_server()->Started());
    std::string host_port = embedded_test_server()->host_port_pair().ToString();
    command_line->AppendSwitchASCII(network::switches::kHostResolverRules,
                                    "MAP *.example.com " + host_port + "," +
                                        "MAP *.new-example.com " + host_port +
                                        "," + "MAP *.a.com " + host_port);
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();

    supervised_user_service_ =
        SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  }

  // Acts like a synchronous call to history's QueryHistory. Modified from
  // history_querying_unittest.cc.
  void QueryHistory(history::HistoryService* history_service,
                    const std::string& text_query,
                    const history::QueryOptions& options,
                    history::QueryResults* results) {
    base::RunLoop run_loop;
    base::CancelableTaskTracker history_task_tracker;
    history_service->QueryHistory(
        base::UTF8ToUTF16(text_query), options,
        base::BindLambdaForTesting([&](history::QueryResults r) {
          *results = std::move(r);
          run_loop.Quit();
        }),
        &history_task_tracker);
    run_loop.Run();  // Will go until ...Complete calls Quit.
  }

  SupervisedUserService* supervised_user_service_ = nullptr;

  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, chromeos::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};
};

// Tests the filter mode in which all sites are blocked by default.
class SupervisedUserBlockModeTest : public SupervisedUserURLFilterTest {
 public:
  void SetUpOnMainThread() override {
    SupervisedUserURLFilterTest::SetUpOnMainThread();
    Profile* profile = browser()->profile();
    SupervisedUserSettingsService* supervised_user_settings_service =
        SupervisedUserSettingsServiceFactory::GetForKey(
            profile->GetProfileKey());
    supervised_user_settings_service->SetLocalSetting(
        supervised_users::kContentPackDefaultFilteringBehavior,
        std::make_unique<base::Value>(SupervisedUserURLFilter::BLOCK));
  }
};

class TabClosingObserver : public TabStripModelObserver {
 public:
  TabClosingObserver(TabStripModel* tab_strip, content::WebContents* contents)
      : tab_strip_(tab_strip), contents_(contents) {
    tab_strip_->AddObserver(this);
  }

  void WaitForContentsClosing() {
    if (!contents_)
      return;

    run_loop_.Run();
  }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kRemoved)
      return;

    auto* remove = change.GetRemove();
    if (!remove->will_be_deleted)
      return;

    for (const auto& contents : remove->contents) {
      if (contents_ == contents.contents) {
        if (run_loop_.running())
          run_loop_.Quit();
        contents_ = nullptr;
        return;
      }
    }
  }

 private:
  TabStripModel* tab_strip_ = nullptr;

  base::RunLoop run_loop_;

  // Contents to wait for.
  content::WebContents* contents_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TabClosingObserver);
};

// Navigates to a blocked URL.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest,
                       SendAccessRequestOnBlockedURL) {
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  SendAccessRequest(tab);

  // TODO(sergiu): Properly check that the access request was sent here.

  GoBackAndWaitForNavigation(tab);

  // Make sure that the tab is still there.
  EXPECT_EQ(tab, browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_FALSE(ShownPageIsInterstitial(browser()));
}

// Navigates to a blocked URL in a new tab. We expect the tab to be closed
// automatically on pressing the "back" button on the interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest, OpenBlockedURLInNewTab) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  WebContents* prev_tab = tab_strip->GetActiveWebContents();

  // Open blocked URL in a new tab.
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Check that we got the interstitial.
  WebContents* tab = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  // On pressing the "back" button, the new tab should be closed, and we should
  // get back to the previous active tab.
  TabClosingObserver observer(tab_strip, tab);
  GoBack(tab);
  observer.WaitForContentsClosing();

  EXPECT_EQ(prev_tab, tab_strip->GetActiveWebContents());
}

// Navigates to a page in a new tab, then blocks it (which makes the
// interstitial page behave differently from the preceding test, where the
// navigation is blocked before it commits). The expected behavior is the same
// though: the tab should be closed when going back.
IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, BlockNewTabAfterLoading) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  WebContents* prev_tab = tab_strip->GetActiveWebContents();

  // Open URL in a new tab.
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Check that there is no interstitial.
  WebContents* tab = tab_strip->GetActiveWebContents();
  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  {
    // Block the current URL.
    SupervisedUserSettingsService* supervised_user_settings_service =
        SupervisedUserSettingsServiceFactory::GetForKey(
            browser()->profile()->GetProfileKey());
    supervised_user_settings_service->SetLocalSetting(
        supervised_users::kContentPackDefaultFilteringBehavior,
        std::make_unique<base::Value>(SupervisedUserURLFilter::BLOCK));

    const SupervisedUserURLFilter* filter =
        supervised_user_service_->GetURLFilter();
    ASSERT_EQ(SupervisedUserURLFilter::BLOCK,
              filter->GetFilteringBehaviorForURL(test_url));

    content::TestNavigationObserver observer(tab);
    observer.Wait();

    // Check that we got the interstitial.
    ASSERT_TRUE(ShownPageIsInterstitial(browser()));
  }

  {
    // On pressing the "back" button, the new tab should be closed, and we
    // should get back to the previous active tab.
    TabClosingObserver observer(tab_strip, tab);
    GoBack(tab);
    observer.WaitForContentsClosing();
    EXPECT_EQ(prev_tab, tab_strip->GetActiveWebContents());
  }
}

// Tests that we don't end up canceling an interstitial (thereby closing the
// whole tab) by attempting to show a second one above it.
IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, DontShowInterstitialTwice) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  // Open URL in a new tab.
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Check that there is no interstitial.
  WebContents* tab = tab_strip->GetActiveWebContents();
  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Block the current URL.
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackDefaultFilteringBehavior,
      std::make_unique<base::Value>(SupervisedUserURLFilter::BLOCK));

  const SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();
  ASSERT_EQ(SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(test_url));

  content::TestNavigationObserver observer(tab);
  observer.Wait();

  // Check that we got the interstitial.
  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  // Trigger a no-op change to the site lists, which will notify observers of
  // the URL filter.
  supervised_user_service_->OnSiteListUpdated();

  EXPECT_EQ(tab, tab_strip->GetActiveWebContents());
}

// Tests that it's possible to navigate from a blocked page to another blocked
// page.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest,
                       NavigateFromBlockedPageToBlockedPage) {
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  GURL test_url2("http://www.a.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url2);

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));
  EXPECT_EQ(test_url2, tab->GetVisibleURL());
}

// Tests whether a visit attempt adds a special history entry.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest, HistoryVisitRecorded) {
  GURL allowed_url("http://www.example.com/simple.html");

  const SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();

  // Set the host as allowed.
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetKey(allowed_url.host(), base::Value(true));
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(allowed_url));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(allowed_url.GetWithEmptyPath()));

  ui_test_utils::NavigateToURL(browser(), allowed_url);

  // Navigate to it and check that we don't get an interstitial.
  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Navigate to a blocked page and go back on the interstitial.
  GURL blocked_url("http://www.new-example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), blocked_url);

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GoBackAndWaitForNavigation(tab);

  EXPECT_EQ(allowed_url.spec(), tab->GetURL().spec());
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(allowed_url.GetWithEmptyPath()));
  EXPECT_EQ(SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(blocked_url.GetWithEmptyPath()));

  // Query the history entry.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser()->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::QueryOptions options;
  history::QueryResults results;
  QueryHistory(history_service, "", options, &results);

  // Check that the entries have the correct blocked_visit value.
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(blocked_url.spec(), results[1].url().spec());
  EXPECT_TRUE(results[1].blocked_visit());
  EXPECT_EQ(allowed_url.spec(), results[0].url().spec());
  EXPECT_FALSE(results[0].blocked_visit());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, GoBackOnDontProceed) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Ensure navigation completes.
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  // We start out at the initial navigation.
  ASSERT_EQ(0, web_contents->GetController().GetCurrentEntryIndex());

  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Set the host as blocked and wait for the interstitial to appear.
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey(test_url.host(), base::Value(false));
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));

  const SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();
  ASSERT_EQ(SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(test_url));

  content::TestNavigationObserver block_observer(web_contents);
  block_observer.Wait();

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  GoBack(web_contents);
  observer.Wait();

  // We should have gone back to the initial navigation.
  EXPECT_EQ(0, web_contents->GetController().GetCurrentEntryIndex());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest,
                       ClosingBlockedTabDoesNotCrash) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Ensure navigation completes.
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  ASSERT_EQ(0, web_contents->GetController().GetCurrentEntryIndex());

  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Set the host as blocked and wait for the interstitial to appear.
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey(test_url.host(), base::Value(false));
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));

  const SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();
  ASSERT_EQ(SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(test_url));

  // Verify that there is no crash when closing the blocked tab
  // (https://crbug.com/719708).
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabStripModel::CLOSE_USER_GESTURE);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, BlockThenUnblock) {
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Set the host as blocked and wait for the interstitial to appear.
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey(test_url.host(), base::Value(false));
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));

  const SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();
  ASSERT_EQ(SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(test_url));

  content::TestNavigationObserver block_observer(web_contents);
  block_observer.Wait();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey(test_url.host(), base::Value(true));
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));
  ASSERT_EQ(SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(test_url));

  content::TestNavigationObserver unblock_observer(web_contents);
  unblock_observer.Wait();

  ASSERT_EQ(test_url, web_contents->GetURL());

  EXPECT_FALSE(ShownPageIsInterstitial(browser()));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest, Unblock) {
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  // Set the host as allowed.
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetKey(test_url.host(), base::Value(true));
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));

  const SupervisedUserURLFilter* filter =
      supervised_user_service_->GetURLFilter();
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(test_url.GetWithEmptyPath()));

  observer.Wait();
  EXPECT_EQ(test_url, web_contents->GetURL());
}

}  // namespace
