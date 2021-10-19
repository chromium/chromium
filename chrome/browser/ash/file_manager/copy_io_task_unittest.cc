// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_io_task.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Return;

namespace file_manager {
namespace io_task {
namespace {

const size_t kTestFileSize = 32;

class CopyIOTaskTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());
  }

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe(path));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://abc");

  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

TEST_F(CopyIOTaskTest, Basic) {
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("foo.txt"),
                              base::RandBytesAsString(kTestFileSize)));
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("bar.txt"),
                              base::RandBytesAsString(kTestFileSize)));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("foo.txt"),
      CreateFileSystemURL("bar.txt"),
  };
  auto dest = CreateFileSystemURL("");
  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, OperationType::kCopy),
            Field(&ProgressStatus::source_urls, source_urls),
            Field(&ProgressStatus::destination_folder, dest),
            Field(&ProgressStatus::total_bytes, 2 * kTestFileSize));
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  // Progress callback may be called any number of times, so this expectation
  // catches extra calls.
  EXPECT_CALL(progress_callback,
              Run(AllOf(Field(&ProgressStatus::state, State::kInProgress),
                        base_matcher)))
      .Times(AnyNumber());
  // We should get one progress callback when the first file completes.
  EXPECT_CALL(
      progress_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kInProgress),
                Field(&ProgressStatus::bytes_transferred, kTestFileSize),
                Field(&ProgressStatus::errors,
                      ElementsAre(base::File::FILE_OK, absl::nullopt)),
                base_matcher)))
      .Times(AtLeast(1));
  // We should get one complete callback when the copy finishes.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::bytes_transferred, 2 * kTestFileSize),
                Field(&ProgressStatus::errors,
                      ElementsAre(base::File::FILE_OK, base::File::FILE_OK)),
                base_matcher)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  CopyIOTask task(source_urls, dest, &profile_, file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  EXPECT_TRUE(base::ContentsEqual(temp_dir_.GetPath().Append("foo.txt"),
                                  temp_dir_.GetPath().Append("foo (1).txt")));
  EXPECT_TRUE(base::ContentsEqual(temp_dir_.GetPath().Append("bar.txt"),
                                  temp_dir_.GetPath().Append("bar (1).txt")));
}

TEST_F(CopyIOTaskTest, FolderCopy) {
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("folder")));
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("folder/foo.txt"),
                              base::RandBytesAsString(kTestFileSize)));
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("folder/bar.txt"),
                              base::RandBytesAsString(kTestFileSize)));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("folder2")));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("folder"),
  };
  auto dest = CreateFileSystemURL("folder2");
  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, OperationType::kCopy),
            Field(&ProgressStatus::source_urls, source_urls),
            Field(&ProgressStatus::destination_folder, dest),
            Field(&ProgressStatus::total_bytes, 2 * kTestFileSize));
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(
          Field(&ProgressStatus::state, State::kSuccess),
          Field(&ProgressStatus::bytes_transferred, 2 * kTestFileSize),
          Field(&ProgressStatus::errors, ElementsAre(base::File::FILE_OK)),
          base_matcher)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  CopyIOTask task(source_urls, dest, &profile_, file_system_context_);
  task.Execute(base::DoNothing(), complete_callback.Get());
  run_loop.Run();

  EXPECT_TRUE(
      base::ContentsEqual(temp_dir_.GetPath().Append("folder2/folder/foo.txt"),
                          temp_dir_.GetPath().Append("folder/foo.txt")));
  EXPECT_TRUE(
      base::ContentsEqual(temp_dir_.GetPath().Append("folder2/folder/bar.txt"),
                          temp_dir_.GetPath().Append("folder/bar.txt")));
}

TEST_F(CopyIOTaskTest, Cancel) {
  base::RunLoop run_loop;

  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("foo.txt"),
      CreateFileSystemURL("bar.txt"),
  };
  auto dest = CreateFileSystemURL("");
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback, Run(_)).Times(0);
  {
    CopyIOTask task(source_urls, dest, &profile_, file_system_context_);
    task.Execute(progress_callback.Get(), complete_callback.Get());
    task.Cancel();
    EXPECT_EQ(State::kCancelled, task.progress().state);
    // Once a task is cancelled, it must be synchronously destroyed, so destroy
    // it now.
  }
  base::RunLoop().RunUntilIdle();
}

TEST_F(CopyIOTaskTest, MissingSource) {
  base::RunLoop run_loop;

  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("nonexistent_foo.txt"),
      CreateFileSystemURL("nonexistent_bar.txt"),
  };
  auto dest = CreateFileSystemURL("");
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback,
              Run(AllOf(Field(&ProgressStatus::type, OperationType::kCopy),
                        Field(&ProgressStatus::source_urls, source_urls),
                        Field(&ProgressStatus::destination_folder, dest),
                        Field(&ProgressStatus::state, State::kError),
                        Field(&ProgressStatus::bytes_transferred, 0),
                        Field(&ProgressStatus::errors,
                              ElementsAre(base::File::FILE_ERROR_NOT_FOUND,
                                          absl::nullopt)))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  CopyIOTask task(source_urls, dest, &profile_, file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(CopyIOTaskTest, MissingDestination) {
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("foo.txt"),
                              base::RandBytesAsString(kTestFileSize)));
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("bar.txt"),
                              base::RandBytesAsString(kTestFileSize)));
  base::RunLoop run_loop;

  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("foo.txt"),
      CreateFileSystemURL("bar.txt"),
  };
  auto dest = CreateFileSystemURL("nonexistent_folder/");
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::type, OperationType::kCopy),
                Field(&ProgressStatus::source_urls, source_urls),
                Field(&ProgressStatus::destination_folder, dest),
                Field(&ProgressStatus::state, State::kError),
                Field(&ProgressStatus::bytes_transferred, 2 * kTestFileSize),
                Field(&ProgressStatus::total_bytes, 2 * kTestFileSize),
                Field(&ProgressStatus::errors,
                      ElementsAre(base::File::FILE_ERROR_NOT_FOUND,
                                  base::File::FILE_ERROR_NOT_FOUND)))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  CopyIOTask task(source_urls, dest, &profile_, file_system_context_);
  task.Execute(base::DoNothing(), complete_callback.Get());
  run_loop.Run();
}

}  // namespace
}  // namespace io_task
}  // namespace file_manager
