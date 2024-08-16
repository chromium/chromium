// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/file_stream_forwarder.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace arc {

namespace {

class FileStreamForwarderTest : public testing::Test {
 public:
  FileStreamForwarderTest() {}

  void SetUp() override {
    // Prepare a temporary directory and the destination file.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    dest_file_path_ = temp_dir_.GetPath().AppendASCII("dest");
    base::File dest_file(dest_file_path_,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(dest_file.IsValid());
    dest_fd_ = base::ScopedFD(dest_file.TakePlatformFile());

    base::FilePath temp_path = temp_dir_.GetPath();
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_path,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>());
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    context_ = storage::CreateFileSystemContextForTesting(
        quota_manager_proxy_.get(), temp_path);

    // Prepare a file system.
    constexpr char kURLOrigin[] = "http://origin/";

    context_->OpenFileSystem(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        /*bucket=*/std::nullopt, storage::kFileSystemTypeTemporary,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce([](const storage::FileSystemURL& root_url,
                          const std::string& name, base::File::Error result) {
          EXPECT_EQ(base::File::FILE_OK, result);
        }));
    base::RunLoop().RunUntilIdle();

    // Prepare a 64KB file in the file system.
    url_ = context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        storage::kFileSystemTypeTemporary,
        base::FilePath().AppendASCII("test.dat"));

    constexpr int kTestDataSize = 1024 * 64;
    test_data_ = base::RandBytesAsString(kTestDataSize);

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFileWithData(
                  context_.get(), url_, test_data_));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath dest_file_path_;
  base::ScopedFD dest_fd_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<storage::FileSystemContext> context_;
  storage::FileSystemURL url_;
  std::string test_data_;
};

TEST_F(FileStreamForwarderTest, ForwardAll) {
  constexpr int kOffset = 0;
  const int kSize = test_data_.size();
  base::RunLoop run_loop;
  FileStreamForwarderPtr forwarder(new FileStreamForwarder(
      context_, url_, kOffset, kSize, std::move(dest_fd_),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool result) {
            EXPECT_TRUE(result);
            run_loop->Quit();
          },
          &run_loop)));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(dest_file_path_, &contents));
  EXPECT_EQ(test_data_, contents);
}

TEST_F(FileStreamForwarderTest, ForwardPartially) {
  constexpr int kOffset = 12345;
  constexpr int kSize = 6789;
  base::RunLoop run_loop;
  FileStreamForwarderPtr forwarder(new FileStreamForwarder(
      context_, url_, kOffset, kSize, std::move(dest_fd_),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool result) {
            EXPECT_TRUE(result);
            run_loop->Quit();
          },
          &run_loop)));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(dest_file_path_, &contents));
  EXPECT_EQ(test_data_.substr(kOffset, kSize), contents);
}

TEST_F(FileStreamForwarderTest, ForwardPartially2) {
  constexpr int kOffset = 1;
  const int kSize = test_data_.size() - 1;
  base::RunLoop run_loop;
  FileStreamForwarderPtr forwarder(new FileStreamForwarder(
      context_, url_, kOffset, kSize, std::move(dest_fd_),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool result) {
            EXPECT_TRUE(result);
            run_loop->Quit();
          },
          &run_loop)));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(dest_file_path_, &contents));
  EXPECT_EQ(test_data_.substr(kOffset, kSize), contents);
}

TEST_F(FileStreamForwarderTest, ForwardTooMuch) {
  constexpr int kOffset = 0;
  const int kSize = test_data_.size() + 1;  // Request more than provided.
  base::RunLoop run_loop;
  FileStreamForwarderPtr forwarder(new FileStreamForwarder(
      context_, url_, kOffset, kSize, std::move(dest_fd_),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool result) {
            EXPECT_FALSE(result);
            run_loop->Quit();
          },
          &run_loop)));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(dest_file_path_, &contents));
  EXPECT_EQ(test_data_, contents);
}

TEST_F(FileStreamForwarderTest, ForwardTooMuch2) {
  constexpr int kOffset = 1;
  const int kSize = test_data_.size();  // Request more than provided.
  base::RunLoop run_loop;
  FileStreamForwarderPtr forwarder(new FileStreamForwarder(
      context_, url_, kOffset, kSize, std::move(dest_fd_),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool result) {
            EXPECT_FALSE(result);
            run_loop->Quit();
          },
          &run_loop)));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(dest_file_path_, &contents));
  EXPECT_EQ(test_data_.substr(kOffset), contents);
}

TEST_F(FileStreamForwarderTest, InvalidURL) {
  storage::FileSystemURL invalid_url = context_->CreateCrackedFileSystemURL(
      blink::StorageKey::CreateFromStringForTesting("http://invalid-origin/"),
      storage::kFileSystemTypeTemporary,
      base::FilePath().AppendASCII("invalid.dat"));
  constexpr int kOffset = 0;
  const int kSize = test_data_.size();
  base::RunLoop run_loop;
  FileStreamForwarderPtr forwarder(new FileStreamForwarder(
      context_, invalid_url, kOffset, kSize, std::move(dest_fd_),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool result) {
            EXPECT_FALSE(result);
            run_loop->Quit();
          },
          &run_loop)));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(dest_file_path_, &contents));
  EXPECT_TRUE(contents.empty());
}

}  // namespace

}  // namespace arc
