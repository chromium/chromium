// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_file_stream_reader.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_file_system_instance.h"
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

constexpr char kData[] = "abcdefghijklmnopqrstuvwxyz";

constexpr char kMimeType[] = "application/octet-stream";

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

  ~ArcContentFileSystemFileStreamReaderTest() override = default;

  void SetUp() override {
    fake_file_system_.AddFile(
        File(kArcUrlFile, kData, kMimeType, File::Seekable::YES));
    fake_file_system_.AddFile(
        File(kArcUrlPipe, kData, kMimeType, File::Seekable::NO));

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
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  FakeFileSystemInstance fake_file_system_;

  // Use the same initialization/destruction order as
  // ChromeBrowserMainPartsChromeos.
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(ArcContentFileSystemFileStreamReaderTest);
};

}  // namespace

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadRegularFile) {
  ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlFile), 0);
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(strlen(kData));
  EXPECT_TRUE(ReadData(&reader, buffer.get()));
  EXPECT_EQ(base::StringPiece(kData, strlen(kData)),
            base::StringPiece(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadRegularFileWithOffset) {
  constexpr size_t kOffset = 10;
  ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlFile), kOffset);
  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(strlen(kData) - kOffset);
  EXPECT_TRUE(ReadData(&reader, buffer.get()));
  EXPECT_EQ(base::StringPiece(kData + kOffset, strlen(kData) - kOffset),
            base::StringPiece(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadPipe) {
  ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlPipe), 0);
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(strlen(kData));
  EXPECT_TRUE(ReadData(&reader, buffer.get()));
  EXPECT_EQ(base::StringPiece(kData, strlen(kData)),
            base::StringPiece(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadPipeWithOffset) {
  constexpr size_t kOffset = 10;
  ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlPipe), kOffset);
  auto buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(strlen(kData) - kOffset);
  EXPECT_TRUE(ReadData(&reader, buffer.get()));
  EXPECT_EQ(base::StringPiece(kData + kOffset, strlen(kData) - kOffset),
            base::StringPiece(buffer->data(), buffer->size()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, GetLength) {
  ArcContentFileSystemFileStreamReader reader(GURL(kArcUrlFile), 0);

  net::TestInt64CompletionCallback callback;
  EXPECT_EQ(static_cast<int64_t>(strlen(kData)),
            callback.GetResult(reader.GetLength(callback.callback())));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, ReadError) {
  ArcContentFileSystemFileStreamReader reader(
      GURL("content://org.chromium.foo/error"), 0);
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(strlen(kData));
  EXPECT_FALSE(ReadData(&reader, buffer.get()));
}

TEST_F(ArcContentFileSystemFileStreamReaderTest, GetLengthError) {
  ArcContentFileSystemFileStreamReader reader(
      GURL("content://org.chromium.foo/error"), 0);
  net::TestInt64CompletionCallback callback;
  EXPECT_LT(callback.GetResult(reader.GetLength(callback.callback())), 0);
}

}  // namespace arc
