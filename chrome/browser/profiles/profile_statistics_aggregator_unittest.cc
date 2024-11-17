// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics_aggregator.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/test/test_file_util.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

scoped_refptr<autofill::AutofillWebDataService> BuildFakeAutofillWebDataService(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    os_crypt_async::OSCryptAsync* os_crypt) {
  scoped_refptr<WebDatabaseService> web_database_service =
      base::MakeRefCounted<WebDatabaseService>(
          base::FilePath(WebDatabase::kInMemoryPath),
          /*ui_task_runner=*/task_runner,
          /*db_task_runner=*/task_runner);
  // The 1 table needed by the *Autofill*WebDataService is the autocomplete one.
  web_database_service->AddTable(
      std::make_unique<autofill::AutocompleteTable>());
  web_database_service->LoadDatabase(os_crypt);
  return base::MakeRefCounted<autofill::AutofillWebDataService>(
      std::move(web_database_service),
      /*ui_task_runner=*/task_runner);
}

class BookmarkStatHelper {
 public:
  void StatsCallback(profiles::ProfileCategoryStats stats) {
    if (stats.back().category == profiles::kProfileStatisticsBookmarks) {
      ++num_of_times_called_;
    }
  }

  int GetNumOfTimesCalled() { return num_of_times_called_; }

 private:
  int num_of_times_called_ = 0;
};
}  // namespace

class ProfileStatisticsAggregatorTest : public testing::Test {
 public:
  ProfileStatisticsAggregatorTest() {
    history_service_.Init(history::HistoryDatabaseParams(
        base::CreateUniqueTempDirectoryScopedToTest(),
        /*download_interrupt_reason_none=*/0,
        /*download_interrupt_reason_crash=*/0, version_info::Channel::UNKNOWN));
    profile_password_store_->Init(&pref_service_,
                                  /*affiliated_match_helper=*/nullptr);
    autofill_web_data_service_->Init(base::DoNothing());
  }

  ~ProfileStatisticsAggregatorTest() override {
    profile_password_store_->ShutdownOnUIThread();
  }

  std::unique_ptr<ProfileStatisticsAggregator> CreateAggregator(
      base::OnceClosure done_callback) {
    return std::make_unique<ProfileStatisticsAggregator>(
        autofill_web_data_service_, &personal_data_manager_, &bookmark_model_,
        &history_service_, profile_password_store_, &pref_service_,
        /*user_annotations_service=*/nullptr,
        /*platform_credential_store=*/nullptr, std::move(done_callback));
  }

  bookmarks::BookmarkModel* bookmark_model() { return &bookmark_model_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  autofill::TestPersonalDataManager personal_data_manager_;
  bookmarks::BookmarkModel bookmark_model_{
      std::make_unique<bookmarks::TestBookmarkClient>()};
  history::HistoryService history_service_;
  const scoped_refptr<password_manager::TestPasswordStore>
      profile_password_store_ =
          base::MakeRefCounted<password_manager::TestPasswordStore>();
  const std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_ =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/true);
  const scoped_refptr<autofill::AutofillWebDataService>
      autofill_web_data_service_ = BuildFakeAutofillWebDataService(
          task_environment_.GetMainThreadTaskRunner(),
          os_crypt_.get());
};

TEST_F(ProfileStatisticsAggregatorTest, WaitOrCountBookmarks) {
  // Run ProfileStatisticsAggregator::WaitOrCountBookmarks.
  BookmarkStatHelper bookmark_stat_helper;
  base::RunLoop run_loop_aggregator_done;

  std::unique_ptr<ProfileStatisticsAggregator> aggregator =
      CreateAggregator(run_loop_aggregator_done.QuitClosure());
  aggregator->AddCallbackAndStartAggregator(
      base::BindRepeating(&BookmarkStatHelper::StatsCallback,
                          base::Unretained(&bookmark_stat_helper)));

  // Wait until ProfileStatisticsAggregator::WaitOrCountBookmarks is run.
  base::RunLoop run_loop1;
  run_loop1.RunUntilIdle();
  EXPECT_EQ(0, bookmark_stat_helper.GetNumOfTimesCalled());

  // Run ProfileStatisticsAggregator::WaitOrCountBookmarks again.
  aggregator->AddCallbackAndStartAggregator(
      profiles::ProfileStatisticsCallback());
  // Wait until ProfileStatisticsAggregator::WaitOrCountBookmarks is run.
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();
  EXPECT_EQ(0, bookmark_stat_helper.GetNumOfTimesCalled());

  // Load the bookmark model. When the model is loaded (asynchronously), the
  // observer added by WaitOrCountBookmarks is run.
  bookmark_model()->LoadEmptyForTest();

  run_loop_aggregator_done.Run();
  EXPECT_EQ(1, bookmark_stat_helper.GetNumOfTimesCalled());
}
