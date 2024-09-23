// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/server_client/push_notification_server_client_desktop_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/push_notification/metrics/push_notification_metrics.h"
#include "chrome/browser/push_notification/protos/notifications_multi_login_update.pb.h"
#include "chrome/browser/push_notification/server_client/push_notification_desktop_api_call_flow.h"
#include "chrome/browser/push_notification/server_client/push_notification_desktop_api_call_flow_impl.h"
#include "chrome/browser/push_notification/server_client/push_notification_server_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kGet[] = "GET";
const char kPost[] = "POST";
const char kPatch[] = "PATCH";
const char kAccessToken[] = "access_token";
const char kMultiLoginPath[] = "multiloginupdate";
const char kEmail[] = "test@gmail.com";
const char kTestGoogleApisUrl[] = "https://notifications-pa.googleapis.com";
const char kRegistrationId[] = "my-device";
const char kApplicationId[] = "com.google.chrome.push_notification";
const char kToken[] = "my-token";
const char kClientId[] = "ChromeDesktop";
const char kOAuthTokenRetrievalResult[] =
    "PushNotification.ChromeOS.OAuth.Token.RetrievalResult";

class FakePushNotificationApiCallFlow
    : public push_notification::PushNotificationDesktopApiCallFlow {
 public:
  FakePushNotificationApiCallFlow() = default;
  ~FakePushNotificationApiCallFlow() override = default;
  FakePushNotificationApiCallFlow(FakePushNotificationApiCallFlow&) = delete;
  FakePushNotificationApiCallFlow& operator=(FakePushNotificationApiCallFlow&) =
      delete;
  void StartPostRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override {
    http_method_ = kPost;
    request_url_ = request_url;
    serialized_request_ = serialized_request;
    url_loader_factory_ = url_loader_factory;
    result_callback_ = std::move(result_callback);
    error_callback_ = std::move(error_callback);
    EXPECT_EQ(kAccessToken, access_token);
  }
  void StartPatchRequest(
      const GURL& request_url,
      const std::string& serialized_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override {
    http_method_ = kPatch;
    request_url_ = request_url;
    serialized_request_ = serialized_request;
    url_loader_factory_ = url_loader_factory;
    result_callback_ = std::move(result_callback);
    error_callback_ = std::move(error_callback);
    EXPECT_EQ(kAccessToken, access_token);
  }
  void StartGetRequest(
      const GURL& request_url,
      const QueryParameters& request_as_query_parameters,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token,
      ResultCallback&& result_callback,
      ErrorCallback&& error_callback) override {
    http_method_ = kGet;
    request_url_ = request_url;
    request_as_query_parameters_ = request_as_query_parameters;
    url_loader_factory_ = url_loader_factory;
    result_callback_ = std::move(result_callback);
    error_callback_ = std::move(error_callback);
    EXPECT_EQ(kAccessToken, access_token);
  }
  void SetPartialNetworkTrafficAnnotation(
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      override {
    NOTIMPLEMENTED();
  }

  std::string http_method_;
  GURL request_url_;
  std::string serialized_request_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  ResultCallback result_callback_;
  ErrorCallback error_callback_;
  QueryParameters request_as_query_parameters_;
};

// Callback that should never be invoked.
template <class T>
void NotCalled(T type) {
  EXPECT_TRUE(false);
}
// Callback that should never be invoked.
template <class T>
void NotCalledConstRef(const T& type) {
  EXPECT_TRUE(false);
}

}  // namespace

namespace push_notification {

class PushNotificationServerClientDesktopImplTest : public testing::Test {
 protected:
  PushNotificationServerClientDesktopImplTest() {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            base::BindOnce([]() -> network::mojom::URLLoaderFactory* {
              ADD_FAILURE() << "Did not expect this to actually be used";
              return nullptr;
            }));
  }

  void SetUp() override {
    identity_test_environment_.MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSync);
    std::unique_ptr<FakePushNotificationApiCallFlow> api_call_flow =
        std::make_unique<FakePushNotificationApiCallFlow>();
    api_call_flow_ = api_call_flow.get();
    client_ = PushNotificationServerClientDesktopImpl::Factory::Create(
        std::move(api_call_flow), identity_test_environment_.identity_manager(),
        shared_factory_);
    histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                        /*bucket: failure=*/0, 0);
    histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                        /*bucket: success=*/1, 0);
  }

  const std::string& http_method() { return api_call_flow_->http_method_; }

  const GURL& request_url() { return api_call_flow_->request_url_; }

  const std::string& serialized_request() {
    return api_call_flow_->serialized_request_;
  }

  // Returns |response_proto| as the result to the current API request.
  void FinishApiCallFlow(const google::protobuf::MessageLite* response_proto) {
    std::move(api_call_flow_->result_callback_)
        .Run(response_proto->SerializeAsString());
  }

  // Returns |serialized_proto| as the result to the current API request.
  void FinishApiCallFlowRaw(const std::string& serialized_proto) {
    std::move(api_call_flow_->result_callback_).Run(serialized_proto);
  }

  // Ends the current API request with |error|.
  void FailApiCallFlow(
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError
          error) {
    std::move(api_call_flow_->error_callback_).Run(error);
  }

  push_notification::proto::NotificationsMultiLoginUpdateRequest
  CreateRequestProto() {
    push_notification::proto::NotificationsMultiLoginUpdateRequest
        request_proto;
    request_proto.mutable_target()->set_channel_type(
        push_notification::proto::ChannelType::GCM_DEVICE_PUSH);
    request_proto.mutable_target()
        ->mutable_delivery_address()
        ->mutable_gcm_device_address()
        ->set_registration_id(kRegistrationId);
    request_proto.mutable_target()
        ->mutable_delivery_address()
        ->mutable_gcm_device_address()
        ->set_application_id(kApplicationId);
    request_proto.add_registrations();

    request_proto.mutable_registrations(0)
        ->mutable_user_id()
        ->mutable_gaia_credentials()
        ->set_oauth_token(kToken);
    request_proto.set_client_id(kClientId);
    return request_proto;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  signin::IdentityTestEnvironment identity_test_environment_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<PushNotificationServerClient> client_;
  raw_ptr<FakePushNotificationApiCallFlow> api_call_flow_;
};

TEST_F(PushNotificationServerClientDesktopImplTest,
       RegisterWithPushNotificationServiceSuccess) {
  base::test::TestFuture<
      const push_notification::proto::NotificationsMultiLoginUpdateResponse&>
      future;
  auto request_proto = CreateRequestProto();

  client_->RegisterWithPushNotificationService(
      request_proto, future.GetCallback(),
      base::BindOnce(&NotCalled<PushNotificationDesktopApiCallFlow::
                                    PushNotificationApiCallFlowError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());
  EXPECT_EQ(kPost, http_method());
  EXPECT_EQ(request_url(), GURL(std::string(kTestGoogleApisUrl) + "/v1/" +
                                std::string(kMultiLoginPath)));
  push_notification::proto::NotificationsMultiLoginUpdateRequest
      expected_request;
  EXPECT_TRUE(expected_request.ParseFromString(serialized_request()));
  EXPECT_EQ(kClientId, expected_request.client_id());
  push_notification::proto::NotificationsMultiLoginUpdateResponse
      response_proto;
  push_notification::proto::NotificationsMultiLoginUpdateResponse::
      RegistrationResult* registration_result =
          response_proto.add_registration_results();
  push_notification::proto::StatusProto* status =
      registration_result->mutable_status();
  status->set_code(0);
  status->set_message("OK");
  FinishApiCallFlow(&response_proto);
  // Check that the result received in callback is the same as the response.
  push_notification::proto::NotificationsMultiLoginUpdateResponse result_proto =
      future.Take();
  EXPECT_EQ(0, result_proto.registration_results(0).status().code());
  EXPECT_EQ("OK", result_proto.registration_results(0).status().message());
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: failure=*/0, 0);
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: success=*/1, 1);
}

TEST_F(PushNotificationServerClientDesktopImplTest,
       RegisterWithPushNotificationServiceFailure) {
  auto request_proto = CreateRequestProto();

  base::test::TestFuture<
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError>
      future;
  client_->RegisterWithPushNotificationService(
      request_proto,
      base::BindOnce(
          &NotCalledConstRef<
              push_notification::proto::NotificationsMultiLoginUpdateResponse>),
      future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());
  EXPECT_EQ(kPost, http_method());
  EXPECT_EQ(request_url(), GURL(std::string(kTestGoogleApisUrl) + "/v1/" +
                                std::string(kMultiLoginPath)));
  FailApiCallFlow(PushNotificationDesktopApiCallFlow::
                      PushNotificationApiCallFlowError::kInternalServerError);
  EXPECT_EQ(PushNotificationDesktopApiCallFlow::
                PushNotificationApiCallFlowError::kInternalServerError,
            future.Get());
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: failure=*/0, 0);
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: success=*/1, 1);
}

TEST_F(PushNotificationServerClientDesktopImplTest, FetchAccessTokenFailure) {
  base::test::TestFuture<
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError>
      future;
  client_->RegisterWithPushNotificationService(
      push_notification::proto::NotificationsMultiLoginUpdateRequest(),
      base::BindOnce(
          &NotCalledConstRef<
              push_notification::proto::NotificationsMultiLoginUpdateResponse>),
      future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  EXPECT_EQ(PushNotificationDesktopApiCallFlow::
                PushNotificationApiCallFlowError::kAuthenticationError,
            future.Get());
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: success=*/1, 0);
}

TEST_F(PushNotificationServerClientDesktopImplTest, ParseResponseProtoFailure) {
  auto request_proto = CreateRequestProto();

  base::test::TestFuture<
      PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError>
      future;
  client_->RegisterWithPushNotificationService(
      request_proto,
      base::BindOnce(
          &NotCalledConstRef<
              push_notification::proto::NotificationsMultiLoginUpdateResponse>),
      future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());
  EXPECT_EQ(kPost, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/" +
                               std::string(kMultiLoginPath));
  FinishApiCallFlowRaw("Not a valid serialized response message.");
  EXPECT_EQ(PushNotificationDesktopApiCallFlow::
                PushNotificationApiCallFlowError::kResponseMalformed,
            future.Get());
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: failure=*/0, 0);
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: success=*/1, 1);
}

TEST_F(PushNotificationServerClientDesktopImplTest,
       MakeSecondRequestBeforeFirstRequestSucceeds) {
  auto request_proto = CreateRequestProto();

  // Make first request.
  base::test::TestFuture<
      const push_notification::proto::NotificationsMultiLoginUpdateResponse&>
      future;
  client_->RegisterWithPushNotificationService(
      request_proto, future.GetCallback(),
      base::BindOnce(&NotCalled<PushNotificationDesktopApiCallFlow::
                                    PushNotificationApiCallFlowError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());
  EXPECT_EQ(kPost, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/" +
                               std::string(kMultiLoginPath));
  // With request pending, make second request.
  {
    base::test::TestFuture<
        PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError>
        future2;
    EXPECT_DCHECK_DEATH(client_->RegisterWithPushNotificationService(
        push_notification::proto::NotificationsMultiLoginUpdateRequest(),
        base::BindOnce(
            &NotCalledConstRef<push_notification::proto::
                                   NotificationsMultiLoginUpdateResponse>),
        future2.GetCallback()));
  }
  // Complete first request.
  {
    push_notification::proto::NotificationsMultiLoginUpdateResponse
        response_proto;
    push_notification::proto::NotificationsMultiLoginUpdateResponse::
        RegistrationResult* registration_result =
            response_proto.add_registration_results();
    push_notification::proto::StatusProto* status =
        registration_result->mutable_status();
    status->set_code(0);
    status->set_message("OK");
    FinishApiCallFlow(&response_proto);
  }
  push_notification::proto::NotificationsMultiLoginUpdateResponse result_proto =
      future.Take();
  EXPECT_EQ(0, result_proto.registration_results(0).status().code());
  EXPECT_EQ("OK", result_proto.registration_results(0).status().message());
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: failure=*/0, 0);
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: success=*/1, 1);
}

TEST_F(PushNotificationServerClientDesktopImplTest,
       MakeSecondRequestAfterFirstRequestSucceeds) {
  // Make first request successfully.
  {
    base::test::TestFuture<
        const push_notification::proto::NotificationsMultiLoginUpdateResponse&>
        future;
    auto request_proto = CreateRequestProto();

    client_->RegisterWithPushNotificationService(
        request_proto, future.GetCallback(),
        base::BindOnce(&NotCalled<PushNotificationDesktopApiCallFlow::
                                      PushNotificationApiCallFlowError>));
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            kAccessToken, base::Time::Max());
    EXPECT_EQ(kPost, http_method());
    EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/" +
                                 std::string(kMultiLoginPath));
    push_notification::proto::NotificationsMultiLoginUpdateResponse
        response_proto;
    push_notification::proto::NotificationsMultiLoginUpdateResponse::
        RegistrationResult* registration_result =
            response_proto.add_registration_results();
    push_notification::proto::StatusProto* status =
        registration_result->mutable_status();
    status->set_code(0);
    status->set_message("OK");
    FinishApiCallFlow(&response_proto);
    push_notification::proto::NotificationsMultiLoginUpdateResponse
        result_proto = future.Take();
    EXPECT_EQ(0, result_proto.registration_results(0).status().code());
    EXPECT_EQ("OK", result_proto.registration_results(0).status().message());
  }
  // Second request fails.
  {
    base::test::TestFuture<
        PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError>
        future;
    EXPECT_DCHECK_DEATH(client_->RegisterWithPushNotificationService(
        push_notification::proto::NotificationsMultiLoginUpdateRequest(),
        base::BindOnce(
            &NotCalledConstRef<push_notification::proto::
                                   NotificationsMultiLoginUpdateResponse>),
        future.GetCallback()));
  }
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: failure=*/0, 0);
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: success=*/1, 1);
}

TEST_F(PushNotificationServerClientDesktopImplTest, GetAccessTokenUsed) {
  EXPECT_FALSE(client_->GetAccessTokenUsed().has_value());
  base::test::TestFuture<
      const push_notification::proto::NotificationsMultiLoginUpdateResponse&>
      future;
  auto request_proto = CreateRequestProto();

  client_->RegisterWithPushNotificationService(
      request_proto, future.GetCallback(),
      base::BindOnce(&NotCalled<PushNotificationDesktopApiCallFlow::
                                    PushNotificationApiCallFlowError>));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          kAccessToken, base::Time::Max());
  EXPECT_EQ(kPost, http_method());
  EXPECT_EQ(request_url(), std::string(kTestGoogleApisUrl) + "/v1/" +
                               std::string(kMultiLoginPath));
  EXPECT_EQ(kAccessToken, client_->GetAccessTokenUsed().value());
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: failure=*/0, 0);
  histogram_tester_.ExpectBucketCount(kOAuthTokenRetrievalResult,
                                      /*bucket: success=*/1, 1);
}

}  // namespace push_notification
