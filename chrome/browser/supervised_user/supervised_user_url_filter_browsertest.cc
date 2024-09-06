// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filter.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/strong_alias.h"
#include "base/values.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/embedded_test_server_setup_mixin.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/history/core/browser/history_service.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_interstitial.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

constexpr char kBlockedContentUkmName[] = "FamilyLinkUser.BlockedContent";
constexpr char kBlockedContentUkmMainFrameMetricName[] = "MainFrameBlocked";
constexpr char kBlockedContentUkmIFrameMetricName[] = "NumBlockedIframes";

using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace {

// Tests filtering for supervised users.
class SupervisedUserURLFilterTestBase : public MixinBasedInProcessBrowserTest {
 public:
  // Indicates whether the interstitial should proceed or not.
  enum InterstitialAction {
    INTERSTITIAL_PROCEED,
    INTERSTITIAL_DONTPROCEED,
  };

  SupervisedUserURLFilterTestBase() {
    // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    feature_list_.InitWithFeatures({}, {features::kHttpsUpgrades});
  }
  ~SupervisedUserURLFilterTestBase() override { feature_list_.Reset(); }

  bool ShownPageIsInterstitial(Browser* browser) {
    WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_FALSE(tab->IsCrashed());
    std::u16string title;
    ui_test_utils::GetCurrentTabTitle(browser, &title);
    return tab->GetController().GetLastCommittedEntry()->GetPageType() ==
               content::PAGE_TYPE_ERROR &&
           title == u"Site blocked";
  }

  void SendAccessRequest(WebContents* tab) {
    tab->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"supervisedUserErrorPageController.requestPermission()",
        base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
    return;
  }

  void GoBack(WebContents* tab) {
    tab->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"supervisedUserErrorPageController.goBack()", base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
    return;
  }

  void GoBackAndWaitForNavigation(WebContents* tab) {
    content::TestNavigationObserver observer(tab);
    GoBack(tab);
    observer.Wait();
  }

 protected:
  // Acts like a synchronous call to history's QueryHistory. Modified from
  // history_querying_unittest.cc.
  void QueryHistory(history::HistoryService* history_service,
                    history::QueryResults* results) {
    base::RunLoop run_loop;
    history::QueryOptions options;
    base::CancelableTaskTracker history_task_tracker;
    history_service->QueryHistory(
        u"", options, base::BindLambdaForTesting([&](history::QueryResults r) {
          *results = std::move(r);
          run_loop.Quit();
        }),
        &history_task_tracker);
    run_loop.Run();  // Will go until ...Complete calls Quit.
  }

  supervised_user::KidsManagementApiServerMock& kids_management_api_mock() {
    return supervision_mixin_.api_mock_setup_mixin().api_mock();
  }

  supervised_user::SupervisedUserService* GetSupervisedUserService() const {
    return SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {
          .sign_in_mode =
              supervised_user::SupervisionMixin::SignInMode::kSupervised,
          .embedded_test_server_options =
              {.resolver_rules_map_host_list =
                   "*.example.com, *.new-example.com"},
      }};
};

class TabClosingObserver : public TabStripModelObserver {
 public:
  TabClosingObserver(TabStripModel* tab_strip, content::WebContents* contents)
      : tab_strip_(tab_strip), contents_(contents) {
    tab_strip_->AddObserver(this);
  }

  TabClosingObserver(const TabClosingObserver&) = delete;
  TabClosingObserver& operator=(const TabClosingObserver&) = delete;

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
    for (const auto& contents : remove->contents) {
      if (contents_ == contents.contents &&
          contents.remove_reason ==
              TabStripModelChange::RemoveReason::kDeleted) {
        if (run_loop_.running())
          run_loop_.Quit();
        contents_ = nullptr;
        return;
      }
    }
  }

 private:
  raw_ptr<TabStripModel> tab_strip_;

  base::RunLoop run_loop_;

  // Contents to wait for.
  raw_ptr<content::WebContents> contents_;
};

using SupervisedUserURLFilterTest = SupervisedUserURLFilterTestBase;

// Navigates to a page in a new tab, then blocks it (which makes the
// interstitial page behave differently from the preceding test, where the
// navigation is blocked before it commits). The expected behavior is the same
// though: the tab should be closed when going back.
IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, BlockNewTabAfterLoading) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  WebContents* prev_tab = tab_strip->GetActiveWebContents();
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Open URL in a new tab.
  GURL test_url("http://www.example.com/simple.html");
  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Check that there is no interstitial.
  WebContents* tab = tab_strip->GetActiveWebContents();
  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Check that no UKM is recorded.
  EXPECT_TRUE(ukm_recorder
                  .GetEntries(kBlockedContentUkmName,
                              {kBlockedContentUkmMainFrameMetricName,
                               kBlockedContentUkmIFrameMetricName})
                  .empty());

  {
    // Block the current URL.
    supervised_user::SupervisedUserSettingsService*
        supervised_user_settings_service =
            SupervisedUserSettingsServiceFactory::GetForKey(
                browser()->profile()->GetProfileKey());
    supervised_user_settings_service->SetLocalSetting(
        supervised_user::kContentPackDefaultFilteringBehavior,
        base::Value(
            static_cast<int>(supervised_user::FilteringBehavior::kBlock)));

    supervised_user::SupervisedUserURLFilter* filter =
        GetSupervisedUserService()->GetURLFilter();
    ASSERT_EQ(supervised_user::FilteringBehavior::kBlock,
              filter->GetFilteringBehaviorForURL(test_url));

    content::TestNavigationObserver observer(tab);
    observer.Wait();

    // Check that we got the interstitial.
    ASSERT_TRUE(ShownPageIsInterstitial(browser()));

    // Check that no UKM is recorded (because the mainframe was blocked due to
    // a manual block rather than due to safesites).
    EXPECT_TRUE(ukm_recorder
                    .GetEntries(kBlockedContentUkmName,
                                {kBlockedContentUkmMainFrameMetricName,
                                 kBlockedContentUkmIFrameMetricName})
                    .empty());
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
  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Check that there is no interstitial.
  WebContents* tab = tab_strip->GetActiveWebContents();
  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Block the current URL.
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior,
      base::Value(
          static_cast<int>(supervised_user::FilteringBehavior::kBlock)));

  supervised_user::SupervisedUserURLFilter* filter =
      GetSupervisedUserService()->GetURLFilter();
  ASSERT_EQ(supervised_user::FilteringBehavior::kBlock,
            filter->GetFilteringBehaviorForURL(test_url));

  content::TestNavigationObserver observer(tab);
  observer.Wait();

  // Check that we got the interstitial.
  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  // Set the host as blocked through manual blocklisting, should not change the
  // interstitial state.
  base::Value::Dict dict;
  dict.Set(test_url.host(), false);
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts, std::move(dict));

  EXPECT_EQ(tab, tab_strip->GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, GoBackOnDontProceed) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Ensure navigation completes.
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  // We start out at the initial navigation.
  ASSERT_EQ(0, web_contents->GetController().GetCurrentEntryIndex());

  GURL test_url("http://www.example.com/simple.html");
  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Set the host as blocked and wait for the interstitial to appear.
  base::Value::Dict dict;
  dict.Set(test_url.host(), false);
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts, std::move(dict));

  supervised_user::SupervisedUserURLFilter* filter =
      GetSupervisedUserService()->GetURLFilter();
  ASSERT_EQ(supervised_user::FilteringBehavior::kBlock,
            filter->GetFilteringBehaviorForURL(test_url));

  content::TestNavigationObserver block_observer(web_contents);
  block_observer.Wait();

  content::LoadStopObserver observer(web_contents);
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
  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Set the host as blocked and wait for the interstitial to appear.
  base::Value::Dict dict;
  dict.Set(test_url.host(), false);
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts, std::move(dict));

  supervised_user::SupervisedUserURLFilter* filter =
      GetSupervisedUserService()->GetURLFilter();
  ASSERT_EQ(supervised_user::FilteringBehavior::kBlock,
            filter->GetFilteringBehaviorForURL(test_url));

  // Verify that there is no crash when closing the blocked tab
  // (https://crbug.com/719708).
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, BlockThenUnblock) {
  GURL test_url("http://www.example.com/simple.html");
  kids_management_api_mock().AllowSubsequentClassifyUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  // Set the host as blocked and wait for the interstitial to appear.
  {
    base::Value::Dict dict;
    dict.Set(test_url.host(), false);
    supervised_user_settings_service->SetLocalSetting(
        supervised_user::kContentPackManualBehaviorHosts, std::move(dict));
  }

  supervised_user::SupervisedUserURLFilter* filter =
      GetSupervisedUserService()->GetURLFilter();
  ASSERT_EQ(supervised_user::FilteringBehavior::kBlock,
            filter->GetFilteringBehaviorForURL(test_url));

  content::TestNavigationObserver block_observer(web_contents);
  block_observer.Wait();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));
  {
    base::Value::Dict dict;
    dict.Set(test_url.host(), true);
    supervised_user_settings_service->SetLocalSetting(
        supervised_user::kContentPackManualBehaviorHosts, std::move(dict));
    ASSERT_EQ(supervised_user::FilteringBehavior::kAllow,
              filter->GetFilteringBehaviorForURL(test_url));
  }

  content::TestNavigationObserver unblock_observer(web_contents);
  unblock_observer.Wait();

  ASSERT_EQ(test_url, web_contents->GetLastCommittedURL());

  EXPECT_FALSE(ShownPageIsInterstitial(browser()));
}

// Tests the filter mode in which all sites are blocked by default.
class SupervisedUserBlockModeTest : public SupervisedUserURLFilterTestBase {
 public:
  void SetUpOnMainThread() override {
    SupervisedUserURLFilterTestBase::SetUpOnMainThread();
    Profile* profile = browser()->profile();
    supervised_user::SupervisedUserSettingsService*
        supervised_user_settings_service =
            SupervisedUserSettingsServiceFactory::GetForKey(
                profile->GetProfileKey());
    supervised_user_settings_service->SetLocalSetting(
        supervised_user::kContentPackDefaultFilteringBehavior,
        base::Value(
            static_cast<int>(supervised_user::FilteringBehavior::kBlock)));
  }
};

IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterTest, RecordBlockedContentUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Open URL in a new tab, which is blocked by ClassifyUrl async checks.
  GURL test_url("http://www.example.com/simple.html");
  kids_management_api_mock().RestrictSubsequentClassifyUrl();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Check that the interstitial is displayed.
  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  // Check that a UKM is recorded for the main frame only.
  const auto ukm_entries =
      ukm_recorder.GetEntriesByName(kBlockedContentUkmName);
  CHECK_EQ(ukm_entries.size(), 1u);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      ukm_entries[0], kBlockedContentUkmMainFrameMetricName, true);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      ukm_entries[0], kBlockedContentUkmIFrameMetricName, 0);
}

// Tests that it's possible to navigate from a blocked page to another blocked
// page.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest,
                       NavigateFromBlockedPageToBlockedPage) {
  GURL test_url("http://www.example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  GURL test_url2("http://www.a.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url2));

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));
  EXPECT_EQ(test_url2, tab->GetVisibleURL());
}

// Tests whether a visit attempt adds a special history entry.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest, HistoryVisitRecorded) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser()->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);

  GURL allowed_url("http://www.example.com/simple.html");
  supervised_user::SupervisedUserURLFilter* filter =
      GetSupervisedUserService()->GetURLFilter();

  // Set the host as allowed.
  base::Value::Dict dict;
  dict.Set(allowed_url.host(), true);
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts, std::move(dict));
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter->GetFilteringBehaviorForURL(allowed_url));
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter->GetFilteringBehaviorForURL(allowed_url.GetWithEmptyPath()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));
  // Navigate to it and check that we don't get an interstitial.
  ASSERT_FALSE(ShownPageIsInterstitial(browser()));

  // Query the history entry.
  {
    history::QueryResults results;
    QueryHistory(history_service, &results);
    ASSERT_EQ(1u, results.size());
    EXPECT_EQ(allowed_url.spec(), results[0].url().spec());
    EXPECT_FALSE(results[0].blocked_visit());
  }

  // Navigate to a blocked page and go back on the interstitial.
  GURL blocked_url("http://www.new-example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), blocked_url));
  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  GoBackAndWaitForNavigation(tab);

  EXPECT_EQ(allowed_url.spec(), tab->GetLastCommittedURL().spec());
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter->GetFilteringBehaviorForURL(allowed_url.GetWithEmptyPath()));
  EXPECT_EQ(supervised_user::FilteringBehavior::kBlock,
            filter->GetFilteringBehaviorForURL(blocked_url.GetWithEmptyPath()));

  // Query the history entry.
  {
    history::QueryResults results;
    QueryHistory(history_service, &results);
    ASSERT_EQ(2u, results.size());
    EXPECT_EQ(allowed_url.spec(), results[0].url().spec());
    EXPECT_FALSE(results[0].blocked_visit());
    EXPECT_EQ(blocked_url.spec(), results[1].url().spec());
    EXPECT_TRUE(results[1].blocked_visit());
  }
}

// Navigates to a blocked URL.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest,
                       SendAccessRequestOnBlockedURL) {
  GURL test_url("http://www.example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

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

IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest, Unblock) {
  GURL test_url("http://www.example.com/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(browser()));

  content::LoadStopObserver observer(web_contents);

  // Set the host as allowed.
  base::Value::Dict dict;
  dict.Set(test_url.host(), true);
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              browser()->profile()->GetProfileKey());
  supervised_user_settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts, std::move(dict));

  supervised_user::SupervisedUserURLFilter* filter =
      GetSupervisedUserService()->GetURLFilter();
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter->GetFilteringBehaviorForURL(test_url.GetWithEmptyPath()));

  observer.Wait();
  EXPECT_EQ(test_url, web_contents->GetLastCommittedURL());
}

class MockSupervisedUserURLFilterObserver
    : public supervised_user::SupervisedUserURLFilter::Observer {
 public:
  explicit MockSupervisedUserURLFilterObserver(
      supervised_user::SupervisedUserURLFilter* filter)
      : filter_(filter) {
    filter_->AddObserver(this);
  }
  ~MockSupervisedUserURLFilterObserver() { filter_->RemoveObserver(this); }

  MockSupervisedUserURLFilterObserver(
      const MockSupervisedUserURLFilterObserver&) = delete;
  MockSupervisedUserURLFilterObserver& operator=(
      const MockSupervisedUserURLFilterObserver&) = delete;

  // SupervisedUserURLFilter::Observer:
  MOCK_METHOD(void,
              OnURLChecked,
              (const GURL& url,
               supervised_user::FilteringBehavior behavior,
               supervised_user::FilteringBehaviorDetails details),
              (override));

 private:
  const raw_ptr<supervised_user::SupervisedUserURLFilter> filter_;
};

class SupervisedUserURLFilterPrerenderingTest
    : public SupervisedUserURLFilterTest {
 public:
  SupervisedUserURLFilterPrerenderingTest()
      : prerender_test_helper_(base::BindRepeating(
            &SupervisedUserURLFilterPrerenderingTest::GetWebContents,
            base::Unretained(this))) {}
  ~SupervisedUserURLFilterPrerenderingTest() override = default;

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
};

// Tests that prerendering doesn't check SupervisedUserURLFilter.
IN_PROC_BROWSER_TEST_F(SupervisedUserURLFilterPrerenderingTest, OnURLChecked) {
  MockSupervisedUserURLFilterObserver observer(
      GetSupervisedUserService()->GetURLFilter());

  GURL test_url("http://www.example.com/simple.html");
  EXPECT_CALL(observer, OnURLChecked).Times(1);
  kids_management_api_mock().AllowSubsequentClassifyUrl();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Load a page in prerendering.
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetWebContents());
  // We do not yet support prerendering for supervised users and prerendering is
  // canceled even though it tries to start prerendering. So, OnURLChecked() is
  // never called in prerendering.
  EXPECT_CALL(observer, OnURLChecked).Times(0);
  GURL prerender_url("http://www.example.com/title1.html");
  // Try prerendering.
  prerender_helper().AddPrerenderAsync(prerender_url);
  // Ensure that prerendering has started.
  registry_observer.WaitForTrigger(prerender_url);
  auto prerender_id = prerender_helper().GetHostForUrl(prerender_url);
  EXPECT_TRUE(prerender_id);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     prerender_id);
  // Prerendering is canceled.
  host_observer.WaitForDestroyed();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Navigate the primary page to the URL.
  EXPECT_CALL(observer, OnURLChecked).Times(1);
  prerender_helper().NavigatePrimaryPage(prerender_url);
}
}  // namespace
