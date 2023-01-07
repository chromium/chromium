// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/http_request_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace network_diagnostics {

namespace {

const char kFakeUrl[] =
    "https://abcdefgh-ccd-testing-v4.metric.gstatic.com/generate_204";
const int timeout_ms = 500;

}  // namespace

class HttpRequestManagerTest : public ::testing::Test {
 public:
  HttpRequestManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    http_request_manager_ = std::make_unique<HttpRequestManager>(
        /*profile=*/nullptr);
    http_request_manager_->SetURLLoaderFactoryForTesting(
        shared_url_loader_factory_);
  }
  HttpRequestManagerTest(const HttpRequestManagerTest&) = delete;
  HttpRequestManagerTest& operator=(const HttpRequestManagerTest&) = delete;

  void VerifyConnected(bool connected) {
    EXPECT_EQ(expected_connected_, connected);
    callback_invoked_ = true;
  }

 protected:
  void VerifyCallbackInvoked(bool callback_invoked) {
    EXPECT_EQ(callback_invoked_, callback_invoked);
  }

  void ResetCallbackInvoked() { callback_invoked_ = false; }

  void ResetHttpRequestManager() { http_request_manager_.reset(); }

  void SetExpectedConnectionResult(bool expected_connected) {
    expected_connected_ = expected_connected;
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  HttpRequestManager* http_request_manager() {
    return http_request_manager_.get();
  }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

  base::WeakPtr<HttpRequestManagerTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<HttpRequestManager> http_request_manager_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  bool expected_connected_ = false;
  bool callback_invoked_ = false;
  base::WeakPtrFactory<HttpRequestManagerTest> weak_factory_{this};
};

TEST_F(HttpRequestManagerTest, TestSuccessfulConnection) {
  SetExpectedConnectionResult(true);
  http_request_manager()->MakeRequest(
      GURL(kFakeUrl), base::Milliseconds(timeout_ms),
      base::BindOnce(&HttpRequestManagerTest::VerifyConnected, weak_ptr()));
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  EXPECT_TRUE(test_url_loader_factory().SimulateResponseForPendingRequest(
      kFakeUrl, /*content=*/"", net::HTTP_NO_CONTENT));
  VerifyCallbackInvoked(true);
}

TEST_F(HttpRequestManagerTest, TestUnsuccessfulConnection) {
  SetExpectedConnectionResult(false);
  http_request_manager()->MakeRequest(
      GURL(kFakeUrl), base::Milliseconds(timeout_ms),
      base::BindOnce(&HttpRequestManagerTest::VerifyConnected, weak_ptr()));
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  EXPECT_TRUE(test_url_loader_factory().SimulateResponseForPendingRequest(
      kFakeUrl, /*content=*/"", net::HTTP_BAD_REQUEST));
  VerifyCallbackInvoked(true);
}

TEST_F(HttpRequestManagerTest, TestTimeoutExceeded) {
  SetExpectedConnectionResult(false);
  http_request_manager()->MakeRequest(
      GURL(kFakeUrl), base::Milliseconds(timeout_ms),
      base::BindOnce(&HttpRequestManagerTest::VerifyConnected, weak_ptr()));
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  // Advance the clock by |timeout_ms| + 1 milliseconds.
  task_environment().FastForwardBy(base::Milliseconds(timeout_ms + 1));
  // HTTP requests time out after |timeout_ms| milliseconds.
  EXPECT_EQ(0, test_url_loader_factory().NumPending());
  VerifyCallbackInvoked(true);
}

TEST_F(HttpRequestManagerTest, TestRetryHttpRequest) {
  SetExpectedConnectionResult(true);
  http_request_manager()->MakeRequest(
      GURL(kFakeUrl), base::Milliseconds(timeout_ms),
      base::BindOnce(&HttpRequestManagerTest::VerifyConnected, weak_ptr()));
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  EXPECT_TRUE(test_url_loader_factory().SimulateResponseForPendingRequest(
      kFakeUrl, /*content=*/"", net::HTTP_INTERNAL_SERVER_ERROR));
  // HTTP requests are retried on HTTP 5XX errors, hence it is expected there is
  // a pending request.
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  EXPECT_TRUE(test_url_loader_factory().SimulateResponseForPendingRequest(
      kFakeUrl, /*content=*/"", net::HTTP_NO_CONTENT));
  EXPECT_EQ(0, test_url_loader_factory().NumPending());
  VerifyCallbackInvoked(true);
}

TEST_F(HttpRequestManagerTest, TestOverlappingRequests) {
  SetExpectedConnectionResult(true);
  http_request_manager()->MakeRequest(
      GURL(kFakeUrl), base::Milliseconds(timeout_ms),
      base::BindOnce(&HttpRequestManagerTest::VerifyConnected, weak_ptr()));
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  // Advance the the clock by |timeout_ms| - 1 milliseconds, ensuring the
  // request has not timed out.
  task_environment().FastForwardBy(base::Milliseconds(timeout_ms - 1));
  // Launch another HTTP request.
  http_request_manager()->MakeRequest(
      GURL(kFakeUrl), base::Milliseconds(timeout_ms),
      base::BindOnce(&HttpRequestManagerTest::VerifyConnected, weak_ptr()));
  // Only one request is expected because the first request was cancelled when
  // the second one was created.
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  EXPECT_TRUE(test_url_loader_factory().SimulateResponseForPendingRequest(
      kFakeUrl, /*content=*/"", net::HTTP_NO_CONTENT));
  EXPECT_EQ(0, test_url_loader_factory().NumPending());
  VerifyCallbackInvoked(true);
}

TEST_F(HttpRequestManagerTest, TestNonOverlappingRequests) {
  SetExpectedConnectionResult(false);
  http_request_manager()->MakeRequest(
      GURL(kFakeUrl), base::Milliseconds(timeout_ms),
      base::BindOnce(&HttpRequestManagerTest::VerifyConnected, weak_ptr()));
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  EXPECT_TRUE(test_url_loader_factory().SimulateResponseForPendingRequest(
      kFakeUrl, /*content=*/"", net::HTTP_BAD_REQUEST));
  EXPECT_EQ(0, test_url_loader_factory().NumPending());
  VerifyCallbackInvoked(true);

  ResetCallbackInvoked();
  // Advance the clock by |timeout_ms| + 1 milliseconds to simulate that the
  // second request does not overlap with the first.
  task_environment().FastForwardBy(base::Milliseconds(timeout_ms + 1));
  SetExpectedConnectionResult(true);
  http_request_manager()->MakeRequest(
      GURL(kFakeUrl), base::Milliseconds(timeout_ms),
      base::BindOnce(&HttpRequestManagerTest::VerifyConnected, weak_ptr()));
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  EXPECT_TRUE(test_url_loader_factory().SimulateResponseForPendingRequest(
      kFakeUrl, /*content=*/"", net::HTTP_NO_CONTENT));
  VerifyCallbackInvoked(true);
}

TEST_F(HttpRequestManagerTest, TestManagerDestroyedWhenRequestPending) {
  // A connection result will not be returned in this scenario and
  // VerifyConnected() should be invoked.
  http_request_manager()->MakeRequest(
      GURL(kFakeUrl), base::Milliseconds(timeout_ms),
      base::BindOnce(&HttpRequestManagerTest::VerifyConnected, weak_ptr()));
  EXPECT_EQ(1, test_url_loader_factory().NumPending());
  ResetHttpRequestManager();
  // Http request canceled.
  EXPECT_EQ(0, test_url_loader_factory().NumPending());
  VerifyCallbackInvoked(false);
}

}  // namespace network_diagnostics
}  // namespace ash
