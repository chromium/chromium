// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/gcd_api_flow.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/printing/cloud_print/gcd_api_flow_impl.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::WithArgs;

namespace cloud_print {

namespace {

const char kConfirmRequest[] =
    "https://www.google.com/cloudprint/confirm?token=SomeToken";

const char kSampleConfirmResponse[] = "{}";

const char kFailedConfirmResponseBadJson[] = "[]";

const char kAccountId[] = "account_id@gmail.com";

class MockDelegate : public CloudPrintApiFlowRequest {
 public:
  MOCK_METHOD1(OnGCDApiFlowError, void(GCDApiFlow::Status));
  MOCK_METHOD1(OnGCDApiFlowComplete, void(const base::DictionaryValue&));
  MOCK_METHOD0(GetURL, GURL());
  MOCK_METHOD0(GetNetworkTrafficAnnotationType,
               GCDApiFlow::Request::NetworkTrafficAnnotation());
};

class GCDApiFlowTest : public testing::Test {
 public:
  GCDApiFlowTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  ~GCDApiFlowTest() override {}

 protected:
  void SetUp() override {
    identity_test_environment_.MakePrimaryAccountAvailable(kAccountId);

    std::unique_ptr<MockDelegate> delegate = std::make_unique<MockDelegate>();
    mock_delegate_ = delegate.get();
    EXPECT_CALL(*mock_delegate_, GetURL())
        .WillRepeatedly(Return(
            GURL("https://www.google.com/cloudprint/confirm?token=SomeToken")));
    gcd_flow_ = std::make_unique<GCDApiFlowImpl>(
        test_shared_url_loader_factory_.get(),
        identity_test_environment_.identity_manager());
    gcd_flow_->Start(std::move(delegate));
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<GCDApiFlowImpl> gcd_flow_;
  MockDelegate* mock_delegate_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

TEST_F(GCDApiFlowTest, SuccessOAuth2) {
  std::set<GURL> requested_urls;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        requested_urls.insert(request.url);
        std::string oauth_header;
        EXPECT_TRUE(request.headers.GetHeader("Authorization", &oauth_header));
        EXPECT_EQ("Bearer SomeToken", oauth_header);

        std::string proxy;
        EXPECT_TRUE(request.headers.GetHeader("X-Cloudprint-Proxy", &proxy));
        EXPECT_EQ("Chrome", proxy);
      }));

  gcd_flow_->OnAccessTokenFetchComplete(
      GoogleServiceAuthError::AuthErrorNone(),
      signin::AccessTokenInfo(
          "SomeToken", base::Time::Now() + base::TimeDelta::FromHours(1),
          std::string() /* No extra information needed for this test */));

  EXPECT_TRUE(base::Contains(requested_urls, GURL(kConfirmRequest)));

  test_url_loader_factory_.AddResponse(kConfirmRequest, kSampleConfirmResponse);

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_delegate_, OnGCDApiFlowComplete(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() { run_loop.Quit(); }));
  run_loop.Run();
}

TEST_F(GCDApiFlowTest, BadToken) {
  EXPECT_CALL(*mock_delegate_, OnGCDApiFlowError(GCDApiFlow::ERROR_TOKEN));
  gcd_flow_->OnAccessTokenFetchComplete(
      GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP),
      signin::AccessTokenInfo());
}

TEST_F(GCDApiFlowTest, BadJson) {
  std::set<GURL> requested_urls;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        requested_urls.insert(request.url);
      }));

  gcd_flow_->OnAccessTokenFetchComplete(
      GoogleServiceAuthError::AuthErrorNone(),
      signin::AccessTokenInfo(
          "SomeToken", base::Time::Now() + base::TimeDelta::FromHours(1),
          std::string() /* No extra information needed for this test */));

  EXPECT_TRUE(base::Contains(requested_urls, GURL(kConfirmRequest)));
  test_url_loader_factory_.AddResponse(kConfirmRequest,
                                       kFailedConfirmResponseBadJson);
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_delegate_,
              OnGCDApiFlowError(GCDApiFlow::ERROR_MALFORMED_RESPONSE))
      .WillOnce(testing::InvokeWithoutArgs([&]() { run_loop.Quit(); }));
  run_loop.Run();
}

}  // namespace

}  // namespace cloud_print
