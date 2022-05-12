// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/share_info_file_stream_adapter.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
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
  ShareInfoFileStreamAdapterTest()
      : consumer_stream_watcher_(FROM_HERE,
                                 mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

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

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr /*quota_manager_proxy=*/, temp_dir_.GetPath());

    file_system_context_->OpenFileSystem(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        /*bucket=*/absl::nullopt, storage::kFileSystemTypeTemporary,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindOnce([](const storage::FileSystemURL& root_url,
                          const std::string& name, base::File::Error result) {
          ASSERT_EQ(base::File::FILE_OK, result);
        }));
    base::RunLoop().RunUntilIdle();

    // Setup a test file in the file system with random data.
    url_ = file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        storage::kFileSystemTypeTemporary,
        base::FilePath().AppendASCII("test.dat"));
    test_data_ = base::RandBytesAsString(kTestFileSize);

    ASSERT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFileWithData(
                  file_system_context_.get(), url_, test_data_.data(),
                  test_data_.size()));
  }

  void TearDown() override { stream_adapter_.reset(); }

 protected:
  void SetupDataPipe(int capacity) {
    // Setup mojo data pipe for testing.
    MojoCreateDataPipeOptions options{};
    options.struct_size = sizeof(options);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = capacity;
    MojoResult result =
        CreateDataPipe(&options, producer_stream_, consumer_stream_);
    CHECK_EQ(result, MOJO_RESULT_OK);
    CHECK(producer_stream_.is_valid());

    consumer_stream_watcher_.Watch(
        consumer_stream_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
            MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(
            &ShareInfoFileStreamAdapterTest::OnConsumerStreamChanged,
            base::Unretained(this)));
    consumer_stream_watcher_.ArmOrNotify();
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath test_file_path_;
  base::ScopedFD test_fd_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<ShareInfoFileStreamAdapter> stream_adapter_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  storage::FileSystemURL url_;
  std::string test_data_;

  // Mojo Data Pipe testing support.
  mojo::ScopedDataPipeProducerHandle producer_stream_;
  mojo::ScopedDataPipeConsumerHandle consumer_stream_;
  mojo::SimpleWatcher consumer_stream_watcher_;
  std::vector<uint8_t> consumer_data_;

 private:
  // Mojo SimpleWatcher callback to save all data being sent from a connection.
  void OnConsumerStreamChanged(MojoResult result,
                               const mojo::HandleSignalsState& state) {
    if (!consumer_stream_.is_valid()) {
      return;
    }
    ASSERT_EQ(result, MOJO_RESULT_OK);

    uint32_t num_bytes = 0;
    result = consumer_stream_->ReadData(nullptr, &num_bytes,
                                        MOJO_READ_DATA_FLAG_QUERY);
    ASSERT_EQ(result, MOJO_RESULT_OK);
    if (num_bytes == 0) {
      // Nothing to read.
      return;
    }
    auto offset = consumer_data_.size();
    consumer_data_.resize(offset + num_bytes);
    result = consumer_stream_->ReadData(consumer_data_.data() + offset,
                                        &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    ASSERT_EQ(result, MOJO_RESULT_OK);
    consumer_data_.resize(offset + num_bytes);
    consumer_stream_watcher_.ArmOrNotify();
  }
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
  EXPECT_EQ(std::string(test_data_.begin() + kOffset,
                        test_data_.begin() + kOffset + kSize),
            contents);
}

TEST_F(ShareInfoFileStreamAdapterTest, ReadEntireStreamAndWritePipe) {
  constexpr int kOffset = 0;
  const size_t kSize = test_data_.size();
  constexpr int kDataPipeCapacity = 64 * 1024;
  base::RunLoop run_loop;
  SetupDataPipe(kDataPipeCapacity);
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kDefaultBufSize,
      std::move(producer_stream_),
      base::BindLambdaForTesting([&run_loop](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  EXPECT_EQ(kSize, consumer_data_.size());
  EXPECT_EQ(std::string(test_data_.begin(), test_data_.end()),
            std::string(consumer_data_.begin(), consumer_data_.end()));
}

TEST_F(ShareInfoFileStreamAdapterTest, ReadPartialStreamAndWritePipe) {
  constexpr int kOffset = 0;
  constexpr int kDataPipeCapacity = 64 * 1024;

  // Test value greater than kDefaultBufSize.
  constexpr size_t kSize = 40 * 1024;

  base::RunLoop run_loop;
  SetupDataPipe(kDataPipeCapacity);
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kDefaultBufSize,
      std::move(producer_stream_),
      base::BindLambdaForTesting([&run_loop](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  EXPECT_EQ(kSize, consumer_data_.size());
  EXPECT_EQ(std::string(test_data_.begin(), test_data_.begin() + kSize),
            std::string(consumer_data_.begin(), consumer_data_.end()));
}

TEST_F(ShareInfoFileStreamAdapterTest, ReadStreamAndWritePipeSmallCapacity) {
  constexpr int kOffset = 0;
  constexpr size_t kSize = 72 * 1024;
  // Pipe capacity is smaller than |kDefaultBufSize}, so the producer side needs
  // to wait for the consumer side to catch up.
  constexpr int kDataPipeCapacity = 16 * 1024;
  base::RunLoop run_loop;
  SetupDataPipe(kDataPipeCapacity);
  stream_adapter_ = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      file_system_context_, url_, kOffset, kSize, kDefaultBufSize,
      std::move(producer_stream_),
      base::BindLambdaForTesting([&run_loop](bool result) {
        EXPECT_TRUE(result);
        run_loop.Quit();
      }));
  stream_adapter_->StartRunnerForTesting();
  run_loop.Run();

  EXPECT_EQ(kSize, consumer_data_.size());
  EXPECT_EQ(std::string(test_data_.begin(), test_data_.begin() + kSize),
            std::string(consumer_data_.begin(), consumer_data_.end()));
}

}  // namespace arc
