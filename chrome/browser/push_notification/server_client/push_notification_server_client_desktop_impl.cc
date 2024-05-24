// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/server_client/push_notification_server_client_desktop_impl.h"

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/push_notification/metrics/push_notification_metrics.h"
#include "chrome/browser/push_notification/protos/notifications_multi_login_update.pb.h"
#include "chrome/browser/push_notification/server_client/push_notification_desktop_api_call_flow_impl.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Push Notification Service v1 Endpoints
const char kDefaultPushNotificationV1HTTPHost[] =
    "https://notifications-pa.googleapis.com";

const char kPushNotificationV1Path[] = "v1/";

const char kMultiLoginUpdatePath[] = "multiloginupdate";

const char kPushNotificationOath2Scope[] =
    "https://www.googleapis.com/auth/notifications";

const char kPushNotificationOAuthName[] = "push_notification_client";

// Creates the full Push Notification v1 URL for endpoint to the API with
// |request_path|.
GURL CreateV1RequestUrl(const std::string& request_path) {
  GURL google_apis_url = GURL(kDefaultPushNotificationV1HTTPHost);
  return google_apis_url.Resolve(kPushNotificationV1Path + request_path);
}

const net::PartialNetworkTrafficAnnotationTag&
GetRegisterPushNotificationServiceAnnotation() {
  static const net::PartialNetworkTrafficAnnotationTag annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "register_push_notification_service", "oauth2_api_call_flow",
          R"(
      semantics {
        sender: "Chrome Desktop Push Notification Service"
        description:
          "Registers the Chrome Desktop Push Notification Service"
          "with the MultiLoginUpdate API of the Chime Service. "
          "the call sends an OAuth 2.0 token and GCM token"
        trigger:
          "Automatically called during initialization of the  "
          "Chrome Desktop Push Notification Service "
        data:
          "Sends an OAuth 2.0 token and GCM token"
        destination: GOOGLE_OWNED_SERVICE
        internal {
            contacts {
              email: "akingsb@google.com"
            }
            contacts {
              email: "chromeos-cross-device-eng@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: ARBITRARY_DATA
          }
          last_reviewed: "2024-04-29"
      }
      policy {
        setting:
          "Only sent when Chrome Desktop Push Notification Service"
          "is enabled and the user is signed in with their Google account."
        chrome_policy {
          SigninAllowed {
            SigninAllowed: false
          }
        }
      })");
  return annotation;
}

}  // namespace

namespace push_notification {

// static
PushNotificationServerClientDesktopImpl::Factory*
    PushNotificationServerClientDesktopImpl::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<PushNotificationServerClient>
PushNotificationServerClientDesktopImpl::Factory::Create(
    std::unique_ptr<PushNotificationDesktopApiCallFlow> api_call_flow,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(std::move(api_call_flow),
                                           identity_manager,
                                           std::move(url_loader_factory));
  }

  return base::WrapUnique(new PushNotificationServerClientDesktopImpl(
      std::move(api_call_flow), identity_manager,
      std::move(url_loader_factory)));
}

// static
void PushNotificationServerClientDesktopImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  g_test_factory_ = test_factory;
}

PushNotificationServerClientDesktopImpl::Factory::~Factory() = default;

PushNotificationServerClientDesktopImpl::
    PushNotificationServerClientDesktopImpl(
        std::unique_ptr<PushNotificationDesktopApiCallFlow> api_call_flow,
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : api_call_flow_(std::move(api_call_flow)),
      url_loader_factory_(std::move(url_loader_factory)),
      has_call_started_(false),
      identity_manager_(identity_manager) {}

PushNotificationServerClientDesktopImpl::
    ~PushNotificationServerClientDesktopImpl() = default;

void PushNotificationServerClientDesktopImpl::
    RegisterWithPushNotificationService(
        const proto::NotificationsMultiLoginUpdateRequest& request,
        RegisterWithPushNotificationServiceCallback&& callback,
        ErrorCallback&& error_callback) {
  MakeApiCall(CreateV1RequestUrl(kMultiLoginUpdatePath), RequestType::kPost,
              request.SerializeAsString(),
              /*request_as_query_parameters=*/std::nullopt, std::move(callback),
              std::move(error_callback),
              GetRegisterPushNotificationServiceAnnotation());
}

std::optional<std::string>
PushNotificationServerClientDesktopImpl::GetAccessTokenUsed() {
  return access_token_used_;
}

template <class ResponseProto>
void PushNotificationServerClientDesktopImpl::MakeApiCall(
    const GURL& request_url,
    RequestType request_type,
    const std::optional<std::string>& serialized_request,
    const std::optional<PushNotificationDesktopApiCallFlow::QueryParameters>&
        request_as_query_parameters,
    base::OnceCallback<void(const ResponseProto&)>&& response_callback,
    ErrorCallback&& error_callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  CHECK(!has_call_started_) << "PushNotificationServerClientDesktopImpl::"
                               "MakeApiCall(): Tried to make an API "
                            << "call, but the client had already been used.";
  has_call_started_ = true;

  api_call_flow_->SetPartialNetworkTrafficAnnotation(
      partial_traffic_annotation);

  request_url_ = request_url;
  error_callback_ = std::move(error_callback);

  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(kPushNotificationOath2Scope);

  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          kPushNotificationOAuthName, identity_manager_, scopes,
          base::BindOnce(
              &PushNotificationServerClientDesktopImpl::OnAccessTokenFetched<
                  ResponseProto>,
              weak_ptr_factory_.GetWeakPtr(), request_type, serialized_request,
              request_as_query_parameters, std::move(response_callback)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

template <class ResponseProto>
void PushNotificationServerClientDesktopImpl::OnAccessTokenFetched(
    RequestType request_type,
    const std::optional<std::string>& serialized_request,
    const std::optional<PushNotificationDesktopApiCallFlow::QueryParameters>&
        request_as_query_parameters,
    base::OnceCallback<void(const ResponseProto&)>&& response_callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    OnApiCallFailed(PushNotificationDesktopApiCallFlow::
                        PushNotificationApiCallFlowError::kAuthenticationError);
    metrics::RecordPushNotificationOAuthTokenRetrievalResult(/*success=*/false);
    return;
  }
  metrics::RecordPushNotificationOAuthTokenRetrievalResult(/*success=*/true);
  access_token_used_ = access_token_info.token;
  CHECK(access_token_used_.has_value());
  proto::NotificationsMultiLoginUpdateRequest request_proto;

  switch (request_type) {
    case RequestType::kGet:
      CHECK(request_as_query_parameters && !serialized_request);
      api_call_flow_->StartGetRequest(
          request_url_, *request_as_query_parameters, url_loader_factory_,
          access_token_used_.value(),
          base::BindOnce(
              &PushNotificationServerClientDesktopImpl::OnFlowSuccess<
                  ResponseProto>,
              weak_ptr_factory_.GetWeakPtr(), std::move(response_callback)),
          base::BindOnce(
              &PushNotificationServerClientDesktopImpl::OnApiCallFailed,
              weak_ptr_factory_.GetWeakPtr()));
      break;
    case RequestType::kPost:
      CHECK(serialized_request && !request_as_query_parameters);

      // Copy a new `NotificationsMultiLoginUpdateRequest` proto so we can add
      // the access token as the oauth token.
      request_proto.ParseFromString(serialized_request.value());
      request_proto.mutable_registrations(0)
          ->mutable_user_id()
          ->mutable_gaia_credentials()
          ->set_oauth_token(access_token_used_.value());

      api_call_flow_->StartPostRequest(
          request_url_, request_proto.SerializeAsString(), url_loader_factory_,
          access_token_used_.value(),
          base::BindOnce(
              &PushNotificationServerClientDesktopImpl::OnFlowSuccess<
                  ResponseProto>,
              weak_ptr_factory_.GetWeakPtr(), std::move(response_callback)),
          base::BindOnce(
              &PushNotificationServerClientDesktopImpl::OnApiCallFailed,
              weak_ptr_factory_.GetWeakPtr()));
      break;
    case RequestType::kPatch:
      CHECK(serialized_request && !request_as_query_parameters);
      api_call_flow_->StartPatchRequest(
          request_url_, *serialized_request, url_loader_factory_,
          access_token_used_.value(),
          base::BindOnce(
              &PushNotificationServerClientDesktopImpl::OnFlowSuccess<
                  ResponseProto>,
              weak_ptr_factory_.GetWeakPtr(), std::move(response_callback)),
          base::BindOnce(
              &PushNotificationServerClientDesktopImpl::OnApiCallFailed,
              weak_ptr_factory_.GetWeakPtr()));
      break;
  }
}

template <class ResponseProto>
void PushNotificationServerClientDesktopImpl::OnFlowSuccess(
    base::OnceCallback<void(const ResponseProto&)>&& result_callback,
    const std::string& serialized_response) {
  ResponseProto response;
  if (!response.ParseFromString(serialized_response)) {
    OnApiCallFailed(PushNotificationDesktopApiCallFlow::
                        PushNotificationApiCallFlowError::kResponseMalformed);
    return;
  }
  std::move(result_callback).Run(response);
}

void PushNotificationServerClientDesktopImpl::OnApiCallFailed(
    PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError
        error) {
  std::move(error_callback_).Run(error);
}

}  // namespace push_notification
