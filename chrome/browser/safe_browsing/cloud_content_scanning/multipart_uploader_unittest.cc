// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::Invoke;

class MultipartUploadRequestTest : public testing::Test {
 public:
  MultipartUploadRequestTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
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

TEST_F(MultipartUploadRequestTest, GeneratesCorrectBody) {
  std::unique_ptr<MultipartUploadRequest> request =
      MultipartUploadRequest::Create(nullptr, GURL(), "metadata", "data",
                                     TRAFFIC_ANNOTATION_FOR_TESTS,
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

TEST_F(MultipartUploadRequestTest,
       EmitsNetworkRequestResponseCodeOrErrorHistogram) {
  {
    base::HistogramTester histograms;
    MockMultipartUploadRequest mock_request;

    EXPECT_CALL(mock_request, SendRequest())
        .WillRepeatedly(Invoke([&mock_request]() {
          mock_request.RetryOrFinish(net::OK, net::HTTP_OK,
                                     std::make_unique<std::string>("response"));
        }));
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();

    histograms.ExpectUniqueSample(
        "SBMultipartUploader.NetworkRequestResponseCodeOrError", net::HTTP_OK,
        1);
  }

  {
    base::HistogramTester histograms;
    MockMultipartUploadRequest mock_request;

    EXPECT_CALL(mock_request, SendRequest())
        .WillRepeatedly(Invoke([&mock_request]() {
          mock_request.RetryOrFinish(net::OK, net::HTTP_FORBIDDEN,
                                     std::make_unique<std::string>("response"));
        }));
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();

    histograms.ExpectUniqueSample(
        "SBMultipartUploader.NetworkRequestResponseCodeOrError",
        net::HTTP_FORBIDDEN, 1);
  }

  {
    base::HistogramTester histograms;
    MockMultipartUploadRequest mock_request;

    EXPECT_CALL(mock_request, SendRequest())
        .WillRepeatedly(Invoke([&mock_request]() {
          mock_request.RetryOrFinish(net::ERR_FAILED, net::HTTP_OK,
                                     std::make_unique<std::string>("response"));
        }));
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();

    histograms.ExpectUniqueSample(
        "SBMultipartUploader.NetworkRequestResponseCodeOrError",
        net::ERR_FAILED, 1);
  }

  {
    base::HistogramTester histograms;
    MockMultipartUploadRequest mock_request;

    EXPECT_CALL(mock_request, SendRequest())
        .WillRepeatedly(Invoke([&mock_request]() {
          mock_request.RetryOrFinish(net::OK, net::HTTP_INTERNAL_SERVER_ERROR,
                                     std::make_unique<std::string>("response"));
        }));
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();

    histograms.ExpectUniqueSample(
        "SBMultipartUploader.NetworkRequestResponseCodeOrError",
        net::HTTP_INTERNAL_SERVER_ERROR, 3);
  }
}

TEST_F(MultipartUploadRequestTest, EmitsUploadSuccessHistogram) {
  {
    base::HistogramTester histograms;
    MockMultipartUploadRequest mock_request;

    EXPECT_CALL(mock_request, SendRequest())
        .WillRepeatedly(Invoke([&mock_request]() {
          mock_request.RetryOrFinish(net::OK, net::HTTP_OK,
                                     std::make_unique<std::string>("response"));
        }));
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();

    histograms.ExpectUniqueSample("SBMultipartUploader.UploadSuccess", true, 1);
  }

  {
    base::HistogramTester histograms;
    MockMultipartUploadRequest mock_request;

    EXPECT_CALL(mock_request, SendRequest())
        .WillRepeatedly(Invoke([&mock_request]() {
          mock_request.RetryOrFinish(net::OK, net::HTTP_FORBIDDEN,
                                     std::make_unique<std::string>("response"));
        }));
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();

    histograms.ExpectUniqueSample("SBMultipartUploader.UploadSuccess", false,
                                  1);
  }
}

TEST_F(MultipartUploadRequestTest, EmitsRetriesNeededHistogram) {
  {
    base::HistogramTester histograms;
    MockMultipartUploadRequest mock_request;

    EXPECT_CALL(mock_request, SendRequest())
        .WillRepeatedly(Invoke([&mock_request]() {
          mock_request.RetryOrFinish(net::OK, net::HTTP_OK,
                                     std::make_unique<std::string>("response"));
        }));
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();

    histograms.ExpectUniqueSample("SBMultipartUploader.RetriesNeeded", 0, 1);
  }
}

}  // namespace safe_browsing
