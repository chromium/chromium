// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/zero_state_file_provider.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/local_file_suggestion_provider.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

using ::file_manager::file_tasks::FileTasksObserver;
using ::testing::UnorderedElementsAre;

MATCHER_P(Title, title, "") {
  return arg->title() == title;
}

}  // namespace

class ZeroStateFileProviderTest : public testing::Test {
 protected:
  ZeroStateFileProviderTest() = default;
  ~ZeroStateFileProviderTest() override = default;

  void SetUp() override {
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        TestingBrowserProcess::GetGlobal()->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            TestingBrowserProcess::GetGlobal()));

    const AccountId account_id = AccountId::FromUserEmailGaiaId(
        "primary_profile@test", GaiaId("123456"));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(account_id));
    user_session_test_environment_->LogIn(account_id);

    profile_ = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            account_id));
    ASSERT_TRUE(profile_);

    // The downloads directory depends on whether it is inside or outside
    // chromeos. So this needs to be in scope before |provider_| and
    // |downloads_folder_|.
    base::test::ScopedRunningOnChromeOS running_on_chromeos;

    auto provider = std::make_unique<ZeroStateFileProvider>(profile_);
    provider_ = provider.get();
    search_controller_.AddProvider(std::move(provider));

    downloads_folder_ =
        file_manager::util::GetDownloadsFolderForProfile(profile_);
    ASSERT_TRUE(base::CreateDirectory(downloads_folder_));

    ASSERT_TRUE(base::test::RunUntil(
        [this] { return LocalFileProvider()->IsInitialized(); }));
  }

  base::FilePath Path(const std::string& filename) {
    return profile_->GetPath().AppendASCII(filename);
  }

  base::FilePath DownloadsPath(const std::string& filename) {
    return downloads_folder_.AppendASCII(filename);
  }

  void WriteFile(const base::FilePath& path) {
    CHECK(base::WriteFile(path, "abcd"));
    CHECK(base::PathExists(path));
  }

  FileTasksObserver::FileOpenEvent OpenEvent(const base::FilePath& path) {
    FileTasksObserver::FileOpenEvent e;
    e.path = path;
    e.open_type = FileTasksObserver::OpenType::kOpen;
    return e;
  }

  void StartSearch(const std::u16string& query) {
    search_controller_.StartSearch(query);
  }

  void StartZeroStateSearch() {
    search_controller_.StartZeroState(base::DoNothing(), base::TimeDelta());
  }

  const SearchProvider::Results& LastResults() {
    return search_controller_.last_results();
  }

  ash::LocalFileSuggestionProvider* LocalFileProvider() {
    auto* keyed_service =
        ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            profile_);
    return keyed_service->local_file_suggestion_provider_for_test();
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<Profile> profile_ = nullptr;
  base::ScopedTempDir temp_dir_;
  base::FilePath downloads_folder_;

  TestSearchController search_controller_;
  raw_ptr<ZeroStateFileProvider> provider_ = nullptr;
};

TEST_F(ZeroStateFileProviderTest, NoResultsWithQuery) {
  StartSearch(u"query");
  EXPECT_TRUE(LastResults().empty());
}

TEST_F(ZeroStateFileProviderTest, FilterScreenshots) {
  WriteFile(Path("ScreenshotNonDownload.png"));
  WriteFile(DownloadsPath("ScreenshotNonPng.jpg"));
  WriteFile(DownloadsPath("NotScreenshot.png"));
  WriteFile(DownloadsPath("Screenshot123.png"));

  auto* keyed_service =
      ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile_);
  keyed_service->local_file_suggestion_provider_for_test()->OnFilesOpened(
      {OpenEvent(Path("ScreenshotNonDownload.png")),
       OpenEvent(DownloadsPath("ScreenshotNonPng.jpg")),
       OpenEvent(DownloadsPath("NotScreenshot.png")),
       OpenEvent(DownloadsPath("Screenshot123.png"))});

  SearchResultsChangedWaiter results_waiter(
      &search_controller_, {ash::AppListSearchResultType::kZeroStateFile});
  StartZeroStateSearch();
  results_waiter.Wait();

  // Screenshot123 matches the criteria for a screenshot and should be filtered
  // out.
  EXPECT_THAT(LastResults(),
              UnorderedElementsAre(Title(u"ScreenshotNonDownload.png"),
                                   Title(u"ScreenshotNonPng.jpg"),
                                   Title(u"NotScreenshot.png")));
}

}  // namespace app_list::test
