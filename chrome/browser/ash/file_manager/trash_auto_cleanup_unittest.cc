// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_auto_cleanup.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_io_task.h"
#include "chrome/browser/ash/file_manager/trash_unittest_base.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace file_manager::trash {

inline constexpr size_t kTestFileSize = 32;

class TrashAutoCleanupTest : public file_manager::io_task::TrashBaseTest {
 public:
  TrashAutoCleanupTest() = default;

  TrashAutoCleanupTest(const TrashAutoCleanupTest&) = delete;
  TrashAutoCleanupTest& operator=(const TrashAutoCleanupTest&) = delete;

  void SetUp() override {
    file_manager::io_task::TrashBaseTest::SetUp();
    trash_auto_cleanup_ = base::WrapUnique(new TrashAutoCleanup(profile()));
  }

  void TearDown() override {
    trash_auto_cleanup_.reset();
    file_manager::io_task::TrashBaseTest::TearDown();
  }

  void SetupFileTrashedFromDownloads(const std::string& file_name) {
    // Create file in Downloads.
    const base::FilePath file_path = downloads_dir_.Append(file_name);
    ASSERT_TRUE(
        base::WriteFile(file_path, base::RandBytesAsString(kTestFileSize)));

    // Create the FileSystemURL for the file to trash.
    std::string relative_path = file_path.value();
    EXPECT_TRUE(file_manager::util::ReplacePrefix(
        &relative_path, temp_dir_.GetPath().AsEndingWithSeparator().value(),
        ""));
    std::vector<storage::FileSystemURL> source_urls = {
        file_system_context_->CreateCrackedFileSystemURL(
            kTestStorageKey, storage::kFileSystemTypeTest,
            base::FilePath::FromUTF8Unsafe(relative_path)),
    };

    // Trash the file.
    base::RunLoop run_loop;
    base::MockRepeatingCallback<void(const io_task::ProgressStatus&)>
        progress_callback;
    base::MockOnceCallback<void(io_task::ProgressStatus)> complete_callback;
    EXPECT_CALL(complete_callback,
                Run(::testing::Field(&io_task::ProgressStatus::state,
                                     io_task::State::kSuccess)))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

    io_task::TrashIOTask task(source_urls, profile(), file_system_context_,
                              temp_dir_.GetPath());
    task.Execute(progress_callback.Get(), complete_callback.Get());
    run_loop.Run();

    // Check that the file has been trashed.
    const base::FilePath trashed_file_path = GetTrashedFilePath(file_name);
    const base::FilePath info_file_path = GetInfoFilePath(file_name);
    ASSERT_FALSE(base::PathExists(file_path));
    ASSERT_TRUE(base::PathExists(trashed_file_path));
    ASSERT_TRUE(base::PathExists(info_file_path));
    // Override last_modified time of the trashinfo file to match the task
    // environment's mocked clock.
    base::Time now = base::Time::Now();
    base::TouchFile(info_file_path, now, now);
  }

  void CheckFileExistsInTrash(const std::string& file_name, bool should_exist) {
    ASSERT_EQ(base::PathExists(GetInfoFilePath(file_name)), should_exist);
    ASSERT_EQ(base::PathExists(GetTrashedFilePath(file_name)), should_exist);
  }

  void SetCleanupDoneCallbackForTest(
      base::OnceCallback<void(AutoCleanupResult result)> callback) {
    trash_auto_cleanup_->SetCleanupDoneCallbackForTest(std::move(callback));
  }

  void StartCleanup() { trash_auto_cleanup_->StartCleanup(); }

  void InitAutoCleanup() { trash_auto_cleanup_->Init(); }

  std::string GenerateTrashInfoContents(const base::Time& deletion_time) {
    return base::StrCat(
        {"[Trash Info]\nPath=", "/Downloads/bar/", "original_file.txt",
         "\nDeletionDate=", base::TimeFormatAsIso8601(deletion_time)});
  }

  base::FilePath GetInfoFilePath(const std::string& file_name) {
    return downloads_dir_.Append(trash::kTrashFolderName)
        .Append(trash::kInfoFolderName)
        .Append(base::StrCat({file_name, ".trashinfo"}));
  }

  base::FilePath GetTrashedFilePath(const std::string& file_name) {
    return downloads_dir_.Append(trash::kTrashFolderName)
        .Append(trash::kFilesFolderName)
        .Append(file_name);
  }

  Profile* profile() const { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TrashAutoCleanup> trash_auto_cleanup_;
};

TEST_F(TrashAutoCleanupTest, CleanupIteration) {
  EnsureTrashDirectorySetup(downloads_dir_);

  // Setup files in trash directory.
  const std::string first_file_name = "first_file.txt";
  const std::string second_file_name = "second_file.txt";

  // Setup trashed files. The first file is trashed at T = 0, the second file is
  // trashed at T = 1 day.
  SetupFileTrashedFromDownloads(first_file_name);
  task_environment_.FastForwardBy(base::Days(1));
  SetupFileTrashedFromDownloads(second_file_name);

  // At T = 29 days, no file should be removed.
  task_environment_.FastForwardBy(base::Days(28));
  // Start auto cleanup.
  base::test::TestFuture<AutoCleanupResult> future;
  SetCleanupDoneCallbackForTest(future.GetCallback<AutoCleanupResult>());
  StartCleanup();
  ASSERT_TRUE(future.Wait());
  // Check that no file has been removed.
  ASSERT_EQ(future.Take(), AutoCleanupResult::kNoOldFilesToCleanup);
  CheckFileExistsInTrash(first_file_name, /*should_exist=*/true);
  CheckFileExistsInTrash(second_file_name, /*should_exist=*/true);

  // At T = 30 days, only the first file should be removed.
  task_environment_.FastForwardBy(base::Days(1));
  // Start auto cleanup.
  SetCleanupDoneCallbackForTest(future.GetCallback<AutoCleanupResult>());
  StartCleanup();
  ASSERT_TRUE(future.Wait());
  // Check that the first file has been removed.
  ASSERT_EQ(future.Take(), AutoCleanupResult::kCleanupSuccessful);
  CheckFileExistsInTrash(first_file_name, /*should_exist=*/false);
  CheckFileExistsInTrash(second_file_name, /*should_exist=*/true);

  // At T = 31 days, the second file should also be removed.
  task_environment_.FastForwardBy(base::Days(1));
  // Start auto cleanup.
  SetCleanupDoneCallbackForTest(future.GetCallback<AutoCleanupResult>());
  StartCleanup();
  ASSERT_TRUE(future.Wait());
  // Check that the second file has been removed.
  ASSERT_EQ(future.Take(), AutoCleanupResult::kCleanupSuccessful);
  CheckFileExistsInTrash(first_file_name, /*should_exist=*/false);
  CheckFileExistsInTrash(second_file_name, /*should_exist=*/false);
}

TEST_F(TrashAutoCleanupTest, PeriodicCleanup) {
  // Setup files in trash directory.
  const std::string first_file_name = "first_file.txt";
  const std::string second_file_name = "second_file.txt";

  // Setup trashed files. The first file is trashed at T = 0, the second file is
  // trashed a bit later at T = 10 minutes.
  SetupFileTrashedFromDownloads(first_file_name);
  task_environment_.FastForwardBy(base::Minutes(10));
  SetupFileTrashedFromDownloads(second_file_name);

  // Init the autocleanup process at T = 30 days (minus kCleanupCheckInterval,
  // the delay before the initial cleanup iteration). Only the first file should
  // be removed.
  task_environment_.FastForwardBy(base::Minutes(50) + base::Hours(23) +
                                  base::Days(29) - kCleanupCheckInterval);
  // Init periodic cleanup.
  base::test::TestFuture<AutoCleanupResult> future;
  SetCleanupDoneCallbackForTest(future.GetCallback<AutoCleanupResult>());
  InitAutoCleanup();
  ASSERT_TRUE(future.Wait());
  // Check that the first file has been removed.
  ASSERT_EQ(future.Take(), AutoCleanupResult::kCleanupSuccessful);
  CheckFileExistsInTrash(first_file_name, /*should_exist=*/false);
  CheckFileExistsInTrash(second_file_name, /*should_exist=*/true);

  // The second file should persist while we are within `kCleanupInterval` of
  // the last cleanup iteration.
  // Note: the mocked time stays constant until `future.Wait()` is called, so
  // `last_cleanup_iteration` is accurate.
  const base::Time last_cleanup_iteration = base::Time::Now();
  task_environment_.FastForwardBy(kCleanupInterval - kCleanupCheckInterval);
  CheckFileExistsInTrash(second_file_name, /*should_exist=*/true);
  // The next cleanup check should happen `kCleanupInterval` after the last
  // iteration. Check that the second file has now been removed.
  SetCleanupDoneCallbackForTest(future.GetCallback<AutoCleanupResult>());
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(base::Time::Now() - last_cleanup_iteration, kCleanupInterval);
  ASSERT_EQ(future.Take(), AutoCleanupResult::kCleanupSuccessful);
  CheckFileExistsInTrash(second_file_name, /*should_exist=*/false);
}

TEST_F(TrashAutoCleanupTest, MultipleBatchesCleanup) {
  // Setup a number of files in trash directory that equals more than twice the
  // batch size.
  std::vector<std::string> file_names;
  const int n_files = 2.5 * kMaxBatchSize;
  for (int i = 0; i < n_files; ++i) {
    const std::string file_name = "file" + base::NumberToString(i);
    file_names.emplace_back(file_name);
    SetupFileTrashedFromDownloads(file_name);
  }

  task_environment_.FastForwardBy(base::Days(30));
  base::test::TestFuture<AutoCleanupResult> future;
  SetCleanupDoneCallbackForTest(future.GetCallback<AutoCleanupResult>());
  InitAutoCleanup();
  ASSERT_TRUE(future.Wait());
  base::Time last_cleanup_iteration = base::Time::Now();
  // Check that the first batch of files has been removed.
  ASSERT_EQ(future.Take(), AutoCleanupResult::kCleanupSuccessful);
  // Check that `kMaxBatchSize` files have been removed.
  int n_remaining_files = 0;
  for (int i = 0; i < n_files; ++i) {
    const std::string file_name = "file" + base::NumberToString(i);
    if (!base::PathExists(GetTrashedFilePath(file_name))) {
      ASSERT_FALSE(base::PathExists(GetInfoFilePath(file_name)));
    } else {
      ASSERT_TRUE(base::PathExists(GetInfoFilePath(file_name)));
      ++n_remaining_files;
    }
  }
  ASSERT_EQ(n_remaining_files, n_files - kMaxBatchSize);

  // After this iteration, the next batch of files should be processed.
  SetCleanupDoneCallbackForTest(future.GetCallback<AutoCleanupResult>());
  ASSERT_TRUE(future.Wait());
  // This iteration should happen `kCleanupCheckInterval` after the preview one,
  // instead of the next day, since not all the batches have been processed
  // yet.
  ASSERT_EQ(base::Time::Now() - last_cleanup_iteration, kCleanupCheckInterval);
  last_cleanup_iteration = base::Time::Now();
  // Check that the second batch of files has been removed.
  ASSERT_EQ(future.Take(), AutoCleanupResult::kCleanupSuccessful);
  // Check that `2 * kMaxBatchSize` files have been removed.
  n_remaining_files = 0;
  for (int i = 0; i < n_files; ++i) {
    const std::string file_name = "file" + base::NumberToString(i);
    if (!base::PathExists(GetTrashedFilePath(file_name))) {
      ASSERT_FALSE(base::PathExists(GetInfoFilePath(file_name)));
    } else {
      ASSERT_TRUE(base::PathExists(GetInfoFilePath(file_name)));
      ++n_remaining_files;
    }
  }
  ASSERT_EQ(n_remaining_files, n_files - 2 * kMaxBatchSize);

  // After the last iteration, no file should be remaining.
  SetCleanupDoneCallbackForTest(future.GetCallback<AutoCleanupResult>());
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(base::Time::Now() - last_cleanup_iteration, kCleanupCheckInterval);
  // Check that all files have been removed.
  ASSERT_EQ(future.Take(), AutoCleanupResult::kCleanupSuccessful);
  for (int i = 0; i < n_files; ++i) {
    const std::string file_name = "file" + base::NumberToString(i);
    CheckFileExistsInTrash(file_name, /*should_exist=*/false);
  }
}

}  // namespace file_manager::trash
