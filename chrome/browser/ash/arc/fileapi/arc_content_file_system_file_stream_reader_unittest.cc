// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_file_stream_reader.h"

#include <string.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_size_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using File = arc::FakeFileSystemInstance::File;

namespace arc {

namespace {

// URL which returns a file descriptor of a regular file.
constexpr char kArcUrlFile[] = "content://org.chromium.foo/file";

// URL which returns a file descriptor of a pipe's read end.
constexpr char kArcUrlPipe[] = "content://org.chromium.foo/pipe";

// URL which returns a file descriptor of a regular file with unknown size.
constexpr char kArcUrlFileUnknownSize[] =
    "content://org.chromium.foo/file-unknown-size";

// URL which returns a file descriptor of a pipe's read end with unknown size.
constexpr char kArcUrlPipeUnknownSize[] =
    "content://org.chromium.foo/pipe-unknown-size";

constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";

constexpr char kMimeType[] = "application/octet-stream";

// Non-zero read offsets used in unit tests.
constexpr size_t kOffset5 = 5;
constexpr size_t kOffset10 = 10;
constexpr size_t kOffset15 = 15;

// Reads data from the reader to fill the buffer.
bool ReadData(ArcContentFileSystemFileStreamReader* reader,
              net::IOBufferWithSize* buffer) {
  auto drainable_buffer =
      base::MakeRefCounted<net::DrainableIOBuffer>(buffer, buffer->size());
  while (drainable_buffer->BytesRemaining()) {
    net::TestCompletionCallback callback;
    int result = callback.GetResult(
        reader->Read(drainable_buffer.get(), drainable_buffer->BytesRemaining(),
                     callback.callback()));
    if (result < 0) {
      LOG(ERROR) << "Read failed: " << result;
      return false;
    }
    drainable_buffer->DidConsume(result);
  }
  return true;
}

std::unique_ptr<KeyedService> CreateFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return ArcFileSystemOperationRunner::CreateForTesting(
      context, ArcServiceManager::Get()->arc_bridge_service());
}

class ArcContentFileSystemFileStreamReaderTest : public testing::Test {
 public:
  ArcContentFileSystemFileStreamReaderTest() = default;

  ArcContentFileSystemFileStreamReaderTest(
      const ArcContentFileSystemFileStreamReaderTest&) = delete;
  ArcContentFileSystemFileStreamReaderTest& operator=(
      const ArcContentFileSystemFileStreamReaderTest&) = delete;

  ~ArcContentFileSystemFileStreamReaderTest() override = default;

  void SetUp() override {
    fake_file_system_.AddFile(
        File(kArcUrlFile, kData, kMimeType, File::Seekable::YES));
    fake_file_system_.AddFile(
        File(kArcUrlPipe, kData, kMimeType, File::Seekable::NO));
    fake_file_system_.AddFile(File(kArcUrlFileUnknownSize, kData, kMimeType,
                                   File::Seekable::YES, -1));
    fake_file_system_.AddFile(
        File(kArcUrlPipeUnknownSize, kData, kMimeType, File::Seekable::NO, -1));

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    profile_ = std::make_unique<TestingProfile>();
    arc_service_manager_->set_browser_context(profile_.get());
    ArcFileSystemOperationRunner::GetFactory()->SetTestingFactoryAndUse(
        profile_.get(),
        base::BindRepeating(&CreateFileSystemOperationRunnerForTesting));
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &fake_file_system_);
    WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());
  }

  void TearDown() override {
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);
    arc_service_manager_->set_browser_context(nullptr);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  FakeFileSystemInstance fake_file_system_;

  // Use the same initialization/destruction order as
  // `ChromeBrowserMainPartsAsh`.
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;
};

}  // namespace

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadRegularFile) {
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(strlen(kData));
  {
    ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlFile),
                                                /*offset=*/0);
    EXPECT_TRUE(ReadData(&reader, buffer.get()));
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string_view(kData, strlen(kData)),
            std::string_view(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadRegularFileWithOffset) {
  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(strlen(kData) - kOffset10);
  {
    ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlFile), kOffset10);
    EXPECT_TRUE(ReadData(&reader, buffer.get()));
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string_view(kData + kOffset10, strlen(kData) - kOffset10),
            std::string_view(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadRegularFileWithOffsets) {
  auto buffer1 =
      base::MakeRefCounted<net::IOBufferWithSize>(kOffset15 - kOffset5);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(
      strlen(kData) - kOffset5 - kOffset15);
  {
    ArcContentFileSystemFileStreamReader reader1(GURL(kArcUrlFile), kOffset5);
    EXPECT_TRUE(ReadData(&reader1, buffer1.get()));
    ArcContentFileSystemFileStreamReader reader2(GURL(kArcUrlFile), kOffset15);
    EXPECT_TRUE(ReadData(&reader2, buffer2.get()));
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string_view(kData + kOffset5, kOffset15 - kOffset5),
            std::string_view(buffer1->data(), buffer1->size()));
  EXPECT_EQ(
      std::string_view(kData + kOffset15, strlen(kData) - kOffset5 - kOffset15),
      std::string_view(buffer2->data(), buffer2->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest,
       ReadRegularFileWithOffsets_CloseWithWait) {
  auto buffer1 =
      base::MakeRefCounted<net::IOBufferWithSize>(kOffset15 - kOffset5);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(
      strlen(kData) - kOffset5 - kOffset15);
  {
    ArcContentFileSystemFileStreamReader reader1(GURL(kArcUrlFile), kOffset5);
    EXPECT_TRUE(ReadData(&reader1, buffer1.get()));
  }
  base::RunLoop().RunUntilIdle();
  {
    ArcContentFileSystemFileStreamReader reader2(GURL(kArcUrlFile), kOffset15);
    EXPECT_TRUE(ReadData(&reader2, buffer2.get()));
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string_view(kData + kOffset5, kOffset15 - kOffset5),
            std::string_view(buffer1->data(), buffer1->size()));
  EXPECT_EQ(
      std::string_view(kData + kOffset15, strlen(kData) - kOffset5 - kOffset15),
      std::string_view(buffer2->data(), buffer2->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadPipe) {
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(strlen(kData));
  {
    ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlPipe),
                                                /*offset=*/0);
    EXPECT_TRUE(ReadData(&reader, buffer.get()));
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string_view(kData, strlen(kData)),
            std::string_view(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadPipeWithOffset) {
  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(strlen(kData) - kOffset10);
  {
    ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlPipe), kOffset10);
    EXPECT_TRUE(ReadData(&reader, buffer.get()));
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string_view(kData + kOffset10, strlen(kData) - kOffset10),
            std::string_view(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadPipeWithOffsets) {
  auto buffer1 =
      base::MakeRefCounted<net::IOBufferWithSize>(kOffset15 - kOffset5);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(
      strlen(kData) - kOffset5 - kOffset15);
  {
    ArcContentFileSystemFileStreamReader reader1(GURL(kArcUrlPipe), kOffset5);
    EXPECT_TRUE(ReadData(&reader1, buffer1.get()));
    ArcContentFileSystemFileStreamReader reader2(GURL(kArcUrlPipe), kOffset15);
    EXPECT_TRUE(ReadData(&reader2, buffer2.get()));
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string_view(kData + kOffset5, kOffset15 - kOffset5),
            std::string_view(buffer1->data(), buffer1->size()));
  EXPECT_EQ(
      std::string_view(kData + kOffset15, strlen(kData) - kOffset5 - kOffset15),
      std::string_view(buffer2->data(), buffer2->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest,
       ReadPipeWithOffsets_CloseWithoutWait) {
  auto buffer1 =
      base::MakeRefCounted<net::IOBufferWithSize>(kOffset15 - kOffset5);
  auto buffer2 = base::MakeRefCounted<net::IOBufferWithSize>(
      strlen(kData) - kOffset5 - kOffset15);
  {
    ArcContentFileSystemFileStreamReader reader1(GURL(kArcUrlPipe), kOffset5);
    EXPECT_TRUE(ReadData(&reader1, buffer1.get()));
  }
  {
    ArcContentFileSystemFileStreamReader reader2(GURL(kArcUrlPipe), kOffset15);
    EXPECT_TRUE(ReadData(&reader2, buffer2.get()));
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string_view(kData + kOffset5, kOffset15 - kOffset5),
            std::string_view(buffer1->data(), buffer1->size()));
  EXPECT_EQ(
      std::string_view(kData + kOffset15, strlen(kData) - kOffset5 - kOffset15),
      std::string_view(buffer2->data(), buffer2->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, GetLength) {
  {
    ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlFile),
                                                /*offset=*/0);

    net::TestInt64CompletionCallback callback;
    EXPECT_EQ(static_cast<int64_t>(strlen(kData)),
              callback.GetResult(reader.GetLength(callback.callback())));
  }
  base::RunLoop().RunUntilIdle();
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, GetLengthUnknownSizeSeekable) {
  {
    ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlFileUnknownSize),
                                                /*offset=*/0);

    net::TestInt64CompletionCallback callback;
    EXPECT_EQ(static_cast<int64_t>(strlen(kData)),
              callback.GetResult(reader.GetLength(callback.callback())));
  }
  base::RunLoop().RunUntilIdle();
}

TEST_F(ArcContentFileSystemFileStreamReaderTest,
       GetLengthUnknownSizeNotSeekable) {
  {
    ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlPipeUnknownSize),
                                                /*offset=*/0);

    net::TestInt64CompletionCallback callback;
    EXPECT_EQ(static_cast<int64_t>(net::ERR_FAILED),
              callback.GetResult(reader.GetLength(callback.callback())));
  }
  base::RunLoop().RunUntilIdle();
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadError) {
  {
    ArcContentFileSystemFileStreamReader reader(
        GURL("content://org.chromium.foo/error"), /*offset=*/0);
    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(strlen(kData));
    EXPECT_FALSE(ReadData(&reader, buffer.get()));
  }
  base::RunLoop().RunUntilIdle();
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, GetLengthError) {
  {
    ArcContentFileSystemFileStreamReader reader(
        GURL("content://org.chromium.foo/error"), /*offset=*/0);
    net::TestInt64CompletionCallback callback;
    EXPECT_LT(callback.GetResult(reader.GetLength(callback.callback())), 0);
  }
  base::RunLoop().RunUntilIdle();
}

}  // namespace arc
