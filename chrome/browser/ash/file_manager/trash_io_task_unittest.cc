// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_io_task.h"

#include <sys/xattr.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
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

namespace file_manager::io_task {
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
  std::vector<std::optional<base::File::Error>> errors;
  for (const auto& status : arg) {
    errors.push_back(status.error);
  }
  return testing::ExplainMatchResult(matcher, errors, result_listener);
}

std::string GetTrackedExtendedAttributeAsString(const base::FilePath& path) {
  ssize_t output_size =
      lgetxattr(path.value().c_str(), trash::kTrackedDirectoryName, nullptr, 0);
  EXPECT_GT(output_size, 0);
  std::vector<char> output_value(output_size);
  EXPECT_GT(lgetxattr(path.value().c_str(), trash::kTrackedDirectoryName,
                      output_value.data(), output_size),
            0);
  std::string xattr;
  xattr.assign(output_value.data(), output_size);
  return xattr;
}

class TrashIOTaskTest : public TrashBaseTest {
 public:
  TrashIOTaskTest() = default;

  TrashIOTaskTest(const TrashIOTaskTest&) = delete;
  TrashIOTaskTest& operator=(const TrashIOTaskTest&) = delete;
};

void AssertTrashSetup(const base::FilePath& parent_path) {
  base::FilePath trash_path = parent_path.Append(trash::kTrashFolderName);
  ASSERT_TRUE(base::DirectoryExists(trash_path));

  auto files_path = trash_path.Append(trash::kFilesFolderName);
  ASSERT_TRUE(base::DirectoryExists(files_path));

  auto info_path = trash_path.Append(trash::kInfoFolderName);
  ASSERT_TRUE(base::DirectoryExists(info_path));

  int mode = 0;
  ASSERT_TRUE(base::GetPosixFilePermissions(trash_path, &mode));
  EXPECT_EQ(mode, 0711);

  constexpr char expected_files_xattr[] = "trash_files";
  auto actual_files_xattr = GetTrackedExtendedAttributeAsString(files_path);
  EXPECT_EQ(actual_files_xattr, expected_files_xattr);

  constexpr char expected_info_xattr[] = "trash_info";
  auto actual_info_xattr = GetTrackedExtendedAttributeAsString(info_path);
  EXPECT_EQ(actual_info_xattr, expected_info_xattr);
}

void ExpectFileContents(const base::FilePath& path,
                        const std::string& expected) {
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path, &contents));
  EXPECT_EQ(expected, contents);
}

TEST_F(TrashIOTaskTest, NoSourceUrlsShouldReturnSuccess) {
  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls;

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // We should get one complete callback when the size check of `source_urls`
  // finds none.
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  TrashIOTask task(source_urls, profile_.get(), file_system_context_,
                   temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
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

  base::HistogramTester histogram_tester;

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
  histogram_tester.ExpectTotalCount(trash::kDirectorySetupHistogramName, 0);
}

TEST_F(TrashIOTaskTest, OrphanedFilesAreOverwritten) {
  base::HistogramTester histogram_tester;

  const std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const std::string file_name = "new\nline.txt";
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
    // Override the `base::Time::Now()` function to return
    // base::Time::UnixEpoch(). This ensures the DeletionDate is static in tests
    // to verify file contents.
    base::subtle::ScopedTimeClockOverrides mock_time_now(
        []() { return base::Time::UnixEpoch(); }, nullptr, nullptr);
    TrashIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
    task.Execute(progress_callback.Get(), complete_callback.Get());
    run_loop.Run();
  }

  AssertTrashSetup(downloads_dir_);
  ExpectFileContents(GenerateInfoPath(file_name), file_trashinfo_contents);
  ExpectFileContents(GenerateFilesPath(file_name), foo_contents);

  histogram_tester.ExpectTotalCount(trash::kDirectorySetupHistogramName, 0);
}

TEST_F(TrashIOTaskTest, MultipleFilesInvokeProgress) {
  base::HistogramTester histogram_tester;

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
        []() { return base::Time::UnixEpoch(); }, nullptr, nullptr);
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

  histogram_tester.ExpectTotalCount(trash::kDirectorySetupHistogramName, 0);
}

}  // namespace
}  // namespace file_manager::io_task
