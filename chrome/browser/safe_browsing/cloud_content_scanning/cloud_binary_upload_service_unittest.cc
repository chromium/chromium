// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace safe_browsing {

namespace {

enterprise_connectors::CloudAnalysisSettings CloudAnalysisSettingsWithUrl(
    const std::string& url) {
  enterprise_connectors::CloudAnalysisSettings settings;
  settings.analysis_url = GURL(url);
  return settings;
}

}  // namespace

using ::enterprise_connectors::ConnectorUploadRequest;
using ::enterprise_connectors::ConnectorUploadRequestFactory;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

class MockRequest : public BinaryUploadService::Request {
 public:
  MockRequest(BinaryUploadService::ContentAnalysisCallback callback,
              enterprise_connectors::CloudAnalysisSettings settings)
      : BinaryUploadService::Request(
            std::move(callback),
            enterprise_connectors::CloudOrLocalAnalysisSettings(
                std::move(settings))) {}
  MOCK_METHOD1(GetRequestData, void(DataCallback));
};

class FakeConnectorUploadRequest : public ConnectorUploadRequest {
 public:
  FakeConnectorUploadRequest(
      bool should_succeed,
      std::optional<enterprise_connectors::ContentAnalysisResponse> response,
      int http_status,
      Callback callback)
      : ConnectorUploadRequest(nullptr,
                               GURL(),
                               /*metadata=*/"",
                               /*data=*/"",
                               /*histogram_suffix=*/"",
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               base::DoNothing()),
        should_succeed_(should_succeed),
        response_(response),
        http_status_(http_status),
        callback_(std::move(callback)) {}

  void Start() override {
    // The server never returns a response, so the completion callback should
    // not be invoked.
    if (!response_.has_value()) {
      return;
    }

    std::string serialized_response;
    response_.value().SerializeToString(&serialized_response);
    std::move(callback_).Run(should_succeed_, http_status_,
                             serialized_response);
  }

  std::string GetUploadInfo() override { return "upload info"; }

 private:
  bool should_succeed_;
  std::optional<enterprise_connectors::ContentAnalysisResponse> response_;
  int http_status_;
  Callback callback_;
};

class FakeConnectorUploadRequestFactory : public ConnectorUploadRequestFactory {
 public:
  FakeConnectorUploadRequestFactory() = default;

  FakeConnectorUploadRequestFactory(
      bool should_succeed,
      std::optional<enterprise_connectors::ContentAnalysisResponse> response,
      int http_status)
      : should_succeed_(should_succeed),
        http_status_(http_status),
        response_(response) {}

  std::unique_ptr<ConnectorUploadRequest> CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ConnectorUploadRequest::Callback callback) override {
    called_ = true;
    return std::make_unique<FakeConnectorUploadRequest>(
        should_succeed_, response_, http_status_, std::move(callback));
  }

  std::unique_ptr<ConnectorUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      const base::FilePath& path,
      uint64_t file_size,
      bool is_obfuscated,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ConnectorUploadRequest::Callback callback) override {
    called_ = true;
    return std::make_unique<FakeConnectorUploadRequest>(
        should_succeed_, response_, http_status_, std::move(callback));
  }

  std::unique_ptr<ConnectorUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      base::ReadOnlySharedMemoryRegion page_region,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ConnectorUploadRequest::Callback callback) override {
    called_ = true;
    return std::make_unique<FakeConnectorUploadRequest>(
        should_succeed_, response_, http_status_, std::move(callback));
  }

  bool called() const { return called_; }

 private:
  bool should_succeed_ = true;
  int http_status_ = net::HTTP_OK;
  std::optional<enterprise_connectors::ContentAnalysisResponse> response_ =
      enterprise_connectors::ContentAnalysisResponse();
  bool called_ = false;
};

class CloudBinaryUploadServiceTest : public ::testing::Test {
 public:
  CloudBinaryUploadServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    MultipartUploadRequest::RegisterFactoryForTests(&fake_factory_);

    // Since we have mocked the MultipartUploadRequest, we don't need a
    // URLLoaderFactory, so pass nullptr here.
    service_ = std::make_unique<CloudBinaryUploadService>(nullptr, &profile_);
    scoped_feature_list_.InitWithFeatures(
        {enterprise_connectors::kDlpScanPastedImages}, {});
  }

  void TearDown() override {
    ResumableUploadRequest::RegisterFactoryForTests(nullptr);
    MultipartUploadRequest::RegisterFactoryForTests(nullptr);
  }

  void ExpectNetworkResponse(
      bool should_succeed,
      std::optional<enterprise_connectors::ContentAnalysisResponse> response,
      int http_status = -1) {
    if (http_status == -1) {
      http_status = should_succeed ? net::HTTP_OK : net::HTTP_BAD_REQUEST;
    }
    fake_factory_ = FakeConnectorUploadRequestFactory(should_succeed, response,
                                                      http_status);
  }

  void UploadForDeepScanning(
      std::unique_ptr<BinaryUploadService::Request> request,
      enterprise_connectors::ScanRequestUploadResult auth_check_for_enterprise =
          enterprise_connectors::ScanRequestUploadResult::SUCCESS) {
    service_->SetAuthForTesting("fake_device_token", auth_check_for_enterprise);
    service_->MaybeUploadForDeepScanning(std::move(request));
  }

  void ReceiveMessageForRequest(
      BinaryUploadService::Request::Id request_id,
      const enterprise_connectors::ContentAnalysisResponse& response) {
    service_->OnGetResponse(request_id, response);
  }

  void ReceiveResponseFromUpload(BinaryUploadService::Request::Id request_id,
                                 bool success,
                                 const std::string& response) {
    service_->OnUploadComplete(request_id, success,
                               success ? net::HTTP_OK : net::HTTP_BAD_REQUEST,
                               response);
  }

  std::unique_ptr<MockRequest> MakeRequest(
      enterprise_connectors::ScanRequestUploadResult* scanning_result,
      enterprise_connectors::ContentAnalysisResponse* scanning_response,
      bool is_advanced_protection) {
    auto request = std::make_unique<NiceMock<MockRequest>>(
        base::BindOnce(
            [](base::RepeatingClosure request_done_closure,
               enterprise_connectors::ScanRequestUploadResult* target_result,
               enterprise_connectors::ContentAnalysisResponse* target_response,
               enterprise_connectors::ScanRequestUploadResult result,
               enterprise_connectors::ContentAnalysisResponse response) {
              *target_result = result;
              *target_response = response;
              if (!request_done_closure.is_null()) {
                request_done_closure.Run();
              }
            },
            request_done_closure_, scanning_result, scanning_response),
        enterprise_connectors::CloudAnalysisSettings());
    if (!is_advanced_protection) {
      request->set_device_token("fake_device_token");
    }
    ON_CALL(*request, GetRequestData(_))
        .WillByDefault([](BinaryUploadService::Request::DataCallback callback) {
          BinaryUploadService::Request::Data data;
          data.contents = "contents";
          data.size = data.contents.size();
          std::move(callback).Run(
              enterprise_connectors::ScanRequestUploadResult::SUCCESS,
              std::move(data));
        });

    return request;
  }

  void ValidateAuthorizationTimerIdle() {
    EXPECT_FALSE(service_->timer_.IsRunning());
    EXPECT_EQ(base::Hours(0), service_->timer_.GetCurrentDelay());
  }

  void ValidateAuthorizationTimerStarted() {
    EXPECT_TRUE(service_->timer_.IsRunning());
    EXPECT_EQ(base::Hours(24), service_->timer_.GetCurrentDelay());
  }

  void SetRequestDoneClosure(base::RepeatingClosure closure) {
    request_done_closure_ = closure;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<CloudBinaryUploadService> service_;
  FakeConnectorUploadRequestFactory fake_factory_;
  base::RepeatingClosure request_done_closure_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CloudBinaryUploadServiceTest, PassesForLargeFile) {
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  ASSERT_TRUE(base::WriteFile(file_path, "test"));

  ExpectNetworkResponse(/*should_succeed=*/true,
                        enterprise_connectors::ContentAnalysisResponse());

  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED);
  ON_CALL(*request, GetRequestData(_))
      .WillByDefault(
          [file_path](BinaryUploadService::Request::DataCallback callback) {
            BinaryUploadService::Request::Data data;
            data.path = file_path;
            data.size = 4;  // Must not be zero.
            std::move(callback).Run(
                enterprise_connectors::ScanRequestUploadResult::FILE_TOO_LARGE,
                std::move(data));
          });
  UploadForDeepScanning(std::move(request));

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::FILE_TOO_LARGE);
}

TEST_F(CloudBinaryUploadServiceTest, FailsForLargeFile) {
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  ASSERT_TRUE(base::WriteFile(file_path, "test"));

  ExpectNetworkResponse(/*should_succeed=*/false,
                        enterprise_connectors::ContentAnalysisResponse());

  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED);
  ON_CALL(*request, GetRequestData(_))
      .WillByDefault(
          [file_path](BinaryUploadService::Request::DataCallback callback) {
            BinaryUploadService::Request::Data data;
            data.path = file_path;
            data.size = 4;  // Must not be zero.
            std::move(callback).Run(
                enterprise_connectors::ScanRequestUploadResult::FILE_TOO_LARGE,
                std::move(data));
          });
  UploadForDeepScanning(std::move(request));

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UPLOAD_FAILURE);
}

TEST_F(CloudBinaryUploadServiceTest, FailsForEncryptedFile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      enterprise_connectors::kEnableEncryptedFileUpload);
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &file_path));
  file_path = file_path.AppendASCII("safe_browsing")
                  .AppendASCII("download_protection")
                  .AppendASCII("encrypted.zip");

  ExpectNetworkResponse(/*should_succeed=*/false,
                        enterprise_connectors::ContentAnalysisResponse());

  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED);
  ON_CALL(*request, GetRequestData(_))
      .WillByDefault(
          [file_path](BinaryUploadService::Request::DataCallback callback) {
            BinaryUploadService::Request::Data data;
            data.path = file_path;
            data.size = 4;  // Must not be zero.
            std::move(callback).Run(
                enterprise_connectors::ScanRequestUploadResult::FILE_ENCRYPTED,
                std::move(data));
          });
  UploadForDeepScanning(std::move(request));

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UPLOAD_FAILURE);
}

TEST_F(CloudBinaryUploadServiceTest, PassesForEncryptedFileIfEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableEncryptedFileUpload);
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &file_path));
  file_path = file_path.AppendASCII("safe_browsing")
                  .AppendASCII("download_protection")
                  .AppendASCII("encrypted.zip");
  ExpectNetworkResponse(/*should_succeed=*/true,
                        enterprise_connectors::ContentAnalysisResponse());

  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED);
  ON_CALL(*request, GetRequestData(_))
      .WillByDefault(
          [file_path](BinaryUploadService::Request::DataCallback callback) {
            BinaryUploadService::Request::Data data;
            data.path = file_path;
            data.size = 4;  // Must not be zero.
            std::move(callback).Run(
                enterprise_connectors::ScanRequestUploadResult::FILE_ENCRYPTED,
                std::move(data));
          });
  UploadForDeepScanning(std::move(request));

  // TODO(crbug.com/418020892): Use BrowserTaskEnvironment::RunUntilIdle()
  // instead.
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::FILE_ENCRYPTED);
}

TEST_F(CloudBinaryUploadServiceTest, Succeeds) {
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, SucceedsForAuthentication) {
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);

  // The auth request never requests an instance ID, so it should get a normal
  // response.
  base::RunLoop run_loop;
  service_->IsAuthorized(
      GURL(), /*per_profile_request*/ false,
      base::BindLambdaForTesting(
          [&run_loop](enterprise_connectors::ScanRequestUploadResult
                          auth_check_result) {
            EXPECT_EQ(auth_check_result,
                      enterprise_connectors::ScanRequestUploadResult::SUCCESS);
            run_loop.Quit();
          }),
      "fake_device_token",
      enterprise_connectors::AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED);
  run_loop.Run();

  service_->MaybeUploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, PasteSucceeds) {
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY);

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, PasteImageResumableSucceeds) {
  FakeConnectorUploadRequestFactory resumable_factory{
      /*should_succeed=*/true, enterprise_connectors::ContentAnalysisResponse(),
      net::HTTP_OK};
  ResumableUploadRequest::RegisterFactoryForTests(&resumable_factory);

  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  auto settings = enterprise_connectors::CloudAnalysisSettings();
  auto request = std::make_unique<NiceMock<MockRequest>>(
      base::BindLambdaForTesting(
          [&](enterprise_connectors::ScanRequestUploadResult result,
              enterprise_connectors::ContentAnalysisResponse response) {
            scanning_result = result;
            scanning_response = response;
          }),
      std::move(settings));
  request->set_device_token("fake_device_token");
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY);
  ON_CALL(*request, GetRequestData(_))
      .WillByDefault([](BinaryUploadService::Request::DataCallback callback) {
        BinaryUploadService::Request::Data data;
        data.contents = "contents";
        data.size = data.contents.size();
        std::move(callback).Run(
            enterprise_connectors::ScanRequestUploadResult::SUCCESS,
            std::move(data));
      });

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(resumable_factory.called());
  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::SUCCESS);

  ResumableUploadRequest::RegisterFactoryForTests(nullptr);
}

TEST_F(CloudBinaryUploadServiceTest, PasteImageResumableFails) {
  FakeConnectorUploadRequestFactory resumable_factory{
      /*should_succeed=*/false,
      enterprise_connectors::ContentAnalysisResponse(), net::HTTP_OK};
  ResumableUploadRequest::RegisterFactoryForTests(&resumable_factory);

  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  auto settings = enterprise_connectors::CloudAnalysisSettings();
  auto request = std::make_unique<NiceMock<MockRequest>>(
      base::BindLambdaForTesting(
          [&](enterprise_connectors::ScanRequestUploadResult result,
              enterprise_connectors::ContentAnalysisResponse response) {
            scanning_result = result;
            scanning_response = response;
          }),
      std::move(settings));
  request->set_device_token("fake_device_token");
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY);
  ON_CALL(*request, GetRequestData(_))
      .WillByDefault([](BinaryUploadService::Request::DataCallback callback) {
        BinaryUploadService::Request::Data data;
        data.contents = "contents";
        data.size = data.contents.size();
        std::move(callback).Run(
            enterprise_connectors::ScanRequestUploadResult::SUCCESS,
            std::move(data));
      });

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(resumable_factory.called());
  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UPLOAD_FAILURE);

  ResumableUploadRequest::RegisterFactoryForTests(nullptr);
}

TEST_F(CloudBinaryUploadServiceTest, FailsWhenUploadFails) {
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);

  ExpectNetworkResponse(false,
                        enterprise_connectors::ContentAnalysisResponse());

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UPLOAD_FAILURE);
}

TEST_F(CloudBinaryUploadServiceTest, FailsWhenUploadFails_Authentication) {
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);

  ExpectNetworkResponse(false, enterprise_connectors::ContentAnalysisResponse(),
                        net::HTTP_UNAUTHORIZED);

  service_->MaybeUploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  // The auth request failing to upload means that the result for the real
  // request is UNAUTHORIZED.
  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UNAUTHORIZED);
}

TEST_F(CloudBinaryUploadServiceTest,
       ReturnsIncompleteResponseWhenTheFirstResponseIsIncomplete) {
  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());

  MockRequest* raw_request = request.get();
  UploadForDeepScanning(std::move(request));
  BinaryUploadService::Request::Id request_id = raw_request->id();
  content::RunAllTasksUntilIdle();

  // Simulate receiving the DLP response
  enterprise_connectors::ContentAnalysisResponse response;
  auto* dlp_result = response.add_results();
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  ReceiveMessageForRequest(request_id, response);
  content::RunAllTasksUntilIdle();

  // All the scans should be synchronous, so the result is marked as incomplete.
  EXPECT_EQ(
      scanning_result,
      enterprise_connectors::ScanRequestUploadResult::INCOMPLETE_RESPONSE);

  // Simulate receiving the malware response
  response.clear_results();
  auto* malware_result = response.add_results();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  ReceiveMessageForRequest(request_id, response);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(
      scanning_result,
      enterprise_connectors::ScanRequestUploadResult::INCOMPLETE_RESPONSE);
}

TEST_F(CloudBinaryUploadServiceTest, TimesOut) {
  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  // No response is returned from the server.
  ExpectNetworkResponse(true, std::nullopt);

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardBy(base::Seconds(300));

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::TIMEOUT);
}

TEST_F(CloudBinaryUploadServiceTest, OnUploadCompleteAfterTimeout) {
  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  // No response is returned from the server.
  ExpectNetworkResponse(true, std::nullopt);

  MockRequest* raw_request = request.get();
  UploadForDeepScanning(std::move(request));
  BinaryUploadService::Request::Id request_id = raw_request->id();
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardBy(base::Seconds(300));
  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::TIMEOUT);

  // Expect nothing to change if the upload finishes after the timeout.
  ReceiveResponseFromUpload(request_id, false, "");
  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::TIMEOUT);
}

TEST_F(CloudBinaryUploadServiceTest, OnGetResponseAfterTimeout) {
  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  // No response is returned from the server.
  ExpectNetworkResponse(true, std::nullopt);

  MockRequest* raw_request = request.get();
  UploadForDeepScanning(std::move(request));
  BinaryUploadService::Request::Id request_id = raw_request->id();
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardBy(base::Seconds(300));
  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::TIMEOUT);

  // Expect nothing to change if we get a message after the timeout.
  ReceiveMessageForRequest(request_id,
                           enterprise_connectors::ContentAnalysisResponse());
  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::TIMEOUT);
}

TEST_F(CloudBinaryUploadServiceTest,
       OnUnauthorized_RetrySucceeds_ShouldReturnSuccess) {
  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  enterprise_connectors::ContentAnalysisResponse simulated_response;
  auto* dlp_result = simulated_response.add_results();
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  auto* malware_result = simulated_response.add_results();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");

  // Let the new auth request go through.
  ExpectNetworkResponse(true, simulated_response);

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UNKNOWN);
  // Set the previous auth check to be unauthorized.
  UploadForDeepScanning(std::move(request),
                        /*auth_check_for_enterprise=*/enterprise_connectors::
                            ScanRequestUploadResult::UNAUTHORIZED);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest,
       OnUnauthorized_RetryFails_ShouldReturnUnAuthorized) {
  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  // Fail the new auth request.
  ExpectNetworkResponse(false, enterprise_connectors::ContentAnalysisResponse(),
                        net::HTTP_UNAUTHORIZED);
  UploadForDeepScanning(std::move(request),
                        /*auth_check_for_enterprise=*/enterprise_connectors::
                            ScanRequestUploadResult::UNAUTHORIZED);

  content::RunAllTasksUntilIdle();
  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UNAUTHORIZED);
}

TEST_F(CloudBinaryUploadServiceTest,
       TwoUploads_AuthCheckFailsThenSucceeds_ShouldReturnSuccess) {
  enterprise_connectors::ScanRequestUploadResult scanning_result_1 =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response_1;
  std::unique_ptr<MockRequest> request_1 =
      MakeRequest(&scanning_result_1, &scanning_response_1,
                  /*is_advanced_protection*/ false);

  // Fail the first auth request.
  ExpectNetworkResponse(false, enterprise_connectors::ContentAnalysisResponse(),
                        net::HTTP_UNAUTHORIZED);
  service_->MaybeUploadForDeepScanning(std::move(request_1));

  content::RunAllTasksUntilIdle();
  EXPECT_EQ(scanning_result_1,
            enterprise_connectors::ScanRequestUploadResult::UNAUTHORIZED);

  // Prepare the second scanning request.
  enterprise_connectors::ScanRequestUploadResult scanning_result_2 =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response_2;
  std::unique_ptr<MockRequest> request_2 =
      MakeRequest(&scanning_result_2, &scanning_response_2,
                  /*is_advanced_protection*/ false);
  request_2->add_tag("dlp");
  request_2->add_tag("malware");

  enterprise_connectors::ContentAnalysisResponse simulated_response;
  auto* dlp_result = simulated_response.add_results();
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  auto* malware_result = simulated_response.add_results();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");

  // Let the second auth request go through.
  ExpectNetworkResponse(true, simulated_response);
  service_->MaybeUploadForDeepScanning(std::move(request_2));

  content::RunAllTasksUntilIdle();
  EXPECT_EQ(scanning_result_2,
            enterprise_connectors::ScanRequestUploadResult::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, OnGetSynchronousResponse) {
  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  enterprise_connectors::ContentAnalysisResponse simulated_response;
  auto* dlp_result = simulated_response.add_results();
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  auto* malware_result = simulated_response.add_results();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  ExpectNetworkResponse(true, simulated_response);

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest,
       ReturnsSynchronouslyWithIncompleteResponse) {
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  base::RunLoop run_loop;
  SetRequestDoneClosure(run_loop.QuitClosure());
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  enterprise_connectors::ContentAnalysisResponse simulated_response;
  auto* dlp_result = simulated_response.add_results();
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  ExpectNetworkResponse(true, simulated_response);

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();
  run_loop.Run();

  EXPECT_EQ(
      scanning_result,
      enterprise_connectors::ScanRequestUploadResult::INCOMPLETE_RESPONSE);
}

TEST_F(CloudBinaryUploadServiceTest, IsAuthorizedValidTimer) {
  // The 24 hours timer should be started on the first IsAuthorized call.
  ValidateAuthorizationTimerIdle();
  service_->IsAuthorized(
      GURL(), /*per_profile_request*/ false, base::DoNothing(),
      "fake_device_token",
      enterprise_connectors::AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED);
  ValidateAuthorizationTimerStarted();
}

TEST_F(CloudBinaryUploadServiceTest, IsAuthorizedMultipleDMTokens) {
  service_->SetAuthForTesting(
      "valid_dm_token",
      enterprise_connectors::ScanRequestUploadResult::SUCCESS);
  service_->SetAuthForTesting(
      "invalid_dm_token",
      enterprise_connectors::ScanRequestUploadResult::UNAUTHORIZED);
  // Fail all requests so that the auth retry logic will not change the pre-set
  // authorization status.
  ExpectNetworkResponse(false, enterprise_connectors::ContentAnalysisResponse(),
                        net::HTTP_UNAUTHORIZED);

  for (auto connector : {
           enterprise_connectors::AnalysisConnector::
               ANALYSIS_CONNECTOR_UNSPECIFIED,
           enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY,
           enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
           enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
           enterprise_connectors::AnalysisConnector::PRINT,
#if BUILDFLAG(IS_CHROMEOS)
           enterprise_connectors::AnalysisConnector::FILE_TRANSFER,
#endif
       }) {
    service_->IsAuthorized(
        GURL(), /*per_profile_request*/ false,
        base::BindOnce([](enterprise_connectors::ScanRequestUploadResult
                              auth_check_result) {
          EXPECT_EQ(auth_check_result,
                    enterprise_connectors::ScanRequestUploadResult::SUCCESS);
        }),
        "valid_dm_token", connector);
    service_->IsAuthorized(
        GURL(), /*per_profile_request*/ false,
        base::BindOnce([](enterprise_connectors::ScanRequestUploadResult
                              auth_check_result) {
          EXPECT_EQ(
              auth_check_result,
              enterprise_connectors::ScanRequestUploadResult::UNAUTHORIZED);
        }),
        "invalid_dm_token", connector);
  }
}

TEST_F(CloudBinaryUploadServiceTest,
       AdvancedProtectionMalwareRequestAuthorized) {
  AdvancedProtectionStatusManagerFactory::GetForProfile(&profile_)
      ->SetAdvancedProtectionStatusForTesting(/*enrolled=*/true);

  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ true);
  request->add_tag("malware");

  enterprise_connectors::ContentAnalysisResponse simulated_response;
  auto* dlp_result = simulated_response.add_results();
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  auto* malware_result = simulated_response.add_results();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  ExpectNetworkResponse(true, simulated_response);

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UNKNOWN);

  UploadForDeepScanning(std::move(request),
                        /*auth_check_for_enterprise=*/enterprise_connectors::
                            ScanRequestUploadResult::UNAUTHORIZED);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, AdvancedProtectionDlpRequestUnauthorized) {
  AdvancedProtectionStatusManagerFactory::GetForProfile(&profile_)
      ->SetAdvancedProtectionStatusForTesting(/*enrolled=*/true);

  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ true);

  request->add_tag("dlp");
  request->add_tag("malware");

  enterprise_connectors::ContentAnalysisResponse simulated_response;
  auto* dlp_result = simulated_response.add_results();
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  auto* malware_result = simulated_response.add_results();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  ExpectNetworkResponse(true, simulated_response);

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UNKNOWN);

  UploadForDeepScanning(std::move(request),
                        /*auth_check_for_enterprise=*/enterprise_connectors::
                            ScanRequestUploadResult::UNAUTHORIZED);

  // The result is set synchronously on unauthorized requests, so it is
  // UNAUTHORIZED before and after waiting.
  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UNAUTHORIZED);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UNAUTHORIZED);
}

TEST_F(CloudBinaryUploadServiceTest,
       DeepScanESBEnabledEnhancedProtectionMalwareRequestAuthorized) {
  safe_browsing::SetEnhancedProtectionPrefForTests(profile_.GetPrefs(),
                                                   /*value*/ true);

  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ true);
  request->add_tag("malware");

  enterprise_connectors::ContentAnalysisResponse simulated_response;

  auto* malware_result = simulated_response.add_results();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  ExpectNetworkResponse(true, simulated_response);

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::UNKNOWN);

  UploadForDeepScanning(std::move(request),
                        /*auth_check_for_enterprise=*/enterprise_connectors::
                            ScanRequestUploadResult::UNAUTHORIZED);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, ConnectorUrlParams) {
  {
    MockRequest request(
        base::DoNothing(),
        CloudAnalysisSettingsWithUrl(
            "https://safebrowsing.google.com/safebrowsing/uploads/scan"));
    request.set_device_token("fake_token1");
    request.set_analysis_connector(enterprise_connectors::FILE_ATTACHED);
    request.add_tag("dlp");
    request.add_tag("malware");

    ASSERT_EQ(GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
                   "scan?device_token=fake_token1&connector=OnFileAttached&tag="
                   "dlp&tag=malware"),
              request.GetUrlWithParams());
  }
  {
    MockRequest request(
        base::DoNothing(),
        CloudAnalysisSettingsWithUrl(
            "https://safebrowsing.google.com/safebrowsing/uploads/scan"));
    request.set_device_token("fake_token2");
    request.set_analysis_connector(enterprise_connectors::FILE_DOWNLOADED);
    request.add_tag("malware");

    ASSERT_EQ(GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
                   "scan?device_token=fake_token2&connector=OnFileDownloaded&"
                   "tag=malware"),
              request.GetUrlWithParams());
  }
  {
    MockRequest request(
        base::DoNothing(),
        CloudAnalysisSettingsWithUrl(
            "https://safebrowsing.google.com/safebrowsing/uploads/scan"));
    request.set_device_token("fake_token3");
    request.set_analysis_connector(enterprise_connectors::BULK_DATA_ENTRY);
    request.add_tag("dlp");

    ASSERT_EQ(
        GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
             "scan?device_token=fake_token3&connector=OnBulkDataEntry&tag=dlp"),
        request.GetUrlWithParams());
  }
  {
    MockRequest request(
        base::DoNothing(),
        CloudAnalysisSettingsWithUrl(
            "https://safebrowsing.google.com/safebrowsing/uploads/scan"));
    request.set_device_token("fake_token4");

    ASSERT_EQ(GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
                   "scan?device_token=fake_token4"),
              request.GetUrlWithParams());
  }
  {
    MockRequest request(
        base::DoNothing(),
        CloudAnalysisSettingsWithUrl(
            "https://safebrowsing.google.com/safebrowsing/uploads/scan"));
    request.set_device_token("fake_token5");
    request.set_analysis_connector(
        enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED);

    ASSERT_EQ(GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
                   "scan?device_token=fake_token5"),
              request.GetUrlWithParams());
  }
#if BUILDFLAG(IS_CHROMEOS)
  {
    MockRequest request(
        base::DoNothing(),
        CloudAnalysisSettingsWithUrl(
            "https://safebrowsing.google.com/safebrowsing/uploads/scan"));
    request.set_device_token("fake_token6");
    request.set_analysis_connector(enterprise_connectors::FILE_TRANSFER);
    request.add_tag("malware");

    ASSERT_EQ(GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
                   "scan?device_token=fake_token6&connector=OnFileTransfer&"
                   "tag=malware"),
              request.GetUrlWithParams());
  }
#endif
}

TEST_F(CloudBinaryUploadServiceTest, UrlOverride) {
  MockRequest request(
      base::DoNothing(),
      CloudAnalysisSettingsWithUrl(
          "https://safebrowsing.google.com/safebrowsing/uploads/scan"));
  request.set_device_token("fake_token");
  request.set_analysis_connector(enterprise_connectors::FILE_ATTACHED);
  request.add_tag("dlp");
  request.add_tag("malware");

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("binary-upload-service-url",
                                  "https://test.com/scan");
  policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();

  ASSERT_EQ(GURL("https://test.com/scan?device_token=fake_token&connector="
                 "OnFileAttached&tag=dlp&tag=malware"),
            request.GetUrlWithParams());

  command_line->RemoveSwitch("binary-upload-service-url");

  // The flag being empty should not affect the URL at all.
  ASSERT_EQ(GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
                 "scan?device_token=fake_token&connector=OnFileAttached&tag="
                 "dlp&tag=malware"),
            request.GetUrlWithParams());
}

TEST_F(CloudBinaryUploadServiceTest, GetUploadUrl) {
  // testing enterprise scenario
  ASSERT_EQ(CloudBinaryUploadService::GetUploadUrl(
                /*is_consumer_scan_eligible */ false),
            GURL("https://safebrowsing.google.com/safebrowsing/uploads/scan"));

  // testing APP scenario with Deep Scanning for ESB Feature enabled
  ASSERT_EQ(
      CloudBinaryUploadService::GetUploadUrl(
          /*is_consumer_scan_eligible */ true),
      GURL("https://safebrowsing.google.com/safebrowsing/uploads/consumer"));
}

TEST_F(CloudBinaryUploadServiceTest, RequestQueue) {
  enterprise_connectors::ScanRequestUploadResult scanning_result =
      enterprise_connectors::ScanRequestUploadResult::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::vector<MockRequest*> requests;

  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());

  // Uploading 2*max requests before any response is received ensures that the
  // queue is populated and processed correctly.
  for (size_t i = 0;
       i < 2 * CloudBinaryUploadService::GetParallelActiveRequestsMax(); ++i) {
    std::unique_ptr<MockRequest> request = MakeRequest(
        &scanning_result, &scanning_response, /*is_advanced_protection*/ false);
    request->add_tag("dlp");
    request->add_tag("malware");
    requests.push_back(request.get());
    UploadForDeepScanning(std::move(request));
  }

  for (MockRequest* request : requests) {
    enterprise_connectors::ContentAnalysisResponse simulated_response;
    auto* dlp_result = simulated_response.add_results();
    dlp_result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    dlp_result->set_tag("dlp");
    auto* malware_result = simulated_response.add_results();
    malware_result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    malware_result->set_tag("malware");
    ReceiveResponseFromUpload(request->id(), true,
                              simulated_response.SerializeAsString());
  }
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result,
            enterprise_connectors::ScanRequestUploadResult::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, TestMaxParallelRequestsFlag) {
  EXPECT_EQ(15UL, CloudBinaryUploadService::GetParallelActiveRequestsMax());

  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "wp-max-parallel-active-requests", "10");
    EXPECT_EQ(10UL, CloudBinaryUploadService::GetParallelActiveRequestsMax());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "wp-max-parallel-active-requests", "100");
    EXPECT_EQ(100UL, CloudBinaryUploadService::GetParallelActiveRequestsMax());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "wp-max-parallel-active-requests", "0");
    EXPECT_EQ(15UL, CloudBinaryUploadService::GetParallelActiveRequestsMax());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "wp-max-parallel-active-requests", "foo");
    EXPECT_EQ(15UL, CloudBinaryUploadService::GetParallelActiveRequestsMax());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "wp-max-parallel-active-requests", "-1");
    EXPECT_EQ(15UL, CloudBinaryUploadService::GetParallelActiveRequestsMax());
  }
}

TEST_F(CloudBinaryUploadServiceTest, EmptyFileRequest) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());

  base::RunLoop run_loop;
  std::unique_ptr<FileAnalysisRequest> request =
      std::make_unique<FileAnalysisRequest>(
          enterprise_connectors::AnalysisSettings(), file_path,
          file_path.BaseName(), "fake/mimetype", false,
          base::BindLambdaForTesting(
              [&run_loop](
                  enterprise_connectors::ScanRequestUploadResult result,
                  enterprise_connectors::ContentAnalysisResponse response) {
                ASSERT_EQ(
                    enterprise_connectors::ScanRequestUploadResult::SUCCESS,
                    result);
                run_loop.Quit();
              }));
  request->set_device_token("fake_device_token");

  UploadForDeepScanning(std::move(request));
  run_loop.Run();
}

TEST_F(CloudBinaryUploadServiceTest, RunsStartCallback) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());

  bool was_started = false;
  base::RunLoop run_loop;
  std::unique_ptr<FileAnalysisRequest> request =
      std::make_unique<FileAnalysisRequest>(
          enterprise_connectors::AnalysisSettings(), file_path,
          file_path.BaseName(), "fake/mimetype", false, base::DoNothing(),
          base::BindLambdaForTesting(
              [&run_loop,
               &was_started](const BinaryUploadService::Request& request) {
                was_started = true;
                run_loop.Quit();
              }));
  request->set_device_token("fake_device_token");

  UploadForDeepScanning(std::move(request));
  run_loop.Run();

  EXPECT_TRUE(was_started);
}

TEST_F(CloudBinaryUploadServiceTest, VerifyBlockingSet) {
  enterprise_connectors::ScanRequestUploadResult scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request = MakeRequest(
      &scanning_result, &scanning_response, /*is_advanced_protection*/ false);

  request->set_blocking(true);
  ASSERT_TRUE(request->blocking());

  request->set_blocking(false);
  ASSERT_FALSE(request->blocking());
}

}  // namespace safe_browsing
