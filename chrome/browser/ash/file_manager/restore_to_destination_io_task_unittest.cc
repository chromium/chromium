// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/restore_to_destination_io_task.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/trash_unittest_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/trash_service/public/cpp/trash_service.h"
#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom-forward.h"
#include "chromeos/ash/components/trash_service/trash_service_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager::io_task {
namespace {

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::Field;

class RestoreToDestinationIOTaskTest : public TrashBaseTest {
 public:
  RestoreToDestinationIOTaskTest() = default;

  RestoreToDestinationIOTaskTest(const RestoreToDestinationIOTaskTest&) =
      delete;
  RestoreToDestinationIOTaskTest& operator=(
      const RestoreToDestinationIOTaskTest&) = delete;

  void SetUp() override {
    TrashBaseTest::SetUp();

    // The TrashService launches a sandboxed process to perform parsing in, in
    // unit tests this is not possible. So instead override the launcher to
    // start an in-process TrashService and have `LaunchTrashService` invoke it.
    ash::trash_service::SetTrashServiceLaunchOverrideForTesting(
        base::BindRepeating(
            &RestoreToDestinationIOTaskTest::CreateInProcessTrashService,
            base::Unretained(this)));
  }

  mojo::PendingRemote<ash::trash_service::mojom::TrashService>
  CreateInProcessTrashService() {
    mojo::PendingRemote<ash::trash_service::mojom::TrashService> remote;
    trash_service_impl_ =
        std::make_unique<ash::trash_service::TrashServiceImpl>(
            remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  std::string GenerateTrashInfoContents(const std::string& restore_path) {
    return base::StrCat({"[Trash Info]\nPath=", restore_path, "\nDeletionDate=",
                         base::TimeToISO8601(base::Time::UnixEpoch())});
  }

 private:
  // Maintains ownership of the in-process parsing service.
  std::unique_ptr<ash::trash_service::TrashServiceImpl> trash_service_impl_;
};

TEST_F(RestoreToDestinationIOTaskTest,
       RestorePathWithDifferentNameInTrashInfoSucceeds) {
  EnsureTrashDirectorySetup(downloads_dir_);

  // Setup the destination directory where the file will be restored to.
  base::FilePath destination_path = temp_dir_.GetPath().Append("dest_folder");
  ASSERT_TRUE(base::CreateDirectory(destination_path));

  // Setup the contents for files in the Trash directory.
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string foo_metadata_contents =
      GenerateTrashInfoContents("/Downloads/bar/baz.txt");

  // Write contents to the files in the trash directory.
  const base::FilePath trash_path =
      downloads_dir_.Append(trash::kTrashFolderName);
  const base::FilePath info_file_path =
      trash_path.Append(trash::kInfoFolderName).Append("foo.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(info_file_path, foo_metadata_contents));
  const base::FilePath files_path =
      trash_path.Append(trash::kFilesFolderName).Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(files_path, foo_contents));

  // Setup source and destination locations.
  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(info_file_path),
  };
  auto dest = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      destination_path.BaseName());

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreToDestinationIOTask task(source_urls, dest, profile_.get(),
                                  file_system_context_, temp_dir_.GetPath(),
                                  /*show_notification=*/true);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  // The underlying move task should have the same ID.
  ASSERT_TRUE(task.GetMoveTaskForTesting());
  EXPECT_EQ(task.progress().task_id,
            task.GetMoveTaskForTesting()->progress().task_id);

  EXPECT_TRUE(base::PathExists(destination_path.Append("baz.txt")));
  std::string contents;
  ASSERT_TRUE(
      base::ReadFileToString(destination_path.Append("baz.txt"), &contents));
  EXPECT_EQ(foo_contents, contents);
}

TEST_F(RestoreToDestinationIOTaskTest, PauseAndResume) {
  EnsureTrashDirectorySetup(downloads_dir_);

  // Setup the destination directory where the file will be restored to.
  base::FilePath destination_path = temp_dir_.GetPath().Append("dest_folder");
  ASSERT_TRUE(base::CreateDirectory(destination_path));

  // Setup the contents for files in the Trash directory.
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string foo_metadata_contents_1 =
      GenerateTrashInfoContents("/Downloads/bar/baz1.txt");
  std::string foo_metadata_contents_2 =
      GenerateTrashInfoContents("/Downloads/bar/baz2.txt");

  // Write contents to the files in the trash directory.
  const base::FilePath trash_path =
      downloads_dir_.Append(trash::kTrashFolderName);
  const base::FilePath file_1 =
      trash_path.Append(trash::kInfoFolderName).Append("foo1.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(file_1, foo_metadata_contents_1));
  const base::FilePath path_1 =
      trash_path.Append(trash::kFilesFolderName).Append("foo1.txt");
  ASSERT_TRUE(base::WriteFile(path_1, foo_contents));

  const base::FilePath file_2 =
      trash_path.Append(trash::kInfoFolderName).Append("foo2.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(file_2, foo_metadata_contents_2));
  const base::FilePath path_2 =
      trash_path.Append(trash::kFilesFolderName).Append("foo2.txt");
  ASSERT_TRUE(base::WriteFile(path_2, foo_contents));

  // Setup source and destination locations.
  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_1),
      CreateFileSystemURL(file_2),
  };
  auto dest = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      destination_path.BaseName());

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  RestoreToDestinationIOTask task(source_urls, dest, profile_.get(),
                                  file_system_context_, temp_dir_.GetPath(),
                                  /*show_notification=*/true);

  // Expect an in progress status and pause.
  EXPECT_CALL(progress_callback,
              Run(Field(&ProgressStatus::state, State::kInProgress)))
      .WillOnce([&task]() {
        PauseParams pause_params;
        pause_params.conflict_params.emplace();
        task.Pause(std::move(pause_params));

        ASSERT_TRUE(task.GetMoveTaskForTesting());
        EXPECT_TRUE(task.progress().IsPaused());
        EXPECT_TRUE(task.GetMoveTaskForTesting()->progress().IsPaused());
      });
  // Expect a paused status and resume.
  EXPECT_CALL(progress_callback,
              Run(Field(&ProgressStatus::state, State::kPaused)))
      .WillOnce([&task]() {
        ResumeParams resume_params;
        resume_params.conflict_params.emplace();
        resume_params.conflict_params->conflict_resolve = "replace";
        task.Resume(std::move(resume_params));
      });
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  EXPECT_TRUE(base::PathExists(destination_path.Append("baz1.txt")));
  std::string contents;
  ASSERT_TRUE(
      base::ReadFileToString(destination_path.Append("baz1.txt"), &contents));
  EXPECT_EQ(foo_contents, contents);

  EXPECT_TRUE(base::PathExists(destination_path.Append("baz2.txt")));
  std::string contents2;
  ASSERT_TRUE(
      base::ReadFileToString(destination_path.Append("baz2.txt"), &contents2));
  EXPECT_EQ(foo_contents, contents2);
}

}  // namespace
}  // namespace file_manager::io_task
