// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_data_pipe_getter.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class MultipartDataPipeGetterTest : public testing::Test {
 public:
  absl::optional<base::File> CreateFile(const std::string& content) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = temp_dir_.GetPath().AppendASCII("test.txt");
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_READ |
                              base::File::FLAG_WRITE);
    if (!file.IsValid())
      return absl::nullopt;

    if (file.WriteAtCurrentPos(content.data(), content.size()) < 0)
      return absl::nullopt;

    return file;
  }

  base::ReadOnlySharedMemoryRegion CreatePage(const std::string& content) {
    base::MappedReadOnlyRegion region =
        base::ReadOnlySharedMemoryRegion::Create(content.size());
    if (!region.IsValid())
      return base::ReadOnlySharedMemoryRegion();

    std::memcpy(region.mapping.memory(), content.data(), content.size());
    return std::move(region.region);
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

TEST_F(MultipartDataPipeGetterTest, InvalidPage) {
  ASSERT_EQ(nullptr,
            MultipartDataPipeGetter::Create(
                "boundary", "metadata", base::ReadOnlySharedMemoryRegion()));
}

// Parametrization to share tests between the file and page implementations.
class MultipartDataPipeGetterParametrizedTest
    : public MultipartDataPipeGetterTest,
      public testing::WithParamInterface<bool> {
 public:
  bool is_file_data_pipe() { return GetParam(); }
  bool is_page_data_pipe() { return !GetParam(); }

  // Helper to create a data pipe with its content either in memory or in a
  // files. If there is no space left on the device, return nullptr so the test
  // can end early.
  std::unique_ptr<MultipartDataPipeGetter> CreateDataPipeGetter(
      const std::string& content) {
    if (is_file_data_pipe()) {
      absl::optional<base::File> file = CreateFile(content);
      if (!file)
        return nullptr;

      return MultipartDataPipeGetter::Create("boundary", metadata_,
                                             std::move(*file));
    } else {
      base::ReadOnlySharedMemoryRegion page = CreatePage(content);
      if (!page.IsValid())
        return nullptr;

      return MultipartDataPipeGetter::Create("boundary", metadata_,
                                             std::move(page));
    }
  }

  void set_metadata(const std::string& metadata) { metadata_ = metadata; }

 private:
  std::string metadata_ = "metadata";
};

INSTANTIATE_TEST_SUITE_P(,
                         MultipartDataPipeGetterParametrizedTest,
                         testing::Bool());

TEST_P(MultipartDataPipeGetterParametrizedTest, SmallFile) {
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

  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter("small file content");
  EXPECT_TRUE(data_pipe_getter);
  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());

  ASSERT_EQ(expected_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
}

TEST_P(MultipartDataPipeGetterParametrizedTest, LargeFile) {
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

  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter(large_file_content);
  // It's possible the large file couldn't be created due to a lack of space on
  // the device, in this case stop the test early.
  if (!data_pipe_getter)
    return;

  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());

  ASSERT_EQ(expected_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
}

TEST_P(MultipartDataPipeGetterParametrizedTest, LargeFileAndMetadata) {
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

  set_metadata(large_data);
  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter(large_data);
  // It's possible the large file couldn't be created due to a lack of space on
  // the device, in this case stop the test early.
  if (!data_pipe_getter)
    return;

  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());

  ASSERT_EQ(expected_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
}

TEST_P(MultipartDataPipeGetterParametrizedTest, MultipleReads) {
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

  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter("small file content");
  EXPECT_TRUE(data_pipe_getter);
  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());

  for (int i = 0; i < 4; ++i) {
    ASSERT_EQ(expected_body,
              GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
  }
}

TEST_P(MultipartDataPipeGetterParametrizedTest, ResetsCorrectly) {
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

  std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter(large_file_content);
  // It's possible the large file couldn't be created due to a lack of space on
  // the device, in this case stop the test early.
  if (!data_pipe_getter)
    return;

  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());

  // Reads part of the body, which validates that the next read is able to read
  // the entire body correctly after a reset.
  std::string partial_body = expected_body.substr(0, 5 * 1024);
  ASSERT_EQ(partial_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size(),
                            /*max_chunks*/ 5));

  data_pipe_getter->Reset();

  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());
  ASSERT_EQ(expected_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
}

}  // namespace safe_browsing
