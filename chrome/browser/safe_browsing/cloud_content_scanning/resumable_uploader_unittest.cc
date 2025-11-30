// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"

#include <memory>

#include "base/base64.h"
#include "base/compiler_specific.h"
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
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_upload_request.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/uploader_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
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
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      ResumableUploadRequest::VerdictReceivedCallback verdict_received_callback,
      ResumableUploadRequest::ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload)
      : ResumableUploadRequest(url_loader_factory,
                               GURL("https://google.com"),
                               "metadata",
                               get_data_result,
                               path,
                               123,
                               false,
                               "DummySuffix",
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               std::move(verdict_received_callback),
                               std::move(content_uploaded_callback),
                               force_sync_upload) {}

  MockResumableUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::ReadOnlySharedMemoryRegion page_region,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      ResumableUploadRequest::VerdictReceivedCallback verdict_received_callback,
      ResumableUploadRequest::ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload)
      : ResumableUploadRequest(url_loader_factory,
                               GURL("https://google.com"),
                               "metadata",
                               get_data_result,
                               std::move(page_region),
                               "DummySuffix",
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               std::move(verdict_received_callback),
                               std::move(content_uploaded_callback),
                               force_sync_upload) {}
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
    file.WriteAtCurrentPos(base::as_byte_span(content));
    return path;
  }

  base::ReadOnlySharedMemoryRegion CreatePage(const std::string& content) {
    base::MappedReadOnlyRegion region =
        base::ReadOnlySharedMemoryRegion::Create(content.size());
    EXPECT_TRUE(region.IsValid());
    region.mapping.GetMemoryAsSpan<char>().copy_from(content);
    return std::move(region.region);
  }

  template <typename RequestT>
  std::unique_ptr<RequestT> CreateFileRequest(
      const std::string& content,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      ResumableUploadRequest::VerdictReceivedCallback verdict_received_callback,
      ResumableUploadRequest::ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload) {
    return std::make_unique<RequestT>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        CreateFile("text.txt", content), get_data_result,
        std::move(verdict_received_callback),
        std::move(content_uploaded_callback), force_sync_upload);
  }

  template <typename RequestT>
  std::unique_ptr<RequestT> CreatePageRequest(
      const std::string& content,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      ResumableUploadRequest::VerdictReceivedCallback verdict_received_callback,
      ResumableUploadRequest::ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload) {
    return std::make_unique<RequestT>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        CreatePage(content), get_data_result,
        std::move(verdict_received_callback),
        std::move(content_uploaded_callback), force_sync_upload);
  }

  template <typename RequestT>
  std::unique_ptr<RequestT> CreateRequest(
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      ResumableUploadRequest::VerdictReceivedCallback verdict_received_callback,
      ResumableUploadRequest::ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload) {
    return is_file_request()
               ? CreateFileRequest<RequestT>(
                     "file content", get_data_result,
                     std::move(verdict_received_callback),
                     std::move(content_uploaded_callback), force_sync_upload)
               : CreatePageRequest<RequestT>(
                     "page content", get_data_result,
                     std::move(verdict_received_callback),
                     std::move(content_uploaded_callback), force_sync_upload);
  }

  virtual bool is_file_request() { return true; }

  void VerifyMetadataRequestHeaders(
      const network::ResourceRequest& resource_request,
      std::string expected_size,
      const std::string& expected_content_type = "application/octet-stream") {
    ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Protocol"));
    ASSERT_THAT(resource_request.headers.GetHeader("X-Goog-Upload-Protocol"),
                testing::Optional(std::string("resumable")));

    ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Command"));
    ASSERT_THAT(resource_request.headers.GetHeader("X-Goog-Upload-Command"),
                testing::Optional(std::string("start")));

    ASSERT_TRUE(resource_request.headers.HasHeader(
        "X-Goog-Upload-Header-Content-Type"));
    ASSERT_THAT(
        resource_request.headers.GetHeader("X-Goog-Upload-Header-Content-Type"),
        testing::Optional(expected_content_type));

    ASSERT_TRUE(resource_request.headers.HasHeader(
        "X-Goog-Upload-Header-Content-Length"));
    ASSERT_THAT(resource_request.headers.GetHeader(
                    "X-Goog-Upload-Header-Content-Length"),
                testing::Optional(expected_size));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(ResumableUploadRequestTest,
       GeneratesCorrectMetadataHeaders_FileRequest) {
  network::ResourceRequest resource_request;
  auto connector_request = ResumableUploadRequest::CreateFileRequest(
      nullptr, GURL(), "metadata",
      enterprise_connectors::ScanRequestUploadResult::SUCCESS,
      CreateFile("my_file_name.foo", "file_data"), 9, false, "histogram_suffix",
      TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing(), base::DoNothing(),
      false);
  auto* request = static_cast<ResumableUploadRequest*>(connector_request.get());
  request->SetMetadataRequestHeaders(&resource_request);

  VerifyMetadataRequestHeaders(std::move(resource_request), "9");
}

TEST_F(ResumableUploadRequestTest,
       GeneratesCorrectMetadataHeaders_FileRequest_TooLarge) {
  network::ResourceRequest resource_request;
  auto connector_request = ResumableUploadRequest::CreateFileRequest(
      nullptr, GURL(), "metadata",
      enterprise_connectors::ScanRequestUploadResult::FILE_TOO_LARGE,
      CreateFile("my_file_name.foo", "file_data"), 9, false, "histogram_suffix",
      TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing(), base::DoNothing(),
      false);
  auto* request = static_cast<ResumableUploadRequest*>(connector_request.get());
  request->SetMetadataRequestHeaders(&resource_request);

  VerifyMetadataRequestHeaders(std::move(resource_request), "9");
}

TEST_F(ResumableUploadRequestTest,
       GeneratesCorrectMetadataHeaders_FileRequest_Encrypted) {
  network::ResourceRequest resource_request;
  auto connector_request = ResumableUploadRequest::CreateFileRequest(
      nullptr, GURL(), "metadata",
      enterprise_connectors::ScanRequestUploadResult::FILE_ENCRYPTED,
      CreateFile("my_file_name.foo", "file_data"), 9, false, "histogram_suffix",
      TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing(), base::DoNothing(),
      false);
  auto* request = static_cast<ResumableUploadRequest*>(connector_request.get());
  request->SetMetadataRequestHeaders(&resource_request);

  VerifyMetadataRequestHeaders(std::move(resource_request), "9");
}

TEST_F(ResumableUploadRequestTest,
       GeneratesCorrectMetadataHeaders_PageRequest) {
  network::ResourceRequest resource_request;
  auto connector_request = ResumableUploadRequest::CreatePageRequest(
      nullptr, GURL(), "metadata",
      enterprise_connectors::ScanRequestUploadResult::SUCCESS,
      CreatePage("print_data"), "histogram_suffix",
      TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing(), base::DoNothing(),
      false);
  auto* request = static_cast<ResumableUploadRequest*>(connector_request.get());
  request->SetMetadataRequestHeaders(&resource_request);

  VerifyMetadataRequestHeaders(std::move(resource_request), "10");
}

class ResumableUploadStringRequestTest : public ResumableUploadRequestTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      enterprise_connectors::kDlpScanPastedImages};
};

TEST_F(ResumableUploadStringRequestTest,
       GeneratesCorrectMetadataHeaders_StringRequest) {
  network::ResourceRequest resource_request;
  auto connector_request = ResumableUploadRequest::CreateStringRequest(
      nullptr, GURL(), "metadata", "string_data", "histogram_suffix",
      TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing(), base::DoNothing(),
      false);
  auto* request = static_cast<ResumableUploadRequest*>(connector_request.get());
  request->SetMetadataRequestHeaders(&resource_request);

  VerifyMetadataRequestHeaders(std::move(resource_request), "11", "image/png");
}

class ResumableUploadSendMetadataRequestTest
    : public ResumableUploadRequestTest,
      public testing::WithParamInterface<bool> {
 public:
  bool is_file_request() override { return GetParam(); }
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
        metadata_content_type =
            request.headers.GetHeader(net::HttpRequestHeaders::kContentType)
                .value_or(std::string());
        method = request.method;
        url = request.url;
        body = network::GetUploadData(request);
      }));

  auto callback = base::BindLambdaForTesting(
      [&run_loop](bool success, int http_status,
                  const std::string& response_data) { run_loop.Quit(); });
  auto mock_request = CreateRequest<MockResumableUploadRequest>(
      enterprise_connectors::ScanRequestUploadResult::SUCCESS,
      std::move(callback), base::DoNothing(), false);
  mock_request->Start();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  EXPECT_EQ(metadata_content_type, "application/json");
  EXPECT_EQ(method, "POST");
  EXPECT_EQ(body, "metadata\r\n");
  EXPECT_EQ(url, GURL("https://google.com"));
  EXPECT_EQ(mock_request->GetUploadInfo(), "Resumable - Pending");
}

TEST_P(ResumableUploadSendMetadataRequestTest, HandlesFailedMetadataScan) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  std::string body;

  auto callback =
      base::BindLambdaForTesting([&run_loop](bool success, int http_status,
                                             const std::string& response_data) {
        EXPECT_FALSE(success);
        EXPECT_EQ(net::HTTP_UNAUTHORIZED, http_status);
        EXPECT_EQ("response", response_data);
        run_loop.Quit();
      });
  auto mock_request = CreateRequest<MockResumableUploadRequest>(
      enterprise_connectors::ScanRequestUploadResult::SUCCESS,
      std::move(callback), base::DoNothing(), false);
  mock_request->Start();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0U);
  ASSERT_TRUE(pending_request);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      pending_request, network::CreateURLResponseHead(net::HTTP_UNAUTHORIZED),
      "response", network::URLLoaderCompletionStatus(net::OK));

  run_loop.Run();
  EXPECT_EQ(mock_request->GetUploadInfo(), "Resumable - Metadata only scan");

  histogram_tester.ExpectUniqueSample(
      /*name=*/"SafeBrowsing.ResumableUploader.NetworkResult.DummySuffix",
      /*sample=*/net::HTTP_UNAUTHORIZED,
      /*expected_bucket_count=*/1);
}

TEST_P(ResumableUploadSendMetadataRequestTest,
       HandlesSuccessfulMetadataOnlyScan) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  std::string body;

  auto callback =
      base::BindLambdaForTesting([&run_loop](bool success, int http_status,
                                             const std::string& response_data) {
        EXPECT_TRUE(success);
        EXPECT_EQ(net::HTTP_OK, http_status);
        EXPECT_EQ("final_response", response_data);
        run_loop.Quit();
      });

  auto mock_request = CreateRequest<MockResumableUploadRequest>(
      enterprise_connectors::ScanRequestUploadResult::SUCCESS,
      std::move(callback), base::DoNothing(), false);
  mock_request->Start();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0U);
  ASSERT_TRUE(pending_request);
  auto head = network::CreateURLResponseHead(net::HTTP_OK);
  head->headers->AddHeader("X-Goog-Upload-Status", "final");
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      pending_request, std::move(head), "final_response",
      network::URLLoaderCompletionStatus(net::OK));

  run_loop.Run();
  EXPECT_EQ(mock_request->GetUploadInfo(), "Resumable - Metadata only scan");

  histogram_tester.ExpectUniqueSample(
      /*name=*/"SafeBrowsing.ResumableUploader.NetworkResult.DummySuffix",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);
}

enum class UploadRequestType { kFile, kPage, kString };

class ResumableUploadSendContentRequestTest
    : public ResumableUploadRequestTest,
      public testing::WithParamInterface<UploadRequestType> {
 public:
  ResumableUploadSendContentRequestTest() {
    if (GetRequestType() == UploadRequestType::kString) {
      feature_list_.InitWithFeatures(
          {enterprise_connectors::kDlpScanPastedImages}, {});
    }
  }

  UploadRequestType GetRequestType() { return GetParam(); }

  std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>
  CreateTestRequest(
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      ResumableUploadRequest::VerdictReceivedCallback verdict_received_callback,
      ResumableUploadRequest::ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload) {
    switch (GetRequestType()) {
      case UploadRequestType::kFile:
        return CreateFileRequest<MockResumableUploadRequest>(
            GetContent(), get_data_result, std::move(verdict_received_callback),
            std::move(content_uploaded_callback), force_sync_upload);
      case UploadRequestType::kPage:
        return CreatePageRequest<MockResumableUploadRequest>(
            GetContent(), get_data_result, std::move(verdict_received_callback),
            std::move(content_uploaded_callback), force_sync_upload);
      case UploadRequestType::kString:
        return ResumableUploadRequest::CreateStringRequest(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            GURL("https://google.com"), "metadata", GetContent(), "DummySuffix",
            TRAFFIC_ANNOTATION_FOR_TESTS, std::move(verdict_received_callback),
            std::move(content_uploaded_callback), force_sync_upload);
    }
  }

  std::string GetContent() {
    switch (GetRequestType()) {
      case UploadRequestType::kFile:
        return "file content";
      case UploadRequestType::kPage:
        return "page content";
      case UploadRequestType::kString:
        return "string content";
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         ResumableUploadSendContentRequestTest,
                         testing::Values(UploadRequestType::kFile,
                                         UploadRequestType::kPage,
                                         UploadRequestType::kString));

TEST_P(ResumableUploadSendContentRequestTest, HandlesSuccessfulContentScan) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  std::string content_upload_body;
  std::string content_upload_method;
  std::string content_upload_command;
  std::string content_upload_offset;

  auto callback =
      base::BindLambdaForTesting([&run_loop](bool success, int http_status,
                                             const std::string& response_data) {
        EXPECT_TRUE(success);
        EXPECT_EQ(net::HTTP_OK, http_status);
        EXPECT_EQ("final_response", response_data);
        run_loop.Quit();
      });

  auto connector_request =
      CreateTestRequest(enterprise_connectors::ScanRequestUploadResult::SUCCESS,
                        std::move(callback), base::DoNothing(), false);
  auto* request = static_cast<ResumableUploadRequest*>(connector_request.get());

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
          if (GetRequestType() == UploadRequestType::kString) {
            content_upload_body = network::GetUploadData(request);
          }
          content_upload_method = request.method;
          content_upload_command =
              request.headers.GetHeader("X-Goog-Upload-Command")
                  .value_or(std::string());
          content_upload_offset =
              request.headers.GetHeader("X-Goog-Upload-Offset")
                  .value_or(std::string());
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
  request->Start();
  run_loop.Run();

  if (GetRequestType() == UploadRequestType::kString) {
    EXPECT_EQ(GetContent(), content_upload_body);
  } else {
    EXPECT_EQ(GetContent(), enterprise_connectors::GetBodyFromFileOrPageRequest(
                                request->data_pipe_getter_for_testing()));
  }
  EXPECT_EQ(content_upload_method, "POST");
  EXPECT_EQ(content_upload_command, "upload, finalize");
  EXPECT_EQ(content_upload_offset, "0");
  EXPECT_EQ(request->GetUploadInfo(), "Resumable - Full content scan");

  histogram_tester.ExpectUniqueSample(
      /*name=*/"SafeBrowsing.ResumableUploader.NetworkResult.DummySuffix",
      /*sample=*/net::HTTP_OK,
      /*expected_bucket_count=*/1);
}

TEST_P(ResumableUploadSendContentRequestTest, HandlesFileTooLarge) {
  if (GetRequestType() == UploadRequestType::kString) {
    GTEST_SKIP();
  }
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  auto callback =
      base::BindLambdaForTesting([&run_loop](bool success, int http_status,
                                             const std::string& response_data) {
        EXPECT_FALSE(success);
        EXPECT_EQ(net::HTTP_BAD_REQUEST, http_status);
        run_loop.Quit();
      });

  auto mock_request = CreateTestRequest(
      enterprise_connectors::ScanRequestUploadResult::FILE_TOO_LARGE,
      std::move(callback), base::DoNothing(), false);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        // Return a response that asks for more data.
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
        } else {
          NOTREACHED();
        }
      }));
  mock_request->Start();
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      /*name=*/"SafeBrowsing.ResumableUploader.NetworkResult.DummySuffix",
      /*sample=*/net::ERR_FAILED,
      /*expected_bucket_count=*/1);
}

TEST_P(ResumableUploadSendContentRequestTest, HandlesEncryptedFile) {
  if (GetRequestType() == UploadRequestType::kString) {
    GTEST_SKIP();
  }
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  auto callback =
      base::BindLambdaForTesting([&run_loop](bool success, int http_status,
                                             const std::string& response_data) {
        EXPECT_FALSE(success);
        EXPECT_EQ(net::HTTP_BAD_REQUEST, http_status);
        run_loop.Quit();
      });

  auto mock_request = CreateTestRequest(
      enterprise_connectors::ScanRequestUploadResult::FILE_ENCRYPTED,
      std::move(callback), base::DoNothing(), false);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        // Return a response that asks for more data.
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
        } else {
          NOTREACHED();
        }
      }));
  mock_request->Start();
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      /*name=*/"SafeBrowsing.ResumableUploader.NetworkResult.DummySuffix",
      /*sample=*/net::ERR_FAILED,
      /*expected_bucket_count=*/1);
}

TEST_P(ResumableUploadSendContentRequestTest, HandlesFailedContentScan) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  std::string content_upload_body;
  std::string content_upload_method;
  std::string content_upload_command;
  std::string content_upload_offset;

  auto callback =
      base::BindLambdaForTesting([&run_loop](bool success, int http_status,
                                             const std::string& response_data) {
        EXPECT_FALSE(success);
        EXPECT_EQ(net::HTTP_UNAUTHORIZED, http_status);
        EXPECT_EQ("final_response", response_data);
        run_loop.Quit();
      });
  auto connector_request =
      CreateTestRequest(enterprise_connectors::ScanRequestUploadResult::SUCCESS,
                        std::move(callback), base::DoNothing(), false);
  auto* request = static_cast<ResumableUploadRequest*>(connector_request.get());

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
          if (GetRequestType() == UploadRequestType::kString) {
            content_upload_body = network::GetUploadData(request);
          }
          content_upload_method = request.method;
          content_upload_command =
              request.headers.GetHeader("X-Goog-Upload-Command")
                  .value_or(std::string());
          content_upload_offset =
              request.headers.GetHeader("X-Goog-Upload-Offset")
                  .value_or(std::string());
          test_url_loader_factory_.AddResponse(
              GURL(kUploadUrl),
              network::CreateURLResponseHead(net::HTTP_UNAUTHORIZED),
              "final_response", network::URLLoaderCompletionStatus(net::OK));
        } else {
          NOTREACHED();
        }
      }));

  request->Start();
  run_loop.Run();

  if (GetRequestType() == UploadRequestType::kString) {
    EXPECT_EQ(GetContent(), content_upload_body);
  } else {
    EXPECT_EQ(GetContent(), enterprise_connectors::GetBodyFromFileOrPageRequest(
                                request->data_pipe_getter_for_testing()));
  }
  EXPECT_EQ(content_upload_method, "POST");
  EXPECT_EQ(content_upload_command, "upload, finalize");
  EXPECT_EQ(content_upload_offset, "0");
  EXPECT_EQ(request->GetUploadInfo(), "Resumable - Full content scan");

  histogram_tester.ExpectUniqueSample(
      /*name=*/"SafeBrowsing.ResumableUploader.NetworkResult.DummySuffix",
      /*sample=*/net::HTTP_UNAUTHORIZED,
      /*expected_bucket_count=*/1);
}

TEST_P(ResumableUploadSendContentRequestTest,
       HandlesEncryptedFileContentUploadIfEnabled) {
  if (GetRequestType() == UploadRequestType::kString) {
    GTEST_SKIP();
  }

  base::RunLoop run_loop;
  base::RunLoop async_content_upload_run_loop;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableEncryptedFileUpload);

  auto verdict_callback = base::BindLambdaForTesting(
      [&](bool success, int http_status, const std::string& response_data) {
        run_loop.Quit();
      });

  auto content_callback =
      base::BindLambdaForTesting([&async_content_upload_run_loop]() {
        async_content_upload_run_loop.Quit();
      });

  auto mock_request = CreateTestRequest(
      enterprise_connectors::ScanRequestUploadResult::FILE_ENCRYPTED,
      std::move(verdict_callback), std::move(content_callback), false);

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
          ASSERT_EQ(request.method, "POST");
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
  async_content_upload_run_loop.Run();
}

struct AsyncScanRequestUploadResult {
  bool success;
  net::HttpStatusCode response_code;
  bool decode_result;
  std::string intermediate_value;
  bool force_sync_upload;
};

std::string GetEncodedContentAnalysisResponse() {
  // Create a ContentAnalysisResponse instance with arbitrary values.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.mutable_results()->Add();
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  return base::Base64Encode(response.SerializeAsString());
}

const AsyncScanRequestUploadResult kTestCases[] = {
    {.success = true,
     .response_code = net::HTTP_OK,
     .decode_result = true,
     .intermediate_value = GetEncodedContentAnalysisResponse(),
     .force_sync_upload = false},
    {.success = false,
     .response_code = net::HTTP_BAD_REQUEST,
     .decode_result = false,
     .intermediate_value = "bad-cep-header",
     .force_sync_upload = false},
    {.success = true,
     .response_code = net::HTTP_OK,
     .decode_result = true,
     .intermediate_value = GetEncodedContentAnalysisResponse(),
     .force_sync_upload = true}};

class MockResumableUploadRequestForAsync : public MockResumableUploadRequest {
 public:
  MockResumableUploadRequestForAsync(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::FilePath& path,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      ResumableUploadRequest::VerdictReceivedCallback verdict_received_callback,
      ResumableUploadRequest::ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload)
      : MockResumableUploadRequest(url_loader_factory,
                                   path,
                                   get_data_result,
                                   std::move(verdict_received_callback),
                                   std::move(content_uploaded_callback),
                                   force_sync_upload) {}

  MockResumableUploadRequestForAsync(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::ReadOnlySharedMemoryRegion page_region,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      ResumableUploadRequest::VerdictReceivedCallback verdict_received_callback,
      ResumableUploadRequest::ContentUploadedCallback content_uploaded_callback,
      bool force_sync_upload)
      : MockResumableUploadRequest(url_loader_factory,
                                   std::move(page_region),
                                   get_data_result,
                                   std::move(verdict_received_callback),
                                   std::move(content_uploaded_callback),
                                   force_sync_upload) {}

  void SendContentSoon(const std::string& upload_url) override {
    // a null callback indicates that the user has already been unblocked.
    ASSERT_EQ(verdict_received_callback_.is_null(), !force_sync_upload());

    // Invoke the callback here to quit the run loop.
    if (!verdict_received_callback_.is_null()) {
      std::move(verdict_received_callback_)
          .Run(/*success=*/true, net::HTTP_OK, /*response=*/"");
    } else {
      ASSERT_EQ(GetUploadInfo(), "Resumable - Async content upload");
    }
  }
};

class ResumableUploadSendContentAsyncTest
    : public ResumableUploadRequestTest,
      public testing::WithParamInterface<
          std::tuple<bool, AsyncScanRequestUploadResult>> {
 public:
  ResumableUploadSendContentAsyncTest() = default;

  bool is_file_request() override { return std::get<0>(GetParam()); }

  const AsyncScanRequestUploadResult& get_upload_result() {
    return std::get<1>(GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ResumableUploadSendContentAsyncTest,
       VerdictCallbackInvokedBeforeContentUpload) {
  base::RunLoop run_loop;

  auto mock_request = CreateRequest<MockResumableUploadRequestForAsync>(
      enterprise_connectors::ScanRequestUploadResult::SUCCESS,
      base::BindLambdaForTesting(
          [&](bool success, int http_status, const std::string& response_data) {
            std::string decoded_response;
            bool decode_result = base::Base64Decode(
                get_upload_result().intermediate_value, &decoded_response);
            EXPECT_EQ(get_upload_result().decode_result, decode_result);
            EXPECT_EQ(get_upload_result().success, success);
            EXPECT_EQ(get_upload_result().response_code, http_status);
            run_loop.Quit();
          }),
      base::DoNothing(), get_upload_result().force_sync_upload);

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url == GURL("https://google.com")) {
          auto metadata_response_head =
              network::CreateURLResponseHead(get_upload_result().response_code);
          metadata_response_head->headers->AddHeader("X-Goog-Upload-Status",
                                                     "active");
          metadata_response_head->headers->AddHeader("X-Goog-Upload-URL",
                                                     kUploadUrl);
          metadata_response_head->headers->AddHeader(
              "X-Goog-Upload-Header-Cep-Response",
              get_upload_result().intermediate_value);
          test_url_loader_factory_.AddResponse(
              GURL("https://google.com"), std::move(metadata_response_head),
              "metadata_response", network::URLLoaderCompletionStatus(net::OK));
        } else {
          EXPECT_EQ(request.url, GURL(kUploadUrl));
        }
      }));

  mock_request->Start();
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(,
                         ResumableUploadSendContentAsyncTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::ValuesIn(kTestCases)));

}  // namespace safe_browsing
