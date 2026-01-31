// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_http_service_handler.h"

#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/devtools/devtools_dispatch_http_request_params.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This class uses the actual implementation of the CanMakeRequest
class TestServiceHandler : public DevToolsHttpServiceHandler {
 public:
  TestServiceHandler() = default;
  ~TestServiceHandler() override = default;

  GURL BaseURL() const override { return GURL("http://localhost:8000"); }
  signin::OAuthConsumerId OAuthConsumerId() const override {
    return signin::OAuthConsumerId::kDevtoolsAida;
  }
  net::NetworkTrafficAnnotationTag NetworkTrafficAnnotationTag()
      const override {
    return TRAFFIC_ANNOTATION_FOR_TESTS;
  }
};

class DevToolsHttpServiceHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    mock_handler_ = base::WrapUnique(new TestServiceHandler());

    params_.service = "unknownService";
    params_.path = "/path";
    params_.method = "GET";

    interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindRepeating(&DevToolsHttpServiceHandlerTest::InterceptRequest,
                            base::Unretained(this)));
  }

  void TearDown() override {
    interceptor_.reset();
    mock_handler_.reset();
    identity_test_env_adaptor_.reset();
    profile_.reset();
  }

  bool InterceptRequest(content::URLLoaderInterceptor::RequestParams* params) {
    if (intercept_callback_) {
      return intercept_callback_.Run(params);
    }
    return false;
  }

  DevToolsDispatchHttpRequestParams params_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<TestServiceHandler> mock_handler_;
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
  base::RepeatingCallback<bool(content::URLLoaderInterceptor::RequestParams*)>
      intercept_callback_;
};

TEST_F(DevToolsHttpServiceHandlerTest, RequestWithNullProfileFails) {
  base::test::TestFuture<std::unique_ptr<DevToolsHttpServiceHandler::Result>>
      result_future;

  mock_handler_->Request(nullptr, params_, std::nullopt,
                         result_future.GetCallback());

  std::unique_ptr<DevToolsHttpServiceHandler::Result> result =
      result_future.Take();

  ASSERT_TRUE(result);
  EXPECT_EQ(result->error,
            DevToolsHttpServiceHandler::Result::Error::kValidationFailed);
}

TEST_F(DevToolsHttpServiceHandlerTest, RequestWithOTRProfileFails) {
  base::test::TestFuture<std::unique_ptr<DevToolsHttpServiceHandler::Result>>
      result_future;

  auto* incognito_profile = profile_->GetPrimaryOTRProfile(true);
  mock_handler_->Request(incognito_profile, params_, std::nullopt,
                         result_future.GetCallback());

  std::unique_ptr<DevToolsHttpServiceHandler::Result> result =
      result_future.Take();

  ASSERT_TRUE(result);
  EXPECT_EQ(result->error,
            DevToolsHttpServiceHandler::Result::Error::kValidationFailed);
}

TEST_F(DevToolsHttpServiceHandlerTest, RequestStreamingSuccess) {
  std::string received_data;
  auto stream_writer = base::BindLambdaForTesting(
      [&received_data](std::string_view chunk) { received_data += chunk; });

  intercept_callback_ = base::BindLambdaForTesting(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        content::URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\nContent-Type: text/plain\n\n", "Hello, world!",
            params->client.get());
        return true;
      });

  base::test::TestFuture<std::unique_ptr<DevToolsHttpServiceHandler::Result>>
      result_future;

  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "test@google.com", signin::ConsentLevel::kSignin);

  mock_handler_->Request(profile_.get(), params_, std::move(stream_writer),
                         result_future.GetCallback());

  identity_test_env_adaptor_->identity_test_env()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "test_token", base::Time::Max());

  std::unique_ptr<DevToolsHttpServiceHandler::Result> result =
      result_future.Take();

  ASSERT_TRUE(result);
  EXPECT_EQ(result->error, DevToolsHttpServiceHandler::Result::Error::kNone);
  EXPECT_EQ(result->http_status, 200);
  EXPECT_EQ(received_data, "Hello, world!");
  EXPECT_TRUE(!result->response_body.has_value());
}

TEST_F(DevToolsHttpServiceHandlerTest, RequestStreamingNetworkError) {
  auto stream_writer =
      base::BindLambdaForTesting([](std::string_view chunk) {});

  intercept_callback_ = base::BindLambdaForTesting(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        params->client->OnComplete(
            network::URLLoaderCompletionStatus(net::ERR_FAILED));
        return true;
      });

  base::test::TestFuture<std::unique_ptr<DevToolsHttpServiceHandler::Result>>
      result_future;

  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "test@google.com", signin::ConsentLevel::kSignin);

  mock_handler_->Request(profile_.get(), params_, std::move(stream_writer),
                         result_future.GetCallback());

  identity_test_env_adaptor_->identity_test_env()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "test_token", base::Time::Max());

  std::unique_ptr<DevToolsHttpServiceHandler::Result> result =
      result_future.Take();

  ASSERT_TRUE(result);
  EXPECT_EQ(result->error,
            DevToolsHttpServiceHandler::Result::Error::kNetworkError);
  EXPECT_EQ(result->net_error, net::ERR_FAILED);
}

TEST_F(DevToolsHttpServiceHandlerTest, RequestStreamingHttpError) {
  auto stream_writer =
      base::BindLambdaForTesting([](std::string_view chunk) {});

  intercept_callback_ = base::BindLambdaForTesting(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        content::URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 500 Internal Server Error\n\n", "Error",
            params->client.get());
        return true;
      });

  base::test::TestFuture<std::unique_ptr<DevToolsHttpServiceHandler::Result>>
      result_future;

  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "test@google.com", signin::ConsentLevel::kSignin);

  mock_handler_->Request(profile_.get(), params_, std::move(stream_writer),
                         result_future.GetCallback());

  identity_test_env_adaptor_->identity_test_env()
      ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "test_token", base::Time::Max());

  std::unique_ptr<DevToolsHttpServiceHandler::Result> result =
      result_future.Take();

  ASSERT_TRUE(result);
  EXPECT_EQ(result->error,
            DevToolsHttpServiceHandler::Result::Error::kHttpError);
  EXPECT_EQ(result->http_status, 500);
}

}  // namespace
