// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/file_downloader.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

const char kURL[] = "https://www.url.com/path";
const char kFilename[] = "filename.ext";
const char kFileContents1[] = "file contents";
const char kFileContents2[] = "different contents";

class FileDownloaderTest : public testing::Test {
 public:
  FileDownloaderTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    path_ = dir_.GetPath().AppendASCII(kFilename);
    ASSERT_FALSE(base::PathExists(path_));
  }

  MOCK_METHOD1(OnDownloadFinished, void(FileDownloader::Result));

 protected:
  const base::FilePath& path() const { return path_; }

  void SetValidResponse() {
    test_url_loader_factory_.AddResponse(kURL, kFileContents1);
  }

  void SetValidResponse2() {
    test_url_loader_factory_.AddResponse(kURL, kFileContents2);
  }

  void SetFailedResponse() {
    test_url_loader_factory_.AddResponse(
        GURL(kURL), network::mojom::URLResponseHead::New(), std::string(),
        network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  }

  void Download(bool overwrite, FileDownloader::Result expected_result) {
    FileDownloader downloader(
        GURL(kURL), path_, overwrite, test_shared_loader_factory_,
        base::BindOnce(&FileDownloaderTest::OnDownloadFinished,
                       base::Unretained(this)),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_CALL(*this, OnDownloadFinished(expected_result));
    // Wait for the FileExists check to happen if necessary.
    content::RunAllTasksUntilIdle();
  }

 private:
  base::ScopedTempDir dir_;
  base::FilePath path_;

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(FileDownloaderTest, Success) {
  SetValidResponse();
  Download(true, FileDownloader::DOWNLOADED);
  EXPECT_TRUE(base::PathExists(path()));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path(), &contents));
  EXPECT_EQ(std::string(kFileContents1), contents);
}

TEST_F(FileDownloaderTest, Failure) {
  SetFailedResponse();
  Download(true, FileDownloader::FAILED);
  EXPECT_FALSE(base::PathExists(path()));
}

TEST_F(FileDownloaderTest, Overwrite) {
  SetValidResponse();
  Download(true, FileDownloader::DOWNLOADED);
  ASSERT_TRUE(base::PathExists(path()));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path(), &contents));
  ASSERT_EQ(std::string(kFileContents1), contents);

  SetValidResponse2();
  Download(true, FileDownloader::DOWNLOADED);
  // The file should have been overwritten with the new contents.
  EXPECT_TRUE(base::PathExists(path()));
  ASSERT_TRUE(base::ReadFileToString(path(), &contents));
  EXPECT_EQ(std::string(kFileContents2), contents);
}

TEST_F(FileDownloaderTest, DontOverwrite) {
  SetValidResponse();
  Download(true, FileDownloader::DOWNLOADED);
  ASSERT_TRUE(base::PathExists(path()));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path(), &contents));
  EXPECT_EQ(std::string(kFileContents1), contents);

  SetValidResponse2();
  Download(false, FileDownloader::EXISTS);
  // The file should still have the old contents.
  EXPECT_TRUE(base::PathExists(path()));
  ASSERT_TRUE(base::ReadFileToString(path(), &contents));
  EXPECT_EQ(std::string(kFileContents1), contents);
}
