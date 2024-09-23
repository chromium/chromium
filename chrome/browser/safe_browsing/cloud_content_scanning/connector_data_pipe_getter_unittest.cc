// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/connector_data_pipe_getter.h"

#include <memory>

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/rand_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr size_t kDataChunkSize = 524288;  // default download buffer size

// Helper function to divide data in obfuscated chunks.
void ObfuscateContentInChunks(const std::vector<uint8_t>& input,
                              std::string& output) {
  enterprise_obfuscation::DownloadObfuscator obfuscator;

  size_t offset = 0;

  while (offset < input.size()) {
    size_t chunk_size = std::min(kDataChunkSize, input.size() - offset);
    bool is_last_chunk = (offset + chunk_size == input.size());

    auto result = obfuscator.ObfuscateChunk(
        base::span(input).subspan(offset, chunk_size), is_last_chunk);

    ASSERT_TRUE(result.has_value());

    output.insert(output.end(), result->begin(), result->end());
    offset += chunk_size;
  }
}
}  // namespace

class ConnectorDataPipeGetterTest : public testing::Test {
 public:
  std::optional<base::File> CreateFile(const std::string& content) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = temp_dir_.GetPath().AppendASCII("test.txt");
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_READ |
                              base::File::FLAG_WRITE);
    if (!file.IsValid()) {
      return std::nullopt;
    }
    if (!file.WriteAtCurrentPosAndCheck(base::as_byte_span(content))) {
      return std::nullopt;
    }
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

  std::string GetBodyFromPipe(ConnectorDataPipeGetter* data_pipe_getter,
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
      std::string buffer(1024, '\0');
      size_t read_size = 0;
      MojoResult result = data_pipe_consumer_->ReadData(
          MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
          read_size);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        base::RunLoop().RunUntilIdle();
        continue;
      }
      if (result != MOJO_RESULT_OK) {
        break;
      }
      body.append(std::string_view(buffer).substr(0, read_size));
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

TEST_F(ConnectorDataPipeGetterTest, InvalidFile) {
  ASSERT_EQ(nullptr, ConnectorDataPipeGetter::CreateMultipartPipeGetter(
                         "boundary", "metadata", base::File()));
  ASSERT_EQ(nullptr,
            ConnectorDataPipeGetter::CreateResumablePipeGetter(base::File()));
}

TEST_F(ConnectorDataPipeGetterTest, InvalidPage) {
  ASSERT_EQ(nullptr,
            ConnectorDataPipeGetter::CreateMultipartPipeGetter(
                "boundary", "metadata", base::ReadOnlySharedMemoryRegion()));
  ASSERT_EQ(nullptr, ConnectorDataPipeGetter::CreateResumablePipeGetter(
                         base::ReadOnlySharedMemoryRegion()));
}

// Parametrization to share tests between:
// 1. the file and page implementations.
// 2. multipart and resumable upload.
class ConnectorDataPipeGetterParametrizedTest
    : public ConnectorDataPipeGetterTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  bool is_file_data_pipe() { return std::get<0>(GetParam()); }
  bool is_page_data_pipe() { return !std::get<0>(GetParam()); }
  bool is_resumable_upload() { return std::get<1>(GetParam()); }

  // Helper to create a data pipe with its content either in memory or in a
  // files. If there is no space left on the device, return nullptr so the test
  // can end early.
  std::unique_ptr<ConnectorDataPipeGetter> CreateDataPipeGetter(
      const std::string& content) {
    if (is_file_data_pipe()) {
      std::optional<base::File> file = CreateFile(content);
      if (!file)
        return nullptr;

      return is_resumable_upload()
                 ? ConnectorDataPipeGetter::CreateResumablePipeGetter(
                       std::move(*file))
                 : ConnectorDataPipeGetter::CreateMultipartPipeGetter(
                       "boundary", metadata_, std::move(*file));
    } else {
      base::ReadOnlySharedMemoryRegion page = CreatePage(content);
      if (!page.IsValid())
        return nullptr;

      return is_resumable_upload()
                 ? ConnectorDataPipeGetter::CreateResumablePipeGetter(
                       std::move(page))
                 : ConnectorDataPipeGetter::CreateMultipartPipeGetter(
                       "boundary", metadata_, std::move(page));
    }
  }

  void set_metadata(const std::string& metadata) { metadata_ = metadata; }

 private:
  std::string metadata_ = "metadata";
};

INSTANTIATE_TEST_SUITE_P(,
                         ConnectorDataPipeGetterParametrizedTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(ConnectorDataPipeGetterParametrizedTest, SmallFile) {
  std::string small_file_content = "small file content";
  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "metadata\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n" +
      small_file_content +
      "\r\n"
      "--boundary--\r\n";

  std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter(small_file_content);
  EXPECT_TRUE(data_pipe_getter);
  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());

  if (is_resumable_upload()) {
    ASSERT_EQ(small_file_content, GetBodyFromPipe(data_pipe_getter.get(),
                                                  small_file_content.size()));
  } else {
    ASSERT_EQ(expected_body,
              GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
  }
}

TEST_P(ConnectorDataPipeGetterParametrizedTest, LargeFile) {
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

  std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter(large_file_content);
  // It's possible the large file couldn't be created due to a lack of space
  // on the device, in this case stop the test early.
  if (!data_pipe_getter)
    return;

  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());

  if (is_resumable_upload()) {
    ASSERT_EQ(large_file_content, GetBodyFromPipe(data_pipe_getter.get(),
                                                  large_file_content.size()));
  } else {
    ASSERT_EQ(expected_body,
              GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
  }
}

TEST_P(ConnectorDataPipeGetterParametrizedTest, LargeFileAndMetadata) {
  if (is_resumable_upload()) {
    // Terminates early since this testcase is the same as the LargeFile one for
    // resumable upload.
    return;
  }
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
  std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter(large_data);
  // It's possible the large file couldn't be created due to a lack of space
  // on the device, in this case stop the test early.
  if (!data_pipe_getter)
    return;

  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());

  ASSERT_EQ(expected_body,
            GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
}

TEST_P(ConnectorDataPipeGetterParametrizedTest, MultipleReads) {
  std::string small_file_content = "small file content";
  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "metadata\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n" +
      small_file_content +
      "\r\n"
      "--boundary--\r\n";

  std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter(small_file_content);
  EXPECT_TRUE(data_pipe_getter);
  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());

  for (int i = 0; i < 4; ++i) {
    if (is_resumable_upload()) {
      ASSERT_EQ(small_file_content, GetBodyFromPipe(data_pipe_getter.get(),
                                                    small_file_content.size()));
    } else {
      ASSERT_EQ(expected_body,
                GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
    }
  }
}

TEST_P(ConnectorDataPipeGetterParametrizedTest, ResetsCorrectly) {
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

  std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter(large_file_content);
  // It's possible the large file couldn't be created due to a lack of space on
  // the device, in this case stop the test early.
  if (!data_pipe_getter)
    return;

  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());

  // Reads part of the body, which validates that the next read is able to read
  // the entire body correctly after a reset.
  if (is_resumable_upload()) {
    std::string partial_content = large_file_content.substr(0, 5 * 1024);
    ASSERT_EQ(partial_content,
              GetBodyFromPipe(data_pipe_getter.get(), large_file_content.size(),
                              /*max_chunks*/ 5));
  } else {
    std::string partial_body = expected_body.substr(0, 5 * 1024);
    ASSERT_EQ(partial_body,
              GetBodyFromPipe(data_pipe_getter.get(), expected_body.size(),
                              /*max_chunks*/ 5));
  }

  data_pipe_getter->Reset();

  EXPECT_EQ(data_pipe_getter->is_page_data_pipe(), is_page_data_pipe());
  EXPECT_EQ(data_pipe_getter->is_file_data_pipe(), is_file_data_pipe());
  if (is_resumable_upload()) {
    ASSERT_EQ(large_file_content, GetBodyFromPipe(data_pipe_getter.get(),
                                                  large_file_content.size()));
  } else {
    ASSERT_EQ(expected_body,
              GetBodyFromPipe(data_pipe_getter.get(), expected_body.size()));
  }
}

TEST_P(ConnectorDataPipeGetterParametrizedTest, DeobfuscationTest) {
  if (!is_file_data_pipe() || !is_resumable_upload()) {
    // This test only applies to file-based resumable uploads.
    return;
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      enterprise_obfuscation::kEnterpriseFileObfuscation);

  std::vector<uint8_t> original_content =
      base::RandBytesAsVector(2 * kDataChunkSize + 1024);
  std::string obfuscated_content;
  ObfuscateContentInChunks(original_content, obfuscated_content);

  std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter =
      CreateDataPipeGetter(obfuscated_content);
  ASSERT_TRUE(data_pipe_getter);

  std::string deobfuscated_string =
      GetBodyFromPipe(data_pipe_getter.get(), original_content.size());

  std::vector<uint8_t> deobfuscated_content(deobfuscated_string.begin(),
                                            deobfuscated_string.end());

  ASSERT_EQ(original_content, deobfuscated_content);
}
}  // namespace safe_browsing
