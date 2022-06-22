// Copyright 2022 The Chromium Authors. All rights reserved.
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
using ::testing::ElementsAre;
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

class EmptyTrashIOTaskTest : public TrashBaseTest {
 public:
  EmptyTrashIOTaskTest() = default;

  EmptyTrashIOTaskTest(const EmptyTrashIOTaskTest&) = delete;
  EmptyTrashIOTaskTest& operator=(const EmptyTrashIOTaskTest&) = delete;
};

TEST_F(EmptyTrashIOTaskTest, EnabledTrashDirsAreTrashed) {
  base::FilePath my_files_trash_dir = my_files_dir_.Append(kTrashFolderName);
  base::FilePath downloads_trash_dir = downloads_dir_.Append(kTrashFolderName);
  base::FilePath crostini_trash_dir =
      crostini_dir_.Append(".local/share/Trash");
  ASSERT_TRUE(EnsureTrashDirectorySetup(my_files_trash_dir));
  ASSERT_TRUE(EnsureTrashDirectorySetup(downloads_trash_dir));
  ASSERT_TRUE(EnsureTrashDirectorySetup(crostini_trash_dir));

  base::RunLoop run_loop;
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // We should get one complete callback to be invoked once with a success
  // message and the list of outputs containing the enabled trash locations.
  EXPECT_CALL(complete_callback,
              Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                        Field(&ProgressStatus::sources, IsEmpty()),
                        Field(&ProgressStatus::outputs,
                              EntryStatusPaths(ElementsAre(
                                  my_files_trash_dir, downloads_trash_dir,
                                  crostini_trash_dir))))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  EmptyTrashIOTask task(kTestStorageKey, profile_.get(), file_system_context_,
                        temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  ASSERT_FALSE(base::PathExists(my_files_trash_dir));
  ASSERT_FALSE(base::PathExists(downloads_trash_dir));
  ASSERT_FALSE(base::PathExists(crostini_trash_dir));
}

}  // namespace
}  // namespace file_manager::io_task
