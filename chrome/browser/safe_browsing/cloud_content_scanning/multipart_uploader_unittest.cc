// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"

#include <memory>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/connectors/test/uploader_test_utils.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
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
    file.WriteAtCurrentPos(base::as_byte_span(content));
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

  MOCK_METHOD1(CompleteSendRequest,
               void(std::unique_ptr<network::ResourceRequest> request));
};

TEST_F(MultipartUploadRequestTest, GeneratesCorrectBody) {
  auto connector_request = MultipartUploadRequest::CreateStringRequest(
      nullptr, GURL(), "metadata", "data", TRAFFIC_ANNOTATION_FOR_TESTS,
      base::DoNothing());
  auto* request = static_cast<MultipartUploadRequest*>(connector_request.get());

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
                                     "response");
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
                                     "response");
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
                    enterprise_connectors::test::GetBodyFromFileOrPageRequest(
                        mock_request->data_pipe_getter_for_testing()));
          mock_request->RetryOrFinish(net::OK, net::HTTP_OK, "response");
          mock_request->MarkScanAsCompleteForTesting();
        });
    mock_request->Start();
    task_environment_.FastForwardUntilNoTasksRemain();
    run_loop.Run();
    EXPECT_EQ(mock_request->GetUploadInfo(), "Multipart - Complete");
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
                    enterprise_connectors::test::GetBodyFromFileOrPageRequest(
                        mock_request->data_pipe_getter_for_testing()));

          ++retry_count;
          mock_request->RetryOrFinish(
              net::OK,
              retry_count < 3 ? net::HTTP_SERVICE_UNAVAILABLE : net::HTTP_OK,
              "response");
          if (retry_count == 3) {
            mock_request->MarkScanAsCompleteForTesting();
          }
        });
    mock_request->Start();
    task_environment_.FastForwardUntilNoTasksRemain();
    run_loop.Run();
    EXPECT_EQ(mock_request->GetUploadInfo(), "Multipart - Complete");
  }
}

TEST_P(MultipartUploadDataPipeRequestTest, DataControls) {
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
  file_access::MockScopedFileAccessDelegate scoped_files_access_delegate;

  if (is_file_request()) {
    EXPECT_CALL(scoped_files_access_delegate, RequestFilesAccessForSystem)
        .WillOnce(base::test::RunOnceCallback<1>(
            file_access::ScopedFileAccess::Allowed()));
  } else {
    EXPECT_CALL(scoped_files_access_delegate, RequestFilesAccessForSystem)
        .Times(0);
  }

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
                  enterprise_connectors::test::GetBodyFromFileOrPageRequest(
                      mock_request->data_pipe_getter_for_testing()));
        mock_request->RetryOrFinish(net::OK, net::HTTP_OK, "response");
        mock_request->MarkScanAsCompleteForTesting();
      });
  mock_request->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  run_loop.Run();
  EXPECT_EQ(mock_request->GetUploadInfo(), "Multipart - Complete");
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
  EXPECT_EQ(expected_body,
            enterprise_connectors::test::GetBodyFromFileOrPageRequest(
                data_pipe_request->data_pipe_getter_for_testing()));
}

TEST_F(MultipartUploadRequestTest, GeneratesCorrectHeaders_StringRequest) {
  network::ResourceRequest resource_request;

  auto connector_request = MultipartUploadRequest::CreateStringRequest(
      nullptr, GURL(), "metadata", "data", TRAFFIC_ANNOTATION_FOR_TESTS,
      base::DoNothing());
  auto* request = static_cast<MultipartUploadRequest*>(connector_request.get());

  request->SetRequestHeaders(&resource_request);
  ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Protocol"));
  ASSERT_THAT(resource_request.headers.GetHeader("X-Goog-Upload-Protocol"),
              testing::Optional(std::string("multipart")));
  ASSERT_TRUE(resource_request.headers.HasHeader(
      "X-Goog-Upload-Header-Content-Length"));
  ASSERT_THAT(
      resource_request.headers.GetHeader("X-Goog-Upload-Header-Content-Length"),
      testing::Optional(std::string("4")));
  EXPECT_EQ(request->GetUploadInfo(), "Multipart - Pending");
}

TEST_F(MultipartUploadRequestTest, GeneratesCorrectHeaders_FileRequest) {
  network::ResourceRequest resource_request;

  auto connector_request = MultipartUploadRequest::CreateFileRequest(
      nullptr, GURL(), "metadata", CreateFile("my_file_name.foo", "file_data"),
      9, TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing());
  auto* request = static_cast<MultipartUploadRequest*>(connector_request.get());

  request->SetRequestHeaders(&resource_request);
  ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Protocol"));
  ASSERT_THAT(resource_request.headers.GetHeader("X-Goog-Upload-Protocol"),
              testing::Optional(std::string("multipart")));
  ASSERT_TRUE(resource_request.headers.HasHeader(
      "X-Goog-Upload-Header-Content-Length"));
  ASSERT_THAT(
      resource_request.headers.GetHeader("X-Goog-Upload-Header-Content-Length"),
      testing::Optional(std::string("9")));
  EXPECT_EQ(request->GetUploadInfo(), "Multipart - Pending");
}

TEST_F(MultipartUploadRequestTest, GeneratesCorrectHeaders_PageRequest) {
  network::ResourceRequest resource_request;

  auto connector_request = MultipartUploadRequest::CreatePageRequest(
      nullptr, GURL(), "metadata", CreatePage("print_data"),
      TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing());
  auto* request = static_cast<MultipartUploadRequest*>(connector_request.get());

  request->SetRequestHeaders(&resource_request);
  ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Protocol"));
  ASSERT_THAT(resource_request.headers.GetHeader("X-Goog-Upload-Protocol"),
              testing::Optional(std::string("multipart")));
  ASSERT_TRUE(resource_request.headers.HasHeader(
      "X-Goog-Upload-Header-Content-Length"));
  ASSERT_THAT(
      resource_request.headers.GetHeader("X-Goog-Upload-Header-Content-Length"),
      testing::Optional(std::string("10")));
  EXPECT_EQ(request->GetUploadInfo(), "Multipart - Pending");
}

}  // namespace safe_browsing
