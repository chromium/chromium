// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"

#include "base/time/time.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kTabUrl1[] = "http://example.com";
constexpr char kTabUrl1SubDomain1[] = "http://example.child.com";
constexpr char kTabUrl1SubDomain2[] = "http://example.b.com";
constexpr char kTabUrl1FrontSubDomain1[] = "http://sub.example.com";
constexpr char kTabUrl1WithPath[] = "http://example.child.com/random/path/";
constexpr char kTabUrl1WithSubPage[] = "http://example.com/blah-blah";
constexpr char kTabUrl1WithRandomQuery[] =
    "http://example.child.com/q?randomness";
constexpr char kTabUrl1DomainRedirect[] =
    "http://example.child.com/redirected/url/path.html";
constexpr char kTabUrlRedirectedUrl[] = "http://redirect-url.com/q?randomness";
constexpr char kTabUrl2[] = "http://company.org";
constexpr char kTabUrl2SubDomain1[] = "http://company.a.org";

}  // namespace

class OnTaskLockedSessionWindowTrackerTest : public BrowserWithTestWindowTest {
 public:
  std::unique_ptr<Browser> CreateTestBrowser(bool popup) {
    auto window = std::make_unique<TestBrowserWindow>();
    Browser::Type type = popup ? Browser::TYPE_APP_POPUP : Browser::TYPE_NORMAL;

    std::unique_ptr<Browser> browser =
        CreateBrowser(profile(), type, false, window.get());
    // Self deleting.
    new TestBrowserWindowOwner(std::move(window));
    return browser;
  }

  void CreateWindowTrackerServiceForTesting() {
    LockedSessionWindowTrackerFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          PrefService* pref_service = user_prefs::UserPrefs::Get(context);
          auto url_blocklist_manager =
              std::make_unique<policy::URLBlocklistManager>(
                  pref_service, policy::policy_prefs::kUrlBlocklist,
                  policy::policy_prefs::kUrlAllowlist);
          auto on_task_blocklist = std::make_unique<OnTaskBlocklist>(
              std::move(url_blocklist_manager));
          return std::make_unique<LockedSessionWindowTracker>(
              std::move(on_task_blocklist));
        }));
    task_environment()->RunUntilIdle();
  }

  void TearDown() override {
    auto* const window_tracker =
        LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
    if (window_tracker) {
      window_tracker->InitializeBrowserInfoForTracking(nullptr);
    }
    BrowserWithTestWindowTest::TearDown();
  }
};

TEST_F(OnTaskLockedSessionWindowTrackerTest, RegisterUrlsAndRestrictionLevels) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_b(kTabUrl2);
  const GURL url_a_subdomain(kTabUrl1SubDomain1);
  const GURL url_b_subdomain(kTabUrl2SubDomain1);
  const GURL url_a_subdomain2(kTabUrl1SubDomain2);
  AddTab(browser(), url_a);
  AddTab(browser(), url_b);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();
  blocklist->SetParentURLRestrictionLevel(
      url_a, OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  blocklist->SetParentURLRestrictionLevel(
      url_b, OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  blocklist->SetParentURLRestrictionLevel(
      url_a_subdomain,
      OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  blocklist->SetParentURLRestrictionLevel(
      url_b_subdomain,
      OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  blocklist->SetParentURLRestrictionLevel(
      url_a_subdomain2,
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);
  EXPECT_EQ(blocklist->parent_tab_url_to_nav_filters().size(), 5u);
  EXPECT_EQ(blocklist->parent_tab_url_to_nav_filters()[url_a.GetContent()],
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  EXPECT_EQ(blocklist->parent_tab_url_to_nav_filters()[url_b.GetContent()],
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  EXPECT_EQ(
      blocklist->parent_tab_url_to_nav_filters()[url_a_subdomain.GetContent()],
      OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  EXPECT_EQ(
      blocklist->parent_tab_url_to_nav_filters()[url_b_subdomain.GetContent()],
      OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  EXPECT_EQ(
      blocklist->parent_tab_url_to_nav_filters()[url_a_subdomain2.GetContent()],
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       RegisterChildUrlsWithRestrictions) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_child(kTabUrl1SubDomain1);
  AddTab(browser(), url_a);
  AddTab(browser(), url_a_child);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();

  blocklist->SetParentURLRestrictionLevel(
      url_a, OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  blocklist->SetURLRestrictionLevel(
      url_a_child, OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  EXPECT_EQ(blocklist->parent_tab_url_to_nav_filters().size(), 1u);
  EXPECT_EQ(blocklist->child_tab_url_to_nav_filters().size(), 1u);

  EXPECT_EQ(blocklist->parent_tab_url_to_nav_filters()[url_a.GetContent()],
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  EXPECT_EQ(blocklist->child_tab_url_to_nav_filters()[url_a_child.GetContent()],
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       NavigateCurrentTabWithNewRestrictedLevel) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url(kTabUrl1);
  const GURL url_subdomain(kTabUrl1SubDomain1);
  const GURL url_with_query(kTabUrl1WithRandomQuery);
  const GURL url_with_path(kTabUrl1WithPath);
  AddTab(browser(), url);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();

  blocklist->SetParentURLRestrictionLevel(
      url, OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  ASSERT_EQ(blocklist->parent_tab_url_to_nav_filters().size(), 1u);
  ASSERT_EQ(blocklist->parent_tab_url_to_nav_filters()[url.GetContent()],
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  blocklist->SetURLRestrictionLevel(
      url_subdomain, OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  NavigateAndCommitActiveTab(url_subdomain);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);

  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);

  NavigateAndCommitActiveTab(url_with_query);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  NavigateAndCommitActiveTab(url_with_path);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       NavigateCurrentTabWithNewRestrictedLevelFromParentUrl) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url(kTabUrl1);
  const GURL url_subdomain(kTabUrl1SubDomain1);
  const GURL url_with_query(kTabUrl1WithRandomQuery);
  const GURL url_with_path(kTabUrl1WithPath);
  AddTab(browser(), url);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();

  blocklist->SetParentURLRestrictionLevel(
      url, OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  ASSERT_EQ(blocklist->parent_tab_url_to_nav_filters().size(), 1u);
  ASSERT_EQ(blocklist->parent_tab_url_to_nav_filters()[url.GetContent()],
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  blocklist->SetParentURLRestrictionLevel(
      url_subdomain,
      OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  NavigateAndCommitActiveTab(url_subdomain);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);

  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);

  NavigateAndCommitActiveTab(url_with_query);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  NavigateAndCommitActiveTab(url_with_path);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       NavigateCurrentTabWithNewRestrictedLevelFromRedirectUrl) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url(kTabUrl1);
  const GURL url_subdomain(kTabUrl1SubDomain1);
  AddTab(browser(), url);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();

  blocklist->SetParentURLRestrictionLevel(
      url, OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  ASSERT_EQ(blocklist->parent_tab_url_to_nav_filters().size(), 1u);
  ASSERT_EQ(blocklist->parent_tab_url_to_nav_filters()[url.GetContent()],
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  blocklist->SetParentURLRestrictionLevel(
      url_subdomain, OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  NavigateAndCommitActiveTab(url_subdomain);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  const GURL url_redirect(kTabUrlRedirectedUrl);

  NavigateAndCommitActiveTab(url_redirect);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       NavigateCurrentTabWithOneLevelDeepFromRedirectUrl) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url(kTabUrl1);
  const GURL url_subdomain(kTabUrl1SubDomain1);
  const GURL url_with_query(kTabUrl1WithRandomQuery);
  AddTab(browser(), url);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();

  blocklist->SetParentURLRestrictionLevel(
      url, OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  ASSERT_EQ(blocklist->parent_tab_url_to_nav_filters().size(), 1u);
  ASSERT_EQ(blocklist->parent_tab_url_to_nav_filters()[url.GetContent()],
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  blocklist->SetParentURLRestrictionLevel(
      url_subdomain,
      OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation);
  NavigateAndCommitActiveTab(url_subdomain);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  const GURL url_redirect(kTabUrlRedirectedUrl);

  NavigateAndCommitActiveTab(url_redirect);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       NavigateCurrentTabWithSameDomainAndOneLevelDeepFromRedirectUrl) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url(kTabUrl1);
  const GURL url_subdomain(kTabUrl1SubDomain1);
  const GURL url_with_query(kTabUrl1WithRandomQuery);
  AddTab(browser(), url);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();

  blocklist->SetParentURLRestrictionLevel(
      url, OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  ASSERT_EQ(blocklist->parent_tab_url_to_nav_filters().size(), 1u);
  ASSERT_EQ(blocklist->parent_tab_url_to_nav_filters()[url.GetContent()],
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  blocklist->SetParentURLRestrictionLevel(
      url_subdomain,
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);
  NavigateAndCommitActiveTab(url_subdomain);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  const GURL url_redirect(kTabUrl1DomainRedirect);

  NavigateAndCommitActiveTab(url_redirect);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(
      blocklist->current_page_restriction_level(),
      OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation);

  const GURL url_redirect_not_same_domain(kTabUrlRedirectedUrl);

  NavigateAndCommitActiveTab(url_redirect_not_same_domain);
  browser()->tab_strip_model()->UpdateWebContentsStateAt(0,
                                                         TabChangeType::kAll);
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest, SwitchTabWithNewRestrictedLevel) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_b(kTabUrl2);
  // Add Tab inserts tab at the 0th index.
  AddTab(browser(), url_a);
  AddTab(browser(), url_b);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();

  blocklist->SetParentURLRestrictionLevel(
      url_a, OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  blocklist->SetParentURLRestrictionLevel(
      url_b, OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  window_tracker->RefreshUrlBlocklist();
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       BlockUrlSuccessfullyForLimitedNav) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_b(kTabUrl2);
  AddTab(browser(), url_a);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();

  blocklist->SetParentURLRestrictionLevel(
      url_a, OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
  EXPECT_EQ(blocklist->GetURLBlocklistState(url_b),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       AllowAndBlockUrlSuccessfullyForSameDomainNav) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_front_subdomain(kTabUrl1FrontSubDomain1);
  const GURL url_a_subpage(kTabUrl1WithSubPage);
  const GURL url_a_subdomain_page(kTabUrl1WithPath);
  const GURL url_a_subdomain(kTabUrl1SubDomain1);
  const GURL url_b(kTabUrl2);

  AddTab(browser(), url_a);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();

  blocklist->SetParentURLRestrictionLevel(
      url_a, OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
  EXPECT_EQ(blocklist->GetURLBlocklistState(url_a_front_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(blocklist->GetURLBlocklistState(url_a_subpage),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(blocklist->GetURLBlocklistState(url_a_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  EXPECT_EQ(blocklist->GetURLBlocklistState(url_a_subdomain_page),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
  EXPECT_EQ(blocklist->GetURLBlocklistState(url_b),
            policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest,
       AllowUrlSuccessfullyForUnrestrictedNav) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_front_subdomain(kTabUrl1FrontSubDomain1);
  const GURL url_a_path(kTabUrl1WithPath);
  const GURL url_a_subdomain(kTabUrl1SubDomain1);
  const GURL url_b(kTabUrl2);

  AddTab(browser(), url_a);
  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  auto* const blocklist = window_tracker->on_task_blocklist();

  blocklist->SetParentURLRestrictionLevel(
      url_a, OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  window_tracker->RefreshUrlBlocklist();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(blocklist->current_page_restriction_level(),
            OnTaskBlocklist::RestrictionLevel::kNoRestrictions);
  EXPECT_EQ(blocklist->GetURLBlocklistState(url_a_front_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(blocklist->GetURLBlocklistState(url_a_path),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(blocklist->GetURLBlocklistState(url_a_subdomain),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
  EXPECT_EQ(blocklist->GetURLBlocklistState(url_b),
            policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST);
}

TEST_F(OnTaskLockedSessionWindowTrackerTest, NewBrowserWindowsDontOpen) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  window_tracker->InitializeBrowserInfoForTracking(browser());
  std::unique_ptr<Browser> normal_browser(CreateTestBrowser(/*popup=*/false));
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(
      static_cast<TestBrowserWindow*>(normal_browser->window())->IsClosed());
}

TEST_F(OnTaskLockedSessionWindowTrackerTest, NewBrowserPopupIsRegistered) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  window_tracker->InitializeBrowserInfoForTracking(browser());
  std::unique_ptr<Browser> popup_browser(CreateTestBrowser(/*popup=*/true));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_FALSE(
      static_cast<TestBrowserWindow*>(popup_browser->window())->IsClosed());
  EXPECT_TRUE(window_tracker->IsFirstTimePopup());
}

TEST_F(OnTaskLockedSessionWindowTrackerTest, BrowserClose) {
  CreateWindowTrackerServiceForTesting();
  auto* const window_tracker =
      LockedSessionWindowTrackerFactory::GetForBrowserContext(profile());
  const GURL url_a(kTabUrl1);
  const GURL url_a_child(kTabUrl1SubDomain1);
  AddTab(browser(), url_a);
  AddTab(browser(), url_a_child);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  window_tracker->InitializeBrowserInfoForTracking(browser());
  ASSERT_EQ(window_tracker->browser(), browser());
  browser()->OnWindowClosing();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(window_tracker->browser());
}
