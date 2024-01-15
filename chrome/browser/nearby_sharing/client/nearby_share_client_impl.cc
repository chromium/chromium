// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/client/nearby_share_client_impl.h"

#include <memory>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_http_notifier.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_switches.h"
#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow_impl.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "components/cross_device/logging/logging.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/nearby/sharing/proto/certificate_rpc.pb.h"
#include "third_party/nearby/sharing/proto/contact_rpc.pb.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

namespace {

// -------------------- Nearby Share Service v1 Endpoints --------------------

const char kDefaultNearbyShareV1HTTPHost[] =
    "https://nearbysharing-pa.googleapis.com";

const char kNearbyShareV1Path[] = "v1/";

const char kListContactPeoplePath[] = "contactRecords";
const char kListPublicCertificatesPath[] = "publicCertificates";

const char kPageSize[] = "page_size";
const char kPageToken[] = "page_token";
const char kSecretIds[] = "secret_ids";

const char kNearbyShareOAuth2Scope[] =
    "https://www.googleapis.com/auth/nearbysharing-pa";

// Creates the full Nearby Share v1 URL for endpoint to the API with
// |request_path|.
GURL CreateV1RequestUrl(const std::string& request_path) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GURL google_apis_url = command_line->HasSwitch(switches::kNearbyShareHTTPHost)
                             ? GURL(command_line->GetSwitchValueASCII(
                                   switches::kNearbyShareHTTPHost))
                             : GURL(kDefaultNearbyShareV1HTTPHost);
  return google_apis_url.Resolve(kNearbyShareV1Path + request_path);
}

ash::nearby::NearbyApiCallFlow::QueryParameters
ListContactPeopleRequestToQueryParameters(
    const nearby::sharing::proto::ListContactPeopleRequest& request) {
  ash::nearby::NearbyApiCallFlow::QueryParameters query_parameters;
  if (request.page_size() > 0) {
    query_parameters.emplace_back(kPageSize,
                                  base::NumberToString(request.page_size()));
  }
  if (!request.page_token().empty()) {
    query_parameters.emplace_back(kPageToken, request.page_token());
  }
  return query_parameters;
}

ash::nearby::NearbyApiCallFlow::QueryParameters
ListPublicCertificatesRequestToQueryParameters(
    const nearby::sharing::proto::ListPublicCertificatesRequest& request) {
  ash::nearby::NearbyApiCallFlow::QueryParameters query_parameters;
  if (request.page_size() > 0) {
    query_parameters.emplace_back(kPageSize,
                                  base::NumberToString(request.page_size()));
  }
  if (!request.page_token().empty()) {
    query_parameters.emplace_back(kPageToken, request.page_token());
  }
  for (const std::string& id : request.secret_ids()) {
    // NOTE: One Platform requires that byte fields be URL-safe base64 encoded.
    std::string encoded_id;
    base::Base64UrlEncode(id, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &encoded_id);
    query_parameters.emplace_back(kSecretIds, encoded_id);
  }
  return query_parameters;
}

// TODO(crbug.com/1103471): Update "chrome_policy" when a Nearby Share
// enterprise policy is created.
const net::PartialNetworkTrafficAnnotationTag& GetUpdateDeviceAnnotation() {
  static const net::PartialNetworkTrafficAnnotationTag annotation =
      net::DefinePartialNetworkTrafficAnnotation("nearby_share_update_device",
                                                 "oauth2_api_call_flow",
                                                 R"(
      semantics {
        sender: "Nearby Share"
        description:
          "Used as part of the Nearby Share feature that allows users to "
          "share files or text with trusted contacts within a certain physical "
          "proximity. The call sends the local device's user-defined name and "
          "their list of allowed contacts to the Google-owned Nearby server. "
          "Nearby-Share-specific crypto data from the local device is also "
          "uploaded to the server and distributed to trusted contacts to help "
          "establish an authenticated channel during the Nearby Share flow. "
          "This crypto data can be immediately invalidated by the local device "
          "at any time without needing to communicate with the server. For "
          "example, it expires after three days and new data needs to be "
          "uploaded. Crypto data is also invalidated if the user's list of "
          "allowed contacts changes. The server returns the local device "
          "user's full name and icon URL if available on the Google server."
        trigger:
          "Automatically called daily to retrieve any updates to the user's "
          "full name or icon URL. This request is also sent whenever the user "
          "changes their device name in settings, whenever the user changes "
          "their list of allowed contacts, or whenever new crypto data is "
          "generated by the local device and needs to be shared with trusted "
          "contacts."
        data:
          "Sends an OAuth 2.0 token, the local device's name, contact, and/or "
          "Nearby-Share-specific crypto data. Possibly receives the user's "
          "full name and icon URL from the Google server."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        setting:
          "Only sent when Nearby Share is enabled and the user is signed in "
          "with their Google account."
        chrome_policy {
          SigninAllowed {
            SigninAllowed: false
          }
        }
      })");
  return annotation;
}

// TODO(crbug.com/1103471): Update "chrome_policy" when a Nearby Share
// enterprise policy is created.
const net::PartialNetworkTrafficAnnotationTag& GetContactsAnnotation() {
  static const net::PartialNetworkTrafficAnnotationTag annotation =
      net::DefinePartialNetworkTrafficAnnotation("nearby_share_contacts",
                                                 "oauth2_api_call_flow",
                                                 R"(
      semantics {
        sender: "Nearby Share"
        description:
          "Used as part of the Nearby Share feature that allows users to "
          "share files or text with trusted contacts within a certain physical "
          "proximity. The call retrieves the user's list of contacts from the "
          "Google-owned People server via the Google-owned Nearby server."
        trigger:
          "Called multiple times a day to check for possible updates to the "
          "users's contact list. It is also invoked during Nearby Share "
          "onboarding and when the user is in the Nearby Share settings."
        data:
          "Sends an OAuth 2.0 token. Receives the user's contact list, which "
          "includes phone numbers and email addresses."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        setting:
          "Only sent when Nearby Share is enabled and the user is signed in "
          "with their Google account."
        chrome_policy {
          SigninAllowed {
            SigninAllowed: false
          }
        }
          })");
  return annotation;
}

// TODO(crbug.com/1103471): Update "chrome_policy" when a Nearby Share
// enterprise policy is created.
const net::PartialNetworkTrafficAnnotationTag&
GetListPublicCertificatesAnnotation() {
  static const net::PartialNetworkTrafficAnnotationTag annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "nearby_share_list_public_certificates", "oauth2_api_call_flow",
          R"(
      semantics {
        sender: "Nearby Share"
        description:
          "Used as part of the Nearby Share feature that allows users to "
          "share files or text with trusted contacts within a certain physical "
          "proximity. The call retrieves Nearby-Share-specific crypto data "
          "from the Google-owned Nearby server. The data was uploaded by other "
          "devices and is needed to establish an authenticated connection with "
          "those device during the Nearby Share flow."
        trigger:
          "Automatically called at least once a day to retrieve any updates to "
          "the list of crypto data. It is also called when Nearby Share is in "
          "use to ensure up-to-date data."
        data:
          "Sends an OAuth 2.0 token. Receives Nearby-Share-specific crypto "
          "necessary for establishing an authenticated channel with other "
          "devices."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        setting:
          "Only sent when Nearby Share is enabled and the user is signed in "
          "with their Google account."
        chrome_policy {
          SigninAllowed {
            SigninAllowed: false
          }
        }
          })");
  return annotation;
}

}  // namespace

NearbyShareClientImpl::NearbyShareClientImpl(
    std::unique_ptr<ash::nearby::NearbyApiCallFlow> api_call_flow,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    NearbyShareHttpNotifier* notifier)
    : api_call_flow_(std::move(api_call_flow)),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      notifier_(notifier),
      has_call_started_(false) {}

NearbyShareClientImpl::~NearbyShareClientImpl() = default;

void NearbyShareClientImpl::UpdateDevice(
    const nearby::sharing::proto::UpdateDeviceRequest& request,
    UpdateDeviceCallback&& callback,
    ErrorCallback&& error_callback) {
  notifier_->NotifyOfRequest(request);
  MakeApiCall(CreateV1RequestUrl(request.device().name()), RequestType::kPatch,
              request.SerializeAsString(),
              /*request_as_query_parameters=*/std::nullopt, std::move(callback),
              std::move(error_callback), GetUpdateDeviceAnnotation());
}

void NearbyShareClientImpl::ListContactPeople(
    const nearby::sharing::proto::ListContactPeopleRequest& request,
    ListContactPeopleCallback&& callback,
    ErrorCallback&& error_callback) {
  notifier_->NotifyOfRequest(request);
  MakeApiCall(CreateV1RequestUrl(kListContactPeoplePath), RequestType::kGet,
              /*serialized_request=*/std::nullopt,
              ListContactPeopleRequestToQueryParameters(request),
              std::move(callback), std::move(error_callback),
              GetContactsAnnotation());
}

void NearbyShareClientImpl::ListPublicCertificates(
    const nearby::sharing::proto::ListPublicCertificatesRequest& request,
    ListPublicCertificatesCallback&& callback,
    ErrorCallback&& error_callback) {
  notifier_->NotifyOfRequest(request);
  MakeApiCall(
      CreateV1RequestUrl(request.parent() + "/" + kListPublicCertificatesPath),
      RequestType::kGet, /*serialized_request=*/std::nullopt,
      ListPublicCertificatesRequestToQueryParameters(request),
      std::move(callback), std::move(error_callback),
      GetListPublicCertificatesAnnotation());
}

std::string NearbyShareClientImpl::GetAccessTokenUsed() {
  return access_token_used_;
}

template <class ResponseProto>
void NearbyShareClientImpl::MakeApiCall(
    const GURL& request_url,
    RequestType request_type,
    const std::optional<std::string>& serialized_request,
    const std::optional<ash::nearby::NearbyApiCallFlow::QueryParameters>&
        request_as_query_parameters,
    base::OnceCallback<void(const ResponseProto&)>&& response_callback,
    ErrorCallback&& error_callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  DCHECK(!has_call_started_)
      << "NearbyShareClientImpl::MakeApiCall(): Tried to make an API "
      << "call, but the client had already been used.";
  has_call_started_ = true;

  api_call_flow_->SetPartialNetworkTrafficAnnotation(
      partial_traffic_annotation);

  request_url_ = request_url;
  error_callback_ = std::move(error_callback);

  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(kNearbyShareOAuth2Scope);

  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "nearby_share_client", identity_manager_, scopes,
          base::BindOnce(
              &NearbyShareClientImpl::OnAccessTokenFetched<ResponseProto>,
              weak_ptr_factory_.GetWeakPtr(), request_type, serialized_request,
              request_as_query_parameters, std::move(response_callback)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

template <class ResponseProto>
void NearbyShareClientImpl::OnAccessTokenFetched(
    RequestType request_type,
    const std::optional<std::string>& serialized_request,
    const std::optional<ash::nearby::NearbyApiCallFlow::QueryParameters>&
        request_as_query_parameters,
    base::OnceCallback<void(const ResponseProto&)>&& response_callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    OnApiCallFailed(ash::nearby::NearbyHttpError::kAuthenticationError);
    return;
  }
  access_token_used_ = access_token_info.token;

  switch (request_type) {
    case RequestType::kGet:
      DCHECK(request_as_query_parameters && !serialized_request);
      api_call_flow_->StartGetRequest(
          request_url_, *request_as_query_parameters, url_loader_factory_,
          access_token_used_,
          base::BindOnce(&NearbyShareClientImpl::OnFlowSuccess<ResponseProto>,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(response_callback)),
          base::BindOnce(&NearbyShareClientImpl::OnApiCallFailed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case RequestType::kPost:
      DCHECK(serialized_request && !request_as_query_parameters);
      api_call_flow_->StartPostRequest(
          request_url_, *serialized_request, url_loader_factory_,
          access_token_used_,
          base::BindOnce(&NearbyShareClientImpl::OnFlowSuccess<ResponseProto>,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(response_callback)),
          base::BindOnce(&NearbyShareClientImpl::OnApiCallFailed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case RequestType::kPatch:
      DCHECK(serialized_request && !request_as_query_parameters);
      api_call_flow_->StartPatchRequest(
          request_url_, *serialized_request, url_loader_factory_,
          access_token_used_,
          base::BindOnce(&NearbyShareClientImpl::OnFlowSuccess<ResponseProto>,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(response_callback)),
          base::BindOnce(&NearbyShareClientImpl::OnApiCallFailed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
  }
}

template <class ResponseProto>
void NearbyShareClientImpl::OnFlowSuccess(
    base::OnceCallback<void(const ResponseProto&)>&& result_callback,
    const std::string& serialized_response) {
  ResponseProto response;
  if (!response.ParseFromString(serialized_response)) {
    OnApiCallFailed(ash::nearby::NearbyHttpError::kResponseMalformed);
    return;
  }
  notifier_->NotifyOfResponse(response);
  std::move(result_callback).Run(response);
}

void NearbyShareClientImpl::OnApiCallFailed(
    ash::nearby::NearbyHttpError error) {
  CD_LOG(ERROR, Feature::NS)
      << "Nearby Share RPC call failed with error " << error;
  std::move(error_callback_).Run(error);
}

NearbyShareClientFactoryImpl::NearbyShareClientFactoryImpl(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    NearbyShareHttpNotifier* notifier)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      notifier_(notifier) {}

NearbyShareClientFactoryImpl::~NearbyShareClientFactoryImpl() = default;

std::unique_ptr<NearbyShareClient>
NearbyShareClientFactoryImpl::CreateInstance() {
  return std::make_unique<NearbyShareClientImpl>(
      std::make_unique<ash::nearby::NearbyApiCallFlowImpl>(), identity_manager_,
      url_loader_factory_, notifier_);
}
