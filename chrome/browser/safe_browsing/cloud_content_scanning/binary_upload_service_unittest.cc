// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

class MockRequest : public BinaryUploadService::Request {
 public:
  MockRequest(BinaryUploadService::ContentAnalysisCallback callback,
              const GURL& url)
      : BinaryUploadService::Request(std::move(callback), url) {}
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
    std::move(callback_).Run(should_succeed_, serialized_response);
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

  std::unique_ptr<MultipartUploadRequest> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
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
  ~MockBinaryFCMService() = default;

  MOCK_METHOD1(GetInstanceID,
               void(BinaryFCMService::GetInstanceIDCallback callback));
  MOCK_METHOD2(UnregisterInstanceID,
               void(const std::string& token,
                    BinaryFCMService::UnregisterInstanceIDCallback callback));
};

class BinaryUploadServiceTest : public testing::Test {
 public:
  BinaryUploadServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        fake_factory_(true, enterprise_connectors::ContentAnalysisResponse()) {
    MultipartUploadRequest::RegisterFactoryForTests(&fake_factory_);
    auto fcm_service = std::make_unique<MockBinaryFCMService>();
    fcm_service_ = fcm_service.get();

    // Since we have mocked the MultipartUploadRequest, we don't need a
    // URLLoaderFactory, so pass nullptr here.
    service_ = std::make_unique<BinaryUploadService>(nullptr, &profile_,
                                                     std::move(fcm_service));
  }
  ~BinaryUploadServiceTest() override {
    MultipartUploadRequest::RegisterFactoryForTests(nullptr);
  }

  void ExpectNetworkResponse(
      bool should_succeed,
      enterprise_connectors::ContentAnalysisResponse response) {
    fake_factory_ = FakeMultipartUploadRequestFactory(should_succeed, response);
  }

  void ExpectInstanceID(std::string id) {
    EXPECT_CALL(*fcm_service_, GetInstanceID(_))
        .WillOnce(
            Invoke([id](BinaryFCMService::GetInstanceIDCallback callback) {
              std::move(callback).Run(id);
            }));
    EXPECT_CALL(*fcm_service_, UnregisterInstanceID(id, _))
        .WillOnce(
            Invoke([](const std::string& token,
                      BinaryFCMService::UnregisterInstanceIDCallback callback) {
              std::move(callback).Run(true);
            }));
  }

  void UploadForDeepScanning(
      std::unique_ptr<BinaryUploadService::Request> request,
      bool authorized_for_enterprise = true) {
    service_->SetAuthForTesting(authorized_for_enterprise);
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
    service_->OnUploadComplete(request, success, response);
  }

  void ServiceWithNoFCMConnection() {
    service_ = std::make_unique<BinaryUploadService>(
        nullptr, &profile_, std::unique_ptr<BinaryFCMService>(nullptr));
  }

  std::unique_ptr<MockRequest> MakeRequest(
      BinaryUploadService::Result* scanning_result,
      enterprise_connectors::ContentAnalysisResponse* scanning_response,
      bool is_app) {
    auto request = std::make_unique<MockRequest>(
        base::BindOnce(
            [](BinaryUploadService::Result* target_result,
               enterprise_connectors::ContentAnalysisResponse* target_response,
               BinaryUploadService::Result result,
               enterprise_connectors::ContentAnalysisResponse response) {
              *target_result = result;
              *target_response = response;
            },
            scanning_result, scanning_response),
        GURL());
    if (!is_app)
      request->set_device_token("fake_device_token");
    ON_CALL(*request, GetRequestData(_))
        .WillByDefault(
            Invoke([](BinaryUploadService::Request::DataCallback callback) {
              BinaryUploadService::Request::Data data;
              data.contents = "contents";
              std::move(callback).Run(BinaryUploadService::Result::SUCCESS,
                                      data);
            }));
    return request;
  }

  void ValidateAuthorizationTimerIdle() {
    EXPECT_FALSE(service_->timer_.IsRunning());
    EXPECT_EQ(base::TimeDelta::FromHours(0),
              service_->timer_.GetCurrentDelay());
  }

  void ValidateAuthorizationTimerStarted() {
    EXPECT_TRUE(service_->timer_.IsRunning());
    EXPECT_EQ(base::TimeDelta::FromHours(24),
              service_->timer_.GetCurrentDelay());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BinaryUploadService> service_;
  MockBinaryFCMService* fcm_service_;
  FakeMultipartUploadRequestFactory fake_factory_;
};

TEST_F(BinaryUploadServiceTest, FailsForLargeFile) {
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
                                    data);
          }));
  UploadForDeepScanning(std::move(request));

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::FILE_TOO_LARGE);
}

TEST_F(BinaryUploadServiceTest, FailsWhenMissingInstanceID) {
  BinaryUploadService::Result scanning_result;
  enterprise_connectors::ContentAnalysisResponse scanning_response;

  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response, /*is_app*/ false);

  ExpectInstanceID(BinaryFCMService::kInvalidId);

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::FAILED_TO_GET_TOKEN);
}

TEST_F(BinaryUploadServiceTest, FailsWhenUploadFails) {
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

TEST_F(BinaryUploadServiceTest, HoldsScanResponsesUntilAllReady) {
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

TEST_F(BinaryUploadServiceTest, TimesOut) {
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
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(300));

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(BinaryUploadServiceTest, OnInstanceIDAfterTimeout) {
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
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(300));

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);

  // Expect nothing to change if the InstanceID returns after the timeout.
  std::move(instance_id_callback).Run("valid id");
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(BinaryUploadServiceTest, OnUploadCompleteAfterTimeout) {
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
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(300));
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);

  // Expect nothing to change if the upload finishes after the timeout.
  ReceiveResponseFromUpload(raw_request, false, "");
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(BinaryUploadServiceTest, OnGetResponseAfterTimeout) {
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
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(300));
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);

  // Expect nothing to change if we get a message after the timeout.
  ReceiveMessageForRequest(raw_request,
                           enterprise_connectors::ContentAnalysisResponse());
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(BinaryUploadServiceTest, OnUnauthorized) {
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

TEST_F(BinaryUploadServiceTest, OnGetSynchronousResponse) {
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

TEST_F(BinaryUploadServiceTest, ReturnsAsynchronouslyWithNoFCM) {
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

TEST_F(BinaryUploadServiceTest, IsAuthorizedValidTimer) {
  // The 24 hours timer should be started on the first IsAuthorized call.
  ValidateAuthorizationTimerIdle();
  service_->IsAuthorized(GURL(), base::DoNothing());
  ValidateAuthorizationTimerStarted();
}

TEST_F(BinaryUploadServiceTest, AdvancedProtectionMalwareRequestAuthorized) {
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

TEST_F(BinaryUploadServiceTest, AdvancedProtectionDlpRequestUnauthorized) {
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

TEST_F(BinaryUploadServiceTest, ConnectorUrlParams) {
  {
    MockRequest request(
        base::DoNothing(),
        GURL("https://safebrowsing.google.com/safebrowsing/uploads/scan"));
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
        GURL("https://safebrowsing.google.com/safebrowsing/uploads/scan"));
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
        GURL("https://safebrowsing.google.com/safebrowsing/uploads/scan"));
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
        GURL("https://safebrowsing.google.com/safebrowsing/uploads/scan"));
    request.set_device_token("fake_token4");

    ASSERT_EQ(GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
                   "scan?device_token=fake_token4"),
              request.GetUrlWithParams());
  }
  {
    MockRequest request(
        base::DoNothing(),
        GURL("https://safebrowsing.google.com/safebrowsing/uploads/scan"));
    request.set_device_token("fake_token5");
    request.set_analysis_connector(
        enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED);

    ASSERT_EQ(GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
                   "scan?device_token=fake_token5"),
              request.GetUrlWithParams());
  }
}

TEST_F(BinaryUploadServiceTest, UrlOverride) {
  MockRequest request(
      base::DoNothing(),
      GURL("https://safebrowsing.google.com/safebrowsing/uploads/scan"));
  request.set_device_token("fake_token");
  request.set_analysis_connector(enterprise_connectors::FILE_ATTACHED);
  request.add_tag("dlp");
  request.add_tag("malware");

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII("binary-upload-service-url",
                                  "https://test.com/scan");

  // The flag should only work on Chromium builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ASSERT_EQ(GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
                 "scan?device_token=fake_token&connector=OnFileAttached&tag="
                 "dlp&tag=malware"),
            request.GetUrlWithParams());
#else
  ASSERT_EQ(GURL("https://test.com/scan?device_token=fake_token&connector="
                 "OnFileAttached&tag=dlp&tag=malware"),
            request.GetUrlWithParams());
#endif

  command_line->RemoveSwitch("binary-upload-service-url");

  // The flag being empty should not affect the URL at all, on either builds.
  ASSERT_EQ(GURL("https://safebrowsing.google.com/safebrowsing/uploads/"
                 "scan?device_token=fake_token&connector=OnFileAttached&tag="
                 "dlp&tag=malware"),
            request.GetUrlWithParams());
}

}  // namespace safe_browsing
