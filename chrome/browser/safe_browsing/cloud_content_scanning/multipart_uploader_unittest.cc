// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
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
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = temp_dir_.GetPath().AppendASCII(file_name);
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_READ |
                              base::File::FLAG_WRITE);
    file.WriteAtCurrentPos(content.data(), content.size());
    return path;
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

class MockMultipartUploadFileRequest : public MultipartUploadRequest {
 public:
  MockMultipartUploadFileRequest(const base::FilePath& path,
                                 MultipartUploadRequest::Callback callback)
      : MultipartUploadRequest(nullptr,
                               GURL(),
                               "metadata",
                               path,
                               TRAFFIC_ANNOTATION_FOR_TESTS,
                               std::move(callback)) {}

  std::string GetBodyFromFileRequest() {
    MultipartDataPipeGetter* data_pipe_getter =
        this->file_data_pipe_getter_for_testing();
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

  MOCK_METHOD1(CompleteSendFileRequest,
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

TEST_F(MultipartUploadRequestTest, FileRetries) {
  base::FilePath path = CreateFile("test.txt", "file content");

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
    MockMultipartUploadFileRequest mock_request(
        path, base::BindLambdaForTesting(
                  [&run_loop](bool success, int http_status,
                              const std::string& response_data) {
                    EXPECT_TRUE(success);
                    EXPECT_EQ(net::HTTP_OK, http_status);
                    EXPECT_EQ("response", response_data);
                    run_loop.Quit();
                  }));
    mock_request.set_boundary("boundary");

    EXPECT_CALL(mock_request, CompleteSendFileRequest(_))
        .WillOnce([&mock_request, &expected_body](
                      std::unique_ptr<network::ResourceRequest> request) {
          EXPECT_EQ(expected_body, mock_request.GetBodyFromFileRequest());
          mock_request.RetryOrFinish(net::OK, net::HTTP_OK,
                                     std::make_unique<std::string>("response"));
        });
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();
    run_loop.Run();
  }
  {
    int retry_count = 0;
    base::RunLoop run_loop;
    MockMultipartUploadFileRequest mock_request(
        path, base::BindLambdaForTesting(
                  [&run_loop, &retry_count](bool success, int http_status,
                                            const std::string& response_data) {
                    EXPECT_TRUE(success);
                    EXPECT_EQ(net::HTTP_OK, http_status);
                    EXPECT_EQ("response", response_data);
                    EXPECT_EQ(3, retry_count);
                    run_loop.Quit();
                  }));
    mock_request.set_boundary("boundary");

    EXPECT_CALL(mock_request, CompleteSendFileRequest(_))
        .Times(3)
        .WillRepeatedly([&mock_request, &expected_body, &retry_count](
                            std::unique_ptr<network::ResourceRequest> request) {
          // Every call to CompleteSendFileRequest should be able to get the
          // same body from the request's data pipe getter.
          EXPECT_EQ(expected_body, mock_request.GetBodyFromFileRequest());

          ++retry_count;
          mock_request.RetryOrFinish(
              net::OK,
              retry_count < 3 ? net::HTTP_SERVICE_UNAVAILABLE : net::HTTP_OK,
              std::make_unique<std::string>("response"));
        });
    mock_request.Start();
    task_environment_.FastForwardUntilNoTasksRemain();
    run_loop.Run();
  }
}

TEST_F(MultipartUploadRequestTest, FileAndStringRequestsEquivalent) {
  // The request body should be identical when obtained through a string request
  // and a file request with equivalent content.
  base::FilePath path = CreateFile("test.txt", "data");
  MockMultipartUploadFileRequest file_request(path, base::DoNothing());
  file_request.set_boundary("boundary");

  // Start the file request so the internal data pipe getter is initialized.
  ASSERT_FALSE(file_request.file_data_pipe_getter_for_testing());
  EXPECT_CALL(file_request, CompleteSendFileRequest(_)).Times(1);
  file_request.Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(file_request.file_data_pipe_getter_for_testing());

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
  EXPECT_EQ(expected_body, file_request.GetBodyFromFileRequest());
}

}  // namespace safe_browsing
