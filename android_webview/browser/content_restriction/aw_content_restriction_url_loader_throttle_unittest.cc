// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_url_loader_throttle.h"

#include <atomic>

#include "android_webview/browser/content_restriction/aw_content_restriction_blocked_navigation_tracker.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"
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

}  // namespace
}  // namespace android_webview
