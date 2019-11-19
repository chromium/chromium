// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/android_stream_reader_url_loader.h"

#include <utility>

#include "android_webview/browser/input_stream.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/public/common/resource_type.h"
#include "mojo/core/embedder/embedder.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

namespace {

const GURL kTestURL = GURL("https://www.example.com/");

void VerifyHeaderNameAndValue(net::HttpResponseHeaders* headers,
                              std::string header_name,
                              std::string header_value) {
  EXPECT_TRUE(headers->HasHeader(header_name));
  std::string actual_header_value;
  EXPECT_TRUE(
      headers->EnumerateHeader(NULL, header_name, &actual_header_value));
  EXPECT_EQ(header_value, actual_header_value);
}

}  // namespace

// Always succeeds, depending on constructor uses the given string as contents
// for the input stream and puts it in the IOBuffer |nb_reads| times.
class FakeInputStream : public InputStream {
 public:
  FakeInputStream() : contents_(""), nb_reads_(0) {}
  explicit FakeInputStream(std::string contents)
      : contents_(contents), nb_reads_(1) {}
  explicit FakeInputStream(std::string contents, int nb_reads)
      : contents_(contents), nb_reads_(nb_reads) {}
  ~FakeInputStream() override {}

  bool BytesAvailable(int* bytes_available) const override { return true; }

  bool Skip(int64_t n, int64_t* bytes_skipped) override {
    *bytes_skipped = n;
    return true;
  }

  bool Read(net::IOBuffer* buf, int length, int* bytes_read) override {
    if (nb_reads_ <= 0) {
      *bytes_read = 0;
      return true;
    }
    CHECK_GE(length, static_cast<int>(contents_.length()));
    memcpy(buf->data(), contents_.c_str(), contents_.length());
    *bytes_read = contents_.length();
    buffer_written_ = true;
    nb_reads_--;
    return true;
  }

 private:
  std::string contents_;
  bool buffer_written_;
  int nb_reads_;
};

// Stream that always fails
class FakeFailingInputStream : public InputStream {
 public:
  FakeFailingInputStream() {}
  ~FakeFailingInputStream() override {}
  bool BytesAvailable(int* bytes_available) const override { return false; }
  bool Skip(int64_t n, int64_t* bytes_skipped) override { return false; }
  bool Read(net::IOBuffer* dest, int length, int* bytes_read) override {
    return false;
  }
};

class TestResponseDelegate
    : public AndroidStreamReaderURLLoader::ResponseDelegate {
 public:
  explicit TestResponseDelegate(std::unique_ptr<InputStream> input_stream)
      : input_stream_(std::move(input_stream)) {}
  TestResponseDelegate(std::unique_ptr<InputStream> input_stream,
                       const std::string custom_mime_type)
      : input_stream_(std::move(input_stream)),
        custom_mime_type_(custom_mime_type) {}
  TestResponseDelegate(std::unique_ptr<InputStream> input_stream,
                       const std::string& custom_status,
                       const std::string& custom_header_name,
                       const std::string& custom_header_value)
      : input_stream_(std::move(input_stream)),
        custom_status_(custom_status),
        custom_header_name_(custom_header_name),
        custom_header_value_(custom_header_value) {}
  ~TestResponseDelegate() override {}

  std::unique_ptr<android_webview::InputStream> OpenInputStream(
      JNIEnv* env) override {
    return std::move(input_stream_);
  }

  bool OnInputStreamOpenFailed() override {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    return false;
  }

  bool GetMimeType(JNIEnv* env,
                   const GURL& url,
                   android_webview::InputStream* stream,
                   std::string* mime_type) override {
    if (!custom_mime_type_.empty()) {
      *mime_type = custom_mime_type_;
      return true;
    }
    return false;
  }

  bool GetCharset(JNIEnv* env,
                  const GURL& url,
                  android_webview::InputStream* stream,
                  std::string* charset) override {
    return false;
  }

  void AppendResponseHeaders(JNIEnv* env,
                             net::HttpResponseHeaders* headers) override {
    if (custom_status_.empty() && custom_header_name_.empty() &&
        custom_header_value_.empty()) {
      // no-op
      return;
    }
    headers->ReplaceStatusLine(custom_status_);
    std::string header_line(custom_header_name_);
    header_line.append(": ");
    header_line.append(custom_header_value_);
    headers->AddHeader(header_line);
  }

 private:
  std::unique_ptr<InputStream> input_stream_;
  const std::string custom_mime_type_;
  const std::string custom_status_;
  const std::string custom_header_name_;
  const std::string custom_header_value_;

  base::ThreadChecker thread_checker_;
};

class AndroidStreamReaderURLLoaderTest : public ::testing::Test {
 protected:
  AndroidStreamReaderURLLoaderTest() {}
  ~AndroidStreamReaderURLLoaderTest() override = default;

  void SetUp() override {
    mojo::core::Init();
  }

  network::ResourceRequest CreateRequest(const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.resource_type =
        static_cast<int>(content::ResourceType::kSubResource);
    return request;
  }

  // helper method for creating loaders given a stream
  AndroidStreamReaderURLLoader* CreateLoader(
      const network::ResourceRequest& request,
      network::TestURLLoaderClient* client,
      std::unique_ptr<InputStream> input_stream) {
    return new AndroidStreamReaderURLLoader(
        request, client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::make_unique<TestResponseDelegate>(std::move(input_stream)));
  }

  // helper method for creating loaders given a stream and MIME type
  AndroidStreamReaderURLLoader* CreateLoaderWithMimeType(
      const network::ResourceRequest& request,
      network::TestURLLoaderClient* client,
      std::unique_ptr<InputStream> input_stream,
      const std::string custom_mime_type) {
    return new AndroidStreamReaderURLLoader(
        request, client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::make_unique<TestResponseDelegate>(std::move(input_stream),
                                               custom_mime_type));
  }

  // helper method for creating loaders given a stream and response header
  // values
  AndroidStreamReaderURLLoader* CreateLoaderWithCustomizedResponseHeader(
      const network::ResourceRequest& request,
      network::TestURLLoaderClient* client,
      std::unique_ptr<InputStream> input_stream,
      const std::string custom_status,
      const std::string custom_header_name,
      const std::string custom_header_value) {
    return new AndroidStreamReaderURLLoader(
        request, client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::make_unique<TestResponseDelegate>(
            std::move(input_stream), custom_status, custom_header_name,
            custom_header_value));
  }

  // Extracts the body data that is present in the consumer pipe
  // and returns it as a string.
  std::string ReadAvailableBody(network::TestURLLoaderClient* client) {
    MojoHandle consumer = client->response_body().value();

    uint32_t num_bytes = 0;
    MojoReadDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_READ_DATA_FLAG_QUERY;
    MojoResult result = MojoReadData(consumer, &options, nullptr, &num_bytes);
    CHECK_EQ(MOJO_RESULT_OK, result);
    if (num_bytes == 0)
      return std::string();

    std::vector<char> buffer(num_bytes);
    result = MojoReadData(consumer, nullptr, buffer.data(), &num_bytes);
    CHECK_EQ(MOJO_RESULT_OK, result);
    CHECK_EQ(num_bytes, buffer.size());

    return std::string(buffer.data(), buffer.size());
  }

  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(AndroidStreamReaderURLLoaderTest);
};

TEST_F(AndroidStreamReaderURLLoaderTest, ReadFakeStream) {
  network::ResourceRequest request = CreateRequest(kTestURL);
  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader =
      CreateLoader(request, client.get(), std::make_unique<FakeInputStream>());
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_EQ("HTTP/1.1 200 OK",
            client->response_head()->headers->GetStatusLine());
  VerifyHeaderNameAndValue(client->response_head()->headers.get(), "Client-Via",
                           "shouldInterceptRequest");
}

TEST_F(AndroidStreamReaderURLLoaderTest, ReadFailingStream) {
  network::ResourceRequest request = CreateRequest(kTestURL);
  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader = CreateLoader(
      request, client.get(), std::make_unique<FakeFailingInputStream>());
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_FAILED, client->completion_status().error_code);
}

TEST_F(AndroidStreamReaderURLLoaderTest, ValidRangeRequest) {
  network::ResourceRequest request = CreateRequest(kTestURL);
  request.headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=10-200");

  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader =
      CreateLoader(request, client.get(), std::make_unique<FakeInputStream>());
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_EQ("HTTP/1.1 200 OK",
            client->response_head()->headers->GetStatusLine());
}

TEST_F(AndroidStreamReaderURLLoaderTest, InvalidRangeRequest) {
  network::ResourceRequest request = CreateRequest(kTestURL);
  request.headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=10-0");

  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader =
      CreateLoader(request, client.get(), std::make_unique<FakeInputStream>());
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            client->completion_status().error_code);
}

TEST_F(AndroidStreamReaderURLLoaderTest, NullInputStream) {
  network::ResourceRequest request = CreateRequest(kTestURL);

  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader =
      CreateLoader(request, client.get(), nullptr);
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_EQ("HTTP/1.1 404 Not Found",
            client->response_head()->headers->GetStatusLine());
  VerifyHeaderNameAndValue(client->response_head()->headers.get(), "Client-Via",
                           "shouldInterceptRequest");
}

TEST_F(AndroidStreamReaderURLLoaderTest, ReadFakeStreamWithBody) {
  network::ResourceRequest request = CreateRequest(kTestURL);

  std::string expected_body("test");
  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader = CreateLoader(
      request, client.get(), std::make_unique<FakeInputStream>(expected_body));
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_EQ("HTTP/1.1 200 OK",
            client->response_head()->headers->GetStatusLine());
  VerifyHeaderNameAndValue(client->response_head()->headers.get(), "Client-Via",
                           "shouldInterceptRequest");
  std::string body = ReadAvailableBody(client.get());
  EXPECT_EQ(expected_body, body);
}

TEST_F(AndroidStreamReaderURLLoaderTest, ReadFakeStreamWithBodyMultipleReads) {
  network::ResourceRequest request = CreateRequest(kTestURL);

  std::string expected_body("test");
  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader =
      CreateLoader(request, client.get(),
                   std::make_unique<FakeInputStream>(expected_body, 2));
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_EQ("HTTP/1.1 200 OK",
            client->response_head()->headers->GetStatusLine());
  std::string body = ReadAvailableBody(client.get());
  EXPECT_EQ(expected_body + expected_body, body);
}

TEST_F(AndroidStreamReaderURLLoaderTest,
       ReadFakeStreamCloseConsumerPipeDuringResponse) {
  network::ResourceRequest request = CreateRequest(kTestURL);

  std::string expected_body("test");
  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  // Need a valid MIME type, otherwise we won't get headers until we've already
  // read the input stream (and we need to interrupt the read in this test).
  std::string valid_mime_type("text/html");
  AndroidStreamReaderURLLoader* loader = CreateLoaderWithMimeType(
      request, client.get(), std::make_unique<FakeInputStream>(expected_body),
      valid_mime_type);
  loader->Start();
  client->RunUntilResponseBodyArrived();
  EXPECT_TRUE(client->has_received_response());
  EXPECT_FALSE(client->has_received_completion());
  EXPECT_EQ("HTTP/1.1 200 OK",
            client->response_head()->headers->GetStatusLine());
  auto response_body = client->response_body_release();
  response_body.reset();
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client->completion_status().error_code);
}

TEST_F(AndroidStreamReaderURLLoaderTest, CustomResponseHeaderAndStatus) {
  const std::string custom_status_line = "HTTP/1.1 401 Gone";
  const std::string custom_header_name = "X-Test-Header";
  const std::string custom_header_value = "TestHeaderValue";
  network::ResourceRequest request = CreateRequest(kTestURL);

  std::string expected_body("test");
  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader =
      CreateLoaderWithCustomizedResponseHeader(
          request, client.get(),
          std::make_unique<FakeInputStream>(expected_body), custom_status_line,
          custom_header_name, custom_header_value);
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_EQ(custom_status_line,
            client->response_head()->headers->GetStatusLine());
  VerifyHeaderNameAndValue(client->response_head()->headers.get(),
                           custom_header_name, custom_header_value);
  VerifyHeaderNameAndValue(client->response_head()->headers.get(), "Client-Via",
                           "shouldInterceptRequest");
}

}  // namespace android_webview
