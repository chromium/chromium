// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/download_protection/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/download_protection/multipart_uploader.h"
#include "components/safe_browsing/proto/webprotect.pb.h"
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
  explicit MockRequest(BinaryUploadService::Callback callback)
      : BinaryUploadService::Request(std::move(callback)) {}
  MOCK_METHOD1(GetRequestData, void(DataCallback));
};

class FakeMultipartUploadRequest : public MultipartUploadRequest {
 public:
  FakeMultipartUploadRequest(bool should_succeed,
                             DeepScanningClientResponse response,
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
  DeepScanningClientResponse response_;
  Callback callback_;
};

class FakeMultipartUploadRequestFactory : public MultipartUploadRequestFactory {
 public:
  FakeMultipartUploadRequestFactory(bool should_succeed,
                                    DeepScanningClientResponse response)
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
  DeepScanningClientResponse response_;
};

class MockBinaryFCMService : public BinaryFCMService {
 public:
  MockBinaryFCMService() = default;
  ~MockBinaryFCMService() = default;

  MOCK_METHOD1(GetInstanceID,
               void(BinaryFCMService::GetInstanceIDCallback callback));
};

class BinaryUploadServiceTest : public testing::Test {
 public:
  BinaryUploadServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        fake_factory_(true, DeepScanningClientResponse()) {
    MultipartUploadRequest::RegisterFactoryForTests(&fake_factory_);
    auto fcm_service = std::make_unique<MockBinaryFCMService>();
    fcm_service_ = fcm_service.get();

    // Since we have mocked the MultipartUploadRequest, we don't need a
    // URLLoaderFactory, so pass nullptr here.
    service_ =
        std::make_unique<BinaryUploadService>(nullptr, std::move(fcm_service));
  }
  ~BinaryUploadServiceTest() override = default;

  void ExpectNetworkResponse(bool should_succeed,
                             DeepScanningClientResponse response) {
    fake_factory_ = FakeMultipartUploadRequestFactory(should_succeed, response);
  }

  void ExpectInstanceID(std::string id) {
    ON_CALL(*fcm_service_, GetInstanceID(_))
        .WillByDefault(
            Invoke([id](BinaryFCMService::GetInstanceIDCallback callback) {
              std::move(callback).Run(id);
            }));
  }

  void UploadForDeepScanning(
      std::unique_ptr<BinaryUploadService::Request> request,
      bool authorized = true) {
    service_->MaybeUploadForDeepScanningCallback(std::move(request),
                                                 authorized);
  }

  void ReceiveMessageForRequest(BinaryUploadService::Request* request,
                                const DeepScanningClientResponse& response) {
    service_->OnGetResponse(request, response);
  }

  void ReceiveResponseFromUpload(BinaryUploadService::Request* request,
                                 bool success,
                                 const std::string& response) {
    service_->OnUploadComplete(request, success, response);
  }

  void ServiceWithNoFCMConnection() {
    service_ = std::make_unique<BinaryUploadService>(
        nullptr, std::unique_ptr<BinaryFCMService>(nullptr));
  }

  std::unique_ptr<MockRequest> MakeRequest(
      BinaryUploadService::Result* scanning_result,
      DeepScanningClientResponse* scanning_response) {
    auto request = std::make_unique<MockRequest>(base::BindOnce(
        [](BinaryUploadService::Result* target_result,
           DeepScanningClientResponse* target_response,
           BinaryUploadService::Result result,
           DeepScanningClientResponse response) {
          *target_result = result;
          *target_response = response;
        },
        scanning_result, scanning_response));
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
  std::unique_ptr<BinaryUploadService> service_;
  MockBinaryFCMService* fcm_service_;
  FakeMultipartUploadRequestFactory fake_factory_;
};

TEST_F(BinaryUploadServiceTest, FailsForLargeFile) {
  BinaryUploadService::Result scanning_result;
  DeepScanningClientResponse scanning_response;

  ExpectInstanceID("valid id");
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);
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
  DeepScanningClientResponse scanning_response;

  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);

  ExpectInstanceID(BinaryFCMService::kInvalidId);

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::FAILED_TO_GET_TOKEN);
}

TEST_F(BinaryUploadServiceTest, FailsWhenUploadFails) {
  BinaryUploadService::Result scanning_result;
  DeepScanningClientResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(false, DeepScanningClientResponse());

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UPLOAD_FAILURE);
}

TEST_F(BinaryUploadServiceTest, HoldsScanResponsesUntilAllReady) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  DeepScanningClientResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);
  request->set_request_dlp_scan(DlpDeepScanningClientRequest());
  request->set_request_malware_scan(MalwareDeepScanningClientRequest());

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(true, DeepScanningClientResponse());

  MockRequest* raw_request = request.get();
  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  // Simulate receiving the DLP response
  DeepScanningClientResponse response;
  response.mutable_dlp_scan_verdict();
  ReceiveMessageForRequest(raw_request, response);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);

  // Simulate receiving the malware response
  response.clear_dlp_scan_verdict();
  response.mutable_malware_scan_verdict();
  ReceiveMessageForRequest(raw_request, response);
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(scanning_response.has_dlp_scan_verdict());
  EXPECT_TRUE(scanning_response.has_malware_scan_verdict());
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::SUCCESS);
}

TEST_F(BinaryUploadServiceTest, TimesOut) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  DeepScanningClientResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);
  request->set_request_dlp_scan(DlpDeepScanningClientRequest());
  request->set_request_malware_scan(MalwareDeepScanningClientRequest());

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(true, DeepScanningClientResponse());
  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(BinaryUploadServiceTest, OnInstanceIDAfterTimeout) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  DeepScanningClientResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);
  request->set_request_dlp_scan(DlpDeepScanningClientRequest());
  request->set_request_malware_scan(MalwareDeepScanningClientRequest());

  BinaryFCMService::GetInstanceIDCallback instance_id_callback;
  ON_CALL(*fcm_service_, GetInstanceID(_))
      .WillByDefault(
          Invoke([&instance_id_callback](
                     BinaryFCMService::GetInstanceIDCallback callback) {
            instance_id_callback = std::move(callback);
          }));

  ExpectNetworkResponse(true, DeepScanningClientResponse());
  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);

  // Expect nothing to change if the InstanceID returns after the timeout.
  std::move(instance_id_callback).Run("valid id");
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(BinaryUploadServiceTest, OnUploadCompleteAfterTimeout) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  DeepScanningClientResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);
  request->set_request_dlp_scan(DlpDeepScanningClientRequest());
  request->set_request_malware_scan(MalwareDeepScanningClientRequest());

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(true, DeepScanningClientResponse());

  MockRequest* raw_request = request.get();
  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);

  // Expect nothing to change if the upload finishes after the timeout.
  ReceiveResponseFromUpload(raw_request, false, "");
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(BinaryUploadServiceTest, OnGetResponseAfterTimeout) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  DeepScanningClientResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);
  request->set_request_dlp_scan(DlpDeepScanningClientRequest());
  request->set_request_malware_scan(MalwareDeepScanningClientRequest());

  ExpectInstanceID("valid id");
  ExpectNetworkResponse(true, DeepScanningClientResponse());

  MockRequest* raw_request = request.get();
  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);

  // Expect nothing to change if we get a message after the timeout.
  ReceiveMessageForRequest(raw_request, DeepScanningClientResponse());
  EXPECT_EQ(scanning_result, BinaryUploadService::Result::TIMEOUT);
}

TEST_F(BinaryUploadServiceTest, OnUnauthorized) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  DeepScanningClientResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);
  request->set_request_dlp_scan(DlpDeepScanningClientRequest());
  request->set_request_malware_scan(MalwareDeepScanningClientRequest());

  ExpectInstanceID("valid id");

  DeepScanningClientResponse simulated_response;
  simulated_response.mutable_dlp_scan_verdict();
  simulated_response.mutable_malware_scan_verdict();
  ExpectNetworkResponse(true, simulated_response);

  UploadForDeepScanning(std::move(request), /*authorized=*/false);

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);
}

TEST_F(BinaryUploadServiceTest, OnGetSynchronousResponse) {
  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  DeepScanningClientResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);
  request->set_request_dlp_scan(DlpDeepScanningClientRequest());
  request->set_request_malware_scan(MalwareDeepScanningClientRequest());

  ExpectInstanceID("valid id");

  DeepScanningClientResponse simulated_response;
  simulated_response.mutable_dlp_scan_verdict();
  simulated_response.mutable_malware_scan_verdict();
  ExpectNetworkResponse(true, simulated_response);

  UploadForDeepScanning(std::move(request));
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::SUCCESS);
}

TEST_F(BinaryUploadServiceTest, ReturnsAsynchronouslyWithNoFCM) {
  ServiceWithNoFCMConnection();

  BinaryUploadService::Result scanning_result =
      BinaryUploadService::Result::UNKNOWN;
  DeepScanningClientResponse scanning_response;
  std::unique_ptr<MockRequest> request =
      MakeRequest(&scanning_result, &scanning_response);
  request->set_request_dlp_scan(DlpDeepScanningClientRequest());
  request->set_request_malware_scan(MalwareDeepScanningClientRequest());

  UploadForDeepScanning(std::move(request));

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::UNKNOWN);

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(scanning_result, BinaryUploadService::Result::FAILED_TO_GET_TOKEN);
}

TEST_F(BinaryUploadServiceTest, IsAuthorizedValidTimer) {
  // The 24 hours timer should be started on the first IsAuthorized call.
  ValidateAuthorizationTimerIdle();
  service_->IsAuthorized(base::DoNothing());
  ValidateAuthorizationTimerStarted();
}

}  // namespace safe_browsing
