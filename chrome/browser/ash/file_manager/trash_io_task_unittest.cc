// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_io_task.h"

#include "ash/components/disks/disk_mount_manager.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace file_manager {
namespace io_task {
namespace {

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::Field;
using ::testing::Return;

constexpr size_t kTestFileSize = 32;

class TrashIOTaskTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());
    profile_ =
        std::make_unique<TestingProfile>(base::FilePath(temp_dir_.GetPath()));

    // Register `temp_dir_` as the parent directory for Downloads.
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile_.get()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        temp_dir_.GetPath());

    // Create Downloads inside the `temp_dir_`.
    downloads_dir_ = temp_dir_.GetPath().Append(
        file_manager::util::GetDownloadsMountPointName(profile_.get()));
    ASSERT_TRUE(base::CreateDirectory(downloads_dir_));
  }

  void TearDown() override {
    // Ensure any previously registered mount points for Downloads are revoked.
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile_.get()));
  }

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe(path));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://abc");

  base::ScopedTempDir temp_dir_;
  base::FilePath downloads_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

TEST_F(TrashIOTaskTest, FileInUnsupportedDirectoryShouldError) {
  base::ScopedTempDir unsupported_dir;
  ASSERT_TRUE(unsupported_dir.CreateUniqueTempDir());

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path = unsupported_dir.GetPath().Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path.value()),
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

  TrashIOTask task(source_urls, profile_.get(), file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(TrashIOTaskTest, MixedUnsupportedAndSupportedDirectoriesShouldError) {
  base::ScopedTempDir unsupported_dir;
  ASSERT_TRUE(unsupported_dir.CreateUniqueTempDir());

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path_unsupported =
      unsupported_dir.GetPath().Append("foo.txt");
  const base::FilePath file_path_supported = downloads_dir_.Append("bar.txt");
  ASSERT_TRUE(base::WriteFile(file_path_unsupported, foo_contents));
  ASSERT_TRUE(base::WriteFile(file_path_supported, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path_unsupported.value()),
      CreateFileSystemURL(file_path_supported.value()),
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

  TrashIOTask task(source_urls, profile_.get(), file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(TrashIOTaskTest, SupportedDirectoryOnly) {
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  const base::FilePath file_path = downloads_dir_.Append("foo.txt");
  ASSERT_TRUE(base::WriteFile(file_path, foo_contents));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL(file_path.value()),
  };

  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // TODO(b/231250202): Update this once the Trash logic has been written.
  EXPECT_CALL(complete_callback,
              Run(Field(&ProgressStatus::state, State::kSuccess)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  TrashIOTask task(source_urls, profile_.get(), file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

}  // namespace
}  // namespace io_task
}  // namespace file_manager
