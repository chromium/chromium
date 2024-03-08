// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/enterprise/connectors/test/uploader_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

constexpr char kUploadUrl[] =
    "http://uploads.google.com?upload_id=ABC&upload_protocol=resumable";

using ::testing::_;

class MockResumableUploadRequest : public ResumableUploadRequest {
 public:
  MockResumableUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::FilePath& path,
      ResumableUploadRequest::Callback callback)
      : ResumableUploadRequest(url_loader_factory,
                               GURL("https://google.com"),
                               "metadata",
                               path,
                               123,
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               std::move(callback)) {}

  MockResumableUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::ReadOnlySharedMemoryRegion page_region,
      ResumableUploadRequest::Callback callback)
      : ResumableUploadRequest(url_loader_factory,
                               GURL("https://google.com"),
                               "metadata",
                               std::move(page_region),
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               std::move(callback)) {}
};

class ResumableUploadRequestTest : public testing::Test {
 public:
  ResumableUploadRequestTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  base::FilePath CreateFile(const std::string& file_name,
                            const std::string& content) {
    if (!temp_dir_.IsValid()) {
      EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    }

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

  std::unique_ptr<MockResumableUploadRequest> CreateFileRequest(
      const std::string& content,
      base::OnceCallback<void(bool success,
                              int http_status,
                              const std::string& response_data)> callback) {
    return std::make_unique<MockResumableUploadRequest>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        CreateFile("text.txt", content), std::move(callback));
  }

  std::unique_ptr<MockResumableUploadRequest> CreatePageRequest(
      const std::string& content,
      base::OnceCallback<void(bool success,
                              int http_status,
                              const std::string& response_data)> callback) {
    return std::make_unique<MockResumableUploadRequest>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        CreatePage(content), std::move(callback));
  }

  void VerifyMetadataRequestHeaders(
      const network::ResourceRequest& resource_request,
      std::string expected_size) {
    std::string header_value;

    ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Protocol"));
    ASSERT_TRUE(resource_request.headers.GetHeader("X-Goog-Upload-Protocol",
                                                   &header_value));
    ASSERT_EQ(header_value, "resumable");

    ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Command"));
    ASSERT_TRUE(resource_request.headers.GetHeader("X-Goog-Upload-Command",
                                                   &header_value));
    ASSERT_EQ(header_value, "start");

    ASSERT_TRUE(resource_request.headers.HasHeader(
        "X-Goog-Upload-Header-Content-Type"));
    ASSERT_TRUE(resource_request.headers.GetHeader(
        "X-Goog-Upload-Header-Content-Type", &header_value));
    ASSERT_EQ(header_value, "application/octet-stream");

    ASSERT_TRUE(resource_request.headers.HasHeader(
        "X-Goog-Upload-Header-Content-Length"));
    ASSERT_TRUE(resource_request.headers.GetHeader(
        "X-Goog-Upload-Header-Content-Length", &header_value));
    ASSERT_EQ(header_value, expected_size);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(ResumableUploadRequestTest,
       GeneratesCorrectMetadataHeaders_FileRequest) {
  network::ResourceRequest resource_request;
  std::unique_ptr<ResumableUploadRequest> request =
      ResumableUploadRequest::CreateFileRequest(
          nullptr, GURL(), "metadata",
          CreateFile("my_file_name.foo", "file_data"), 9,
          TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing());
  request->SetMetadataRequestHeaders(&resource_request);

  VerifyMetadataRequestHeaders(std::move(resource_request), "9");
}

TEST_F(ResumableUploadRequestTest,
       GeneratesCorrectMetadataHeaders_PageRequest) {
  network::ResourceRequest resource_request;
  std::unique_ptr<ResumableUploadRequest> request =
      ResumableUploadRequest::CreatePageRequest(
          nullptr, GURL(), "metadata", CreatePage("print_data"),
          TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing());
  request->SetMetadataRequestHeaders(&resource_request);

  VerifyMetadataRequestHeaders(std::move(resource_request), "10");
}

class ResumableUploadSendMetadataRequestTest
    : public ResumableUploadRequestTest,
      public testing::WithParamInterface<bool> {
 public:
  bool is_file_request() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         ResumableUploadSendMetadataRequestTest,
                         testing::Bool());

TEST_P(ResumableUploadSendMetadataRequestTest, SendsCorrectRequest) {
  base::RunLoop run_loop;
  std::string metadata_content_type;
  std::string method;
  std::string body;
  GURL url;

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request.headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                  &metadata_content_type);
        method = request.method;
        url = request.url;
        body = network::GetUploadData(request);
      }));

  std::unique_ptr<MockResumableUploadRequest> mock_request =
      is_file_request()
          ? CreateFileRequest(
                "file content",
                base::BindLambdaForTesting(
                    [&run_loop](bool success, int http_status,
                                const std::string& response_data) {
                      run_loop.Quit();
                    }))
          : CreatePageRequest(
                "page content",
                base::BindLambdaForTesting(
                    [&run_loop](bool success, int http_status,
                                const std::string& response_data) {
                      run_loop.Quit();
                    }));
  mock_request->Start();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_EQ(metadata_content_type, "application/json");
  EXPECT_EQ(method, "POST");
  EXPECT_EQ(body, "metadata\r\n");
  EXPECT_EQ(url, GURL("https://google.com"));
}

TEST_P(ResumableUploadSendMetadataRequestTest, HandlesFailedMetadataScan) {
  base::RunLoop run_loop;
  std::string body;

  std::unique_ptr<MockResumableUploadRequest> mock_request =
      is_file_request()
          ? CreateFileRequest(
                "file content",
                base::BindLambdaForTesting(
                    [&run_loop](bool success, int http_status,
                                const std::string& response_data) {
                      EXPECT_FALSE(success);
                      EXPECT_EQ(net::HTTP_UNAUTHORIZED, http_status);
                      EXPECT_EQ("response", response_data);
                      run_loop.Quit();
                    }))
          : CreatePageRequest(
                "page content",
                base::BindLambdaForTesting(
                    [&run_loop](bool success, int http_status,
                                const std::string& response_data) {
                      EXPECT_FALSE(success);
                      EXPECT_EQ(net::HTTP_UNAUTHORIZED, http_status);
                      EXPECT_EQ("response", response_data);
                      run_loop.Quit();
                    }));
  mock_request->Start();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0U);
  ASSERT_TRUE(pending_request);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      pending_request, network::CreateURLResponseHead(net::HTTP_UNAUTHORIZED),
      "response", network::URLLoaderCompletionStatus(net::OK));

  run_loop.Run();
}

TEST_P(ResumableUploadSendMetadataRequestTest,
       HandlesSuccessfulMetadataOnlyScan) {
  base::RunLoop run_loop;
  std::string body;

  std::unique_ptr<MockResumableUploadRequest> mock_request =
      is_file_request()
          ? CreateFileRequest(
                "file content",
                base::BindLambdaForTesting(
                    [&run_loop](bool success, int http_status,
                                const std::string& response_data) {
                      EXPECT_TRUE(success);
                      EXPECT_EQ(net::HTTP_OK, http_status);
                      EXPECT_EQ("response", response_data);
                      run_loop.Quit();
                    }))
          : CreatePageRequest(
                "page content",
                base::BindLambdaForTesting(
                    [&run_loop](bool success, int http_status,
                                const std::string& response_data) {
                      EXPECT_TRUE(success);
                      EXPECT_EQ(net::HTTP_OK, http_status);
                      EXPECT_EQ("response", response_data);
                      run_loop.Quit();
                    }));
  mock_request->Start();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0U);
  ASSERT_TRUE(pending_request);
  auto head = network::CreateURLResponseHead(net::HTTP_OK);
  head->headers->AddHeader("X-Goog-Upload-Status", "final");
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      pending_request, std::move(head), "response",
      network::URLLoaderCompletionStatus(net::OK));

  run_loop.Run();
}

class ResumableUploadSendContentRequestTest
    : public ResumableUploadRequestTest,
      public testing::WithParamInterface<bool> {
 public:
  bool is_file_request() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         ResumableUploadSendContentRequestTest,
                         testing::Bool());

TEST_P(ResumableUploadSendContentRequestTest, HandlesSuccessfulContentScan) {
  base::RunLoop run_loop;
  std::string content_upload_method;
  std::string content_upload_command;
  std::string content_upload_offset;

  std::unique_ptr<MockResumableUploadRequest> mock_request =
      is_file_request()
          ? CreateFileRequest(
                "file content",
                base::BindLambdaForTesting(
                    [&run_loop](bool success, int http_status,
                                const std::string& response_data) {
                      EXPECT_TRUE(success);
                      EXPECT_EQ(net::HTTP_OK, http_status);
                      EXPECT_EQ("final_response", response_data);
                      run_loop.Quit();
                    }))
          : CreatePageRequest(
                "page content",
                base::BindLambdaForTesting(
                    [&run_loop](bool success, int http_status,
                                const std::string& response_data) {
                      EXPECT_TRUE(success);
                      EXPECT_EQ(net::HTTP_OK, http_status);
                      EXPECT_EQ("final_response", response_data);
                      run_loop.Quit();
                    }));

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url == GURL("https://google.com")) {
          auto metadata_response_head =
              network::CreateURLResponseHead(net::HTTP_OK);
          metadata_response_head->headers->AddHeader("X-Goog-Upload-Status",
                                                     "active");
          metadata_response_head->headers->AddHeader("X-Goog-Upload-URL",
                                                     kUploadUrl);
          test_url_loader_factory_.AddResponse(
              GURL("https://google.com"), std::move(metadata_response_head),
              "metadata_response", network::URLLoaderCompletionStatus(net::OK));
        } else if (request.url == GURL(kUploadUrl)) {
          content_upload_method = request.method;
          request.headers.GetHeader("X-Goog-Upload-Command",
                                    &content_upload_command);
          request.headers.GetHeader("X-Goog-Upload-Offset",
                                    &content_upload_offset);
          auto content_response_head =
              network::CreateURLResponseHead(net::HTTP_OK);
          content_response_head->headers->AddHeader("X-Goog-Upload-Status",
                                                    "final");
          test_url_loader_factory_.AddResponse(
              GURL(kUploadUrl), std::move(content_response_head),
              "final_response", network::URLLoaderCompletionStatus(net::OK));
        } else {
          NOTREACHED();
        }
      }));
  mock_request->Start();
  run_loop.Run();

  if (is_file_request()) {
    EXPECT_EQ("file content",
              enterprise_connectors::test::GetBodyFromFileOrPageRequest(
                  mock_request->data_pipe_getter_for_testing()));
  } else {
    EXPECT_EQ("page content",
              enterprise_connectors::test::GetBodyFromFileOrPageRequest(
                  mock_request->data_pipe_getter_for_testing()));
  }
  EXPECT_EQ(content_upload_method, "POST");
  EXPECT_EQ(content_upload_command, "upload, finalize");
  EXPECT_EQ(content_upload_offset, "0");
}

TEST_P(ResumableUploadSendContentRequestTest, HandlesFailedContentScan) {
  base::RunLoop run_loop;
  std::string content_upload_method;
  std::string content_upload_command;
  std::string content_upload_offset;

  std::unique_ptr<MockResumableUploadRequest> mock_request =
      is_file_request()
          ? CreateFileRequest(
                "file content",
                base::BindLambdaForTesting(
                    [&run_loop](bool success, int http_status,
                                const std::string& response_data) {
                      EXPECT_FALSE(success);
                      EXPECT_EQ(net::HTTP_UNAUTHORIZED, http_status);
                      EXPECT_EQ("final_response", response_data);
                      run_loop.Quit();
                    }))
          : CreatePageRequest(
                "page content",
                base::BindLambdaForTesting(
                    [&run_loop](bool success, int http_status,
                                const std::string& response_data) {
                      EXPECT_FALSE(success);
                      EXPECT_EQ(net::HTTP_UNAUTHORIZED, http_status);
                      EXPECT_EQ("final_response", response_data);
                      run_loop.Quit();
                    }));

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url == GURL("https://google.com")) {
          auto metadata_response_head =
              network::CreateURLResponseHead(net::HTTP_OK);
          metadata_response_head->headers->AddHeader("X-Goog-Upload-Status",
                                                     "active");
          metadata_response_head->headers->AddHeader("X-Goog-Upload-URL",
                                                     kUploadUrl);
          test_url_loader_factory_.AddResponse(
              GURL("https://google.com"), std::move(metadata_response_head),
              "metadata_response", network::URLLoaderCompletionStatus(net::OK));
        } else if (request.url == GURL(kUploadUrl)) {
          content_upload_method = request.method;
          request.headers.GetHeader("X-Goog-Upload-Command",
                                    &content_upload_command);
          request.headers.GetHeader("X-Goog-Upload-Offset",
                                    &content_upload_offset);
          test_url_loader_factory_.AddResponse(
              GURL(kUploadUrl),
              network::CreateURLResponseHead(net::HTTP_UNAUTHORIZED),
              "final_response", network::URLLoaderCompletionStatus(net::OK));
        } else {
          NOTREACHED();
        }
      }));

  mock_request->Start();
  run_loop.Run();

  if (is_file_request()) {
    EXPECT_EQ("file content",
              enterprise_connectors::test::GetBodyFromFileOrPageRequest(
                  mock_request->data_pipe_getter_for_testing()));
  } else {
    EXPECT_EQ("page content",
              enterprise_connectors::test::GetBodyFromFileOrPageRequest(
                  mock_request->data_pipe_getter_for_testing()));
  }
  EXPECT_EQ(content_upload_method, "POST");
  EXPECT_EQ(content_upload_command, "upload, finalize");
  EXPECT_EQ(content_upload_offset, "0");
}

}  // namespace safe_browsing
