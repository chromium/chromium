// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/ash/file_suggest/drive_file_suggestion_provider.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"

namespace ash::test {
namespace {

class MockObserver : public FileSuggestKeyedService::Observer {
 public:
  explicit MockObserver(FileSuggestKeyedService* file_suggest_service)
      : file_suggest_service_(file_suggest_service) {
    file_suggest_service_observation_.Observe(file_suggest_service_);
  }
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  void WaitUntilFetchingSuggestData() { run_loop_.Run(); }

  // Returns the most recently fetched suggest data.
  const absl::optional<std::vector<FileSuggestData>>& last_fetched_data()
      const {
    return last_fetched_data_;
  }

 private:
  void OnSuggestFileDataFetched(
      const absl::optional<std::vector<FileSuggestData>>& suggest_data_array) {
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

  FileSuggestKeyedService* const file_suggest_service_;
  base::RunLoop run_loop_;
  absl::optional<std::vector<FileSuggestData>> last_fetched_data_;
  base::ScopedObservation<FileSuggestKeyedService,
                          FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};
};

}  // namespace

class FileSuggestKeyedServiceBrowserTest
    : public drive::DriveIntegrationServiceBrowserTestBase {
  // drive::DriveIntegrationServiceBrowserTestBase:
  void SetUpOnMainThread() override {
    drive::DriveIntegrationServiceBrowserTestBase::SetUpOnMainThread();
    WaitUntilFileSuggestServiceReady(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            browser()->profile()));
  }
};

// Verifies that the file suggest keyed service works as expected when the item
// suggest cache is empty.
IN_PROC_BROWSER_TEST_F(FileSuggestKeyedServiceBrowserTest,
                       QueryWithEmptySuggestCache) {
  base::HistogramTester tester;
  FileSuggestKeyedService* service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          browser()->profile());
  service->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindOnce(
          [](const absl::optional<std::vector<FileSuggestData>>& suggest_data) {
            EXPECT_FALSE(suggest_data.has_value());
          }));
  tester.ExpectBucketCount("Ash.Search.DriveFileSuggestDataValidation.Status",
                           /*sample=*/DriveSuggestValidationStatus::kNoResults,
                           /*expected_count=*/1);
}

// Verifies that the file suggest keyed service responds to the update in
// the item suggest cache correctly.
IN_PROC_BROWSER_TEST_F(FileSuggestKeyedServiceBrowserTest,
                       RespondToItemSuggestCacheUpdate) {
  Profile* profile = browser()->profile();
  InitTestFileMountRoot(profile);

  // Add two drive files.
  const std::string file_id1("abc123");
  base::FilePath absolute_file_path1;
  AddDriveFileWithRelativePath(profile, file_id1, base::FilePath(""),
                               /*new_file_relative_path=*/nullptr,
                               &absolute_file_path1);
  const std::string file_id2("qwertyqwerty");
  base::FilePath absolute_file_path2;
  AddDriveFileWithRelativePath(profile, file_id2, base::FilePath(""),
                               /*new_file_relative_path=*/nullptr,
                               &absolute_file_path2);

  // A file id that does not exist in the drive file system.
  const std::string non_existed_id("non_existed");

  FileSuggestKeyedService* service =
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile);
  base::HistogramTester tester;

  // Verify in the scenario that all suggested file paths are invalid.
  {
    // Ensure that `observer` exists before updating the suggest cache. Because
    // notifying the observer of the suggest cache update is synchronous.
    MockObserver observer(service);

    // Update the item suggest cache with a non-existed file id.
    service->drive_file_suggestion_provider_for_test()
        ->item_suggest_cache_for_test()
        ->UpdateCacheWithJsonForTest(CreateItemSuggestUpdateJsonString(
            {{non_existed_id, "display text 1", "prediction reason 1"}},
            "suggestion id 0"));

    observer.WaitUntilFetchingSuggestData();
    const auto& fetched_data = observer.last_fetched_data();
    EXPECT_FALSE(fetched_data.has_value());
    tester.ExpectBucketCount(
        "Ash.Search.DriveFileSuggestDataValidation.Status",
        /*sample=*/DriveSuggestValidationStatus::kAllFilesErrored,
        /*expected_count=*/1);
  }

  // Verify in the scenario that some suggested file paths are invalid.
  {
    MockObserver observer(service);

    // Update the item suggest cache with two file ids: one is valid and the
    // other is not.
    std::string json_string = CreateItemSuggestUpdateJsonString(
        {{"abc123", "display text 1", "prediction reason 1"},
         {non_existed_id, "display text 2", "prediction reason 2"}},
        "suggestion id 1");
    service->drive_file_suggestion_provider_for_test()
        ->item_suggest_cache_for_test()
        ->UpdateCacheWithJsonForTest(json_string);

    observer.WaitUntilFetchingSuggestData();
    const auto& fetched_data = observer.last_fetched_data();
    EXPECT_TRUE(fetched_data.has_value());
    EXPECT_EQ(1u, fetched_data->size());
    EXPECT_EQ(absolute_file_path1, fetched_data->at(0).file_path);
    EXPECT_EQ(u"prediction reason 1", *fetched_data->at(0).prediction_reason);
    tester.ExpectBucketCount("Ash.Search.DriveFileSuggestDataValidation.Status",
                             /*sample=*/DriveSuggestValidationStatus::kOk,
                             /*expected_count=*/1);
  }

  // Verify in the scenario that all suggested file paths are valid.
  {
    MockObserver observer(service);

    // Update the item suggest cache with two valid ids.
    std::string json_string = CreateItemSuggestUpdateJsonString(
        {{"abc123", "display text 1", "prediction reason 1"},
         {"qwertyqwerty", "display text 2", "prediction reason 2"}},
        "suggestion id 2");
    service->drive_file_suggestion_provider_for_test()
        ->item_suggest_cache_for_test()
        ->UpdateCacheWithJsonForTest(json_string);

    observer.WaitUntilFetchingSuggestData();
    tester.ExpectBucketCount("Ash.Search.DriveFileSuggestDataValidation.Status",
                             /*sample=*/DriveSuggestValidationStatus::kOk,
                             /*expected_count=*/2);

    // Verify the fetched data.
    const auto& fetched_data = observer.last_fetched_data();
    EXPECT_TRUE(fetched_data.has_value());
    EXPECT_EQ(2u, fetched_data->size());
    EXPECT_EQ(absolute_file_path1, fetched_data->at(0).file_path);
    EXPECT_EQ(u"prediction reason 1", *fetched_data->at(0).prediction_reason);
    EXPECT_EQ(absolute_file_path2, fetched_data->at(1).file_path);
    EXPECT_EQ(u"prediction reason 2", *fetched_data->at(1).prediction_reason);
  }
}

}  // namespace ash::test
