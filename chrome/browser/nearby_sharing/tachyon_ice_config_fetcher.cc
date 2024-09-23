// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/tachyon_ice_config_fetcher.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"
#include "chrome/browser/nearby_sharing/proto/duration.pb.h"
#include "chrome/browser/nearby_sharing/proto/ice.pb.h"
#include "chrome/browser/nearby_sharing/proto/tachyon.pb.h"
#include "chrome/browser/nearby_sharing/proto/tachyon_common.pb.h"
#include "chrome/browser/nearby_sharing/proto/tachyon_enums.pb.h"
#include "chrome/services/sharing/public/cpp/sharing_webrtc_metrics.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "components/cross_device/logging/logging.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "url/gurl.h"

namespace {

namespace tachyon_proto = nearbyshare::tachyon_proto;

const char kIceConfigApiUrl[] =
    "https://instantmessaging-pa.googleapis.com/v1/peertopeer:geticeserver";

const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";

// Timeout for network calls to Tachyon servers.
constexpr base::TimeDelta kNetworkTimeout = base::Seconds(4);

// Response with 2 ice server configs takes ~1KB. A loose upper bound of 16KB is
// chosen to avoid breaking the flow in case the response has longer URLs in ice
// configs.
constexpr int kMaxBodySize = 16 * 1024;

const char kAppName[] = "Nearby";
const tachyon_proto::IdType::Type kTachyonIdType =
    tachyon_proto::IdType::NEARBY_ID;
constexpr int kMajorVersion = 1;
constexpr int kMinorVersion = 24;
constexpr int kPointVersion = 0;

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("tachyon_ice_config_fetcher", R"(
        semantics {
          sender: "TachyonIceConfigFetcher"
          description:
            "Fetches ice server configurations for p2p webrtc connection as "
            "described in "
            "https://www.w3.org/TR/webrtc/#rtciceserver-dictionary."
          trigger:
            "User uses any Chrome cross-device sharing feature and selects one"
            " of their devices to send the data to."
          data: "No data is sent in the request."
          destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can disable this behavior by signing out of Chrome."
            chrome_policy {
              BrowserSignin {
                policy_options {mode: MANDATORY}
                BrowserSignin: 0
              }
            }
          })");

// Returns the ISO country code for the locale currently set as the
// user's device language.
const std::string GetCurrentCountryCode() {
  return icu::Locale::getDefault().getCountry();
}

void BuildLocationHint(tachyon_proto::LocationHint* location_hint,
                       const std::string& location,
                       tachyon_proto::LocationStandard_Format format) {
  location_hint->set_location(location);
  location_hint->set_format(format);
}

void BuildId(tachyon_proto::Id* req_id, const std::string& id) {
  DCHECK(req_id);
  req_id->set_id(id);
  req_id->set_app(kAppName);
  req_id->set_type(kTachyonIdType);
  BuildLocationHint(req_id->mutable_location_hint(), GetCurrentCountryCode(),
                    tachyon_proto::LocationStandard_Format::
                        LocationStandard_Format_ISO_3166_1_ALPHA_2);
}

void BuildHeader(tachyon_proto::RequestHeader* header) {
  DCHECK(header);
  header->set_request_id(base::UnguessableToken::Create().ToString());
  header->set_app(kAppName);
  BuildId(header->mutable_requester_id(), std::string());
  tachyon_proto::ClientInfo* info = header->mutable_client_info();
  info->set_api_version(tachyon_proto::ApiVersion::V4);
  info->set_platform_type(tachyon_proto::Platform::DESKTOP);
  info->set_major(kMajorVersion);
  info->set_minor(kMinorVersion);
  info->set_point(kPointVersion);
}

tachyon_proto::GetICEServerRequest BuildRequest() {
  tachyon_proto::GetICEServerRequest request;
  BuildHeader(request.mutable_header());
  return request;
}

void RecordResultMetric(const ash::nearby::NearbyHttpStatus& http_status) {
  bool success = http_status.IsSuccess();
  base::UmaHistogramBoolean(
      "Nearby.Connections.InstantMessaging.TachyonIceConfigFetcher.Result",
      success);
  if (!success) {
    base::UmaHistogramSparse(
        "Nearby.Connections.InstantMessaging.TachyonIceConfigFetcher."
        "FailureReason",
        http_status.GetResultCodeForMetrics());
  }
}

void RecordCacheHitMetric(bool cache_hit) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.InstantMessaging.TachyonIceConfigFetcher.CacheHit",
      cache_hit);
}

void RecordTokenFetchSuccessMetric(bool token_fetch_successful) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.InstantMessaging.TachyonIceConfigFetcher."
      "OAuthTokenFetchResult",
      token_fetch_successful);
}

bool IsLoaderSuccessful(const network::SimpleURLLoader* loader,
                        const std::string& request_id) {
  DCHECK(loader);
  ash::nearby::NearbyHttpStatus status =
      ash::nearby::NearbyHttpStatus(loader->NetError(), loader->ResponseInfo());

  RecordResultMetric(status);

  if (!status.IsSuccess()) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << "TachyonIceConfigFetcher (request_id=" << request_id << ") "
        << status << " " << status.GetResultCodeForMetrics();
    return false;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << "TachyonIceConfigFetcher (request_id=" << request_id
      << ") GetIceServers succeeded";
  return true;
}

std::vector<::sharing::mojom::IceServerPtr> GetDefaultIceServers() {
  ::sharing::mojom::IceServerPtr ice_server(::sharing::mojom::IceServer::New());
  ice_server->urls.emplace_back("stun:stun.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun1.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun2.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun3.l.google.com:19302");
  ice_server->urls.emplace_back("stun:stun4.l.google.com:19302");

  std::vector<::sharing::mojom::IceServerPtr> default_servers;
  default_servers.push_back(std::move(ice_server));
  return default_servers;
}

std::vector<::sharing::mojom::IceServerPtr> CloneIceServerList(
    const std::vector<::sharing::mojom::IceServerPtr>& server_list) {
  // Cannot use vector's default copy operation because IceServerPtr is move
  // only and has to be cloned.
  std::vector<::sharing::mojom::IceServerPtr> new_list;
  for (const auto& server : server_list) {
    new_list.push_back(server.Clone());
  }
  return new_list;
}

void OnOAuthTokenFetched(
    std::unique_ptr<TokenFetcher> token_fetcher,
    base::OnceCallback<void(const std::string& token)> callback,
    const std::string& token) {
  // It is safe to reset the token fetcher now.
  token_fetcher.reset();
  // Note: We do not do anything special for empty tokens.
  RecordTokenFetchSuccessMetric(/*token_fetch_successful=*/!token.empty());
  std::move(callback).Run(token);
}

void GetAccessToken(
    signin::IdentityManager* identity_manager,
    base::OnceCallback<void(const std::string& token)> callback) {
  std::unique_ptr<TokenFetcher> token_fetcher =
      std::make_unique<TokenFetcher>(identity_manager);
  TokenFetcher* token_fetcher_ptr = token_fetcher.get();

  // Pass the token fetcher in the closure so that its lifetime is the same as
  // the request. The access token is cached by the identity manager, so no
  // caching is necessary here.
  token_fetcher_ptr->GetAccessToken(base::BindOnce(
      &OnOAuthTokenFetched, std::move(token_fetcher), std::move(callback)));
}

}  // namespace

TachyonIceConfigFetcher::TachyonIceConfigFetcher(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {}

TachyonIceConfigFetcher::~TachyonIceConfigFetcher() = default;

void TachyonIceConfigFetcher::GetIceServers(GetIceServersCallback callback) {
  // If a previous request cached the ICE servers and the expiration time hasn't
  // lapsed, return a copy of the cached servers immediately.
  if (ice_server_cache_ && ice_server_cache_expiration_ >= base::Time::Now()) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << "TachyonIceConfigFetcher returning cached ice servers";
    std::move(callback).Run(CloneIceServerList(*ice_server_cache_));
    RecordCacheHitMetric(/*cache_hit=*/true);
    return;
  }

  GetAccessToken(
      identity_manager_,
      base::BindOnce(&TachyonIceConfigFetcher::GetIceServersWithToken,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  RecordCacheHitMetric(/*cache_hit=*/false);
}

void TachyonIceConfigFetcher::GetIceServersWithToken(
    GetIceServersCallback callback,
    const std::string& token) {
  if (token.empty()) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << "TachyonIceConfigFetcher failed to fetch OAuth access token, "
           "returning default ICE servers";
    std::move(callback).Run(GetDefaultIceServers());
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kIceConfigApiUrl);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.AddHeaderFromString(
      base::StringPrintf(kAuthorizationHeaderFormat, token.c_str()));

  tachyon_proto::GetICEServerRequest request = BuildRequest();
  const std::string& request_id = request.header().request_id();

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  auto* url_loader_ptr = url_loader.get();
  url_loader->SetTimeoutDuration(kNetworkTimeout);
  url_loader->AttachStringForUpload(request.SerializeAsString(),
                                    "application/x-protobuf");

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__
      << ": Requesting ICE Servers from Tachyon (request_id=" << request_id
      << ")";
  url_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&TachyonIceConfigFetcher::OnIceServersResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     request_id, std::move(url_loader)),
      kMaxBodySize);
}

void TachyonIceConfigFetcher::OnIceServersResponse(
    ::sharing::mojom::IceConfigFetcher::GetIceServersCallback callback,
    const std::string& request_id,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response_body) {
  std::vector<::sharing::mojom::IceServerPtr> ice_servers;

  if (IsLoaderSuccessful(url_loader.get(), request_id) && response_body)
    ice_servers = ParseIceServersResponse(*response_body, request_id);

  sharing::LogWebRtcIceConfigFetched(ice_servers.size());

  if (ice_servers.empty()) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << "TachyonIceConfigFetcher (request_id=" << request_id
        << ") empty response, returning default ICE servers";
    ice_servers = GetDefaultIceServers();
  }

  std::move(callback).Run(std::move(ice_servers));
}

std::vector<::sharing::mojom::IceServerPtr>
TachyonIceConfigFetcher::ParseIceServersResponse(
    const std::string& serialized_proto,
    const std::string& request_id) {
  std::vector<::sharing::mojom::IceServerPtr> servers_mojo;
  tachyon_proto::GetICEServerResponse response;
  if (!response.ParseFromString(serialized_proto)) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__ << ": (request_id=" << request_id
        << ") Failed to parse response";
    return servers_mojo;
  }

  const tachyon_proto::ICEConfiguration& ice_config = response.ice_config();

  for (const tachyon_proto::ICEServerList& server : ice_config.ice_servers()) {
    if (!server.urls_size())
      continue;

    ::sharing::mojom::IceServerPtr server_mojo(
        ::sharing::mojom::IceServer::New());
    for (const std::string& url : server.urls()) {
      server_mojo->urls.emplace_back(url);
    }

    if (!server.username().empty())
      server_mojo->username.emplace(server.username());

    if (!server.credential().empty())
      server_mojo->credential.emplace(server.credential());

    servers_mojo.push_back(std::move(server_mojo));
  }

  if (ice_config.has_lifetime_duration()) {
    ice_server_cache_ = CloneIceServerList(servers_mojo);
    ice_server_cache_expiration_ =
        base::Time::Now() +
        base::Seconds(ice_config.lifetime_duration().seconds());
  }
  return servers_mojo;
}
