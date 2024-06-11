// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/empty_trash_io_task.h"

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

namespace file_manager::io_task {
namespace {

using ::base::test::RunClosure;
using ::testing::AllOf;
using ::testing::ContainerEq;
using ::testing::Field;
using ::testing::IsEmpty;

// Matcher that only verifies the `base::FilePath`  on the
// `storage::FileSystemURL` in a supplied `std::vector<storage::FileSystemURL>`.
// ignoring the `error` fields. The supplied `arg` should be a
// `std::vector<storage::FileSystemURL>` to match against.
MATCHER_P(EntryStatusPaths, matcher, "") {
  std::vector<base::FilePath> paths;
  for (const auto& status : arg) {
    paths.push_back(status.url.path());
  }
  return testing::ExplainMatchResult(matcher, paths, result_listener);
}

struct TrashDirectoriesAndSubDirectories {
  std::vector<base::FilePath> trash_directories;
  std::vector<base::FilePath> trash_subdirectories;
};

class EmptyTrashIOTaskTest : public TrashBaseTest {
 public:
  EmptyTrashIOTaskTest() = default;

  EmptyTrashIOTaskTest(const EmptyTrashIOTaskTest&) = delete;
  EmptyTrashIOTaskTest& operator=(const EmptyTrashIOTaskTest&) = delete;

  base::FilePath SetupTrashDirectory(
      const base::FilePath& trash_parent_path,
      const std::string& relative_trash_folder,
      std::vector<base::FilePath>& trash_subdirectories) {
    base::FilePath trash_directory =
        trash_parent_path.Append(relative_trash_folder);
    EXPECT_TRUE(EnsureTrashDirectorySetup(trash_directory));

    trash_subdirectories.emplace_back(
        trash_directory.Append(trash::kFilesFolderName));
    trash_subdirectories.emplace_back(
        trash_directory.Append(trash::kInfoFolderName));
    return trash_directory;
  }

  const TrashDirectoriesAndSubDirectories SetupTrashAndReturnDirectories() {
    TrashDirectoriesAndSubDirectories directories;

    // Setup ~/MyFiles/.Trash
    directories.trash_directories.emplace_back(
        SetupTrashDirectory(my_files_dir_, trash::kTrashFolderName,
                            directories.trash_subdirectories));

    // Setup ~/MyFiles/Downloads/.Trash
    directories.trash_directories.emplace_back(
        SetupTrashDirectory(downloads_dir_, trash::kTrashFolderName,
                            directories.trash_subdirectories));

    return directories;
  }
};

TEST_F(EmptyTrashIOTaskTest, EnabledTrashDirsAreTrashed) {
  const auto [trash_directories, trash_subdirectories] =
      SetupTrashAndReturnDirectories();

  base::RunLoop run_loop;
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // We should get one complete callback to be invoked once with a success
  // message and the list of outputs containing the enabled trash locations.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::sources, IsEmpty()),
                Field(&ProgressStatus::outputs,
                      EntryStatusPaths(ContainerEq(trash_directories))))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  EmptyTrashIOTask task(kTestStorageKey, profile_.get(), file_system_context_,
                        temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  // Ensure the trash directories (e.g. ~/MyFiles/.Trash) are removed.
  for (const auto& dir : trash_directories) {
    ASSERT_FALSE(base::PathExists(dir));
  }
}

}  // namespace
}  // namespace file_manager::io_task
