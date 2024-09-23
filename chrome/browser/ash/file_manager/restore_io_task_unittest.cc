// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/restore_io_task.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_unittest_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/trash_service/public/cpp/trash_service.h"
#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom-forward.h"
#include "chromeos/ash/components/trash_service/trash_service_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace file_manager::io_task {
namespace {

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::Field;

class ScopedFileForTest {
 public:
  ScopedFileForTest(const base::FilePath& absolute_file_path,
                    const std::string& file_contents)
      : absolute_file_path_(absolute_file_path) {
    EXPECT_TRUE(base::WriteFile(absolute_file_path_, file_contents));
  }

  ~ScopedFileForTest() { EXPECT_TRUE(base::DeleteFile(absolute_file_path_)); }

  const base::FilePath GetPath() { return absolute_file_path_; }

 private:
  const base::FilePath absolute_file_path_;
};

class RestoreIOTaskTest : public TrashBaseTest {
 public:
  RestoreIOTaskTest() = default;

  RestoreIOTaskTest(const RestoreIOTaskTest&) = delete;
  RestoreIOTaskTest& operator=(const RestoreIOTaskTest&) = delete;

  void SetUp() override {
    TrashBaseTest::SetUp();

    // The TrashService launches a sandboxed process to perform parsing in, in
    // unit tests this is not possible. So instead override the launcher to
    // start an in-process TrashService and have `LaunchTrashService` invoke it.
    ash::trash_service::SetTrashServiceLaunchOverrideForTesting(
        base::BindRepeating(&RestoreIOTaskTest::CreateInProcessTrashService,
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
                         base::TimeFormatAsIso8601(base::Time::UnixEpoch())});
  }

  void ExpectRestorePathFailure(const std::string& restore_path_in_trashinfo) {
    std::string foo_contents = base::RandBytesAsString(kTestFileSize);
    std::string foo_metadata_contents =
        GenerateTrashInfoContents(restore_path_in_trashinfo);

    const base::FilePath trash_path =
        downloads_dir_.Append(trash::kTrashFolderName);
    ScopedFileForTest trash_info_file(
        trash_path.Append(trash::kInfoFolderName).Append("foo.txt.trashinfo"),
        foo_metadata_contents);
    ScopedFileForTest trash_files_file(
        trash_path.Append(trash::kFilesFolderName).Append("foo.txt"),
        foo_contents);

    base::RunLoop run_loop;
    std::vector<storage::FileSystemURL> source_urls = {
        CreateFileSystemURL(trash_info_file.GetPath()),
    };

    base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
    base::MockOnceCallback<void(ProgressStatus)> complete_callback;

    EXPECT_CALL(progress_callback, Run(_)).Times(0);
    EXPECT_CALL(complete_callback,
                Run(Field(&ProgressStatus::state, State::kError)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    RestoreIOTask task(source_urls, profile_.get(), file_system_context_,
                       temp_dir_.GetPath());
    task.Execute(progress_callback.Get(), complete_callback.Get());
    run_loop.Run();
  }

 private:
  // Maintains ownership fo the in-process parsing service.
  std::unique_ptr<ash::trash_service::TrashServiceImpl> trash_service_impl_;
};

TEST_F(RestoreIOTaskTest, NoSourceUrlsShouldReturnSuccess) {
  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls;

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // We should get one complete callback when the size check of `source_urls`
  // finds none.
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(RestoreIOTaskTest, URLsWithInvalidSuffixShouldError) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path = temp_dir_.GetPath().Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path),
  };

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

  RestoreIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
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

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Progress callback should not be called as the supplied file is not within
  // the .Trash/info directory.
  EXPECT_CALL(progress_callback, Run(_)).Times(0);

  // We should get one complete callback when the location is invalid.
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kError)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(RestoreIOTaskTest, MetadataWithNoCorrespondingFileShouldError) {
  EnsureTrashDirectorySetup(downloads_dir_);

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path =
      downloads_dir_.Append(trash::kTrashFolderName)
          .Append(trash::kInfoFolderName)
          .Append("foo.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path),
  };

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

  RestoreIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(RestoreIOTaskTest, InvalidRestorePaths) {
  EnsureTrashDirectorySetup(downloads_dir_);

  ExpectRestorePathFailure("/../../../bad/actor/foo.txt");
  ExpectRestorePathFailure("../../../bad/actor/foo.txt");
  ExpectRestorePathFailure("/");
}

TEST_F(RestoreIOTaskTest, ValidRestorePathShouldSucceedAndCreateDirectory) {
  EnsureTrashDirectorySetup(downloads_dir_);

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string foo_metadata_contents =
      GenerateTrashInfoContents("/Downloads/bar/foo.txt");

  const base::FilePath trash_path =
      downloads_dir_.Append(trash::kTrashFolderName);
  const base::FilePath info_file_path =
      trash_path.Append(trash::kInfoFolderName).Append("foo.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(info_file_path, foo_metadata_contents));
  const base::FilePath files_path =
      trash_path.Append(trash::kFilesFolderName).Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(files_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(info_file_path),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  EXPECT_TRUE(base::PathExists(downloads_dir_.Append("bar").Append("foo.txt")));
}

TEST_F(RestoreIOTaskTest, ItemWithExistingConflictAreRenamed) {
  EnsureTrashDirectorySetup(downloads_dir_);

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string foo_metadata_contents =
      GenerateTrashInfoContents("/Downloads/bar/foo.txt");

  const base::FilePath trash_path =
      downloads_dir_.Append(trash::kTrashFolderName);
  const base::FilePath info_file_path =
      trash_path.Append(trash::kInfoFolderName).Append("foo.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(info_file_path, foo_metadata_contents));
  const base::FilePath files_path =
      trash_path.Append(trash::kFilesFolderName).Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(files_path, foo_contents));

  // Create conflicting item at same place restore is going to happen at.
  const base::FilePath bar_dir = downloads_dir_.Append("bar");
  ASSERT_TRUE(base::CreateDirectory(bar_dir));
  ASSERT_TRUE(base::WriteFile(bar_dir.Append("foo.txt"), foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(info_file_path),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  EXPECT_TRUE(base::PathExists(bar_dir.Append("foo.txt")));
  EXPECT_TRUE(base::PathExists(bar_dir.Append("foo (1).txt")));
  EXPECT_FALSE(base::PathExists(info_file_path));
}

class TrashServiceMojoDisconnector
    : public ash::trash_service::mojom::TrashService {
 public:
  explicit TrashServiceMojoDisconnector(
      mojo::PendingReceiver<ash::trash_service::mojom::TrashService> receiver) {
    receivers_.Add(this, std::move(receiver));
  }
  ~TrashServiceMojoDisconnector() override = default;

  TrashServiceMojoDisconnector(const TrashServiceMojoDisconnector&) = delete;
  TrashServiceMojoDisconnector& operator=(const TrashServiceMojoDisconnector&) =
      delete;

  // When the method is called, clear all mojo receivers. This is effectively
  // disconnecting the mojo pipe and emulates the disconnect handler being
  // invoked.
  void ParseTrashInfoFile(
      base::File trash_info_file,
      ash::trash_service::ParseTrashInfoCallback callback) override {
    receivers_.Clear();
  }

 private:
  mojo::ReceiverSet<ash::trash_service::mojom::TrashService> receivers_;
};

class RestoreIOTaskDisconnectMojoTest : public TrashBaseTest {
 public:
  RestoreIOTaskDisconnectMojoTest() = default;

  RestoreIOTaskDisconnectMojoTest(const RestoreIOTaskDisconnectMojoTest&) =
      delete;
  RestoreIOTaskDisconnectMojoTest& operator=(
      const RestoreIOTaskDisconnectMojoTest&) = delete;

  void SetUp() override {
    TrashBaseTest::SetUp();

    // Override the TrashService launch method to instead create an instance of
    // our mock class which will immediately disconnect all receivers when
    // invoked.
    ash::trash_service::SetTrashServiceLaunchOverrideForTesting(
        base::BindRepeating(
            &RestoreIOTaskDisconnectMojoTest::CreateInProcessTrashService,
            base::Unretained(this)));
  }

  mojo::PendingRemote<ash::trash_service::mojom::TrashService>
  CreateInProcessTrashService() {
    mojo::PendingRemote<ash::trash_service::mojom::TrashService> remote;
    trash_service_test_impl_ = std::make_unique<TrashServiceMojoDisconnector>(
        remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

 protected:
  // Maintains ownership fo the in-process parsing service, this is to ensure
  // the service stays running for the duration of the test even if the mojo
  // pipe gets disconnected.
  std::unique_ptr<TrashServiceMojoDisconnector> trash_service_test_impl_;
};

TEST_F(RestoreIOTaskDisconnectMojoTest,
       TrashServiceMojoDisconnectShouldCompleteWithError) {
  EnsureTrashDirectorySetup(downloads_dir_);

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);

  const base::FilePath trash_path =
      downloads_dir_.Append(trash::kTrashFolderName);
  const base::FilePath info_file_path =
      trash_path.Append(trash::kInfoFolderName).Append("foo.txt.trashinfo");
  ASSERT_TRUE(base::WriteFile(info_file_path, foo_contents));
  const base::FilePath files_path =
      trash_path.Append(trash::kFilesFolderName).Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(files_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(info_file_path),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kError)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  RestoreIOTask task(source_urls, profile_.get(), file_system_context_,
                     temp_dir_.GetPath());
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

}  // namespace
}  // namespace file_manager::io_task
