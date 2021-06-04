// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_data_pipe_getter.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class MultipartDataPipeGetterTest : public testing::Test {
 public:
  base::File CreateFile(const std::string& content) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = temp_dir_.GetPath().AppendASCII("test.txt");
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_READ |
                              base::File::FLAG_WRITE);
    file.WriteAtCurrentPos(content.data(), content.size());
    return file;
  }

  std::string GetBodyFromPipe(MultipartDataPipeGetter* data_pipe_getter,
                              size_t expected_size,
                              size_t max_chunks = 0) {
    mojo::ScopedDataPipeProducerHandle data_pipe_producer;

    base::RunLoop run_loop;
    EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, data_pipe_producer,
                                                   data_pipe_consumer_));
    data_pipe_getter->Read(
        std::move(data_pipe_producer),
        base::BindLambdaForTesting(
            [&run_loop, expected_size](int32_t status, uint64_t size) {
              EXPECT_EQ(net::OK, status);
              EXPECT_EQ(expected_size, size);
              run_loop.Quit();
            }));
    run_loop.Run();

    EXPECT_TRUE(data_pipe_consumer_.is_valid());
    std::string body;
    body.reserve(expected_size);
    size_t read_chunks = 0;
    while (true) {
      char buffer[1024];
      uint32_t read_size = sizeof(buffer);
      MojoResult result = data_pipe_consumer_->ReadData(
          buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        base::RunLoop().RunUntilIdle();
        continue;
      }
      if (result != MOJO_RESULT_OK) {
        break;
      }
      body.append(buffer, read_size);
      ++read_chunks;
      if (max_chunks != 0 && read_chunks == max_chunks)
        break;
    }

    return body;
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer_;
};

TEST_F(MultipartDataPipeGetterTest, InvalidFile) {
  ASSERT_EQ(nullptr, MultipartDataPipeGetter::Create("boundary", "metadata",
                                                     base::File()));
}

TEST_F(MultipartDataPipeGetterTest, SmallFile) {
  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "metadata\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "small file content\r\n"
      "--boundary--\r\n";

  base::File file = CreateFile("small file content");
  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter =
      MultipartDataPipeGetter::Create("boundary", "metadata", std::move(file));

  ASSERT_EQ(expected_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
}

TEST_F(MultipartDataPipeGetterTest, LargeFile) {
  std::string large_file_content = std::string(100 * 1024 * 1024, 'a');
  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "metadata\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n" +
      large_file_content +
      "\r\n"
      "--boundary--\r\n";

  base::File file = CreateFile(large_file_content);
  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter =
      MultipartDataPipeGetter::Create("boundary", "metadata", std::move(file));

  ASSERT_EQ(expected_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
}

TEST_F(MultipartDataPipeGetterTest, LargeFileAndMetadata) {
  std::string large_data = std::string(100 * 1024 * 1024, 'a');
  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n" +
      large_data +
      "\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n" +
      large_data +
      "\r\n"
      "--boundary--\r\n";

  base::File file = CreateFile(large_data);
  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter =
      MultipartDataPipeGetter::Create("boundary", large_data, std::move(file));

  ASSERT_EQ(expected_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
}

TEST_F(MultipartDataPipeGetterTest, MultipleReads) {
  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "metadata\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "small file content\r\n"
      "--boundary--\r\n";

  base::File file = CreateFile("small file content");
  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter =
      MultipartDataPipeGetter::Create("boundary", "metadata", std::move(file));

  for (int i = 0; i < 4; ++i) {
    ASSERT_EQ(expected_body,
              GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
  }
}

TEST_F(MultipartDataPipeGetterTest, ResetsCorrectly) {
  std::string large_file_content = std::string(100 * 1024 * 1024, 'a');
  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "metadata\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n" +
      large_file_content +
      "\r\n"
      "--boundary--\r\n";

  base::File file = CreateFile(large_file_content);
  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter =
      MultipartDataPipeGetter::Create("boundary", "metadata", std::move(file));

  // Reads part of the body, which validates that the next read is able to read
  // the entire body correctly after a reset.
  std::string partial_body = expected_body.substr(0, 5 * 1024);
  ASSERT_EQ(partial_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size(),
                            /*max_chunks*/ 5));

  data_pipe_getter->Reset();

  ASSERT_EQ(expected_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
}

}  // namespace safe_browsing
