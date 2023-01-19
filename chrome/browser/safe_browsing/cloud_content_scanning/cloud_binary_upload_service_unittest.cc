// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

enterprise_connectors::CloudAnalysisSettings CloudAnalysisSettingsWithUrl(
    const std::string& url) {
  enterprise_connectors::CloudAnalysisSettings settings;
  settings.analysis_url = GURL(url);
  return settings;
}

}  // namespace

using ::testing::_;
using ::testing::Invoke;
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

class FakeMultipartUploadRequest : public MultipartUploadRequest {
 public:
  FakeMultipartUploadRequest(
      bool should_succeed,
      enterprise_connectors::ContentAnalysisResponse response,
      Callback callback)
      : MultipartUploadRequest(nullptr,
                               GURL(),
                               /*metadata=*/"",
                               /*data=*/"",
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               base::DoNothing()),
        should_succeed_(should_succeed),
        response_(response),
        callback_(std::move(callback)) {}

  void Start() override {
    std::string serialized_response;
    response_.SerializeToString(&serialized_response);
    std::move(callback_).Run(should_succeed_, should_succeed_ ? 200 : 401,
                             serialized_response);
  }

 private:
  bool should_succeed_;
  enterprise_connectors::ContentAnalysisResponse response_;
  Callback callback_;
};

class FakeMultipartUploadRequestFactory : public MultipartUploadRequestFactory {
 public:
  FakeMultipartUploadRequestFactory(
      bool should_succeed,
      enterprise_connectors::ContentAnalysisResponse response)
      : should_succeed_(should_succeed), response_(response) {}

  std::unique_ptr<MultipartUploadRequest> CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback) override {
    return std::make_unique<FakeMultipartUploadRequest>(
        should_succeed_, response_, std::move(callback));
  }

  std::unique_ptr<MultipartUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& path,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback) override {
    return std::make_unique<FakeMultipartUploadRequest>(
        should_succeed_, response_, std::move(callback));
  }

  std::unique_ptr<MultipartUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback) override {
    return std::make_unique<FakeMultipartUploadRequest>(
        should_succeed_, response_, std::move(callback));
  }

 private:
  bool should_succeed_;
  enterprise_connectors::ContentAnalysisResponse response_;
};

class MockBinaryFCMService : public BinaryFCMService {
 public:
  MockBinaryFCMService() = default;
  ~MockBinaryFCMService() override = default;

  MOCK_METHOD1(GetInstanceID,
               void(BinaryFCMService::GetInstanceIDCallback callback));
  MOCK_METHOD2(UnregisterInstanceID,
               void(const std::string& token,
                    BinaryFCMService::UnregisterInstanceIDCallback callback));
  MOCK_METHOD0(Connected, bool());
};

class CloudBinaryUploadServiceTest : public testing::Test {
 public:
  CloudBinaryUploadServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        fake_factory_(true, enterprise_connectors::ContentAnalysisResponse()) {
    MultipartUploadRequest::RegisterFactoryForTests(&fake_factory_);
    auto fcm_service = std::make_unique<NiceMock<MockBinaryFCMService>>();
    fcm_service_ = fcm_service.get();

    // Since we have mocked the MultipartUploadRequest, we don't need a
    // URLLoaderFactory, so pass nullptr here.
    service_ = std::make_unique<CloudBinaryUploadService>(
        nullptr, &profile_, std::move(fcm_service));

    EXPECT_CALL(*fcm_service_, Connected()).WillRepeatedly(Return(true));
  }
  ~CloudBinaryUploadServiceTest() override {
    MultipartUploadRequest::RegisterFactoryForTests(nullptr);
  }

  void ExpectNetworkResponse(
      bool should_succeed,
      enterprise_connectors::ContentAnalysisResponse response) {
    fake_factory_ = FakeMultipartUploadRequestFactory(should_succeed, response);
  }

  void ExpectInstanceID(std::string id, int times = 1) {
    EXPECT_CALL(*fcm_service_, GetInstanceID(_))
        .Times(times)
        .WillRepeatedly(
            Invoke([id](BinaryFCMService::GetInstanceIDCallback callback) {
              std::move(callback).Run(id);
            }));

    if (id == BinaryFCMService::kInvalidId) {
      EXPECT_CALL(*fcm_service_, UnregisterInstanceID(id, _)).Times(0);
    } else {
      EXPECT_CALL(*fcm_service_, UnregisterInstanceID(id, _))
          .Times(times)
          .WillRepeatedly(Invoke(
              [](const std::string& token,
                 BinaryFCMService::UnregisterInstanceIDCallback callback) {
                std::move(callback).Run(true);
              }));
    }
  }

  void UploadForDeepScanning(
      std::unique_ptr<BinaryUploadService::Request> request,
      bool authorized_for_enterprise = true) {
    service_->SetAuthForTesting("fake_device_token", authorized_for_enterprise);
    service_->MaybeUploadForDeepScanning(std::move(request));
  }

  void ReceiveMessageForRequest(
      BinaryUploadService::Request* request,
      const enterprise_connectors::ContentAnalysisResponse& response) {
    service_->OnGetResponse(request, response);
  }

  void ReceiveResponseFromUpload(BinaryUploadService::Request* request,
                                 bool success,
                                 const std::string& response) {
    service_->OnUploadComplete(request, success, success ? 200 : 401, response);
  }

  void ServiceWithNoFCMConnection() {
    service_ = std::make_unique<CloudBinaryUploadService>(
        nullptr, &profile_, std::unique_ptr<BinaryFCMService>(nullptr));
  }

  void ServiceWithDisconnectedFCM() {
    EXPECT_CALL(*fcm_service_, Connected()).WillRepeatedly(Return(false));
  }

  std::unique_ptr<MockRequest> MakeRequest(
      BinaryUploadService::Result* scanning_result,
      enterprise_connectors::ContentAnalysisResponse* scanning_response,
      bool is_app) {
    auto request = std::make_unique<NiceMock<MockRequest>>(
        base::BindOnce(
            [](BinaryUploadService::Result* target_result,
               enterprise_connectors::ContentAnalysisResponse* target_response,
               BinaryUploadService::Result result,
               enterprise_connectors::ContentAnalysisResponse response) {
              *target_result = result;
              *target_response = response;
            },
            scanning_result, scanning_response),
        enterprise_connectors::CloudAnalysisSettings());
    if (!is_app)
      request->set_device_token("fake_device_token");
    ON_CALL(*request, GetRequestData(_))
        .WillByDefault(
            Invoke([](BinaryUploadService::Request::DataCallback callback) {
              BinaryUploadService::Request::Data data;
              data.contents = "contents";
              data.size = data.contents.size();
              std::move(callback).Run(BinaryUploadService::Result::SUCCESS,
                                      std::move(data));
            }));
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

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<CloudBinaryUploadService> service_;
  raw_ptr<MockBinaryFCMService> fcm_service_;
  FakeMultipartUploadRequestFactory fake_factory_;
};

TEST_F(CloudBinaryUploadServiceTest, FailsForLargeFile) {
  BinaryUploadService::Result scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  ExpectInstanceID("valid id");
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
  ON_CALL(*request, GetRequestData(_))
      .WillByDefault(
          Invoke([](BinaryUploadService::Request::DataCallback callback) {
            BinaryUploadService::Request::Data data;
            std::move(callback).Run(BinaryUploadService::Result::FILE_TOO_LARGE,
                                    std::move(data));
          }));
  UploadForDeepScanning(std::move(request));

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::FILE_TOO_LARGE);
}

TEST_F(CloudBinaryUploadServiceTest, FailsWhenMissingInstanceID) {
  BinaryUploadService::Result scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);

  ExpectInstanceID(BinaryFCMService::kInvalidId);

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::FAILED_TO_GET_TOKEN);
}

TEST_F(CloudBinaryUploadServiceTest,
       FailsWhenMissingInstanceID_Authentication) {
  BinaryUploadService::Result scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);

  ExpectInstanceID(BinaryFCMService::kInvalidId);

  // The auth request never requests an instance ID, so it should get a normal
  // response.
  base::RunLoop run_loop;
  service_->IsAuthorized(
      GURL(), /*per_profile_request*/ false,
      base::BindLambdaForTesting([&run_loop](bool authorized) {
        EXPECT_TRUE(authorized);
        run_loop.Quit();
      }),
      "fake_device_token",
      enterprise_connectors::AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED);
  run_loop.Run();

  service_->MaybeUploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::FAILED_TO_GET_TOKEN);
}

TEST_F(CloudBinaryUploadServiceTest, FailsWhenUploadFails) {
  BinaryUploadService::Result scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(false,
                        enterprise_connectors::ContentAnalysisResponse());

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UPLOAD_FAILURE);
}

TEST_F(CloudBinaryUploadServiceTest, FailsWhenUploadFails_Authentication) {
  BinaryUploadService::Result scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);

  ExpectNetworkResponse(false,
                        enterprise_connectors::ContentAnalysisResponse());

  service_->MaybeUploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  // The auth request failing to upload means that the result for the real
  // request is UNAUTHORIZED.
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNAUTHORIZED);
}

TEST_F(CloudBinaryUploadServiceTest, HoldsScanResponsesUntilAllReady) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());

  MockRequest* raw_request = request.get();
  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  // Simulate receiving the DLP response
  enterprise_connectors::ContentAnalysisResponse response;
  auto* dlp_result = response.add_results();
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  ReceiveMessageForRequest(raw_request, response);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);

  // Simulate receiving the malware response
  response.clear_results();
  auto* malware_result = response.add_results();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  ReceiveMessageForRequest(raw_request, response);
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_response.results().at(0).tag(), "dlp");
  EXPECT_EQ(scanning_response.results().at(1).tag(), "malware");
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, TimesOut) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());
  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardBy(base::Seconds(300));

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(CloudBinaryUploadServiceTest, OnInstanceIDAfterTimeout) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  BinaryFCMService::GetInstanceIDCallback instance_id_callback;
  ON_CALL(*fcm_service_, GetInstanceID(_))
      .WillByDefault(
          Invoke([&instance_id_callback](
                     BinaryFCMService::GetInstanceIDCallback callback) {
            instance_id_callback = std::move(callback);
          }));

  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());
  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardBy(base::Seconds(300));

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);

  // Expect nothing to change if the InstanceID returns after the timeout.
  std::move(instance_id_callback).Run("valid id");
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(CloudBinaryUploadServiceTest, OnUploadCompleteAfterTimeout) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());

  MockRequest* raw_request = request.get();
  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardBy(base::Seconds(300));
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);

  // Expect nothing to change if the upload finishes after the timeout.
  ReceiveResponseFromUpload(raw_request, false, "");
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(CloudBinaryUploadServiceTest, OnGetResponseAfterTimeout) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());

  MockRequest* raw_request = request.get();
  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardBy(base::Seconds(300));
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);

  // Expect nothing to change if we get a message after the timeout.
  ReceiveMessageForRequest(raw_request,
                           enterprise_connectors::ContentAnalysisResponse());
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(CloudBinaryUploadServiceTest, OnUnauthorized) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
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

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);

  UploadForDeepScanning(std::move(request),
                        /*authorized_for_enterprise=*/false);

  // The result is set synchronously on unauthorized requests, so it is
  // UNAUTHORIZED before and after waiting.
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNAUTHORIZED);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNAUTHORIZED);
}

TEST_F(CloudBinaryUploadServiceTest, OnGetSynchronousResponse) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  ExpectInstanceID("valid id");

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

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, ReturnsAsynchronouslyWithNoFCM) {
  ServiceWithNoFCMConnection();

  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  UploadForDeepScanning(std::move(request));

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::FAILED_TO_GET_TOKEN);
}

TEST_F(CloudBinaryUploadServiceTest, ReturnsAsynchronouslyWithDisconnectedFCM) {
  ServiceWithDisconnectedFCM();

  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
  request->add_tag("dlp");
  request->add_tag("malware");

  UploadForDeepScanning(std::move(request));

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::FAILED_TO_GET_TOKEN);
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
  service_->SetAuthForTesting("valid_dm_token", true);
  service_->SetAuthForTesting("invalid_dm_token", false);

  for (auto connector : {
         enterprise_connectors::AnalysisConnector::
             ANALYSIS_CONNECTOR_UNSPECIFIED,
             enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY,
             enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
             enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
             enterprise_connectors::AnalysisConnector::PRINT,
#if BUILDFLAG(IS_CHROMEOS_ASH)
             enterprise_connectors::AnalysisConnector::FILE_TRANSFER,
#endif
       }) {
    service_->IsAuthorized(
        GURL(), /*per_profile_request*/ false,
        base::BindOnce([](bool authorized) { EXPECT_TRUE(authorized); }),
        "valid_dm_token", connector);
    service_->IsAuthorized(
        GURL(), /*per_profile_request*/ false,
        base::BindOnce([](bool authorized) { EXPECT_FALSE(authorized); }),
        "invalid_dm_token", connector);
  }
}

TEST_F(CloudBinaryUploadServiceTest,
       AdvancedProtectionMalwareRequestAuthorized) {
  AdvancedProtectionStatusManagerFactory::GetForProfile(&profile_)
      ->SetAdvancedProtectionStatusForTesting(/*enrolled=*/true);

  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ true);
  request->add_tag("malware");

  ExpectInstanceID("valid id");

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

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);

  UploadForDeepScanning(std::move(request),
                        /*authorized_for_enterprise=*/false);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, AdvancedProtectionDlpRequestUnauthorized) {
  AdvancedProtectionStatusManagerFactory::GetForProfile(&profile_)
      ->SetAdvancedProtectionStatusForTesting(/*enrolled=*/true);

  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ true);

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

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);

  UploadForDeepScanning(std::move(request),
                        /*authorized_for_enterprise=*/false);

  // The result is set synchronously on unauthorized requests, so it is
  // UNAUTHORIZED before and after waiting.
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNAUTHORIZED);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNAUTHORIZED);
}

TEST_F(CloudBinaryUploadServiceTest,
       DeepScanESBEnabledEnhancedProtectionMalwareRequestAuthorized) {
  safe_browsing::SetEnhancedProtectionPrefForTests(profile_.GetPrefs(),
                                                   /*value*/ true);

  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ true);
  request->add_tag("malware");

  ExpectInstanceID("valid id");

  enterprise_connectors::ContentAnalysisResponse simulated_response;

  auto* malware_result = simulated_response.add_results();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  ExpectNetworkResponse(true, simulated_response);

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);

  UploadForDeepScanning(std::move(request),
                        /*authorized_for_enterprise=*/false);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::SUCCESS);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse scanning_response;
  std::vector<MockRequest*> requests;

  ExpectInstanceID(
      "valid id", 2 * CloudBinaryUploadService::GetParallelActiveRequestsMax());
  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());

  // Uploading 2*max requests before any response is received ensures that the
  // queue is populated and processed correctly.
  for (size_t i = 0;
       i < 2 * CloudBinaryUploadService::GetParallelActiveRequestsMax(); ++i) {
    std::unique_ptr<MockRequest> request =
        MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);
    request->add_tag("dlp");
    request->add_tag("malware");
    requests.push_back(request.get());
    UploadForDeepScanning(std::move(request));
  }

  content::RunAllTasksUntilIdle();

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
    ReceiveMessageForRequest(request, simulated_response);
  }
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::SUCCESS);
}

TEST_F(CloudBinaryUploadServiceTest, TestMaxParallelRequestsFlag) {
  EXPECT_EQ(5UL, CloudBinaryUploadService::GetParallelActiveRequestsMax());

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
    EXPECT_EQ(5UL, CloudBinaryUploadService::GetParallelActiveRequestsMax());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "wp-max-parallel-active-requests", "foo");
    EXPECT_EQ(5UL, CloudBinaryUploadService::GetParallelActiveRequestsMax());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "wp-max-parallel-active-requests", "-1");
    EXPECT_EQ(5UL, CloudBinaryUploadService::GetParallelActiveRequestsMax());
  }
}

TEST_F(CloudBinaryUploadServiceTest, EmptyFileRequest) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(true, enterprise_connectors::ContentAnalysisResponse());

  base::RunLoop run_loop;
  std::unique_ptr<FileAnalysisRequest> request =
      std::make_unique<FileAnalysisRequest>(
          enterprise_connectors::AnalysisSettings(), file_path,
          file_path.BaseName(), "fake/mimetype", false,
          base::BindLambdaForTesting(
              [&run_loop](
                  BinaryUploadService::Result result,
                  enterprise_connectors::ContentAnalysisResponse response) {
                ASSERT_EQ(BinaryUploadService::Result::SUCCESS, result);
                run_loop.Quit();
              }));
  request->set_device_token("fake_device_token");

  UploadForDeepScanning(std::move(request));
  run_loop.Run();
}

}  // namespace safe_browsing
