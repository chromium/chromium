// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/downgrade/downgrade_manager.h"
#include "chrome/browser/downgrade/user_data_downgrade.h"
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/threading/thread_restrictions.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#endif

namespace downgrade {

namespace {

// A gMock matcher that is satisfied when its argument is a command line
// containing a given switch.
MATCHER_P(HasSwitch, switch_name, "") {
  return arg.HasSwitch(switch_name);
}

int GetPrePrefixCount(std::string_view test_name) {
  static constexpr std::string_view kPrePrefix("PRE_");
  int pre_count = 0;
  while (
      base::StartsWith(test_name, kPrePrefix, base::CompareCase::SENSITIVE)) {
    ++pre_count;
    test_name = test_name.substr(kPrePrefix.size());
  }
  return pre_count;
}

}  // namespace

// A base test fixture for User data snapshot tests. This test that the browser
// behaves the same way after restoring a snapshot. First some user actions are
// simulated using the pure virtual function SimulateUserActions(). Second, an
// update is simulated to trigger a snapshot. Third a downgrade is simulated in
// order to trigger the downgrade processing and snapshot restoration. Last, the
// browser is started to verify that the browser behave the same as before the
// downgrade. At each step the pure virtual function ValidateUserActions() is
// called to verify that the browser behaves the same at each version. To test
// that a feature is supported by snapshots, create a test fixture that inherits
// UserDataSnapshotBrowserTestBase and overload SimulateUserActions() and
// ValidateUserActions() to validate the browser's behavior after a snapshot.
class UserDataSnapshotBrowserTestBase : public InProcessBrowserTest {
 protected:
  static bool IsInitialVersion() {
    return GetPrePrefixCount(::testing::UnitTest::GetInstance()
                                 ->current_test_info()
                                 ->name()) == 3;
  }

  static bool IsUpdatedVersion() {
    return GetPrePrefixCount(::testing::UnitTest::GetInstance()
                                 ->current_test_info()
                                 ->name()) == 2;
  }

  static bool IsRolledbackVersion() {
    return GetPrePrefixCount(::testing::UnitTest::GetInstance()
                                 ->current_test_info()
                                 ->name()) == 1;
  }

  // Returns the next Chrome milestone version.
  static std::string GetNextChromeVersion() {
    return base::NumberToString(version_info::GetVersion().components()[0] + 1);
  }
  static std::string GetPreviousChromeVersion() {
    return base::NumberToString(version_info::GetVersion().components()[0] - 1);
  }

  UserDataSnapshotBrowserTestBase() {
    DowngradeManager::EnableSnapshotsForTesting(true);
  }

  ~UserDataSnapshotBrowserTestBase() override {
    DowngradeManager::EnableSnapshotsForTesting(false);
  }

  // Returns the path to the User Data dir.
  const base::FilePath& user_data_dir() const { return user_data_dir_; }

  bool SetUpUserDataDirectory() override {
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_))
      return false;
    if (IsUpdatedVersion()) {
      // Pretend that a lower version of Chrome previously wrote User Data.
      const std::string last_version = GetPreviousChromeVersion();
      base::WriteFile(user_data_dir_.Append(kDowngradeLastVersionFile),
                      last_version);
    }

    if (IsRolledbackVersion()) {
      // Pretend that a higher version of Chrome previously wrote User Data.
      const std::string last_version = GetNextChromeVersion();
      base::WriteFile(user_data_dir_.Append(kDowngradeLastVersionFile),
                      last_version);
    }
    return true;
  }

  void SetUpInProcessBrowserTestFixture() override {
    if (IsRolledbackVersion()) {
      // Expect a browser relaunch late in browser shutdown.
      mock_relaunch_callback_ = std::make_unique<::testing::StrictMock<
          base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>>>();
      EXPECT_CALL(*mock_relaunch_callback_,
                  Run(HasSwitch(switches::kUserDataMigrated)));
      relaunch_chrome_override_ =
          std::make_unique<upgrade_util::ScopedRelaunchChromeBrowserOverride>(
              mock_relaunch_callback_->Get());

      // Expect that browser startup short-circuits into a relaunch.
      set_expected_exit_code(chrome::RESULT_CODE_DOWNGRADE_AND_RELAUNCH);
    }
  }

  // Validates the state of chrome after a few user actions.
  void SetUpOnMainThread() override {
    if (IsInitialVersion())
      SimulateUserActions();
    ValidateUserActions();
  }

  // Simulates some user actions related to feature to test.
  virtual void SimulateUserActions() = 0;

  // Validates that the state of the browser is the same one after the user
  // actions, update and rollback.
  virtual void ValidateUserActions() = 0;

 private:
  // The location of User Data.
  base::FilePath user_data_dir_;

  std::unique_ptr<
      base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>>
      mock_relaunch_callback_;
  std::unique_ptr<upgrade_util::ScopedRelaunchChromeBrowserOverride>
      relaunch_chrome_override_;
};

class BookmarksSnapshotTest : public UserDataSnapshotBrowserTestBase {
 protected:
  // Bookmarks as such
  // bookmark_bar_node
  // |_ folder
  // |  |_ subfolder
  // |  |  |_www.subfolder.com
  // |  |_ www.folder.com
  // |_ www.thaturl.com
  // other_node
  // |_ www.other.com
  // |_ www.thatotherurl.com
  // mobile_node
  // |_ www.mobile.com
  void SimulateUserActions() override {
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    auto* folder = bookmark_model->AddFolder(
        bookmark_model->bookmark_bar_node(), 0, folder_title_);
    auto* sub_folder = bookmark_model->AddFolder(folder, 0, sub_folder_title_);
    bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 1,
                           that_url_title_, that_url_);
    bookmark_model->AddURL(sub_folder, 0, sub_folder_url_title_,
                           sub_folder_url_);
    bookmark_model->AddURL(folder, 1, folder_url_title_, folder_url_);
    bookmark_model->AddURL(bookmark_model->other_node(), 0, other_url_title_,
                           other_url_);
    bookmark_model->AddURL(bookmark_model->other_node(), 1,
                           that_other_url_title_, that_other_url_);
    bookmark_model->AddURL(bookmark_model->mobile_node(), 0, mobile_url_title_,
                           mobile_url_);
    // This is necessary to ensure the save completes.
    content::RunAllTasksUntilIdle();
  }

  void ValidateUserActions() override {
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    ASSERT_TRUE(bookmark_model->bookmark_bar_node()->children().size() == 2);

    const bookmarks::BookmarkNode* folder =
        bookmark_model->bookmark_bar_node()->children()[0].get();
    EXPECT_EQ(folder->GetTitledUrlNodeTitle(), folder_title_);
    ASSERT_TRUE(folder->children().size() == 2);

    const bookmarks::BookmarkNode* sub_folder = folder->children()[0].get();
    EXPECT_EQ(sub_folder->GetTitledUrlNodeTitle(), sub_folder_title_);
    ASSERT_TRUE(sub_folder->children().size() == 1);

    const bookmarks::BookmarkNode* sub_folder_url =
        sub_folder->children()[0].get();
    EXPECT_EQ(sub_folder_url->GetTitledUrlNodeTitle(), sub_folder_url_title_);
    EXPECT_EQ(sub_folder_url->url(), sub_folder_url_);

    ASSERT_TRUE(bookmark_model->other_node()->children().size() == 2);
    const bookmarks::BookmarkNode* other_url =
        bookmark_model->other_node()->children()[0].get();
    EXPECT_EQ(other_url->GetTitledUrlNodeTitle(), other_url_title_);
    EXPECT_EQ(other_url->url(), other_url_);

    const bookmarks::BookmarkNode* that_other_url =
        bookmark_model->other_node()->children()[1].get();
    EXPECT_EQ(that_other_url->GetTitledUrlNodeTitle(), that_other_url_title_);
    EXPECT_EQ(that_other_url->url(), that_other_url_);

    ASSERT_TRUE(bookmark_model->mobile_node()->children().size() == 1);
    const bookmarks::BookmarkNode* mobile_url =
        bookmark_model->mobile_node()->children()[0].get();
    EXPECT_EQ(mobile_url->GetTitledUrlNodeTitle(), mobile_url_title_);
    EXPECT_EQ(mobile_url->url(), mobile_url_);
  }

 private:
  const GURL that_url_{"https://www.thaturl.com"};
  const GURL sub_folder_url_{"https://www.subfolder.com"};
  const GURL folder_url_{"https://www.folder.com"};
  const GURL other_url_{"https://www.other.com"};
  const GURL that_other_url_{"https://www.thatotherul.com"};
  const GURL mobile_url_{"https://www.mobile.com"};

  const std::u16string folder_title_ = u"Folder";
  const std::u16string sub_folder_title_ = u"Sub Folder";
  const std::u16string sub_folder_url_title_ = u"Sub Folder URL";
  const std::u16string that_url_title_ = u"That URL";
  const std::u16string folder_url_title_ = u"Folder URL";
  const std::u16string other_url_title_ = u"Other URL";
  const std::u16string that_other_url_title_ = u"That Other URL";
  const std::u16string mobile_url_title_ = u"Mobile URL";
};

IN_PROC_BROWSER_TEST_F(BookmarksSnapshotTest, PRE_PRE_PRE_Test) {}
IN_PROC_BROWSER_TEST_F(BookmarksSnapshotTest, PRE_PRE_Test) {}
IN_PROC_BROWSER_TEST_F(BookmarksSnapshotTest, PRE_Test) {}
// TODO(crbug.com/326168468): Flaky on TSan.
#if defined(THREAD_SANITIZER)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(BookmarksSnapshotTest, MAYBE_Test) {}
#undef MAYBE_Test

class HistorySnapshotTest : public UserDataSnapshotBrowserTestBase {
  struct HistoryEntry {
    HistoryEntry(std::string_view url,
                 std::string_view title,
                 base::Time time,
                 history::VisitSource source)
        : url(url),
          title(base::ASCIIToUTF16(title)),
          time(time),
          source(source) {}
    ~HistoryEntry() = default;
    GURL url;
    std::u16string title;
    base::Time time;
    history::VisitSource source;
  };

 protected:
  void SimulateUserActions() override {
    auto* history_service = HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    for (const auto& entry : history_entries_) {
      history_service->AddPage(entry.url, entry.time, entry.source);
      history_service->SetPageTitle(entry.url, entry.title);
    }
  }

  void ValidateUserActions() override {
    auto* history_service = HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);

    history::QueryResults history_query_results;
    base::RunLoop run_loop;
    base::CancelableTaskTracker tracker;
    history_service->QueryHistory(
        std::u16string(), history::QueryOptions(),
        base::BindOnce(&HistorySnapshotTest::SetQueryResults,
                       base::Unretained(this), run_loop.QuitClosure()),
        &tracker);
    run_loop.Run();
    EXPECT_EQ(history_query_results_.size(), history_entries_.size());
    std::map<GURL, const HistoryEntry*> expected_results_map;
    for (const auto& entry : history_entries_)
      expected_results_map[entry.url] = &entry;
    for (const auto& result : history_query_results_) {
      auto it = expected_results_map.find(result.url());
      if (it == expected_results_map.end()) {
        ADD_FAILURE() << "found an unexpected result for url " << result.url();
        continue;
      }
      const auto* expected = it->second;
      EXPECT_EQ(expected->time, result.visit_time());
      EXPECT_EQ(expected->title, result.title());
    }
  }

 private:
  void SetQueryResults(base::RepeatingClosure continuation,
                       history::QueryResults results) {
    history_query_results_ = std::move(results);
    continuation.Run();
  }

  history::QueryResults history_query_results_;
  const std::vector<HistoryEntry> history_entries_{
      HistoryEntry("https://www.website.com",
                   "website",
                   base::Time::FromSecondsSinceUnixEpoch(1000),
                   history::VisitSource::SOURCE_BROWSED),
      HistoryEntry("https://www.website1.com",
                   "website1",
                   base::Time::FromSecondsSinceUnixEpoch(10001),
                   history::VisitSource::SOURCE_EXTENSION),
      HistoryEntry("https://www.website2.com",
                   "website2",
                   base::Time::FromSecondsSinceUnixEpoch(10002),
                   history::VisitSource::SOURCE_FIREFOX_IMPORTED),
      HistoryEntry("https://www.website3.com",
                   "website3",
                   base::Time::FromSecondsSinceUnixEpoch(10003),
                   history::VisitSource::SOURCE_IE_IMPORTED),
      HistoryEntry("https://www.website4.com",
                   "website4",
                   base::Time::FromSecondsSinceUnixEpoch(10004),
                   history::VisitSource::SOURCE_SAFARI_IMPORTED),
      HistoryEntry("https://www.website5.com",
                   "website5",
                   base::Time::FromSecondsSinceUnixEpoch(10005),
                   history::VisitSource::SOURCE_SYNCED),
  };
};

IN_PROC_BROWSER_TEST_F(HistorySnapshotTest, PRE_PRE_PRE_Test) {}
IN_PROC_BROWSER_TEST_F(HistorySnapshotTest, PRE_PRE_Test) {}
IN_PROC_BROWSER_TEST_F(HistorySnapshotTest, PRE_Test) {}
// TODO(crbug.com/326168468): Flaky on TSan.
#if defined(THREAD_SANITIZER)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(HistorySnapshotTest, MAYBE_Test) {}
#undef MAYBE_Test

class TabsSnapshotTest : public UserDataSnapshotBrowserTestBase {
 protected:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    UserDataSnapshotBrowserTestBase::SetUp();
  }

  void SimulateUserActions() override {
    browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);
    browser()->OpenURL(
        content::OpenURLParams(embedded_test_server()->GetURL("/title1.html"),
                               content::Referrer(),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});
    browser()->OpenURL(
        content::OpenURLParams(embedded_test_server()->GetURL("/title2.html"),
                               content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK, false),
        /*navigation_handle_callback=*/{});
  }

  void ValidateUserActions() override {
    EXPECT_EQ(
        browser()->profile()->GetPrefs()->GetInteger(prefs::kRestoreOnStartup),
        1);
    auto* tab_strip = browser()->tab_strip_model();
    // There are 2 tabs that need to be preserved. There might be a 3rd tab,
    // about:blank that is opened by the test itself. That 3rd tab may or may
    // not have been opened at this time.
    ASSERT_GE(tab_strip->count(), 2);
    ASSERT_LE(tab_strip->count(), 3);
    if (tab_strip->count() == 3) {
      EXPECT_TRUE(content::WaitForLoadStop(tab_strip->GetWebContentsAt(2)));
      EXPECT_EQ(tab_strip->GetWebContentsAt(2)->GetLastCommittedURL(),
                GURL("about:blank"));
    }

    // embedded_test_server() might return a different hostname.
    content::WaitForLoadStop(tab_strip->GetWebContentsAt(0));
    EXPECT_EQ(tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path(),
              "/title1.html");
    content::WaitForLoadStop(tab_strip->GetWebContentsAt(1));
    EXPECT_EQ(tab_strip->GetWebContentsAt(1)->GetLastCommittedURL().path(),
              "/title2.html");
  }
};

IN_PROC_BROWSER_TEST_F(TabsSnapshotTest, PRE_PRE_PRE_Test) {}
IN_PROC_BROWSER_TEST_F(TabsSnapshotTest, PRE_PRE_Test) {}
IN_PROC_BROWSER_TEST_F(TabsSnapshotTest, PRE_Test) {}
// TODO(crbug.com/326168468): Flaky on TSan.
#if defined(THREAD_SANITIZER)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(TabsSnapshotTest, MAYBE_Test) {}
#undef MAYBE_Test

// Tests that Google Chrome does not takes snapshots on mid-milestone updates.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, SameMilestoneSnapshot) {
  DowngradeManager::EnableSnapshotsForTesting(true);
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  base::FilePath user_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  auto current_version = version_info::GetVersion().GetString();

  downgrade::DowngradeManager downgrade_manager;

  // No snapshots for same version.
  base::WriteFile(user_data_dir.Append(kDowngradeLastVersionFile),
                  current_version);
  EXPECT_FALSE(downgrade_manager.PrepareUserDataDirectoryForCurrentVersion(
      user_data_dir));
  EXPECT_FALSE(
      base::PathExists(user_data_dir.Append(downgrade::kSnapshotsDir)));

  // Snapshot taken for minor update
  std::vector<uint32_t> last_minor_version_components;
  for (const auto& component : version_info::GetVersion().components()) {
    // Decrement all but the major version.
    last_minor_version_components.push_back(
        !last_minor_version_components.empty() && component > 0 ? component - 1
                                                                : component);
  }
  auto last_minor_version =
      base::Version(last_minor_version_components).GetString();
  base::WriteFile(user_data_dir.Append(kDowngradeLastVersionFile),
                  last_minor_version);

  EXPECT_FALSE(downgrade_manager.PrepareUserDataDirectoryForCurrentVersion(
      user_data_dir));
  EXPECT_FALSE(
      base::PathExists(user_data_dir.Append(downgrade::kSnapshotsDir)));
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Tests that Google Chrome canary takes snapshots on mid-milestone updates.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, CanarySameMilestoneSnapshot) {
  DowngradeManager::EnableSnapshotsForTesting(true);
  install_static::ScopedInstallDetails install_details(
      /*system_level=*/false, install_static::CANARY_INDEX);
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  base::FilePath user_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  auto current_version = version_info::GetVersion().GetString();

  downgrade::DowngradeManager downgrade_manager;

  // No snapshots for same version.
  base::WriteFile(user_data_dir.Append(kDowngradeLastVersionFile),
                  current_version);
  EXPECT_FALSE(downgrade_manager.PrepareUserDataDirectoryForCurrentVersion(
      user_data_dir));
  EXPECT_FALSE(
      base::PathExists(user_data_dir.Append(downgrade::kSnapshotsDir)));

  // Snapshot taken for minor update
  std::vector<uint32_t> last_minor_version_components;
  for (const auto& component : version_info::GetVersion().components()) {
    // Decrement all but the major version.
    last_minor_version_components.push_back(
        !last_minor_version_components.empty() && component > 0 ? component - 1
                                                                : component);
  }
  auto last_minor_version =
      base::Version(last_minor_version_components).GetString();
  base::WriteFile(user_data_dir.Append(kDowngradeLastVersionFile),
                  last_minor_version);

  EXPECT_FALSE(downgrade_manager.PrepareUserDataDirectoryForCurrentVersion(
      user_data_dir));
  EXPECT_TRUE(base::PathExists(user_data_dir.Append(downgrade::kSnapshotsDir)));
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace downgrade
