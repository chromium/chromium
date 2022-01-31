// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for BoxUploadFileChunksHandler.

#include "chrome/browser/enterprise/connectors/file_system/box_upload_file_chunks_handler.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {
const size_t kChunkSize = BoxApiCallFlow::kChunkFileUploadMinSize;
using FileChunksHandler = BoxChunkedUploader::FileChunksHandler;

class BoxUploadFileChunksHandlerTest : public testing::Test {
 public:
  void Quit() {
    ASSERT_TRUE(quit_closure_);
    std::move(quit_closure_).Run();
  }

  void WriteTestFileAndRead(const std::string& content) {
    ASSERT_TRUE(base::WriteFile(file_path_, content)) << file_path_;
    InitializeReaderAndStart(content.size());
  }

  static size_t CalculateExpectedChunkReadCount(size_t content_length) {
    size_t expected_read_count = content_length / kChunkSize;
    if (content_length % kChunkSize != 0) {
      ++expected_read_count;
    }
    return expected_read_count;
  }

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().Append(file_name_);
  }

  void InitializeReaderAndStart(size_t file_size) {
    // |quit_closure_| must be set up before multi-threaded code. Otherwise, if
    // the code ran super quickly such that |quit_closure_| didn't get setup
    // until after the code already finished and ran the callback where Quit()
    // was called, |run_loop_|.Run() would just hang and time out while waiting.
    quit_closure_ = run_loop_.QuitClosure();
    chunked_reader_ =
        std::make_unique<FileChunksHandler>(file_path_, file_size, kChunkSize);
    StartReader();
    run_loop_.Run();
  }

  virtual void StartReader() {
    chunked_reader_->StartReading(
        base::BindRepeating(&BoxUploadFileChunksHandlerTest::OnFileChunkRead,
                            base::Unretained(this)),
        base::BindOnce(&BoxUploadFileChunksHandlerTest::OnFileCompletelyRead,
                       base::Unretained(this)));
  }

  void OnFileCompletelyRead(const std::string& sha1_digest) {
    file_finished_reading_ = true;
    file_successfully_read_ = !sha1_digest.empty();
    file_sha1_digest_ = sha1_digest;
    return Quit();
  }

  void OnFileChunkRead(BoxChunkedUploader::PartInfo part_info) {
    file_chunk_successfully_read_ = part_info.content.size();
    file_read_status_ = part_info.error;
    if (!file_chunk_successfully_read_) {
      LOG(INFO) << file_read_status_;
      return Quit();
    }

    file_content_.append(part_info.content);
    ++file_chunk_read_count_;

    chunked_reader_->ContinueToReadChunk(file_chunk_read_count_ + 1);
  }

  std::unique_ptr<FileChunksHandler> chunked_reader_;
  const base::FilePath file_name_{
      FILE_PATH_LITERAL("box_upload_file_chunk_handler_test.txt")};
  base::FilePath file_path_;
  std::string file_content_;

  base::File::Error file_read_status_ = base::File::Error::FILE_ERROR_MAX;
  bool file_chunk_successfully_read_ = false;
  bool file_finished_reading_ = false;
  bool file_successfully_read_ = false;
  size_t file_chunk_read_count_ = 0;
  std::string file_sha1_digest_;

 private:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  base::OnceClosure quit_closure_;
};

TEST_F(BoxUploadFileChunksHandlerTest, Test1Chunk) {
  std::string expected_file_content("abcdefghijklmn");

  WriteTestFileAndRead(expected_file_content);
  // Includes chunked_reader_->MakePartFileUploadApiCallFlow() and
  // OnPartUploaded() to arrive at OnFileCompletelyRead() and return.

  ASSERT_EQ(file_read_status_, base::File::Error::FILE_OK);
  ASSERT_TRUE(file_chunk_successfully_read_);
  ASSERT_TRUE(file_finished_reading_);
  EXPECT_TRUE(file_successfully_read_);
  EXPECT_EQ(file_chunk_read_count_, 1U);
  EXPECT_EQ(file_content_.size(), expected_file_content.size());
  ASSERT_EQ(file_content_, expected_file_content);

  ASSERT_TRUE(chunked_reader_);
  std::string expected_sha;
  base::Base64Encode(base::SHA1HashString(expected_file_content),
                     &expected_sha);
  ASSERT_EQ(file_sha1_digest_, expected_sha);
}

TEST_F(BoxUploadFileChunksHandlerTest, NoFile) {
  InitializeReaderAndStart(100);  // Without creating a test file to be opened.

  ASSERT_EQ(file_read_status_, base::File::Error::FILE_ERROR_NOT_FOUND);
  ASSERT_FALSE(file_finished_reading_);
  ASSERT_FALSE(file_successfully_read_);
  ASSERT_FALSE(file_chunk_successfully_read_);
}

class BoxUploadFileChunksHandler_MultipleChunksTest
    : public BoxUploadFileChunksHandlerTest,
      public testing::WithParamInterface<size_t> {};

TEST_P(BoxUploadFileChunksHandler_MultipleChunksTest,
       BigFileReadInMultipleChunks) {
  const size_t fill_size = GetParam() * 1024 * 1024;
  const size_t total_size = fill_size * 3;
  ASSERT_GT(total_size, kChunkSize) << "Adjust test files to be bigger?";

  std::string expected_file_content;
  expected_file_content.reserve(total_size);
  expected_file_content.resize(fill_size, 'a');
  expected_file_content += std::string(fill_size, 'b');
  expected_file_content += std::string(fill_size, 'c');

  const size_t expected_chunk_read_count =
      CalculateExpectedChunkReadCount(expected_file_content.size());
  WriteTestFileAndRead(expected_file_content);
  // Includes chunked_reader_->MakePartFileUploadApiCallFlow() and
  // OnPartUploaded() to arrive at OnFileCompletelyRead() and return.

  ASSERT_EQ(file_read_status_, base::File::Error::FILE_OK);
  ASSERT_TRUE(file_chunk_successfully_read_);
  ASSERT_TRUE(file_finished_reading_);
  EXPECT_TRUE(file_successfully_read_);
  EXPECT_EQ(file_chunk_read_count_, expected_chunk_read_count);
  EXPECT_EQ(file_content_.size(), expected_file_content.size());
  ASSERT_EQ(file_content_.compare(expected_file_content), 0);

  ASSERT_TRUE(chunked_reader_);
  std::string expected_sha;
  base::Base64Encode(base::SHA1HashString(expected_file_content),
                     &expected_sha);
  ASSERT_EQ(file_sha1_digest_, expected_sha);
}

INSTANTIATE_TEST_SUITE_P(BoxUploadFileChunksHandlerTest,
                         BoxUploadFileChunksHandler_MultipleChunksTest,
                         testing::Values(8, 11, 15));

class BoxUploadFileChunksHandler_FailureTest
    : public BoxUploadFileChunksHandlerTest {
 public:
  void StartReader() override {
    chunked_reader_->SkipToOnFileChunkReadForTesting(
        std::string(), /* bytes_read = */ -1,
        base::BindRepeating(
            &BoxUploadFileChunksHandler_FailureTest::OnFileChunkRead,
            base::Unretained(this)),
        base::BindOnce(
            &BoxUploadFileChunksHandler_FailureTest::OnFileCompletelyRead,
            base::Unretained(this)));
  }
};

TEST_F(BoxUploadFileChunksHandler_FailureTest, NegativeBytesRead) {
  WriteTestFileAndRead("BoxUploadFileChunksHandler_FailureTest");

  EXPECT_EQ(file_read_status_, base::File::Error::FILE_ERROR_IO);
  EXPECT_FALSE(file_finished_reading_);
  EXPECT_FALSE(file_successfully_read_);
  ASSERT_FALSE(file_chunk_successfully_read_);
}

}  // namespace enterprise_connectors
