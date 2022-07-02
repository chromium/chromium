// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/restore_io_task.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/restore_io_task.h"
#include "chrome/browser/ash/file_manager/trash_unittest_base.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace file_manager {
namespace io_task {
namespace {

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::Field;

class RestoreIOTaskTest : public TrashBaseTest {
 public:
  RestoreIOTaskTest() = default;

  RestoreIOTaskTest(const RestoreIOTaskTest&) = delete;
  RestoreIOTaskTest& operator=(const RestoreIOTaskTest&) = delete;

  // The source_urls are collections of .trashinfo files. For the purpose of
  // testing, all files are restored to the root of the filesystem.
  // For example if source_urls contains a file .Trash/info/foo.txt.trashinfo
  // this will return restore paths of /foo.txt.
  const std::vector<std::string> CreateRestorePathsFromSourceURLs(
      const base::FilePath& base_path,
      const std::vector<storage::FileSystemURL>& source_urls) {
    std::vector<std::string> restore_paths;
    for (const auto& url : source_urls) {
      // source_urls are made relative for testing purposes so we expect these
      // to not be absolute.
      EXPECT_FALSE(url.path().IsAbsolute());
      restore_paths.push_back(base::FilePath("/")
                                  .Append(url.path().RemoveFinalExtension())
                                  .value());
    }
    return restore_paths;
  }
};

TEST_F(RestoreIOTaskTest, URLsWithInvalidSuffixShouldError) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path = temp_dir_.GetPath().Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path),
  };
  std::vector<std::string> restore_paths =
      CreateRestorePathsFromSourceURLs(temp_dir_.GetPath(), source_urls);

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Progress callback should not be called as the suffix doesn't end in
  // .trashinfo.
  EXPECT_CALL(progress_callback, Run(_)).Times(0);

  // We should get one complete callback when the verification of the suffix
  // fails.
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kError)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, restore_paths, profile_.get(),
                     file_system_context_, temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(RestoreIOTaskTest, FilesNotInProperLocationShouldError) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path =
      temp_dir_.GetPath().Append("foo.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path),
  };
  std::vector<std::string> restore_paths =
      CreateRestorePathsFromSourceURLs(temp_dir_.GetPath(), source_urls);

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Progress callback should not be called as the supplied file is not within
  // the .Trash/info directory.
  EXPECT_CALL(progress_callback, Run(_)).Times(0);

  // We should get one complete callback when the location is invalid.
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kError)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, restore_paths, profile_.get(),
                     file_system_context_, temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(RestoreIOTaskTest, MetadataWithNoCorrespondingFileShouldError) {
  EnsureTrashDirectorySetup(downloads_dir_);

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path = downloads_dir_.Append(kTrashFolderName)
                                       .Append(kInfoFolderName)
                                       .Append("foo.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path),
  };
  std::vector<std::string> restore_paths =
      CreateRestorePathsFromSourceURLs(downloads_dir_, source_urls);

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Progress callback should not be called as the corresponding file in the
  // .Trash/files location does not exist.
  EXPECT_CALL(progress_callback, Run(_)).Times(0);

  // We should get one complete callback when the .Trash/files path doesn't
  // exist.
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kError)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, restore_paths, profile_.get(),
                     file_system_context_, temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(RestoreIOTaskTest, RestorePathsShouldNotReferenceParent) {
  EnsureTrashDirectorySetup(downloads_dir_);

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath trash_path = downloads_dir_.Append(kTrashFolderName);
  const base::FilePath info_file_path =
      trash_path.Append(kInfoFolderName).Append("foo.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(info_file_path, foo_contents));
  const base::FilePath files_path =
      trash_path.Append(kFilesFolderName).Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(files_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(info_file_path),
  };
  std::vector<std::string> illegal_paths{"/../../../bad/actor/foo.txt"};

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(progress_callback, Run(_)).Times(0);

  // We should get one complete callback when the restore path is found to have
  // parent path traversal, i.e. ".." characters.
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kError)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, illegal_paths, profile_.get(),
                     file_system_context_, temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(RestoreIOTaskTest, ValidRestorePathShouldSucceedAndCreateDirectory) {
  EnsureTrashDirectorySetup(downloads_dir_);

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath trash_path = downloads_dir_.Append(kTrashFolderName);
  const base::FilePath info_file_path =
      trash_path.Append(kInfoFolderName).Append("foo.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(info_file_path, foo_contents));
  const base::FilePath files_path =
      trash_path.Append(kFilesFolderName).Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(files_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(info_file_path),
  };
  std::vector<std::string> restore_paths{"/bar/foo.txt"};

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, restore_paths, profile_.get(),
                     file_system_context_, temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  EXPECT_TRUE(base::PathExists(downloads_dir_.Append("bar").Append("foo.txt")));
}

TEST_F(RestoreIOTaskTest, ItemWithExistingConflictAreRenamed) {
  EnsureTrashDirectorySetup(downloads_dir_);

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath trash_path = downloads_dir_.Append(kTrashFolderName);
  const base::FilePath info_file_path =
      trash_path.Append(kInfoFolderName).Append("foo.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(info_file_path, foo_contents));
  const base::FilePath files_path =
      trash_path.Append(kFilesFolderName).Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(files_path, foo_contents));

  // Create conflicting item at same place restore is going to happen at.
  const base::FilePath bar_dir = downloads_dir_.Append("bar");
  ASSERT_TRUE(base::CreateDirectory(bar_dir));
  ASSERT_TRUE(base::WriteFile(bar_dir.Append("foo.txt"), foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(info_file_path),
  };
  std::vector<std::string> restore_paths{"/bar/foo.txt"};

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, restore_paths, profile_.get(),
                     file_system_context_, temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  EXPECT_TRUE(base::PathExists(bar_dir.Append("foo.txt")));
  EXPECT_TRUE(base::PathExists(bar_dir.Append("foo (1).txt")));
  EXPECT_FALSE(base::PathExists(info_file_path));
}

}  // namespace
}  // namespace io_task
}  // namespace file_manager
