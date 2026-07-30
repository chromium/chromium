// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_url_loader_throttle.h"

#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <optional>
#include <type_traits>

#include "android_webview/browser/content_restriction/aw_content_restriction_blocked_navigation_tracker.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/strcat.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_data_pipe_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

using testing::_;
using testing::Return;
using testing::WithArgs;

namespace android_webview {
namespace {

constexpr char kTestUrl[] = "https://www.example.com";
constexpr char kTestRequestPayloadContent[] = "test_body";
constexpr int64_t kTestNavigationId = 1;

class MockAwContentRestrictionManagerClient
    : public AwContentRestrictionManagerClient {
 public:
  MockAwContentRestrictionManagerClient() = default;
  ~MockAwContentRestrictionManagerClient() override = default;

  MOCK_METHOD(bool, IsContentRestrictionEnabled, (), (override));
  MOCK_METHOD(void,
              RequestContentClassification,
              (int64_t,
               const network::ResourceRequest&,
               ContentClassificationCallback),
              (override));
  MOCK_METHOD(int, CreateRequestBodyPipeAndGetWriteFd, (int64_t), (override));
};

class TestThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  TestThrottleDelegate() = default;
  ~TestThrottleDelegate() override = default;

  bool resume_called() const { return resume_called_.load(); }
  bool cancel_called() const { return cancel_called_.load(); }
  int error_code() const { return error_code_.load(); }

  // blink::URLLoaderThrottle::Delegate:
  void Resume() override { resume_called_.store(true); }
  void CancelWithError(int error_code,
                       std::string_view custom_reason) override {
    cancel_called_.store(true);
    error_code_.store(error_code);
  }

 private:
  std::atomic<bool> resume_called_ = false;
  std::atomic<bool> cancel_called_ = false;
  std::atomic<int> error_code_ = 0;
};

class FakeChunkedDataPipeGetter : public network::mojom::ChunkedDataPipeGetter {
 public:
  explicit FakeChunkedDataPipeGetter(const std::string& data) : data_(data) {}
  ~FakeChunkedDataPipeGetter() override = default;

  mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter> Bind() {
    mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void set_start_error(int32_t start_error) { start_error_ = start_error; }
  void set_stream_on_demand(bool stream_on_demand) {
    stream_on_demand_ = stream_on_demand;
  }
  void set_total_size(uint64_t total_size) { total_size_ = total_size; }

  void Reset() { receivers_.Clear(); }

  // network::mojom::ChunkedDataPipeGetter:
  void GetSize(GetSizeCallback callback) override {
    const uint64_t size =
        total_size_.has_value() ? total_size_.value() : data_.size();
    std::move(callback).Run(start_error_, size);
  }

  // network::mojom::ChunkedDataPipeGetter:
  void StartReading(mojo::ScopedDataPipeProducerHandle pipe) override {
    if (start_error_ != net::OK) {
      return;
    }
    if (stream_on_demand_) {
      pipe_ = std::move(pipe);
      return;
    }
    size_t bytes_written = 0;
    MojoResult result =
        pipe->WriteData(base::as_byte_span(data_),
                        MOJO_WRITE_DATA_FLAG_ALL_OR_NONE, bytes_written);
    EXPECT_EQ(MOJO_RESULT_OK, result);
  }

  void WriteChunk(std::string_view chunk) {
    ASSERT_TRUE(pipe_.is_valid());
    ASSERT_TRUE(stream_on_demand_);
    size_t bytes_written = 0;
    MojoResult result =
        pipe_->WriteData(base::as_byte_span(chunk),
                         MOJO_WRITE_DATA_FLAG_ALL_OR_NONE, bytes_written);
    EXPECT_EQ(result, MOJO_RESULT_OK);
    EXPECT_EQ(bytes_written, chunk.size());
  }

  void ClosePipe() { pipe_.reset(); }

 private:
  std::string data_;
  int32_t start_error_ = net::OK;
  bool stream_on_demand_ = false;
  std::optional<uint64_t> total_size_;
  mojo::ScopedDataPipeProducerHandle pipe_;
  mojo::ReceiverSet<network::mojom::ChunkedDataPipeGetter> receivers_;
};

class AwContentRestrictionURLLoaderThrottleTest : public testing::Test {
 protected:
  void SetUp() override { throttle_.set_delegate(&delegate_); }

  std::string ReadPayloadFromDataPipeConsumerHandle(
      mojo::ScopedDataPipeConsumerHandle consumer) {
    base::span<const uint8_t> read_buffer;
    MojoResult result =
        consumer->BeginReadData(MOJO_BEGIN_READ_DATA_FLAG_NONE, read_buffer);
    if (result == MOJO_RESULT_OK) {
      std::string payload_data(
          reinterpret_cast<const char*>(read_buffer.data()),
          read_buffer.size());
      consumer->EndReadData(read_buffer.size());
      return payload_data;
    }
    return "";
  }

  std::string ReadDataElementContent(const network::DataElement& element) {
    switch (element.type()) {
      case network::DataElement::Tag::kBytes: {
        const network::DataElementBytes& bytes_element =
            element.As<network::DataElementBytes>();
        base::span<const uint8_t> bytes_span = bytes_element.bytes();
        return std::string(reinterpret_cast<const char*>(bytes_span.data()),
                           bytes_span.size());
      }
      case network::DataElement::Tag::kFile: {
        const network::DataElementFile& file_element =
            element.As<network::DataElementFile>();
        std::string file_content;
        CHECK(base::ReadFileToString(file_element.path(), &file_content));
        return file_content;
      }
      case network::DataElement::Tag::kDataPipe: {
        mojo::Remote<network::mojom::DataPipeGetter> pipe_getter(
            const_cast<network::DataElementDataPipe&>(
                element.As<network::DataElementDataPipe>())
                .ReleaseDataPipeGetter());
        mojo::ScopedDataPipeProducerHandle producer;
        mojo::ScopedDataPipeConsumerHandle consumer;
        CHECK_EQ(mojo::CreateDataPipe(nullptr, producer, consumer),
                 MOJO_RESULT_OK);
        pipe_getter->Read(std::move(producer),
                          base::BindOnce([](int32_t status, uint64_t size) {}));

        // Wait for the data to be written to the pipe.
        task_environment_.RunUntilIdle();
        return ReadPayloadFromDataPipeConsumerHandle(std::move(consumer));
      }
      case network::DataElement::Tag::kChunkedDataPipe: {
        mojo::Remote<network::mojom::ChunkedDataPipeGetter> proxy_remote(
            const_cast<network::DataElementChunkedDataPipe&>(
                element.As<network::DataElementChunkedDataPipe>())
                .ReleaseChunkedDataPipeGetter());
        mojo::ScopedDataPipeProducerHandle producer;
        mojo::ScopedDataPipeConsumerHandle consumer;
        CHECK_EQ(mojo::CreateDataPipe(nullptr, producer, consumer),
                 MOJO_RESULT_OK);
        proxy_remote->StartReading(std::move(producer));

        // Wait for the data to be written to the pipe.
        task_environment_.RunUntilIdle();
        return ReadPayloadFromDataPipeConsumerHandle(std::move(consumer));
      }
      default:
        NOTREACHED();
    }
    NOTREACHED();
  }

  base::ScopedFD CreateAndMockRequestBodyPipe(int* out_write_fd = nullptr) {
    int pipe_fds[2];
    CHECK_EQ(0, pipe(pipe_fds));
    base::ScopedFD read_fd(pipe_fds[0]);
    base::ScopedFD write_fd(pipe_fds[1]);
    if (out_write_fd) {
      *out_write_fd = write_fd.get();
    }
    EXPECT_CALL(mock_client_,
                CreateRequestBodyPipeAndGetWriteFd(kTestNavigationId))
        .WillOnce(Return(write_fd.release()));
    return read_fd;
  }

  void MockRequestContentClassification(bool is_allowed) {
    EXPECT_CALL(mock_client_, RequestContentClassification(_, _, _))
        .WillOnce(WithArgs<2>(
            [is_allowed](
                AwContentRestrictionManagerClient::ContentClassificationCallback
                    callback) { std::move(callback).Run(is_allowed); }));
  }

  void MockRedirectRequestContentClassification(bool is_allowed) {
    EXPECT_CALL(mock_client_, RequestContentClassification(_, _, _))
        .WillOnce(WithArgs<1, 2>(
            [is_allowed](
                const network::ResourceRequest& request,
                AwContentRestrictionManagerClient::ContentClassificationCallback
                    callback) {
              EXPECT_EQ(request.method, "GET");
              EXPECT_EQ(request.url, GURL(kTestUrl));
              std::move(callback).Run(is_allowed);
            }));
  }

  std::string ReadPipeContent(int fd) {
    char buffer[1024];
    ssize_t bytes_read = HANDLE_EINTR(read(fd, buffer, sizeof(buffer)));
    if (bytes_read > 0) {
      return std::string(buffer, bytes_read);
    }
    return "";
  }

  network::ResourceRequest CreateTestResourceRequest(std::string_view method) {
    network::ResourceRequest request;
    request.url = GURL(kTestUrl);
    request.method = std::string(method);
    return request;
  }

  net::RedirectInfo CreateTestRedirectRequest() {
    net::RedirectInfo redirect_info;
    redirect_info.new_url = GURL(kTestUrl);
    redirect_info.new_method = "GET";
    return redirect_info;
  }

  template <typename... Elements>
  network::ResourceRequest CreatePostRequestWithElements(
      Elements... data_elements) {
    static_assert(
        (std::is_same_v<network::DataElement, std::decay_t<Elements>> && ...),
        "All arguments to CreatePostRequestWithElements must be "
        "network::DataElement instances.");
    network::ResourceRequest request = CreateTestResourceRequest("POST");
    scoped_refptr<network::ResourceRequestBody> request_body =
        base::MakeRefCounted<network::ResourceRequestBody>();
    (request_body->elements_mutable()->push_back(std::move(data_elements)),
     ...);
    request.request_body = request_body;
    return request;
  }

  content::BrowserTaskEnvironment task_environment_;
  MockAwContentRestrictionManagerClient mock_client_;
  AwContentRestrictionBlockedNavigationTracker tracker_;
  TestThrottleDelegate delegate_;
  AwContentRestrictionURLLoaderThrottle throttle_{&mock_client_, &tracker_,
                                                  kTestNavigationId};
};

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       AllowRequestsWhenContentRestrictionDisabled) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(false));

  network::ResourceRequest request =
      CreateTestResourceRequest(/*method=*/"GET");
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate_.resume_called());
  EXPECT_FALSE(delegate_.cancel_called());
  EXPECT_FALSE(tracker_.IsNavigationBlocked(kTestNavigationId));
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       AllowRequestsWhenNoNavigationIdSet) {
  // Set up a separate throttle instance with the navigation id not set.
  TestThrottleDelegate delegate;
  AwContentRestrictionURLLoaderThrottle throttle{&mock_client_, &tracker_,
                                                 std::nullopt};
  throttle.set_delegate(&delegate);

  network::ResourceRequest request =
      CreateTestResourceRequest(/*method=*/"GET");
  bool defer = false;
  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate.resume_called());
  EXPECT_FALSE(delegate.cancel_called());
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, AllowRequest) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));
  MockRequestContentClassification(true);

  network::ResourceRequest request =
      CreateTestResourceRequest(/*method=*/"GET");
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);

  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate_.resume_called());
  EXPECT_FALSE(delegate_.cancel_called());
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, BlockRequest) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));
  MockRequestContentClassification(false);

  network::ResourceRequest request =
      CreateTestResourceRequest(/*method=*/"GET");
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);

  EXPECT_TRUE(defer);
  EXPECT_FALSE(delegate_.resume_called());
  EXPECT_TRUE(delegate_.cancel_called());
  EXPECT_EQ(delegate_.error_code(), net::ERR_BLOCKED_BY_CLIENT);
  EXPECT_TRUE(tracker_.IsNavigationBlocked(kTestNavigationId));
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, StreamRequestBody) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(false);

  const std::string body_data(kTestRequestPayloadContent);
  network::ResourceRequest request = CreatePostRequestWithElements(
      network::DataElement(network::DataElementBytes(
          std::vector<uint8_t>(body_data.begin(), body_data.end()))));
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  EXPECT_FALSE(delegate_.resume_called());
  EXPECT_TRUE(delegate_.cancel_called());

  // Wait for data to be streamed to the pipe and verify the content.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ReadPipeContent(read_fd.get()), body_data);
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, StreamRequestBodyFile) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  // Create a temporary file on disk with mock content.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file = temp_dir.GetPath().AppendASCII("test_upload.txt");
  const std::string file_content(kTestRequestPayloadContent);
  ASSERT_TRUE(base::WriteFile(temp_file, file_content));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  network::ResourceRequest request = CreatePostRequestWithElements(
      network::DataElement(network::DataElementFile(
          temp_file, 0, file_content.size(), base::Time())));
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate_.resume_called());
  EXPECT_FALSE(delegate_.cancel_called());

  // Wait for the file streaming operation to complete and then verify content.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ReadPipeContent(read_fd.get()), file_content);

  // Also verify request payload post classification to ensure consistency.
  const std::vector<network::DataElement>* elements =
      request.request_body->elements();
  ASSERT_EQ(1u, elements->size());
  EXPECT_EQ(network::DataElement::Tag::kFile, elements->at(0).type());
  std::string payload_data = ReadDataElementContent(elements->at(0));
  EXPECT_EQ(payload_data, file_content);
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       StreamRequestBodyInvalidFile) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  network::ResourceRequest request = CreatePostRequestWithElements(
      network::DataElement(network::DataElementFile(
          base::FilePath("/non_existent_path_12345.txt"), 0, 100,
          base::Time())));
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate_.resume_called());
  EXPECT_FALSE(delegate_.cancel_called());

  // Wait for all pending tasks to complete and then verify content.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ReadPipeContent(read_fd.get()), "");
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, StreamRequestBodyClosedPipe) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  int raw_write_fd = -1;
  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe(&raw_write_fd);
  MockRequestContentClassification(true);

  const std::string body_data(kTestRequestPayloadContent);
  network::ResourceRequest request = CreatePostRequestWithElements(
      network::DataElement(network::DataElementBytes(
          std::vector<uint8_t>(body_data.begin(), body_data.end()))));

  // Prematurely close the reading end before starting the request.
  read_fd.reset();
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  // Wait for all pending tasks to complete and verify that the write descriptor
  // was fully closed and cleaned up.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(delegate_.resume_called());
  EXPECT_EQ(fcntl(raw_write_fd, F_GETFD), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, StreamRequestBodyDataPipe) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  const std::string body_data(kTestRequestPayloadContent);
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter_remote;
  network::TestDataPipeGetter data_pipe_getter(
      body_data, data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
  network::ResourceRequest request =
      CreatePostRequestWithElements(network::DataElement(
          network::DataElementDataPipe(std::move(data_pipe_getter_remote))));
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  // Wait until all async Mojo tasks are executed and verify the output.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ReadPipeContent(read_fd.get()), body_data);

  // Also verify request payload post classification to ensure consistency.
  const std::vector<network::DataElement>* elements =
      request.request_body->elements();
  ASSERT_EQ(1u, elements->size());
  EXPECT_EQ(network::DataElement::Tag::kDataPipe, elements->at(0).type());
  std::string payload_data = ReadDataElementContent(elements->at(0));
  EXPECT_EQ(payload_data, body_data);
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       StreamRequestBodyStandardDataPipeReadError) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  const std::string body_data(kTestRequestPayloadContent);
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter_remote;
  network::TestDataPipeGetter data_pipe_getter(
      body_data, data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
  data_pipe_getter.set_start_error(net::ERR_FAILED);
  network::ResourceRequest request =
      CreatePostRequestWithElements(network::DataElement(
          network::DataElementDataPipe(std::move(data_pipe_getter_remote))));
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  // Wait until all async Mojo tasks are executed.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ReadPipeContent(read_fd.get()), "");
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       StreamRequestBodyStandardDataPipeClosedEarly) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  const std::string body_data(kTestRequestPayloadContent);
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter_remote;
  network::TestDataPipeGetter data_pipe_getter(
      body_data, data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
  data_pipe_getter.set_pipe_closed_early(true);
  network::ResourceRequest request =
      CreatePostRequestWithElements(network::DataElement(
          network::DataElementDataPipe(std::move(data_pipe_getter_remote))));
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  // Wait until all async Mojo tasks are executed.
  task_environment_.RunUntilIdle();

  // Even though the Mojo data pipe was closed early, the producer successfully
  // wrote all 9 bytes of actual payload data to the pipe before closing it.
  // Therefore, our streamer successfully reads and writes the full 9 bytes of
  // content to the tracking pipe.
  EXPECT_EQ(ReadPipeContent(read_fd.get()), body_data);

  // Since the advertised payload length was 10 bytes (actual size + 1), the
  // consumer detects that the Mojo pipe was closed early by the producer before
  // sending the 10th byte. This triggers the streamer's OnDataComplete() and
  // CleanUp(), closing the internal write_fd_. Under Unix pipe semantics,
  // closing the write end signals EOF (0) to any subsequent read on the read
  // end instead of blocking.
  EXPECT_EQ(ReadPipeContent(read_fd.get()), "");
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       StreamRequestBodyStandardDataPipeDisconnectedEarly) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  const std::string body_data(kTestRequestPayloadContent);
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter_remote;
  auto data_pipe_getter = std::make_unique<network::TestDataPipeGetter>(
      body_data, data_pipe_getter_remote.InitWithNewPipeAndPassReceiver());
  network::ResourceRequest request =
      CreatePostRequestWithElements(network::DataElement(
          network::DataElementDataPipe(std::move(data_pipe_getter_remote))));

  // Trigger the early disconnect immediately by resetting the
  // TestDataPipeGetter pointer, which destroys its bound Receiver.
  data_pipe_getter.reset();
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  // Wait until all async Mojo tasks are executed.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ReadPipeContent(read_fd.get()), "");
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       StreamRequestBodyChunkedDataPipe) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  const std::string body_data(kTestRequestPayloadContent);
  FakeChunkedDataPipeGetter chunked_data_pipe_getter(body_data);
  network::ResourceRequest request = CreatePostRequestWithElements(
      network::DataElement(network::DataElementChunkedDataPipe(
          chunked_data_pipe_getter.Bind(),
          network::DataElementChunkedDataPipe::ReadOnlyOnce(true))));
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  // Wait until all async tasks are executed.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ReadPipeContent(read_fd.get()), body_data);

  // Also verify request payload post classification to ensure consistency.
  ASSERT_TRUE(delegate_.resume_called());
  const std::vector<network::DataElement>* elements =
      request.request_body->elements();
  ASSERT_EQ(1u, elements->size());
  EXPECT_EQ(network::DataElement::Tag::kChunkedDataPipe,
            elements->at(0).type());
  std::string payload_data = ReadDataElementContent(elements->at(0));
  EXPECT_EQ(payload_data, body_data);
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       StreamRequestBodyChunkedDataPipeClosedEarly) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  const std::string body_data(kTestRequestPayloadContent);
  FakeChunkedDataPipeGetter chunked_data_pipe_getter(body_data);
  network::ResourceRequest request = CreatePostRequestWithElements(
      network::DataElement(network::DataElementChunkedDataPipe(
          chunked_data_pipe_getter.Bind(),
          network::DataElementChunkedDataPipe::ReadOnlyOnce(true))));
  chunked_data_pipe_getter.Reset();
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  // Wait until all async tasks are executed.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ReadPipeContent(read_fd.get()), "");
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       StreamRequestBodyChunkedDataPipeGetSizeError) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  const std::string body_data(kTestRequestPayloadContent);
  auto fake_getter = std::make_unique<FakeChunkedDataPipeGetter>(body_data);
  fake_getter->set_start_error(net::ERR_FAILED);
  network::ResourceRequest request = CreatePostRequestWithElements(
      network::DataElement(network::DataElementChunkedDataPipe(
          fake_getter->Bind(),
          network::DataElementChunkedDataPipe::ReadOnlyOnce(true))));
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  // Wait until all async tasks are executed.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ReadPipeContent(read_fd.get()), "");
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       StreamRequestBodyChunkedDataPipeClassificationCompletesBeforeStreaming) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  const std::string chunk_1 = "Chunk1";
  const std::string chunk_2 = "Chunk2";
  const std::string chunk_3 = "Chunk3";
  const uint64_t total_size = chunk_1.size() + chunk_2.size() + chunk_3.size();
  FakeChunkedDataPipeGetter dynamic_pipe_getter(/*data=*/"");
  dynamic_pipe_getter.set_stream_on_demand(true);
  dynamic_pipe_getter.set_total_size(total_size);
  network::ResourceRequest request = CreatePostRequestWithElements(
      network::DataElement(network::DataElementChunkedDataPipe(
          dynamic_pipe_getter.Bind(),
          network::DataElementChunkedDataPipe::ReadOnlyOnce(true))));
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  // Run tasks until idle so that RequestContentClassification completes (which
  // allows the request and resumes the delegate), and dynamic_pipe_getter
  // receives StartReading. No payload data has been streamed yet.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(delegate_.resume_called());

  // Stream chunks on demand and close the producer pipe.
  dynamic_pipe_getter.WriteChunk(chunk_1);
  dynamic_pipe_getter.WriteChunk(chunk_2);
  dynamic_pipe_getter.WriteChunk(chunk_3);
  dynamic_pipe_getter.ClosePipe();
  task_environment_.RunUntilIdle();

  // Now verify that the ProxyChunkedDataPipeGetter bound in the request body
  // can be successfully started by a downstream consumer (such as the Network
  // Service) and has full access to the re-spooled content.
  const std::vector<network::DataElement>* elements =
      request.request_body->elements();
  ASSERT_EQ(1u, elements->size());
  const std::string expected_payload =
      base::StrCat({chunk_1, chunk_2, chunk_3});
  EXPECT_EQ(ReadDataElementContent(elements->at(0)), expected_payload);
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       StreamRequestBodyMultipleDataElements) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  base::ScopedFD read_fd = CreateAndMockRequestBodyPipe();
  MockRequestContentClassification(true);

  // Create a multipart request body.
  const std::string part_1(
      base::StrCat({"Part1_", kTestRequestPayloadContent}));
  const std::string part_2(
      base::StrCat({"Part2_", kTestRequestPayloadContent}));
  const std::string part_3(
      base::StrCat({"Part3_", kTestRequestPayloadContent}));

  network::ResourceRequest request = CreatePostRequestWithElements(
      network::DataElement(network::DataElementBytes(
          std::vector<uint8_t>(part_1.begin(), part_1.end()))),
      network::DataElement(network::DataElementBytes(
          std::vector<uint8_t>(part_2.begin(), part_2.end()))),
      network::DataElement(network::DataElementBytes(
          std::vector<uint8_t>(part_3.begin(), part_3.end()))));
  ASSERT_EQ(request.request_body->elements()->size(), 3u);
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);

  // Wait until all async tasks are executed before verifying all parts are
  // streamed into the pipe.
  task_environment_.RunUntilIdle();
  const std::string expected_payload = base::StrCat({part_1, part_2, part_3});
  EXPECT_EQ(ReadPipeContent(read_fd.get()), expected_payload);

  // A subsequent read should return 0 (EOF) now that the pipe has been closed.
  EXPECT_EQ(ReadPipeContent(read_fd.get()), "");
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       AllowRedirectsWhenContentRestrictionDisabled) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(false));

  net::RedirectInfo redirect_info = CreateTestRedirectRequest();
  bool defer = false;
  network::mojom::URLResponseHead url_response_head;
  throttle_.WillRedirectRequest(&redirect_info, url_response_head, &defer,
                                /*headers_update_params=*/nullptr);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate_.resume_called());
  EXPECT_FALSE(delegate_.cancel_called());
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, AllowRedirectRequest) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  net::RedirectInfo redirect_info = CreateTestRedirectRequest();
  MockRedirectRequestContentClassification(/*is_allowed=*/true);
  bool defer = false;
  network::mojom::URLResponseHead url_response_head;
  throttle_.WillRedirectRequest(&redirect_info, url_response_head, &defer,
                                /*headers_update_params=*/nullptr);

  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate_.resume_called());
  EXPECT_FALSE(delegate_.cancel_called());
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, BlockRedirectRequest) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  net::RedirectInfo redirect_info = CreateTestRedirectRequest();
  MockRedirectRequestContentClassification(/*is_allowed=*/false);
  bool defer = false;
  network::mojom::URLResponseHead url_response_head;
  throttle_.WillRedirectRequest(&redirect_info, url_response_head, &defer,
                                /*headers_update_params=*/nullptr);

  EXPECT_TRUE(defer);
  EXPECT_FALSE(delegate_.resume_called());
  EXPECT_TRUE(delegate_.cancel_called());
  EXPECT_EQ(delegate_.error_code(), net::ERR_BLOCKED_BY_CLIENT);
  EXPECT_TRUE(tracker_.IsNavigationBlocked(kTestNavigationId));
}

}  // namespace
}  // namespace android_webview
