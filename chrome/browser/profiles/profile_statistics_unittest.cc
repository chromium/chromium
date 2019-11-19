// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile_statistics_aggregator.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "chrome/browser/sync/bookmark_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildBookmarkModelWithoutLoad(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model(
      new bookmarks::BookmarkModel(std::make_unique<ChromeBookmarkClient>(
          profile, ManagedBookmarkServiceFactory::GetForProfile(profile),
          BookmarkSyncServiceFactory::GetForProfile(profile))));
  return std::move(bookmark_model);
}

void LoadBookmarkModel(Profile* profile,
                       bookmarks::BookmarkModel* bookmark_model) {
  bookmark_model->Load(
      profile->GetPrefs(), profile->GetPath(), profile->GetIOTaskRunner(),
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI}));
}

bookmarks::BookmarkModel* CreateBookmarkModelWithoutLoad(Profile* profile) {
  return static_cast<bookmarks::BookmarkModel*>(
      BookmarkModelFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindRepeating(&BuildBookmarkModelWithoutLoad)));
}

class BookmarkStatHelper {
 public:
  BookmarkStatHelper() : num_of_times_called_(0) {}

  void StatsCallback(profiles::ProfileCategoryStats stats) {
    if (stats.back().category == profiles::kProfileStatisticsBookmarks)
      ++num_of_times_called_;
  }

  int GetNumOfTimesCalled() { return num_of_times_called_; }

 private:
  base::Closure quit_closure_;
  int num_of_times_called_;
};
}  // namespace

class ProfileStatisticsTest : public testing::Test {
 public:
  ProfileStatisticsTest() : manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ProfileStatisticsTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(manager_.SetUp());
  }

  void TearDown() override {
  }

  TestingProfileManager* manager() { return &manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager manager_;
};

TEST_F(ProfileStatisticsTest, WaitOrCountBookmarks) {
  TestingProfile* profile = manager()->CreateTestingProfile("Test 1");
  ASSERT_TRUE(profile);
  // We need history, autofill and password services for the test to succeed.
  ASSERT_TRUE(profile->CreateHistoryService(true, false));
  profile->CreateWebDataService();
  PasswordStoreFactory::GetInstance()->SetTestingFactory(
      profile,
      base::BindRepeating(
          &password_manager::BuildPasswordStore<
              content::BrowserContext, password_manager::TestPasswordStore>));

  bookmarks::BookmarkModel* bookmark_model =
      CreateBookmarkModelWithoutLoad(profile);
  ASSERT_TRUE(bookmark_model);

  // Run ProfileStatisticsAggregator::WaitOrCountBookmarks.
  BookmarkStatHelper bookmark_stat_helper;
  base::RunLoop run_loop_aggregator_done;

  ProfileStatisticsAggregator aggregator(
      profile, run_loop_aggregator_done.QuitClosure());
  aggregator.AddCallbackAndStartAggregator(
      base::Bind(&BookmarkStatHelper::StatsCallback,
                 base::Unretained(&bookmark_stat_helper)));

  // Wait until ProfileStatisticsAggregator::WaitOrCountBookmarks is run.
  base::RunLoop run_loop1;
  run_loop1.RunUntilIdle();
  EXPECT_EQ(0, bookmark_stat_helper.GetNumOfTimesCalled());

  // Run ProfileStatisticsAggregator::WaitOrCountBookmarks again.
  aggregator.AddCallbackAndStartAggregator(
      profiles::ProfileStatisticsCallback());
  // Wait until ProfileStatisticsAggregator::WaitOrCountBookmarks is run.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_EQ(0, bookmark_stat_helper.GetNumOfTimesCalled());

  // Load the bookmark model. When the model is loaded (asynchronously), the
  // observer added by WaitOrCountBookmarks is run.
  LoadBookmarkModel(profile, bookmark_model);

  run_loop_aggregator_done.Run();
  EXPECT_EQ(1, bookmark_stat_helper.GetNumOfTimesCalled());
}
