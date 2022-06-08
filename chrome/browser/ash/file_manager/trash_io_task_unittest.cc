// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_io_task.h"

#include "ash/components/disks/disk_mount_manager.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/trash_unittest_base.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace file_manager {
namespace io_task {
namespace {

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Return;

// Matcher that only verifies the `url` field from a `std::vector<EntryStatus>`
// ignoring the `error` field. The supplied `arg` should be a
// `std::vector<storage::FileSystemURL>` to match against.
MATCHER_P(EntryStatusUrls, matcher, "") {
  std::vector<storage::FileSystemURL> urls;
  for (const auto& status : arg) {
    urls.push_back(status.url);
  }
  return testing::ExplainMatchResult(matcher, urls, result_listener);
}

// Matcher that only verifies the `error` field from a
// `std::vector<EntryStatus>` ignoring the `url` field. The supplied `arg`
// should be a `std::vector<base::File::Error>` to match against.
MATCHER_P(EntryStatusErrors, matcher, "") {
  std::vector<absl::optional<base::File::Error>> errors;
  for (const auto& status : arg) {
    errors.push_back(status.error);
  }
  return testing::ExplainMatchResult(matcher, errors, result_listener);
}

class TrashIOTaskTest : public TrashBaseTest {
 public:
  TrashIOTaskTest() = default;

  TrashIOTaskTest(const TrashIOTaskTest&) = delete;
  TrashIOTaskTest& operator=(const TrashIOTaskTest&) = delete;
};

void AssertTrashSetup(const base::FilePath& parent_path) {
  base::FilePath trash_path = parent_path.Append(kTrashFolderName);
  ASSERT_TRUE(base::DirectoryExists(trash_path));
  ASSERT_TRUE(base::DirectoryExists(trash_path.Append(kFilesFolderName)));
  ASSERT_TRUE(base::DirectoryExists(trash_path.Append(kInfoFolderName)));
}

void ExpectFileContents(const base::FilePath& path,
                        const std::string& expected) {
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path, &contents));
  EXPECT_EQ(expected, contents);
}

TEST_F(TrashIOTaskTest, FileInUnsupportedDirectoryShouldError) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path = temp_dir_.GetPath().Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Progress callback should not be called as the construction of the IOTask
  // is expected to fail.
  EXPECT_CALL(progress_callback, Run(_)).Times(0);

  // We should get one complete callback when the construction of trash entries
  // fails to finish.
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kError)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  TrashIOTask task(source_urls, profile_.get(), file_system_context_,
                   temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(TrashIOTaskTest, MixedUnsupportedAndSupportedDirectoriesShouldError) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path_unsupported =
      temp_dir_.GetPath().Append("foo.txt");
  const base::FilePath file_path_supported = downloads_dir_.Append("bar.txt");
  ASSERT_TRUE(base::WriteFile(file_path_unsupported, foo_contents));
  ASSERT_TRUE(base::WriteFile(file_path_supported, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path_unsupported),
      CreateFileSystemURL(file_path_supported),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Progress callback should not be called as the construction of the IOTask
  // is expected to fail.
  EXPECT_CALL(progress_callback, Run(_)).Times(0);

  // We should get one complete callback when the construction of trash entries
  // fails to finish.
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kError)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  TrashIOTask task(source_urls, profile_.get(), file_system_context_,
                   temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(TrashIOTaskTest, SupportedDirectoryShouldSucceed) {
  // Force the drive integration service to be created, this ensures the code
  // path that adds the drive mount point is exercised.
  drive::DriveIntegrationServiceFactory::GetForProfile(profile_.get());

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path = downloads_dir_.Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Progress callback is only invoked when there is multiple files being
  // trashed.
  EXPECT_CALL(progress_callback, Run(_)).Times(0);

  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  TrashIOTask task(source_urls, profile_.get(), file_system_context_,
                   temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  AssertTrashSetup(downloads_dir_);
}

TEST_F(TrashIOTaskTest, OrphanedFilesAreOverwritten) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string file_name("foo.txt");
  const base::FilePath file_path = downloads_dir_.Append(file_name);
  const std::string file_trashinfo_contents =
      CreateTrashInfoContentsFromPath(file_path);
  const size_t total_expected_bytes =
      kTestFileSize + file_trashinfo_contents.size();
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  // Ensure the .Trash, info and files directories are setup and create a file
  // in .Trash/info that has no corresponding file in .Trash/files.
  ASSERT_TRUE(EnsureTrashDirectorySetup(downloads_dir_));
  ASSERT_TRUE(base::WriteFile(GenerateInfoPath(file_name),
                              "these contents should be overwritten"));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Completion callback should contain the one metadata file written with the
  // `total_expected_bytes` containing the size of both the file to trash and
  // the size of the metadata.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::bytes_transferred, total_expected_bytes),
                Field(&ProgressStatus::total_bytes, total_expected_bytes),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Field(&ProgressStatus::outputs,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK,
                                                    base::File::FILE_OK))))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  {
    // Override the `base::Time::Now()` function to return 0 (i.e. base::Time())
    // This ensures the DeletionDate is static in tests to verify file contents.
    base::subtle::ScopedTimeClockOverrides mock_time_now(
        []() { return base::Time(); }, nullptr, nullptr);
    TrashIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
    task.Execute(progress_callback.Get(), complete_callback.Get());
    run_loop.Run();
  }

  AssertTrashSetup(downloads_dir_);
  ExpectFileContents(GenerateInfoPath(file_name), file_trashinfo_contents);
  ExpectFileContents(GenerateFilesPath(file_name), foo_contents);
}

TEST_F(TrashIOTaskTest, MultipleFilesInvokeProgress) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string file_name_1("foo.txt");
  const base::FilePath file_path_1 = downloads_dir_.Append(file_name_1);
  const std::string file_trashinfo_contents_1 =
      CreateTrashInfoContentsFromPath(file_path_1);
  std::string file_name_2("bar.txt");
  const base::FilePath file_path_2 = downloads_dir_.Append(file_name_2);
  const std::string file_trashinfo_contents_2 =
      CreateTrashInfoContentsFromPath(file_path_2);
  const size_t expected_total_bytes = (kTestFileSize * 2) +
                                      file_trashinfo_contents_1.size() +
                                      file_trashinfo_contents_2.size();
  ASSERT_TRUE(base::WriteFile(file_path_1, foo_contents));
  ASSERT_TRUE(base::WriteFile(file_path_2, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path_1),
      CreateFileSystemURL(file_path_2),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Expect that all callback (both completion and progress) contains the set of
  // source URLs and the `total_bytes` set to `expected_total_bytes`.
  const auto base_matcher =
      AllOf(Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
            Field(&ProgressStatus::total_bytes, expected_total_bytes));

  // Progress callback may be called any number of times, so this expectation
  // catches extra calls.
  EXPECT_CALL(progress_callback,
              Run(AllOf(Field(&ProgressStatus::state, State::kInProgress),
                        base_matcher)))
      .Times(AnyNumber());

  // Expect the `progress_callback` to be invoked after the first metadata and
  // trash file have been written and moved with their size in the
  // `bytes_transferred`.
  EXPECT_CALL(progress_callback,
              Run(AllOf(Field(&ProgressStatus::state, State::kInProgress),
                        Field(&ProgressStatus::bytes_transferred,
                              file_trashinfo_contents_1.size() + kTestFileSize),
                        Field(&ProgressStatus::outputs,
                              EntryStatusErrors(ElementsAre(
                                  base::File::FILE_OK, base::File::FILE_OK))),
                        base_matcher)))
      .Times(1);

  // Completion callback should contain 4 files successfully being written. Each
  // `base::File::FILE_OK` in the outputs field corresponds to a successful
  // write or move of the file and associated metadata.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::bytes_transferred, expected_total_bytes),
                Field(&ProgressStatus::outputs,
                      EntryStatusErrors(ElementsAre(
                          base::File::FILE_OK, base::File::FILE_OK,
                          base::File::FILE_OK, base::File::FILE_OK))),
                base_matcher)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  {
    base::subtle::ScopedTimeClockOverrides mock_time_now(
        []() { return base::Time(); }, nullptr, nullptr);
    TrashIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
    task.Execute(progress_callback.Get(), complete_callback.Get());
    run_loop.Run();
  }

  AssertTrashSetup(downloads_dir_);
  ExpectFileContents(GenerateInfoPath(file_name_1), file_trashinfo_contents_1);
  ExpectFileContents(GenerateInfoPath(file_name_2), file_trashinfo_contents_2);
  ExpectFileContents(GenerateFilesPath(file_name_1), foo_contents);
  ExpectFileContents(GenerateFilesPath(file_name_2), foo_contents);
}

TEST_F(TrashIOTaskTest, WhenCrostiniContainerIsRunningPathsShouldTrash) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string file_name("foo.txt");
  const base::FilePath file_path = crostini_dir_.Append(file_name);

  // The .trashinfo file gets prepended with the users home directory, the file
  // foo.txt might look like Path=/home/<username>/foo.txt as the restore path.
  const std::string file_trashinfo_contents = CreateTrashInfoContentsFromPath(
      file_path, crostini_dir_, crostini_remote_mount_);
  const size_t total_expected_bytes =
      kTestFileSize + file_trashinfo_contents.size();
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Completion callback should contain the one metadata file written with the
  // `total_expected_bytes` containing the size of both the file to trash and
  // the size of the metadata.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::bytes_transferred, total_expected_bytes),
                Field(&ProgressStatus::total_bytes, total_expected_bytes),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Field(&ProgressStatus::outputs,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK,
                                                    base::File::FILE_OK))))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  {
    base::subtle::ScopedTimeClockOverrides mock_time_now(
        []() { return base::Time(); }, nullptr, nullptr);
    TrashIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
    task.Execute(progress_callback.Get(), complete_callback.Get());
    run_loop.Run();
  }

  // Ensure the contents of the files at .local/share/Trash/files/foo.txt
  // contains the expected content.
  const base::FilePath trash_path =
      crostini_dir_.AppendASCII(".local/share/Trash");
  const base::FilePath files_path =
      GenerateTrashPath(trash_path, kFilesFolderName, file_name);
  ExpectFileContents(files_path, foo_contents);

  // Ensure the contents of the files at
  // .local/share/Trash/info/foo.txt.trashinfo contains the expected content.
  const base::FilePath info_path =
      GenerateTrashPath(trash_path, kInfoFolderName, file_name);
  ExpectFileContents(info_path, file_trashinfo_contents);
}

TEST_F(TrashIOTaskTest,
       WhenCrostiniContainerIsStoppedPathsShouldNotBeTrashable) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string file_name("foo.txt");
  const base::FilePath file_path = crostini_dir_.Append(file_name);
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Ensure the container is stopped before running the IOTask.
  base::RunLoop crostini_stop;
  crostini_manager_->StopVm(
      crostini::kCrostiniDefaultVmName,
      base::BindLambdaForTesting(
          [&crostini_stop](crostini::CrostiniResult result) {
            crostini_stop.QuitClosure().Run();
            ASSERT_EQ(result, crostini::CrostiniResult::SUCCESS);
          }));
  crostini_stop.Run();

  // The supplied files match on a mounted crostini container, however, by
  // stopping the container we expect the paths to no longer match as we check
  // if the container is running.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kError),
                Field(&ProgressStatus::bytes_transferred, 0),
                Field(&ProgressStatus::total_bytes, 0),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(ElementsAre(
                          base::File::FILE_ERROR_INVALID_OPERATION))),
                Field(&ProgressStatus::outputs, IsEmpty()))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  TrashIOTask task(source_urls, profile_.get(), file_system_context_,
                   temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

}  // namespace
}  // namespace io_task
}  // namespace file_manager
