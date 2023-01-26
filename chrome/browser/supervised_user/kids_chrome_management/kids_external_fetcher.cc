// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_external_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {
// Controls the retry count of the simple url loader.
const int kNumFamilyInfoFetcherRetries = 1;

using ::base::BindOnce;
using ::base::JoinString;
using ::base::StrCat;
using ::base::StringPiece;
using ::base::TimeDelta;
using ::base::TimeTicks;
using ::base::UmaHistogramEnumeration;
using ::base::UmaHistogramTimes;
using ::base::Unretained;
using ::kids_chrome_management::ListFamilyMembersRequest;
using ::kids_chrome_management::ListFamilyMembersResponse;
using ::network::ResourceRequest;
using ::signin::IdentityManager;

bool IsLoadingSuccessful(const network::SimpleURLLoader& loader) {
  return loader.NetError() == net::OK;
}

bool HasHttpOkResponse(const network::SimpleURLLoader& loader) {
  if (!loader.ResponseInfo()) {
    return false;
  }
  if (!loader.ResponseInfo()->headers) {
    return false;
  }
  return net::HttpStatusCode(loader.ResponseInfo()->headers->response_code()) ==
         net::HTTP_OK;
}

std::unique_ptr<network::SimpleURLLoader> InitializeSimpleUrlLoader(
    StringPiece payload,
    StringPiece access_token,
    const GURL& url,
    net::NetworkTrafficAnnotationTag traffic_annotation) {
  std::unique_ptr<ResourceRequest> resource_request =
      std::make_unique<ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(supervised_users::kAuthorizationHeaderFormat,
                         access_token));
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  simple_url_loader->SetRetryOptions(
      kNumFamilyInfoFetcherRetries,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  return simple_url_loader;
}

template <typename Request>
std::string ConvertRequestTypeToMetricLabel();
template <>
std::string ConvertRequestTypeToMetricLabel<ListFamilyMembersRequest>() {
  return "ListFamilyMembersRequest";
}

// Metric key
template <typename Request>
std::string CreateMetricKey(StringPiece metric_id) {
  return JoinString(
      {"Signin", ConvertRequestTypeToMetricLabel<Request>(), metric_id}, ".");
}
template <typename Request>
std::string CreateMetricKey(StringPiece metric_id, StringPiece metric_suffix) {
  return JoinString({"Signin", ConvertRequestTypeToMetricLabel<Request>(),
                     metric_id, metric_suffix},
                    ".");
}

// The returned value must match one of the labels in
// chromium/src/tools/metrics/histograms/enums.xml/histogram-configuration/enums/enum[@name='KidsExternalFetcherStatus'],
// and should be reflected in tokens in
// chromium/src/tools/metrics/histograms/metadata/signin/histograms.xml/histogram-configuration/histograms/histogram[@name='Signin.ListFamilyMembersRequest.{Status}.*']
std::string ConvertStateToMetricLabel(KidsExternalFetcherStatus::State state) {
  switch (state) {
    case KidsExternalFetcherStatus::NO_ERROR:
      return "NoError";
    case KidsExternalFetcherStatus::GOOGLE_SERVICE_AUTH_ERROR:
      return "AuthError";
    case KidsExternalFetcherStatus::HTTP_ERROR:
      return "HttpError";
    case KidsExternalFetcherStatus::INVALID_RESPONSE:
      return "ParseError";
    case KidsExternalFetcherStatus::DATA_ERROR:
      return "DataError";
  }
}

// Determines the response type. See go/system-parameters to verity list of
// possible One Platform system params.
const std::string& GetSystemParameters() {
  static const base::NoDestructor<std::string> nonce("alt=proto");
  return *nonce;
}

// Controls what endpoint to call for given Request.
template <typename Request>
const std::string& GetPathForRequest(const Request& request);

template <>
const std::string& GetPathForRequest(const ListFamilyMembersRequest& request) {
  static const base::NoDestructor<std::string> nonce(
      StrCat({"families/mine/members?", GetSystemParameters()}));
  return *nonce;
}

template <typename Request>
net::NetworkTrafficAnnotationTag GetDefaultNetworkTrafficAnnotationTag();

template <>
net::NetworkTrafficAnnotationTag
GetDefaultNetworkTrafficAnnotationTag<ListFamilyMembersRequest>() {
  return net::DefineNetworkTrafficAnnotation(
      "kids_chrome_management_list_family_members", R"(
        semantics {
          sender: "Supervised Users"
          description:
            "Fetches information about the user's family group from the Google "
            "Family API."
          trigger:
            "Triggered in regular intervals to update profile information."
          data:
            "The request is authenticated with an OAuth2 access token "
            "identifying the Google account. No other information is sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings and is only enabled "
            "for child accounts. If sign-in is restricted to accounts from a "
            "managed domain, those accounts are not going to be child accounts."
          chrome_policy {
            RestrictSigninToPattern {
              policy_options {mode: MANDATORY}
              RestrictSigninToPattern: "*@manageddomain.com"
            }
          }
        })");
}

// A fetcher with underlying network::SharedURLLoaderFactory.
template <typename Request, typename Response>
class FetcherImpl final : public KidsExternalFetcher<Request, Response> {
 private:
  using Callback = typename KidsExternalFetcher<Request, Response>::Callback;

 public:
  FetcherImpl() = delete;
  explicit FetcherImpl(
      IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      StringPiece url,
      Request request,
      Callback callback) {
    access_token_fetcher_ = std::make_unique<KidsAccessTokenFetcher>(
        identity_manager,
        BindOnce(&FetcherImpl::StartRequest, Unretained(this),
                 url_loader_factory, GURL(url), request,
                 std::move(callback)));  // Unretained(.) is safe because `this`
                                         // owns `access_token_fetcher_`.
  }

  // Not copyable
  FetcherImpl(const FetcherImpl&) = delete;
  FetcherImpl& operator=(const FetcherImpl&) = delete;

 private:
  static void WrapCallbackWithMetrics(Callback callback,
                                      TimeTicks start_time,
                                      KidsExternalFetcherStatus status,
                                      std::unique_ptr<Response> response) {
    TimeDelta latency = TimeTicks::Now() - start_time;
    UmaHistogramEnumeration(CreateMetricKey<Request>("Status"), status.state());
    UmaHistogramTimes(CreateMetricKey<Request>("Latency"), latency);
    UmaHistogramTimes(CreateMetricKey<Request>(
                          "Latency", ConvertStateToMetricLabel(status.state())),
                      latency);

    DCHECK(
        callback);  // https://chromium.googlesource.com/chromium/src/+/main/docs/callback.md#creating-a-callback-that-does-nothing
    std::move(callback).Run(status, std::move(response));
  }

  void StartRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL endpoint,
      Request request,
      Callback callback,
      base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>
          access_token) {
    DCHECK(
        callback);  // https://chromium.googlesource.com/chromium/src/+/main/docs/callback.md#creating-a-callback-that-does-nothing

    Callback callback_with_metrics = BindOnce(
        WrapCallbackWithMetrics, std::move(callback), TimeTicks::Now());

    if (!access_token.has_value()) {
      std::move(callback_with_metrics)
          .Run(KidsExternalFetcherStatus::GoogleServiceAuthError(
                   access_token.error()),
               std::make_unique<Response>());
      return;
    }

    StringPiece token_value = access_token.value().token;
    net::NetworkTrafficAnnotationTag traffic_annotation =
        GetDefaultNetworkTrafficAnnotationTag<Request>();

    simple_url_loader_ = InitializeSimpleUrlLoader(
        request.SerializeAsString(), token_value,
        endpoint.Resolve(GetPathForRequest(request)), traffic_annotation);

    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(),
        BindOnce(
            &FetcherImpl::OnSimpleUrlLoaderComplete, Unretained(this),
            std::move(
                callback_with_metrics)));  // Unretained(.) is safe because
                                           // `this` owns `simple_url_loader_`.
  }

  void OnSimpleUrlLoaderComplete(Callback callback,
                                 std::unique_ptr<std::string> response_body) {
    if (!IsLoadingSuccessful(*simple_url_loader_) ||
        !HasHttpOkResponse(*simple_url_loader_)) {
      std::move(callback).Run(KidsExternalFetcherStatus::HttpError(),
                              std::make_unique<Response>());
      return;
    }

    std::unique_ptr<Response> response = std::make_unique<Response>();
    if (!response->ParseFromString(*response_body)) {
      std::move(callback).Run(KidsExternalFetcherStatus::InvalidResponse(),
                              std::move(response));
      return;
    }

    std::move(callback).Run(std::move(KidsExternalFetcherStatus::Ok()),
                            std::move(response));
  }

  std::unique_ptr<KidsAccessTokenFetcher> access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

template class FetcherImpl<ListFamilyMembersRequest, ListFamilyMembersResponse>;

ListFamilyMembersRequest CreateListFamilyMembersRequest() {
  ListFamilyMembersRequest request;
  request.set_family_id("mine");  // Required by the contract of the protocol,
                                  // see proto definition.
  return request;
}
}  // namespace

// Main constructor, referenced by the rest.
KidsExternalFetcherStatus::KidsExternalFetcherStatus(
    State state,
    class GoogleServiceAuthError google_service_auth_error)
    : state_(state), google_service_auth_error_(google_service_auth_error) {}
KidsExternalFetcherStatus::~KidsExternalFetcherStatus() = default;

KidsExternalFetcherStatus::KidsExternalFetcherStatus(State state)
    : state_(state) {
  DCHECK(state != State::GOOGLE_SERVICE_AUTH_ERROR);
}
KidsExternalFetcherStatus::KidsExternalFetcherStatus(
    class GoogleServiceAuthError google_service_auth_error)
    : KidsExternalFetcherStatus(GOOGLE_SERVICE_AUTH_ERROR,
                                google_service_auth_error) {}

KidsExternalFetcherStatus::KidsExternalFetcherStatus(
    const KidsExternalFetcherStatus& other) = default;
KidsExternalFetcherStatus& KidsExternalFetcherStatus::operator=(
    const KidsExternalFetcherStatus& other) = default;

KidsExternalFetcherStatus KidsExternalFetcherStatus::Ok() {
  return KidsExternalFetcherStatus(State::NO_ERROR);
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::GoogleServiceAuthError(
    class GoogleServiceAuthError error) {
  return KidsExternalFetcherStatus(error);
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::HttpError() {
  return KidsExternalFetcherStatus(State::HTTP_ERROR);
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::InvalidResponse() {
  return KidsExternalFetcherStatus(State::INVALID_RESPONSE);
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::DataError() {
  return KidsExternalFetcherStatus(State::DATA_ERROR);
}

bool KidsExternalFetcherStatus::IsOk() const {
  return state_ == State::NO_ERROR;
}
bool KidsExternalFetcherStatus::IsTransientError() const {
  if (state_ == State::HTTP_ERROR) {
    return true;
  }
  if (state_ == State::GOOGLE_SERVICE_AUTH_ERROR) {
    return google_service_auth_error_.IsTransientError();
  }
  return false;
}
bool KidsExternalFetcherStatus::IsPersistentError() const {
  if (state_ == State::INVALID_RESPONSE) {
    return true;
  }
  if (state_ == State::DATA_ERROR) {
    return true;
  }
  if (state_ == State::GOOGLE_SERVICE_AUTH_ERROR) {
    return google_service_auth_error_.IsPersistentError();
  }
  return false;
}

KidsExternalFetcherStatus::State KidsExternalFetcherStatus::state() const {
  return state_;
}
const GoogleServiceAuthError&
KidsExternalFetcherStatus::google_service_auth_error() const {
  return google_service_auth_error_;
}

std::unique_ptr<
    KidsExternalFetcher<ListFamilyMembersRequest, ListFamilyMembersResponse>>
FetchListFamilyMembers(
    IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    StringPiece url,
    KidsExternalFetcher<ListFamilyMembersRequest,
                        ListFamilyMembersResponse>::Callback callback) {
  return std::make_unique<
      FetcherImpl<ListFamilyMembersRequest, ListFamilyMembersResponse>>(
      identity_manager, url_loader_factory, url,
      CreateListFamilyMembersRequest(), std::move(callback));
}
