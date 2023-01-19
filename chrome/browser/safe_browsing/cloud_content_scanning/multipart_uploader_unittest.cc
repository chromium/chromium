// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::_;
using ::testing::Invoke;

class MultipartUploadRequestTest : public testing::Test {
 public:
  MultipartUploadRequestTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  base::FilePath CreateFile(const std::string& file_name,
                            const std::string& content) {
    if (!temp_dir_.IsValid())
      EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::FilePath path = temp_dir_.GetPath().AppendASCII(file_name);
    base::File file(path, base::File::FLAG_CREATE_ALWAYS |
                              base::File::FLAG_READ | base::File::FLAG_WRITE);
    file.WriteAtCurrentPos(content.data(), content.size());
    return path;
  }

  base::ReadOnlySharedMemoryRegion CreatePage(const std::string& content) {
    base::MappedReadOnlyRegion region =
        base::ReadOnlySharedMemoryRegion::Create(content.size());
    EXPECT_TRUE(region.IsValid());
    std::memcpy(region.mapping.memory(), content.data(), content.size());
    return std::move(region.region);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

class MockMultipartUploadRequest : public MultipartUploadRequest {
 public:
  MockMultipartUploadRequest()
      : MultipartUploadRequest(nullptr,
                               GURL(),
                               "",
                               "",
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               base::DoNothing()) {}

  MOCK_METHOD0(SendRequest, void());
};

class MockMultipartUploadDataPipeRequest : public MultipartUploadRequest {
 public:
  MockMultipartUploadDataPipeRequest(const base::FilePath& path,
                                     MultipartUploadRequest::Callback callback)
      : MultipartUploadRequest(nullptr,
                               GURL(),
                               "metadata",
                               path,
                               123,
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               std::move(callback)) {}

  MockMultipartUploadDataPipeRequest(
      base::ReadOnlySharedMemoryRegion page_region,
      MultipartUploadRequest::Callback callback)
      : MultipartUploadRequest(nullptr,
                               GURL(),
                               "metadata",
                               std::move(page_region),
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               std::move(callback)) {}

  std::string GetBodyFromFileOrPageRequest() {
    MultipartDataPipeGetter* data_pipe_getter =
        this->data_pipe_getter_for_testing();
    EXPECT_TRUE(data_pipe_getter);

    mojo::ScopedDataPipeProducerHandle data_pipe_producer;
    mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;

    base::RunLoop run_loop;
    EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, data_pipe_producer,
                                                   data_pipe_consumer));
    data_pipe_getter->Read(
        std::move(data_pipe_producer),
        base::BindLambdaForTesting([&run_loop](int32_t status, uint64_t size) {
          EXPECT_EQ(net::OK, status);
          run_loop.Quit();
        }));
    run_loop.Run();

    EXPECT_TRUE(data_pipe_consumer.is_valid());
    std::string body;
    while (true) {
      char buffer[1024];
      uint32_t read_size = sizeof(buffer);
      MojoResult result = data_pipe_consumer->ReadData(
          buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        base::RunLoop().RunUntilIdle();
        continue;
      }
      if (result != MOJO_RESULT_OK) {
        break;
      }
      body.append(buffer, read_size);
    }

    return body;
  }

  MOCK_METHOD1(CompleteSendRequest,
               void(std::unique_ptr<network::ResourceRequest> request));
};

TEST_F(MultipartUploadRequestTest, GeneratesCorrectBody) {
  std::unique_ptr<MultipartUploadRequest> request =
      MultipartUploadRequest::CreateStringRequest(
          nullptr, GURL(), "metadata", "data", TRAFFIC_ANNOTATION_FOR_TESTS,
          base::DoNothing());

  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "metadata\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "file data\r\n"
      "--boundary--\r\n";

  request->set_boundary("boundary");
  EXPECT_EQ(request->GenerateRequestBody("metadata", "file data"),
            expected_body);
}

TEST_F(MultipartUploadRequestTest, RetriesCorrectly) {
  {
    MockMultipartUploadRequest mock_request;

    EXPECT_CALL(mock_request, SendRequest())
        .Times(1)
        .WillRepeatedly(Invoke([&mock_request]() {
          mock_request.RetryOrFinish(net::OK, net::HTTP_BAD_REQUEST,
                                     std::make_unique<std::string>("response"));
        }));
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();
  }
  {
    MockMultipartUploadRequest mock_request;

    EXPECT_CALL(mock_request, SendRequest())
        .Times(3)
        .WillRepeatedly(Invoke([&mock_request]() {
          mock_request.RetryOrFinish(net::OK, net::HTTP_SERVICE_UNAVAILABLE,
                                     std::make_unique<std::string>("response"));
        }));
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();
  }
}

class MultipartUploadDataPipeRequestTest
    : public MultipartUploadRequestTest,
      public testing::WithParamInterface<bool> {
 public:
  bool is_file_request() { return GetParam(); }

  std::unique_ptr<MockMultipartUploadDataPipeRequest> CreateRequest(
      const std::string& content,
      base::OnceCallback<void(bool success,
                              int http_status,
                              const std::string& response_data)> callback) {
    if (is_file_request()) {
      return std::make_unique<MockMultipartUploadDataPipeRequest>(
          CreateFile("text.txt", content), std::move(callback));
    } else {
      return std::make_unique<MockMultipartUploadDataPipeRequest>(
          CreatePage(content), std::move(callback));
    }
  }
};

INSTANTIATE_TEST_SUITE_P(, MultipartUploadDataPipeRequestTest, testing::Bool());

// Disabled due to flakiness on Windows https://crbug.com/1286638
#if BUILDFLAG(IS_WIN)
#define MAYBE_Retries DISABLED_Retries
#else
#define MAYBE_Retries Retries
#endif
TEST_P(MultipartUploadDataPipeRequestTest, MAYBE_Retries) {
  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "metadata\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "file content\r\n"
      "--boundary--\r\n";
  {
    base::RunLoop run_loop;
    std::unique_ptr<MockMultipartUploadDataPipeRequest> mock_request =
        CreateRequest("file content",
                      base::BindLambdaForTesting(
                          [&run_loop](bool success, int http_status,
                                      const std::string& response_data) {
                            EXPECT_TRUE(success);
                            EXPECT_EQ(net::HTTP_OK, http_status);
                            EXPECT_EQ("response", response_data);
                            run_loop.Quit();
                          }));
    mock_request->set_boundary("boundary");

    EXPECT_CALL(*mock_request, CompleteSendRequest(_))
        .WillOnce([&mock_request, &expected_body](
                      std::unique_ptr<network::ResourceRequest> request) {
          EXPECT_EQ(expected_body,
                    mock_request->GetBodyFromFileOrPageRequest());
          mock_request->RetryOrFinish(
              net::OK, net::HTTP_OK, std::make_unique<std::string>("response"));
        });
    mock_request->Start();
    task_environment_.FastForwardUntilNoTasksRemain();
    run_loop.Run();
  }
  {
    int retry_count = 0;
    base::RunLoop run_loop;
    std::unique_ptr<MockMultipartUploadDataPipeRequest> mock_request =
        CreateRequest(
            "file content",
            base::BindLambdaForTesting(
                [&run_loop, &retry_count](bool success, int http_status,
                                          const std::string& response_data) {
                  EXPECT_TRUE(success);
                  EXPECT_EQ(net::HTTP_OK, http_status);
                  EXPECT_EQ("response", response_data);
                  EXPECT_EQ(3, retry_count);
                  run_loop.Quit();
                }));
    mock_request->set_boundary("boundary");

    EXPECT_CALL(*mock_request, CompleteSendRequest(_))
        .Times(3)
        .WillRepeatedly([&mock_request, &expected_body, &retry_count](
                            std::unique_ptr<network::ResourceRequest> request) {
          // Every call to CompleteSendRequest should be able to get the
          // same body from the request's data pipe getter.
          EXPECT_EQ(expected_body,
                    mock_request->GetBodyFromFileOrPageRequest());

          ++retry_count;
          mock_request->RetryOrFinish(
              net::OK,
              retry_count < 3 ? net::HTTP_SERVICE_UNAVAILABLE : net::HTTP_OK,
              std::make_unique<std::string>("response"));
        });
    mock_request->Start();
    task_environment_.FastForwardUntilNoTasksRemain();
    run_loop.Run();
  }
}

TEST_P(MultipartUploadDataPipeRequestTest, EquivalentToStringRequest) {
  // The request body should be identical when obtained through a string request
  // and a data pipe request with equivalent content.
  std::unique_ptr<MockMultipartUploadDataPipeRequest> data_pipe_request =
      CreateRequest("data", base::DoNothing());
  data_pipe_request->set_boundary("boundary");

  // Start the data pipe request to initialize the internal data pipe getter.
  ASSERT_FALSE(data_pipe_request->data_pipe_getter_for_testing());
  EXPECT_CALL(*data_pipe_request, CompleteSendRequest(_)).Times(1);
  data_pipe_request->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(data_pipe_request->data_pipe_getter_for_testing());

  MockMultipartUploadRequest string_request;
  string_request.set_boundary("boundary");

  std::string expected_body =
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "metadata\r\n"
      "--boundary\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "data\r\n"
      "--boundary--\r\n";

  EXPECT_EQ(expected_body,
            string_request.GenerateRequestBody("metadata", "data"));
  EXPECT_EQ(expected_body, data_pipe_request->GetBodyFromFileOrPageRequest());
}

TEST_F(MultipartUploadRequestTest, GeneratesCorrectHeaders_StringRequest) {
  network::ResourceRequest resource_request;
  std::string header_value;

  std::unique_ptr<MultipartUploadRequest> request =
      MultipartUploadRequest::CreateStringRequest(
          nullptr, GURL(), "metadata", "data", TRAFFIC_ANNOTATION_FOR_TESTS,
          base::DoNothing());
  request->SetRequestHeaders(&resource_request);
  ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Protocol"));
  ASSERT_TRUE(resource_request.headers.GetHeader("X-Goog-Upload-Protocol",
                                                 &header_value));
  ASSERT_EQ(header_value, "multipart");
  ASSERT_TRUE(resource_request.headers.HasHeader(
      "X-Goog-Upload-Header-Content-Length"));
  ASSERT_TRUE(resource_request.headers.GetHeader(
      "X-Goog-Upload-Header-Content-Length", &header_value));
  ASSERT_EQ(header_value, "4");
}

TEST_F(MultipartUploadRequestTest, GeneratesCorrectHeaders_FileRequest) {
  network::ResourceRequest resource_request;
  std::string header_value;

  std::unique_ptr<MultipartUploadRequest> request =
      MultipartUploadRequest::CreateFileRequest(
          nullptr, GURL(), "metadata",
          CreateFile("my_file_name.foo", "file_data"), 9,
          TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing());
  request->SetRequestHeaders(&resource_request);
  ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Protocol"));
  ASSERT_TRUE(resource_request.headers.GetHeader("X-Goog-Upload-Protocol",
                                                 &header_value));
  ASSERT_EQ(header_value, "multipart");
  ASSERT_TRUE(resource_request.headers.HasHeader(
      "X-Goog-Upload-Header-Content-Length"));
  ASSERT_TRUE(resource_request.headers.GetHeader(
      "X-Goog-Upload-Header-Content-Length", &header_value));
  ASSERT_EQ(header_value, "9");
}

TEST_F(MultipartUploadRequestTest, GeneratesCorrectHeaders_PageRequest) {
  network::ResourceRequest resource_request;
  std::string header_value;

  std::unique_ptr<MultipartUploadRequest> request =
      MultipartUploadRequest::CreatePageRequest(
          nullptr, GURL(), "metadata", CreatePage("print_data"),
          TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing());
  request->SetRequestHeaders(&resource_request);
  ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Protocol"));
  ASSERT_TRUE(resource_request.headers.GetHeader("X-Goog-Upload-Protocol",
                                                 &header_value));
  ASSERT_EQ(header_value, "multipart");
  ASSERT_TRUE(resource_request.headers.HasHeader(
      "X-Goog-Upload-Header-Content-Length"));
  ASSERT_TRUE(resource_request.headers.GetHeader(
      "X-Goog-Upload-Header-Content-Length", &header_value));
  ASSERT_EQ(header_value, "10");
}

}  // namespace safe_browsing
