// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_search_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace actor {

namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

// Matches a PageMatch by its URL. (TabMatch has its own matcher, MatchTab.)
auto MatchUrl(std::string_view url) {
  return Field("url", &PageMatch::url, GURL(url));
}

#if !BUILDFLAG(IS_ANDROID)
auto MatchTab(tabs::TabInterface* tab) {
  return Field("handle", &TabMatch::handle, tab->GetHandle());
}

class TestBrowserInstance {
 public:
  explicit TestBrowserInstance(Profile* profile)
      : profile_(profile),
        tab_strip_model_(&tab_strip_model_delegate_, profile) {
    tab_strip_model_delegate_.SetBrowserWindowInterface(&mock_bwi_);

    ON_CALL(mock_bwi_, GetProfile()).WillByDefault(testing::Return(profile_));
    ON_CALL(mock_bwi_, GetType())
        .WillByDefault(testing::Return(BrowserWindowInterface::TYPE_NORMAL));
    ON_CALL(mock_bwi_, IsDeleteScheduled())
        .WillByDefault(testing::Return(false));
    ON_CALL(mock_bwi_, GetTabStripModel())
        .WillByDefault(testing::Return(&tab_strip_model_));
  }

  ~TestBrowserInstance() {
    tab_strip_model_.CloseAllTabs();
    tab_strip_model_delegate_.SetBrowserWindowInterface(nullptr);
  }

  BrowserWindowInterface* browser() { return &mock_bwi_; }

  tabs::TabInterface* AddTab(const GURL& url, const std::u16string& title) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(
            profile_, content::SiteInstance::Create(profile_));
    // Set the committed URL directly instead of running a real navigation.
    // NavigateAndCommit() on a chrome:// URL instantiates the actual WebUI
    // controller (chrome://settings builds SettingsUI, which requires a
    // TemplateURLService that TestingProfile does not register), and these
    // tests only read the committed URL and the title.
    content::WebContentsTester* tester =
        content::WebContentsTester::For(contents.get());
    tester->SetLastCommittedURL(url);
    tester->SetTitle(title);
    tab_strip_model_.AppendWebContents(std::move(contents),
                                       /*foreground=*/true);
    return tab_strip_model_.GetTabAtIndex(tab_strip_model_.count() - 1);
  }

  // Closes every tab while keeping the window itself alive, so that handles
  // taken before the call become stale.
  void CloseAllTabs() { tab_strip_model_.CloseAllTabs(); }

 private:
  raw_ptr<Profile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> mock_bwi_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  TabStripModel tab_strip_model_;
};
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

TEST(PageSearchUtilsUrlTest, DefaultFilterAllowsWebAndNewTabPage) {
  EXPECT_TRUE(IsAllowedMatchUrl(GURL("http://example.com")));
  EXPECT_TRUE(IsAllowedMatchUrl(GURL("https://chromium.org/home")));

  // All three NTP host aliases are reachable depending on which
  // implementation is active.
  EXPECT_TRUE(IsAllowedMatchUrl(GURL("chrome://newtab")));
  EXPECT_TRUE(IsAllowedMatchUrl(GURL("chrome://newtab/")));
  EXPECT_TRUE(IsAllowedMatchUrl(GURL("chrome://new-tab-page")));
  EXPECT_TRUE(IsAllowedMatchUrl(GURL("chrome://new-tab-page-third-party")));
}

TEST(PageSearchUtilsUrlTest, DefaultFilterRejectsEverythingElse) {
  EXPECT_FALSE(IsAllowedMatchUrl(GURL()));
  EXPECT_FALSE(IsAllowedMatchUrl(GURL("javascript:alert(1)")));
  EXPECT_FALSE(IsAllowedMatchUrl(GURL("file:///tmp/index.html")));
  EXPECT_FALSE(IsAllowedMatchUrl(GURL("about:blank")));
  EXPECT_FALSE(IsAllowedMatchUrl(GURL("chrome://settings")));
  EXPECT_FALSE(IsAllowedMatchUrl(GURL("chrome://flags")));
  EXPECT_FALSE(IsAllowedMatchUrl(GURL("devtools://devtools/bundled/x.html")));
}

// The filter must actually be consulted, not merely accepted.
TEST(PageSearchUtilsUrlTest, FilterCategoriesAreIndependent) {
  const UrlMatchFilter web_only({UrlMatchCategory::kHttpOrHttps});
  EXPECT_TRUE(IsAllowedMatchUrl(GURL("https://example.com"), web_only));
  EXPECT_FALSE(IsAllowedMatchUrl(GURL("chrome://newtab"), web_only));

  const UrlMatchFilter ntp_only({UrlMatchCategory::kNewTabPage});
  EXPECT_FALSE(IsAllowedMatchUrl(GURL("https://example.com"), ntp_only));
  EXPECT_TRUE(IsAllowedMatchUrl(GURL("chrome://newtab"), ntp_only));

  const UrlMatchFilter none;
  EXPECT_FALSE(IsAllowedMatchUrl(GURL("https://example.com"), none));
  EXPECT_FALSE(IsAllowedMatchUrl(GURL("chrome://newtab"), none));
}

class PageSearchUtilsTest : public testing::Test {
 public:
  PageSearchUtilsTest() = default;
  ~PageSearchUtilsTest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(BookmarkModelFactory::GetInstance(),
                              BookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                              HistoryServiceFactory::GetDefaultFactory());
    profile_ = builder.Build();

    bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(profile());
    ASSERT_TRUE(bookmark_model_);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);

    history_service_ = HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::EXPLICIT_ACCESS);
    ASSERT_TRUE(history_service_);
  }

  TestingProfile* profile() { return profile_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
#if !BUILDFLAG(IS_ANDROID)
  // TabModel eagerly initializes real TabFeatures for every tab it creates.
  // One of them, PinnedTranslateActionListener, fires on tab activation and
  // calls BrowserActions::From(browser)->root_action_item(). The mock window
  // has no BrowserActions attached, so From() returns null and the listener
  // crashes. None of these features are under test here, so skip them.
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_tab_features_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
#endif  // !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_ = nullptr;
  raw_ptr<history::HistoryService> history_service_ = nullptr;
  base::CancelableTaskTracker task_tracker_;
};

TEST_F(PageSearchUtilsTest, FindMatchingBookmarks_EmptyAndNull) {
  EXPECT_THAT(FindMatchingBookmarks(nullptr, "docs"), IsEmpty());
  EXPECT_THAT(FindMatchingBookmarks(profile(), ""), IsEmpty());
}

TEST_F(PageSearchUtilsTest, FindMatchingBookmarks_MatchesTitleAndUrl) {
  const bookmarks::BookmarkNode* other_node = bookmark_model_->other_node();
  bookmark_model_->AddURL(other_node, 0, u"Google Docs",
                          GURL("https://docs.google.com/document"));
  bookmark_model_->AddURL(other_node, 1, u"Google Sheets",
                          GURL("https://docs.google.com/spreadsheets"));
  bookmark_model_->AddURL(other_node, 2, u"Buganizer",
                          GURL("https://b.corp.google.com"));

  // Match multiple bookmarks by shared title/URL keyword.
  EXPECT_THAT(
      FindMatchingBookmarks(profile(), "docs.google.com"),
      UnorderedElementsAre(MatchUrl("https://docs.google.com/document"),
                           MatchUrl("https://docs.google.com/spreadsheets")));

  // Match single bookmark by URL.
  EXPECT_THAT(FindMatchingBookmarks(profile(), "corp.google"),
              ElementsAre(MatchUrl("https://b.corp.google.com")));

  // No match.
  EXPECT_THAT(FindMatchingBookmarks(profile(), "wikipedia"), IsEmpty());
}

TEST_F(PageSearchUtilsTest, FindMatchingBookmarks_CopiesTitle) {
  bookmark_model_->AddURL(bookmark_model_->other_node(), 0, u"Google Docs",
                          GURL("https://docs.google.com/document"));

  std::vector<PageMatch> matches =
      FindMatchingBookmarks(profile(), "Google Docs");
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches[0].title, u"Google Docs");

  // Results are owned copies, so removing the bookmark must not invalidate
  // them. This is the ownership concern raised in review.
  bookmark_model_->RemoveAllUserBookmarks(FROM_HERE);
  EXPECT_EQ(matches[0].title, u"Google Docs");
  EXPECT_EQ(matches[0].url, GURL("https://docs.google.com/document"));
}

TEST_F(PageSearchUtilsTest, FindMatchingBookmarks_SkipsDisallowedUrls) {
  const bookmarks::BookmarkNode* other_node = bookmark_model_->other_node();
  bookmark_model_->AddURL(other_node, 0, u"JavaScript Guide",
                          GURL("javascript:alert(1)"));
  bookmark_model_->AddURL(other_node, 1, u"JavaScript MDN",
                          GURL("https://developer.mozilla.org/JavaScript"));

  EXPECT_THAT(
      FindMatchingBookmarks(profile(), "JavaScript"),
      ElementsAre(MatchUrl("https://developer.mozilla.org/JavaScript")));
}

TEST_F(PageSearchUtilsTest, FindMatchingBookmarks_HonorsFilter) {
  bookmark_model_->AddURL(bookmark_model_->other_node(), 0, u"New Tab",
                          GURL("chrome://newtab"));

  EXPECT_THAT(FindMatchingBookmarks(profile(), "New Tab"),
              ElementsAre(MatchUrl("chrome://newtab")));
  EXPECT_THAT(
      FindMatchingBookmarks(profile(), "New Tab",
                            UrlMatchFilter({UrlMatchCategory::kHttpOrHttps})),
      IsEmpty());
}

TEST_F(PageSearchUtilsTest, FindMatchingHistory_EmptyAndNull) {
  base::test::TestFuture<std::vector<PageMatch>> future1;
  FindMatchingHistory(nullptr, "test", &task_tracker_, future1.GetCallback());
  EXPECT_THAT(future1.Get(), IsEmpty());

  base::test::TestFuture<std::vector<PageMatch>> future2;
  FindMatchingHistory(profile(), "", &task_tracker_, future2.GetCallback());
  EXPECT_THAT(future2.Get(), IsEmpty());

  base::test::TestFuture<std::vector<PageMatch>> future3;
  FindMatchingHistory(profile(), "test", nullptr, future3.GetCallback());
  EXPECT_THAT(future3.Get(), IsEmpty());
}

TEST_F(PageSearchUtilsTest, FindMatchingHistory_MatchesVisits) {
  history::HistoryAddPageArgs page1;
  page1.url = GURL("https://news.ycombinator.com/item?id=1");
  page1.title = u"Hacker News Frontpage";
  page1.time = base::Time::Now() - base::Minutes(5);
  history_service_->AddPage(page1);

  history::HistoryAddPageArgs page2;
  page2.url = GURL("https://news.ycombinator.com/newest");
  page2.title = u"Hacker News Newest";
  page2.time = base::Time::Now();
  history_service_->AddPage(page2);

  // Disallowed scheme should be filtered out even when title matches.
  history::HistoryAddPageArgs disallowed_page;
  disallowed_page.url = GURL("chrome://settings");
  disallowed_page.title = u"Hacker Settings";
  disallowed_page.time = base::Time::Now();
  history_service_->AddPage(disallowed_page);

  history::BlockUntilHistoryProcessesPendingRequests(history_service_);

  base::test::TestFuture<std::vector<PageMatch>> future;
  FindMatchingHistory(profile(), "Hacker", &task_tracker_,
                      future.GetCallback());
  EXPECT_THAT(future.Get(),
              ElementsAre(MatchUrl("https://news.ycombinator.com/newest"),
                          MatchUrl("https://news.ycombinator.com/item?id=1")));

  base::test::TestFuture<std::vector<PageMatch>> no_match_future;
  FindMatchingHistory(profile(), "reddit", &task_tracker_,
                      no_match_future.GetCallback());
  EXPECT_THAT(no_match_future.Get(), IsEmpty());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PageSearchUtilsTest, FindMatchingTabs_EmptyAndNull) {
  TestBrowserInstance browser_instance(profile());
  EXPECT_THAT(FindMatchingTabs(nullptr, "test"), IsEmpty());
  EXPECT_THAT(FindMatchingTabs(browser_instance.browser(), ""), IsEmpty());
}

TEST_F(PageSearchUtilsTest, FindMatchingTabs_MatchesTitleAndUrlInSingleWindow) {
  TestBrowserInstance browser_instance(profile());
  tabs::TabInterface* chromium_tab1 = browser_instance.AddTab(
      GURL("https://chromium.org/home"), u"Chromium Projects");
  tabs::TabInterface* chromium_tab2 = browser_instance.AddTab(
      GURL("https://chromium.org/developers"), u"Chromium Developers");
  tabs::TabInterface* example_tab = browser_instance.AddTab(
      GURL("https://example.com/search"), u"Example Search Page");
  // Disallowed URL scheme should be skipped even when title matches.
  browser_instance.AddTab(GURL("chrome://settings"), u"Chromium Settings");
  browser_instance.AddTab(GURL("file:///tmp/chromium.html"), u"Chromium File");

  // Second browser window whose tabs must NOT be returned when searching
  // `browser_instance`. Tab search is deliberately scoped to one window.
  TestBrowserInstance other_browser_instance(profile());
  other_browser_instance.AddTab(GURL("https://chromium.org/other"),
                                u"Other Window Chromium");

  // Match multiple tabs in tab-strip order.
  EXPECT_THAT(FindMatchingTabs(browser_instance.browser(), "chromium"),
              ElementsAre(MatchTab(chromium_tab1), MatchTab(chromium_tab2)));

  // Case-insensitive title match.
  EXPECT_THAT(FindMatchingTabs(browser_instance.browser(), "PROJECTS"),
              ElementsAre(MatchTab(chromium_tab1)));

  // Match by URL substring.
  EXPECT_THAT(FindMatchingTabs(browser_instance.browser(), "example.com"),
              ElementsAre(MatchTab(example_tab)));

  // No match.
  EXPECT_THAT(FindMatchingTabs(browser_instance.browser(), "nonexistent"),
              IsEmpty());
}

TEST_F(PageSearchUtilsTest, FindMatchingTabs_HonorsFilter) {
  TestBrowserInstance browser_instance(profile());
  browser_instance.AddTab(GURL("chrome://newtab"), u"New Tab");

  EXPECT_EQ(FindMatchingTabs(browser_instance.browser(), "New Tab").size(), 1u);
  EXPECT_THAT(
      FindMatchingTabs(browser_instance.browser(), "New Tab",
                       UrlMatchFilter({UrlMatchCategory::kHttpOrHttps})),
      IsEmpty());
}

// Page titles are routinely non-ASCII. ASCII-only lowercasing silently fails
// to match them, so this guards the ICU-backed matcher.
TEST_F(PageSearchUtilsTest, FindMatchingTabs_MatchesNonAsciiTitles) {
  TestBrowserInstance browser_instance(profile());
  tabs::TabInterface* umlaut_tab =
      browser_instance.AddTab(GURL("https://example.com/de"), u"ÄPFEL Kaufen");
  tabs::TabInterface* accent_tab =
      browser_instance.AddTab(GURL("https://example.com/fr"), u"Café Crème");

  // Case-insensitive across a non-ASCII case pair (Ä/ä).
  EXPECT_THAT(FindMatchingTabs(browser_instance.browser(), "äpfel"),
              ElementsAre(MatchTab(umlaut_tab)));

  // Accent-insensitive: an unaccented query matches an accented title.
  EXPECT_THAT(FindMatchingTabs(browser_instance.browser(), "cafe"),
              ElementsAre(MatchTab(accent_tab)));
}

// Tab search must never cross window boundaries, even when another window
// holds a better match. Regression guard for the single-window guarantee.
TEST_F(PageSearchUtilsTest, FindMatchingTabs_NeverReturnsTabsFromOtherWindows) {
  TestBrowserInstance window_a(profile());
  TestBrowserInstance window_b(profile());

  // An exact-title match lives in window B only.
  window_b.AddTab(GURL("https://example.com/exact"), u"Exact Match");
  // Window A has nothing matching at all.
  window_a.AddTab(GURL("https://unrelated.test/"), u"Unrelated");

  EXPECT_THAT(FindMatchingTabs(window_a.browser(), "Exact Match"), IsEmpty());
  EXPECT_EQ(FindMatchingTabs(window_b.browser(), "Exact Match").size(), 1u);

  // And the reverse direction.
  EXPECT_THAT(FindMatchingTabs(window_b.browser(), "Unrelated"), IsEmpty());
}

TEST_F(PageSearchUtilsTest, ResolveTabInWindow_ResolvesTabInSameWindow) {
  TestBrowserInstance window_a(profile());
  tabs::TabInterface* tab =
      window_a.AddTab(GURL("https://example.com/"), u"Example");

  std::vector<TabMatch> matches =
      FindMatchingTabs(window_a.browser(), "example");
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(ResolveTabInWindow(matches[0], window_a.browser()), tab);
}

// A tab can be dragged into another window between the search and the action
// that uses the result. Resolving against the original window must fail rather
// than silently act on a tab that now lives somewhere else.
TEST_F(PageSearchUtilsTest, ResolveTabInWindow_RejectsTabInDifferentWindow) {
  TestBrowserInstance window_a(profile());
  TestBrowserInstance window_b(profile());
  window_a.AddTab(GURL("https://example.com/"), u"Example");

  std::vector<TabMatch> matches =
      FindMatchingTabs(window_a.browser(), "example");
  ASSERT_EQ(matches.size(), 1u);

  // The handle is live, but it does not belong to window B.
  EXPECT_EQ(ResolveTabInWindow(matches[0], window_b.browser()), nullptr);
  // Sanity: it does still resolve against its own window.
  EXPECT_NE(ResolveTabInWindow(matches[0], window_a.browser()), nullptr);
}

TEST_F(PageSearchUtilsTest, ResolveTabInWindow_RejectsClosedTab) {
  TestBrowserInstance window_a(profile());
  window_a.AddTab(GURL("https://example.com/"), u"Example");

  std::vector<TabMatch> matches =
      FindMatchingTabs(window_a.browser(), "example");
  ASSERT_EQ(matches.size(), 1u);

  window_a.CloseAllTabs();
  EXPECT_EQ(ResolveTabInWindow(matches[0], window_a.browser()), nullptr);
}

TEST_F(PageSearchUtilsTest, ResolveTabInWindow_RejectsNullBrowser) {
  TestBrowserInstance window_a(profile());
  window_a.AddTab(GURL("https://example.com/"), u"Example");

  std::vector<TabMatch> matches =
      FindMatchingTabs(window_a.browser(), "example");
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(ResolveTabInWindow(matches[0], nullptr), nullptr);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace actor
