// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_url_loader_throttle.h"

#include <fcntl.h>
#include <unistd.h>

#include <atomic>

#include "android_webview/browser/content_restriction/aw_content_restriction_blocked_navigation_tracker.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/posix/eintr_wrapper.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
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

class AwContentRestrictionURLLoaderThrottleTest : public testing::Test {
 protected:
  void SetUp() override { throttle_.set_delegate(&delegate_); }

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

  network::ResourceRequest request;
  request.url = GURL(kTestUrl);
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

  network::ResourceRequest request;
  request.url = GURL(kTestUrl);
  bool defer = false;
  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate.resume_called());
  EXPECT_FALSE(delegate.cancel_called());
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, AllowRequest) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_client_, RequestContentClassification(_, _, _))
      .WillOnce(WithArgs<2>(
          [](AwContentRestrictionManagerClient::ContentClassificationCallback
                 callback) { std::move(callback).Run(true); }));

  network::ResourceRequest request;
  request.url = GURL(kTestUrl);
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);

  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate_.resume_called());
  EXPECT_FALSE(delegate_.cancel_called());
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, BlockRequest) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_client_, RequestContentClassification(_, _, _))
      .WillOnce(WithArgs<2>(
          [](AwContentRestrictionManagerClient::ContentClassificationCallback
                 callback) { std::move(callback).Run(false); }));

  network::ResourceRequest request;
  request.url = GURL(kTestUrl);
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

  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));
  base::ScopedFD read_fd(pipe_fds[0]);
  base::ScopedFD write_fd(pipe_fds[1]);
  EXPECT_CALL(mock_client_,
              CreateRequestBodyPipeAndGetWriteFd(kTestNavigationId))
      .WillOnce(Return(write_fd.release()));
  EXPECT_CALL(mock_client_, RequestContentClassification(_, _, _))
      .WillOnce(WithArgs<2>(
          [](AwContentRestrictionManagerClient::ContentClassificationCallback
                 callback) { std::move(callback).Run(false); }));

  network::ResourceRequest request;
  request.url = GURL(kTestUrl);
  request.method = "POST";
  const std::string body_data(kTestRequestPayloadContent);
  request.request_body = network::ResourceRequestBody::CreateFromBytes(
      std::vector<uint8_t>(body_data.begin(), body_data.end()));
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  EXPECT_FALSE(delegate_.resume_called());
  EXPECT_TRUE(delegate_.cancel_called());

  // Wait for data to be streamed to the pipe and verify the content.
  task_environment_.RunUntilIdle();
  char buffer[100];
  ssize_t bytes_read = read(read_fd.get(), buffer, sizeof(buffer));
  ASSERT_GT(bytes_read, 0);
  std::string read_data(buffer, bytes_read);
  EXPECT_EQ(read_data, body_data);
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

  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));
  base::ScopedFD read_fd(pipe_fds[0]);
  base::ScopedFD write_fd(pipe_fds[1]);
  EXPECT_CALL(mock_client_,
              CreateRequestBodyPipeAndGetWriteFd(kTestNavigationId))
      .WillOnce(Return(write_fd.release()));
  EXPECT_CALL(mock_client_, RequestContentClassification(_, _, _))
      .WillOnce(WithArgs<2>(
          [](AwContentRestrictionManagerClient::ContentClassificationCallback
                 callback) { std::move(callback).Run(true); }));

  network::ResourceRequest request;
  request.url = GURL(kTestUrl);
  request.method = "POST";
  auto request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request_body->AppendFileRange(temp_file, 0, file_content.size(),
                                base::Time());
  request.request_body = request_body;
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate_.resume_called());
  EXPECT_FALSE(delegate_.cancel_called());

  // Wait for the file streaming operation to complete and then verify content.
  task_environment_.RunUntilIdle();
  char buffer[100];
  ssize_t bytes_read =
      HANDLE_EINTR(read(read_fd.get(), buffer, sizeof(buffer)));
  ASSERT_GE(bytes_read, 0);
  std::string read_data(buffer, bytes_read);
  EXPECT_EQ(read_data, file_content);
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest,
       StreamRequestBodyInvalidFile) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));
  base::ScopedFD read_fd(pipe_fds[0]);
  base::ScopedFD write_fd(pipe_fds[1]);
  EXPECT_CALL(mock_client_,
              CreateRequestBodyPipeAndGetWriteFd(kTestNavigationId))
      .WillOnce(Return(write_fd.release()));
  EXPECT_CALL(mock_client_, RequestContentClassification(_, _, _))
      .WillOnce(WithArgs<2>(
          [](AwContentRestrictionManagerClient::ContentClassificationCallback
                 callback) { std::move(callback).Run(true); }));

  network::ResourceRequest request;
  request.url = GURL(kTestUrl);
  request.method = "POST";
  auto request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request_body->AppendFileRange(base::FilePath("/non_existent_path_12345.txt"),
                                0, 100, base::Time());
  request.request_body = request_body;
  bool defer = false;
  throttle_.WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate_.resume_called());
  EXPECT_FALSE(delegate_.cancel_called());

  // Wait for all pending tasks to complete and then verify content.
  task_environment_.RunUntilIdle();
  char buffer[100];
  ssize_t bytes_read =
      HANDLE_EINTR(read(read_fd.get(), buffer, sizeof(buffer)));
  EXPECT_EQ(bytes_read, 0);
}

TEST_F(AwContentRestrictionURLLoaderThrottleTest, StreamRequestBodyClosedPipe) {
  EXPECT_CALL(mock_client_, IsContentRestrictionEnabled())
      .WillOnce(Return(true));

  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));
  base::ScopedFD read_fd(pipe_fds[0]);
  base::ScopedFD write_fd(pipe_fds[1]);
  const int raw_write_fd = write_fd.get();
  EXPECT_CALL(mock_client_,
              CreateRequestBodyPipeAndGetWriteFd(kTestNavigationId))
      .WillOnce(Return(write_fd.release()));
  EXPECT_CALL(mock_client_, RequestContentClassification(_, _, _))
      .WillOnce(WithArgs<2>(
          [](AwContentRestrictionManagerClient::ContentClassificationCallback
                 callback) { std::move(callback).Run(true); }));

  network::ResourceRequest request;
  request.url = GURL(kTestUrl);
  request.method = "POST";
  const std::string body_data(kTestRequestPayloadContent);
  request.request_body = network::ResourceRequestBody::CreateFromBytes(
      std::vector<uint8_t>(body_data.begin(), body_data.end()));

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

}  // namespace
}  // namespace android_webview
