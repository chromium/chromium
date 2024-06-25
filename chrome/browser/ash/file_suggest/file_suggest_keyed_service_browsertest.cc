// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/ash/file_suggest/drive_file_suggestion_provider.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

using ::testing::_;
using ::testing::Field;
using ::testing::Not;
using ::testing::Pointee;

namespace ash::test {
namespace {

class FakeFailedSearchQuery : public drivefs::mojom::SearchQuery {
 public:
  FakeFailedSearchQuery() = default;
  FakeFailedSearchQuery(const FakeFailedSearchQuery&) = delete;
  FakeFailedSearchQuery& operator=(const FakeFailedSearchQuery&) = delete;
  ~FakeFailedSearchQuery() override = default;

  void GetNextPage(GetNextPageCallback callback) override {
    std::move(callback).Run(drive::FILE_ERROR_FAILED, {});
  }
};

class FakeSearchQuery : public drivefs::mojom::SearchQuery {
 public:
  FakeSearchQuery() = default;
  explicit FakeSearchQuery(std::vector<drivefs::mojom::QueryItemPtr> results)
      : results_(std::move(results)) {}

  FakeSearchQuery(const FakeSearchQuery&) = delete;
  FakeSearchQuery& operator=(const FakeSearchQuery&) = delete;
  ~FakeSearchQuery() override = default;

  void GetNextPage(GetNextPageCallback callback) override {
    if (next_page_called_) {
      std::move(callback).Run(drive::FILE_ERROR_OK, {});
      return;
    }
    next_page_called_ = true;
    std::move(callback).Run(drive::FILE_ERROR_OK, std::move(results_));
  }

 private:
  std::vector<drivefs::mojom::QueryItemPtr> results_;
  bool next_page_called_ = false;
};

class MockObserver : public FileSuggestKeyedService::Observer {
 public:
  explicit MockObserver(FileSuggestKeyedService* file_suggest_service)
      : file_suggest_service_(file_suggest_service) {
    file_suggest_service_observation_.Observe(file_suggest_service_.get());
  }
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  void WaitUntilFetchingSuggestData() { run_loop_.Run(); }

  // Returns the most recently fetched suggest data.
  const std::optional<std::vector<FileSuggestData>>& last_fetched_data() const {
    return last_fetched_data_;
  }

 private:
  void OnSuggestFileDataFetched(
      const std::optional<std::vector<FileSuggestData>>& suggest_data_array) {
    last_fetched_data_ = suggest_data_array;
    run_loop_.Quit();
  }

  // FileSuggestKeyedService::Observer:
  void OnFileSuggestionUpdated(FileSuggestionType type) override {
    EXPECT_EQ(FileSuggestionType::kDriveFile, type);
    file_suggest_service_->GetSuggestFileData(
        type, base::BindOnce(&MockObserver::OnSuggestFileDataFetched,
                             base::Unretained(this)));
  }

  const raw_ptr<FileSuggestKeyedService> file_suggest_service_;
  base::RunLoop run_loop_;
  std::optional<std::vector<FileSuggestData>> last_fetched_data_;
  base::ScopedObservation<FileSuggestKeyedService,
                          FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};
};

}  // namespace

class FileSuggestKeyedServiceBrowserTest
    : public drive::DriveIntegrationServiceBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  FileSuggestKeyedServiceBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kLauncherContinueSectionWithRecentsRollout,
        UseDriveRecents());
  }
  // drive::DriveIntegrationServiceBrowserTestBase:
  void SetUpOnMainThread() override {
    drive::DriveIntegrationServiceBrowserTestBase::SetUpOnMainThread();
    Profile* profile = browser()->profile();

    WaitUntilFileSuggestServiceReady(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile));

    InitTestFileMountRoot(profile);

    if (UseDriveRecents()) {
      ON_CALL(*GetFakeDriveFsForProfile(profile), StartSearchQuery(_, _))
          .WillByDefault([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                                 pending_receiver,
                             drivefs::mojom::QueryParametersPtr query_params) {
            auto search_query = std::make_unique<FakeSearchQuery>();
            mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                        std::move(pending_receiver));
          });
      // Flush any drive FS search requests that may have been initialized in
      // response to Drive FS, and FileSuggestKeyedService initialization, so
      // they don't interfere with the test flow.
      FlushDriveFsSearch();
    }

    // Add two drive files.
    const std::string file_id1("abc123");
    available_files_.push_back(file_id1);
    base::FilePath absolute_file_path;
    AddDriveFileWithRelativePath(profile, file_id1, base::FilePath(""),
                                 /*new_file_relative_path=*/nullptr,
                                 &absolute_file_path);
    file_paths_[file_id1] = absolute_file_path;

    const std::string file_id2("qwertyqwerty");
    available_files_.push_back(file_id2);
    AddDriveFileWithRelativePath(profile, file_id2, base::FilePath(""),
                                 /*new_file_relative_path=*/nullptr,
                                 &absolute_file_path);
    file_paths_[file_id2] = absolute_file_path;
  }

  drivefs::mojom::QueryItemPtr CreateQueryItemForTestFile(
      const std::string& file_id,
      base::Time timestamp) {
    const base::FilePath absolute_path = GetTestFilePath(file_id);
    base::FilePath drive_path;
    if (!drive::DriveIntegrationServiceFactory::FindForProfile(
             browser()->profile())
             ->GetRelativeDrivePath(absolute_path, &drive_path)) {
      return drivefs::mojom::QueryItemPtr();
    }

    auto result = drivefs::mojom::QueryItem::New();
    result->path = drive_path;
    result->metadata = drivefs::mojom::FileMetadata::New();
    result->metadata->modification_time = timestamp;
    result->metadata->modified_by_me_time = timestamp;
    result->metadata->last_viewed_by_me_time = timestamp;
    result->metadata->capabilities = drivefs::mojom::Capabilities::New();
    return result;
  }

  bool UseDriveRecents() const { return GetParam(); }

  const std::vector<std::string>& available_files() const {
    return available_files_;
  }

  base::FilePath GetTestFilePath(const std::string& file_id) const {
    const auto it = file_paths_.find(file_id);
    if (it == file_paths_.end()) {
      return base::FilePath();
    }
    return it->second;
  }

  void NotifyFilesCreated(const std::vector<std::string>& file_ids) {
    std::vector<drivefs::mojom::FileChangePtr> changes;
    Profile* const profile = browser()->profile();
    for (const auto& file_id : file_ids) {
      base::FilePath drive_path("/");
      base::FilePath absolute_path = GetTestFilePath(file_id);
      EXPECT_FALSE(absolute_path.empty());
      EXPECT_TRUE(drive::DriveIntegrationServiceFactory::FindForProfile(profile)
                      ->GetMountPointPath()
                      .AppendRelativePath(absolute_path, &drive_path));

      auto change = drivefs::mojom::FileChange::New();
      change->path = drive_path;
      change->type = drivefs::mojom::FileChange::Type::kCreate;
      changes.push_back(std::move(change));
    }

    // Simulate the `changes` being sent from the server.
    drivefs_delegate()->OnFilesChanged(std::move(changes));
    drivefs_delegate().FlushForTesting();
  }

  void FlushDriveFsSearch() {
    base::RunLoop suggest_file_data_waiter;
    FileSuggestKeyedService* const service =
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            browser()->profile());
    service->GetSuggestFileData(
        FileSuggestionType::kDriveFile,
        base::BindLambdaForTesting(
            [&](const std::optional<std::vector<FileSuggestData>>&
                    suggest_data) { suggest_file_data_waiter.Quit(); }));
    suggest_file_data_waiter.Run();
  }

  mojo::Remote<drivefs::mojom::DriveFsDelegate>& drivefs_delegate() {
    return GetFakeDriveFsForProfile(browser()->profile())->delegate();
  }

 private:
  // IDs of files added to fake file system.
  std::vector<std::string> available_files_;

  // Maps a test file added during test setup ID to the associated absolute file
  // path.
  std::map<std::string, base::FilePath> file_paths_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(UseDriveRecents,
                         FileSuggestKeyedServiceBrowserTest,
                         ::testing::Bool());

// Verifies that the file suggest keyed service works as expected when the item
// suggest cache is empty.
IN_PROC_BROWSER_TEST_P(FileSuggestKeyedServiceBrowserTest,
                       QueryWithEmptyCache) {
  // TODO(http://b/349164737): Re-enable this test with forest feature enabled.
  if (ash::features::IsForestFeatureEnabled() && !UseDriveRecents()) {
    GTEST_SKIP() << "Skipping test body for Forest Feature enabled and Drive "
                    "Recents disabled.";
  }

  base::HistogramTester histogram_tester;

  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  if (UseDriveRecents()) {
    EXPECT_CALL(*fake_drivefs, StartSearchQuery(_, _))
        .Times(3)
        .WillRepeatedly([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                                pending_receiver,
                            drivefs::mojom::QueryParametersPtr query_params) {
          auto search_query = std::make_unique<FakeSearchQuery>();
          mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                      std::move(pending_receiver));
        });
    // Invalidate cached suggestions by notifying that new files are present.
    NotifyFilesCreated({});
  } else {
    EXPECT_CALL(*fake_drivefs, StartSearchQuery(_, _)).Times(0);
  }

  base::RunLoop suggest_file_data_waiter;

  FileSuggestKeyedService* service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          browser()->profile());
  service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& suggest_data) {
            EXPECT_EQ(UseDriveRecents(), suggest_data.has_value());
            if (UseDriveRecents()) {
              EXPECT_EQ(0u, suggest_data->size());
            }
            suggest_file_data_waiter.Quit();
          }));

  suggest_file_data_waiter.Run();

  if (UseDriveRecents()) {
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.ItemCount.Total", 0, 1);
  } else {
    histogram_tester.ExpectBucketCount(
        "Ash.Search.DriveFileSuggestDataValidation.Status",
        /*sample=*/DriveSuggestValidationStatus::kNoResults,
        /*expected_count=*/1);
  }
}

// Verifies that the file suggest keyed service responds to the update in
// the item suggest cache correctly.
IN_PROC_BROWSER_TEST_P(FileSuggestKeyedServiceBrowserTest,
                       RespondToItemSuggestCacheUpdate) {
  // TODO(http://b/349164737): Re-enable this test with forest feature enabled.
  if (ash::features::IsForestFeatureEnabled() && !UseDriveRecents()) {
    GTEST_SKIP() << "Skipping test body for Forest Feature enabled and Drive "
                    "Recents disabled.";
  }
  base::HistogramTester histogram_tester;

  Profile* profile = browser()->profile();
  FileSuggestKeyedService* service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile);

  MockObserver observer(service);

  ASSERT_GE(available_files().size(), 2u);

  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  if (UseDriveRecents()) {
    EXPECT_CALL(
        *fake_drivefs,
        StartSearchQuery(
            _,
            Not(Pointee(Field(
                &drivefs::mojom::QueryParameters::sort_field,
                drivefs::mojom::QueryParameters::SortField::kSharedWithMe)))))
        .Times(2)
        .WillRepeatedly([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                                pending_receiver,
                            drivefs::mojom::QueryParametersPtr query_params) {
          std::vector<drivefs::mojom::QueryItemPtr> results;
          results.push_back(CreateQueryItemForTestFile(available_files()[0],
                                                       base::Time::Now()));
          results.push_back(CreateQueryItemForTestFile(
              available_files()[1], base::Time::Now() - base::Seconds(1)));
          auto search_query =
              std::make_unique<FakeSearchQuery>(std::move(results));
          mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                      std::move(pending_receiver));
        })
        .RetiresOnSaturation();
    EXPECT_CALL(
        *fake_drivefs,
        StartSearchQuery(
            _, Pointee(Field(
                   &drivefs::mojom::QueryParameters::sort_field,
                   drivefs::mojom::QueryParameters::SortField::kSharedWithMe))))
        .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                          pending_receiver,
                      drivefs::mojom::QueryParametersPtr query_params) {
          auto search_query = std::make_unique<FakeSearchQuery>();
          mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                      std::move(pending_receiver));
        });
    // Invalidate cached suggestions by notifying that new files are present.
    NotifyFilesCreated({available_files()[0], available_files()[1]});
  } else {
    EXPECT_CALL(*fake_drivefs, StartSearchQuery(_, _)).Times(0);

    // Update the item suggest cache with two file ids: one is valid and the
    // other is not.
    std::string json_string = CreateItemSuggestUpdateJsonString(
        {{available_files()[0], "display text 1", "You edited · just now"},
         {available_files()[1], "display text 2", "You edited · just now"}},
        "suggestion id 1");
    DriveFileSuggestionProvider* file_suggestion_provider =
        static_cast<DriveFileSuggestionProvider*>(
            service->drive_file_suggestion_provider_for_test());
    file_suggestion_provider->item_suggest_cache_for_test()
        ->UpdateCacheWithJsonForTest(json_string);
    observer.WaitUntilFetchingSuggestData();
  }

  base::RunLoop suggest_file_data_waiter;
  service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& suggest_data) {
            ASSERT_TRUE(suggest_data.has_value());
            EXPECT_EQ(2u, suggest_data->size());
            if (suggest_data->size() < 2u) {
              suggest_file_data_waiter.Quit();
              return;
            }

            const auto& item1 = (*suggest_data)[0];
            EXPECT_EQ(GetTestFilePath(available_files()[0]), item1.file_path);
            EXPECT_EQ(u"You edited · just now", item1.prediction_reason);

            const auto& item2 = (*suggest_data)[1];
            EXPECT_EQ(GetTestFilePath(available_files()[1]), item2.file_path);
            EXPECT_EQ(u"You edited · just now", item2.prediction_reason);

            suggest_file_data_waiter.Quit();
          }));
  suggest_file_data_waiter.Run();

  const auto& fetched_data = observer.last_fetched_data();
  EXPECT_TRUE(fetched_data.has_value());
  ASSERT_EQ(2u, fetched_data->size());

  const auto& item1 = (*fetched_data)[0];
  EXPECT_EQ(GetTestFilePath(available_files()[0]), item1.file_path);
  EXPECT_EQ(u"You edited · just now", item1.prediction_reason);

  const auto& item2 = (*fetched_data)[1];
  EXPECT_EQ(GetTestFilePath(available_files()[1]), item2.file_path);
  EXPECT_EQ(u"You edited · just now", item2.prediction_reason);

  if (UseDriveRecents()) {
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.QueryResult.Viewed", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.QueryResult.Modified", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.QueryResult.Shared", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.ItemCount.Total", 2, 1);
  } else {
    // Note that the `observer` calls `GetSuggestFileData()` when the item
    // cache is updated, so the file suggest data gets processed twice.
    histogram_tester.ExpectBucketCount(
        "Ash.Search.DriveFileSuggestDataValidation.Status",
        /*sample=*/DriveSuggestValidationStatus::kOk,
        /*expected_count=*/2);
  }
}

// Verifies that the file suggest keyed service responds to the update in
// the item suggest cache correctly when item fetch fails.
IN_PROC_BROWSER_TEST_P(FileSuggestKeyedServiceBrowserTest,
                       RespondToItemSuggestCacheInvalidUpdate) {
  // TODO(http://b/349164737): Re-enable this test with forest feature enabled.
  if (ash::features::IsForestFeatureEnabled() && !UseDriveRecents()) {
    GTEST_SKIP() << "Skipping test body for Forest Feature enabled and Drive "
                    "Recents disabled.";
  }

  base::HistogramTester histogram_tester;

  Profile* profile = browser()->profile();
  FileSuggestKeyedService* service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile);

  // Ensure that `observer` exists before updating the suggest cache. Because
  // notifying the observer of the suggest cache update is synchronous.
  MockObserver observer(service);

  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  if (UseDriveRecents()) {
    EXPECT_CALL(*fake_drivefs, StartSearchQuery(_, _))
        .Times(3)
        .WillRepeatedly([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                                pending_receiver,
                            drivefs::mojom::QueryParametersPtr query_params) {
          auto search_query = std::make_unique<FakeFailedSearchQuery>();
          mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                      std::move(pending_receiver));
        });
    NotifyFilesCreated({});
  } else {
    EXPECT_CALL(*fake_drivefs, StartSearchQuery(_, _)).Times(0);

    // Update the item suggest cache with a non-existed file id.
    DriveFileSuggestionProvider* file_suggestion_provider =
        static_cast<DriveFileSuggestionProvider*>(
            service->drive_file_suggestion_provider_for_test());
    file_suggestion_provider->item_suggest_cache_for_test()
        ->UpdateCacheWithJsonForTest(CreateItemSuggestUpdateJsonString(
            {{"unknown", "display text 1", "prediction reason 1"}},
            "suggestion id 0"));
    observer.WaitUntilFetchingSuggestData();
    const auto& fetched_data = observer.last_fetched_data();
    EXPECT_FALSE(fetched_data.has_value());
  }

  base::RunLoop suggest_file_data_waiter;
  service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& suggest_data) {
            EXPECT_EQ(UseDriveRecents(), suggest_data.has_value());
            if (UseDriveRecents()) {
              EXPECT_EQ(0u, suggest_data->size());
            }
            suggest_file_data_waiter.Quit();
          }));

  suggest_file_data_waiter.Run();

  if (UseDriveRecents()) {
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.QueryResult.Viewed", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.QueryResult.Modified", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.QueryResult.Shared", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.ItemCount.Total", 0, 1);
  } else {
    // Note that the `observer` calls `GetSuggestFileData()` when the item cache
    // is updated, so the file suggest data gets processed twice.
    histogram_tester.ExpectBucketCount(
        "Ash.Search.DriveFileSuggestDataValidation.Status",
        /*sample=*/DriveSuggestValidationStatus::kAllFilesErrored,
        /*expected_count=*/2);
  }
}

// Verifies that the file suggest keyed service responds to the update in
// the item suggest cache correctly if some item fetches fail.
IN_PROC_BROWSER_TEST_P(FileSuggestKeyedServiceBrowserTest,
                       RespondToItemSuggestCachePartiallyInvalidUpdate) {
  // TODO(http://b/349164737): Re-enable this test with forest feature enabled.
  if (ash::features::IsForestFeatureEnabled() && !UseDriveRecents()) {
    GTEST_SKIP() << "Skipping test body for Forest Feature enabled and Drive "
                    "Recents disabled.";
  }

  base::HistogramTester histogram_tester;

  Profile* profile = browser()->profile();
  FileSuggestKeyedService* service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile);

  MockObserver observer(service);

  ASSERT_GE(available_files().size(), 1u);
  const std::string file_id = available_files()[0];

  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  if (UseDriveRecents()) {
    EXPECT_CALL(
        *fake_drivefs,
        StartSearchQuery(
            _, Pointee(Field(
                   &drivefs::mojom::QueryParameters::sort_field,
                   drivefs::mojom::QueryParameters::SortField::kLastModified))))
        .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                          pending_receiver,
                      drivefs::mojom::QueryParametersPtr query_params) {
          std::vector<drivefs::mojom::QueryItemPtr> results;
          results.push_back(
              CreateQueryItemForTestFile(file_id, base::Time::Now()));
          auto search_query =
              std::make_unique<FakeSearchQuery>(std::move(results));
          mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                      std::move(pending_receiver));
        })
        .RetiresOnSaturation();
    EXPECT_CALL(
        *fake_drivefs,
        StartSearchQuery(
            _,
            Not(Pointee(Field(
                &drivefs::mojom::QueryParameters::sort_field,
                drivefs::mojom::QueryParameters::SortField::kLastModified)))))
        .Times(2)
        .WillRepeatedly([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                                pending_receiver,
                            drivefs::mojom::QueryParametersPtr query_params) {
          auto search_query = std::make_unique<FakeFailedSearchQuery>();
          mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                      std::move(pending_receiver));
        });

    // Invalidate cached suggestions by notifying that new files are present.
    NotifyFilesCreated({file_id});
  } else {
    EXPECT_CALL(*fake_drivefs, StartSearchQuery(_, _)).Times(0);

    // Update the item suggest cache with two file ids: one is valid and the
    // other is not.
    std::string json_string = CreateItemSuggestUpdateJsonString(
        {{file_id, "display text 1", "You edited · just now"},
         {"unknown", "display text 2", "prediction reason 2"}},
        "suggestion id 1");
    DriveFileSuggestionProvider* file_suggestion_provider =
        static_cast<DriveFileSuggestionProvider*>(
            service->drive_file_suggestion_provider_for_test());
    file_suggestion_provider->item_suggest_cache_for_test()
        ->UpdateCacheWithJsonForTest(json_string);
    observer.WaitUntilFetchingSuggestData();

    const auto& fetched_data = observer.last_fetched_data();
    EXPECT_TRUE(fetched_data.has_value());
    EXPECT_EQ(1u, fetched_data->size());
    EXPECT_EQ(GetTestFilePath(file_id), fetched_data->at(0).file_path);
    EXPECT_EQ(u"You edited · just now", *fetched_data->at(0).prediction_reason);
  }

  base::RunLoop suggest_file_data_waiter;
  service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindLambdaForTesting(
          [&](const std::optional<std::vector<FileSuggestData>>& suggest_data) {
            ASSERT_TRUE(suggest_data.has_value());
            EXPECT_EQ(1u, suggest_data->size());
            if (suggest_data->size() < 1u) {
              suggest_file_data_waiter.Quit();
              return;
            }

            const auto& item = (*suggest_data)[0];
            EXPECT_EQ(GetTestFilePath(file_id), item.file_path);
            EXPECT_EQ(u"You edited · just now", item.prediction_reason);

            suggest_file_data_waiter.Quit();
          }));
  suggest_file_data_waiter.Run();

  if (UseDriveRecents()) {
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.QueryResult.Viewed", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.QueryResult.Modified", 0, 1);
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.QueryResult.Shared", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "Ash.Search.FileSuggestions.DriveRecents.ItemCount.Total", 1, 1);
  } else {
    // Note that the `observer` calls `GetSuggestFileData()` when the item cache
    // is updated, so the file suggest data gets processed twice.
    histogram_tester.ExpectBucketCount(
        "Ash.Search.DriveFileSuggestDataValidation.Status",
        /*sample=*/DriveSuggestValidationStatus::kOk,
        /*expected_count=*/2);
  }
}

}  // namespace ash::test
