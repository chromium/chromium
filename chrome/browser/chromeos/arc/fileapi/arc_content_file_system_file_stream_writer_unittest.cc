// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_file_stream_writer.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using File = arc::FakeFileSystemInstance::File;

namespace arc {

namespace {

constexpr char kArcUrlPrefix[] = "content://org.chromium.foo/";

std::unique_ptr<KeyedService> CreateFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return ArcFileSystemOperationRunner::CreateForTesting(
      context, ArcServiceManager::Get()->arc_bridge_service());
}

class ArcContentFileSystemFileStreamWriterTest : public testing::Test {
 public:
  ArcContentFileSystemFileStreamWriterTest() = default;

  ~ArcContentFileSystemFileStreamWriterTest() override = default;

  void SetUp() override {
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

 protected:
  std::string ArcUrl(const std::string& name) { return kArcUrlPrefix + name; }

  int WriteStringToWriter(ArcContentFileSystemFileStreamWriter* writer,
                          const std::string& data) {
    scoped_refptr<net::StringIOBuffer> buffer =
        base::MakeRefCounted<net::StringIOBuffer>(data);
    scoped_refptr<net::DrainableIOBuffer> drainable =
        base::MakeRefCounted<net::DrainableIOBuffer>(std::move(buffer),
                                                     data.size());

    while (drainable->BytesRemaining() > 0) {
      net::TestCompletionCallback callback;
      int result = writer->Write(drainable.get(), drainable->BytesRemaining(),
                                 callback.callback());
      if (result == net::ERR_IO_PENDING)
        result = callback.WaitForResult();
      if (result <= 0)
        return result;
      drainable->DidConsume(result);
    }
    return net::OK;
  }

  std::string GetFileContent(const std::string& url) {
    return fake_file_system_.GetFileContent(url);
  }

  std::string GetFileContent(const std::string& url, size_t bytes) {
    return fake_file_system_.GetFileContent(url, bytes);
  }

  std::string CreateFileWithContent(const std::string& name,
                                    const std::string& data,
                                    bool seekable) {
    std::string file_url = ArcUrl(name);
    fake_file_system_.AddFile(
        File(ArcUrl(name), data, "application/octet-stream",
             seekable ? File::Seekable::YES : File::Seekable::NO));
    return file_url;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  FakeFileSystemInstance fake_file_system_;

  // Use the same initialization/destruction order as
  // ChromeBrowserMainPartsChromeos.
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(ArcContentFileSystemFileStreamWriterTest);
};

void NeverCalled(int unused) {
  ADD_FAILURE();
}

}  // namespace

TEST_F(ArcContentFileSystemFileStreamWriterTest, Write) {
  std::string url =
      CreateFileWithContent("file_a", std::string(), true /* seekable */);
  ArcContentFileSystemFileStreamWriter writer(GURL(url), 0);
  EXPECT_EQ(net::OK, WriteStringToWriter(&writer, "foo"));
  EXPECT_EQ(net::OK, WriteStringToWriter(&writer, "bar"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("foobar", GetFileContent(url));
}

TEST_F(ArcContentFileSystemFileStreamWriterTest, WriteMiddle) {
  std::string url =
      CreateFileWithContent("file_a", "foobar", true /* seekable */);
  ArcContentFileSystemFileStreamWriter writer(GURL(url), 2);
  EXPECT_EQ(net::OK, WriteStringToWriter(&writer, "xxx"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("foxxxr", GetFileContent(url));
}

TEST_F(ArcContentFileSystemFileStreamWriterTest, WriteEnd) {
  std::string url =
      CreateFileWithContent("file_a", "foobar", true /* seekable */);
  ArcContentFileSystemFileStreamWriter writer(GURL(url), 6);
  EXPECT_EQ(net::OK, WriteStringToWriter(&writer, "xxx"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("foobarxxx", GetFileContent(url));
}

TEST_F(ArcContentFileSystemFileStreamWriterTest, WriteFailForNonexistingFile) {
  std::string url = ArcUrl("file_a");
  ArcContentFileSystemFileStreamWriter writer(GURL(url), 0);
  EXPECT_EQ(net::ERR_FAILED, WriteStringToWriter(&writer, "foo"));
}

TEST_F(ArcContentFileSystemFileStreamWriterTest, WriteNonSeekable) {
  std::string url =
      CreateFileWithContent("file_a", std::string(), false /* not seekable */);
  ArcContentFileSystemFileStreamWriter writer(GURL(url), 0);
  EXPECT_EQ(net::OK, WriteStringToWriter(&writer, "foo"));
  EXPECT_EQ(net::OK, WriteStringToWriter(&writer, "bar"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("foobar", GetFileContent(url, 6));
}

TEST_F(ArcContentFileSystemFileStreamWriterTest, WriteNonSeekableFailForSeek) {
  std::string url =
      CreateFileWithContent("file_a", "foobar", false /* not seekable */);
  ArcContentFileSystemFileStreamWriter writer(GURL(url), 2);
  EXPECT_EQ(net::ERR_FAILED, WriteStringToWriter(&writer, "xxx"));
}

TEST_F(ArcContentFileSystemFileStreamWriterTest, CancelBeforeOperation) {
  std::string url =
      CreateFileWithContent("file_a", std::string(), true /* seekable */);
  ArcContentFileSystemFileStreamWriter writer(GURL(url), 0);
  // Cancel immediately fails when there's no in-flight operation.
  int cancel_result = writer.Cancel(base::BindOnce(&NeverCalled));
  EXPECT_EQ(net::ERR_UNEXPECTED, cancel_result);
}

TEST_F(ArcContentFileSystemFileStreamWriterTest, CancelAfterFinishedOperation) {
  std::string url =
      CreateFileWithContent("file_a", std::string(), true /* seekable */);
  ArcContentFileSystemFileStreamWriter writer(GURL(url), 0);
  EXPECT_EQ(net::OK, WriteStringToWriter(&writer, "foo"));

  // Cancel immediately fails when there's no in-flight operation.
  int cancel_result = writer.Cancel(base::BindOnce(&NeverCalled));
  EXPECT_EQ(net::ERR_UNEXPECTED, cancel_result);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("foo", GetFileContent(url, 6));
}

TEST_F(ArcContentFileSystemFileStreamWriterTest, CancelWrite) {
  std::string url =
      CreateFileWithContent("file_a", "foobar", true /* seekable */);
  ArcContentFileSystemFileStreamWriter writer(GURL(url), 0);

  scoped_refptr<net::StringIOBuffer> buffer(
      base::MakeRefCounted<net::StringIOBuffer>("xxx"));
  int result =
      writer.Write(buffer.get(), buffer->size(), base::BindOnce(&NeverCalled));
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  net::TestCompletionCallback callback;
  writer.Cancel(callback.callback());
  int cancel_result = callback.WaitForResult();
  EXPECT_EQ(net::OK, cancel_result);
}

}  // namespace arc
