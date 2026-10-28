// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/share_info_file_stream_adapter.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_simple_task_runner.h"
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
const char kURLOrigin[] = "http://remote/";
constexpr int kTestFileSize = 8 * 1024 * 1024;
constexpr int kDefaultBufSize = 32 * 1024;
}  // namespace

class ShareInfoFileStreamAdapterTest : public testing::Test {
 public:
  ShareInfoFileStreamAdapterTest() = default;

  ShareInfoFileStreamAdapterTest(const ShareInfoFileStreamAdapterTest&) =
      delete;
  ShareInfoFileStreamAdapterTest& operator=(
      const ShareInfoFileStreamAdapterTest&) = delete;
  ~ShareInfoFileStreamAdapterTest() override = default;

  void SetUp() override {
    // Setup a temporary directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Setup path and temp file for testing.
    test_file_path_ = temp_dir_.GetPath().AppendASCII("test");
    base::File test_file(test_file_path_,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(test_file.IsValid() && base::PathExists(test_file_path_));
    test_fd_ = base::ScopedFD(test_file.TakePlatformFile());

    base::FilePath temp_path = temp_dir_.GetPath();
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_path,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>());
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        quota_manager_proxy_.get(), temp_path);

    base::File::Error open_file_system_result = base::File::FILE_ERROR_FAILED;
    base::RunLoop open_file_system_loop;
    file_system_context_->OpenFileSystem(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        /*bucket=*/std::nullopt, storage::kFileSystemTypeTemporary,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce(
            [](base::File::Error* open_file_system_result,
               base::RunLoop* run_loop, const storage::FileSystemURL&,
               const std::string&, base::File::Error result) {
              *open_file_system_result = result;
              run_loop->Quit();
            },
            &open_file_system_result, &open_file_system_loop));
    open_file_system_loop.Run();
    ASSERT_EQ(base::File::FILE_OK, open_file_system_result);

    // Setup a test file in the file system with random data.
    url_ = file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        storage::kFileSystemTypeTemporary,
        base::FilePath().AppendASCII("test.dat"));
    test_data_ = base::RandBytesAsString(kTestFileSize);

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFileWithData(
                  file_system_context_.get(), url_, test_data_));
  }

  void TearDown() override { stream_adapter_.reset(); }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath test_file_path_;
  base::ScopedFD test_fd_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<ShareInfoFileStreamAdapter> stream_adapter_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  storage::FileSystemURL url_;
  std::string test_data_;
};

TEST_F(ShareInfoFileStreamAdapterTest, ReadEntireStreamAndWriteFile) {
  constexpr int kOffset = 0;
  const size_t kSize = test_data_.size();
  base::RunLoop run_loop;
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kDefaultBufSize,
      std::move(test_fd_), base::BindLambdaForTesting([&run_loop](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file_path_, &contents));
  EXPECT_EQ(test_data_, contents);
}

TEST_F(ShareInfoFileStreamAdapterTest,
       ReadEntireStreamAndWriteFileSmallBuffer) {
  constexpr int kOffset = 0;
  const int kSize = test_data_.size();
  constexpr int kBufSize = 16 * 1024;
  base::RunLoop run_loop;
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kBufSize, std::move(test_fd_),
      base::BindLambdaForTesting([&run_loop](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file_path_, &contents));
  EXPECT_EQ(test_data_, contents);
}

TEST_F(ShareInfoFileStreamAdapterTest, ReadEntireStreamAndWriteFileTinyBuffer) {
  constexpr int kOffset = 0;
  const int kSize = test_data_.size();
  constexpr int kBufSize = 8 * 1024;
  base::RunLoop run_loop;
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kBufSize, std::move(test_fd_),
      base::BindLambdaForTesting([&run_loop](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file_path_, &contents));
  EXPECT_EQ(test_data_, contents);
}

TEST_F(ShareInfoFileStreamAdapterTest, ReadMidStreamAndWriteFile) {
  constexpr int kOffset = 1024;
  const int kSize = test_data_.size() - 1024;
  base::RunLoop run_loop;
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kDefaultBufSize,
      std::move(test_fd_), base::BindLambdaForTesting([&run_loop](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file_path_, &contents));
  EXPECT_EQ(test_data_.substr(kOffset, kSize), contents);
}

// A chunk write that is already queued when the owner releases its reference
// to the adapter must still be delivered to the destination file, and must
// not touch any file descriptor created after the release.
TEST_F(ShareInfoFileStreamAdapterTest,
       PendingWriteCompletesAfterAdapterReleased) {
  constexpr int kOffset = 0;
  constexpr int kBufSize = 4 * 1024;
  constexpr int kSize = 2 * kBufSize;

  // Use a manually pumped task runner for the blocking IO so the test
  // controls exactly when the posted write runs.
  auto write_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();

  std::optional<bool> stream_result;
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kBufSize, std::move(test_fd_),
      base::BindLambdaForTesting(
          [&stream_result](bool result) { stream_result = result; }));
  stream_adapter_->StartRunnerForTesting(write_task_runner);

  // Run until the first chunk has been read and its write has been posted to
  // |write_task_runner|.
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(write_task_runner->HasPendingTask());

  // Release the only reference to the adapter while the write is still
  // queued, as happens when a share session is torn down mid-transfer.
  stream_adapter_.reset();
  task_environment_.RunUntilIdle();

  // A file opened after the release; the queued write must not affect it.
  const base::FilePath other_file_path =
      temp_dir_.GetPath().AppendASCII("other");
  base::File other_file(other_file_path,
                        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(other_file.IsValid());

  write_task_runner->RunPendingTasks();
  task_environment_.RunUntilIdle();

  // The chunk read before the release should have reached the destination
  // file, and only the destination file.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_file_path_, &contents));
  EXPECT_EQ(test_data_.substr(kOffset, kBufSize), contents);

  std::string other_contents;
  ASSERT_TRUE(base::ReadFileToString(other_file_path, &other_contents));
  EXPECT_TRUE(other_contents.empty());

  // Streaming did not run to completion, so the result callback reports
  // failure.
  ASSERT_TRUE(stream_result.has_value());
  EXPECT_FALSE(stream_result.value());
}

}  // namespace arc
