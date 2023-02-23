// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/family_info_fetcher.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_external_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

const int kNumFamilyInfoFetcherRetries = 1;

const char kIdFamily[] = "family";
const char kIdFamilyId[] = "familyId";
const char kIdProfile[] = "profile";
const char kIdFamilyName[] = "name";
const char kIdMembers[] = "members";
const char kIdUserId[] = "userId";
const char kIdRole[] = "role";
const char kIdDisplayName[] = "displayName";
const char kIdEmail[] = "email";
const char kIdProfileUrl[] = "profileUrl";
const char kIdProfileImageUrl[] = "profileImageUrl";
const char kIdDefaultProfileImageUrl[] = "defaultProfileImageUrl";

const char kListFamilyMembersRequestHistogramPrefix[] =
    "Signin.ListFamilyMembersRequest";
const char kListFamilyMembersRequestStatusHistogramName[] =
    "Signin.ListFamilyMembersRequest.Status";
const char kListFamilyMembersRequestNetOrHttpStatusHistogramName[] =
    "Signin.ListFamilyMembersRequest.NetOrHttpStatus";
const char kListFamilyMembersRequestLatencyHistogramName[] =
    "Signin.ListFamilyMembersRequest.Latency";

namespace {
std::string ToStatusKey(KidsExternalFetcherStatus::State status) {
  switch (status) {
    case KidsExternalFetcherStatus::NO_ERROR:
      return "NoError";
    case KidsExternalFetcherStatus::GOOGLE_SERVICE_AUTH_ERROR:
      return "AuthError";
    case KidsExternalFetcherStatus::NET_OR_HTTP_ERROR:
      return "HttpError";
    case KidsExternalFetcherStatus::INVALID_RESPONSE:
      return "ParseError";
    case KidsExternalFetcherStatus::DATA_ERROR:
      return "DataError";
  }
}

std::string LatencyPerStatusKey(KidsExternalFetcherStatus::State status) {
  return base::JoinString({kListFamilyMembersRequestHistogramPrefix,
                           ToStatusKey(status), "Latency"},
                          /*separator=*/".");
}

bool IgnoreUnsupportedApiCalls(const GURL& request_url) {
  // Ignore tracing all api calls except for family members.
  return request_url != supervised_user::KidsManagementGetFamilyMembersURL();
}

void RecordMetrics(KidsExternalFetcherStatus::State status,
                   base::TimeTicks start_time,
                   const GURL& request_url) {
  if (IgnoreUnsupportedApiCalls(request_url)) {
    return;
  }

  base::TimeDelta latency = base::TimeTicks::Now() - start_time;
  base::UmaHistogramEnumeration(kListFamilyMembersRequestStatusHistogramName,
                                status);
  base::UmaHistogramTimes(kListFamilyMembersRequestLatencyHistogramName,
                          latency);
  base::UmaHistogramTimes(LatencyPerStatusKey(status), latency);
}

void RecordMetricsForNetOrHttpErrorState(int net_or_http_error,
                                         base::TimeTicks start_time,
                                         const GURL& request_url) {
  RecordMetrics(KidsExternalFetcherStatus::State::NET_OR_HTTP_ERROR, start_time,
                request_url);
  if (IgnoreUnsupportedApiCalls(request_url)) {
    return;
  }
  base::UmaHistogramSparse(
      kListFamilyMembersRequestNetOrHttpStatusHistogramName, net_or_http_error);
}
}  // namespace

// These correspond to enum FamilyInfoFetcher::FamilyMemberRole, in order.
const char* const kFamilyMemberRoleStrings[] = {"headOfHousehold", "parent",
                                                "member", "child"};

FamilyInfoFetcher::FamilyProfile::FamilyProfile() = default;

FamilyInfoFetcher::FamilyProfile::FamilyProfile(const std::string& id,
                                                const std::string& name)
    : id(id), name(name) {}

FamilyInfoFetcher::FamilyProfile::~FamilyProfile() = default;

FamilyInfoFetcher::FamilyMember::FamilyMember() = default;

FamilyInfoFetcher::FamilyMember::FamilyMember(
    const std::string& obfuscated_gaia_id,
    FamilyMemberRole role,
    const std::string& display_name,
    const std::string& email,
    const std::string& profile_url,
    const std::string& profile_image_url)
    : obfuscated_gaia_id(obfuscated_gaia_id),
      role(role),
      display_name(display_name),
      email(email),
      profile_url(profile_url),
      profile_image_url(profile_image_url) {}

FamilyInfoFetcher::FamilyMember::FamilyMember(const FamilyMember& other) =
    default;

FamilyInfoFetcher::FamilyMember::~FamilyMember() = default;

FamilyInfoFetcher::FamilyInfoFetcher(
    Consumer* consumer,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : consumer_(consumer),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      access_token_expired_(false) {}

FamilyInfoFetcher::~FamilyInfoFetcher() {}

// static
std::string FamilyInfoFetcher::RoleToString(FamilyMemberRole role) {
  return kFamilyMemberRoleStrings[role];
}

// static
bool FamilyInfoFetcher::StringToRole(
    const std::string& str,
    FamilyInfoFetcher::FamilyMemberRole* role) {
  for (size_t i = 0; i < std::size(kFamilyMemberRoleStrings); i++) {
    if (str == kFamilyMemberRoleStrings[i]) {
      *role = FamilyMemberRole(i);
      return true;
    }
  }
  return false;
}

void FamilyInfoFetcher::StartGetFamilyProfile() {
  request_url_ = supervised_user::KidsManagementGetFamilyProfileURL();
  StartFetchingAccessToken();
}

void FamilyInfoFetcher::StartGetFamilyMembers() {
  request_url_ = supervised_user::KidsManagementGetFamilyMembersURL();
  StartFetchingAccessToken();
}

void FamilyInfoFetcher::StartFetchingAccessToken() {
  OAuth2AccessTokenManager::ScopeSet scopes{
      GaiaConstants::kKidFamilyReadonlyOAuth2Scope};
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "family_info_fetcher", identity_manager_, scopes,
          base::BindOnce(&FamilyInfoFetcher::OnAccessTokenFetchComplete,
                         base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

void FamilyInfoFetcher::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    DLOG(WARNING) << "Failed to get an access token: " << error.ToString();
    RecordMetrics(KidsExternalFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR,
                  simple_url_loader_start_time_, request_url_);
    consumer_->OnFailure(ErrorCode::kTokenError);
    return;
  }
  access_token_ = access_token_info.token;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("family_info", R"(
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

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(supervised_user::kAuthorizationHeaderFormat,
                         access_token_.c_str()));
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->SetRetryOptions(
      kNumFamilyInfoFetcherRetries,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  // TODO re-add data use measurement once SimpleURLLoader supports it
  // data_use_measurement::DataUseUserData::SUPERVISED_USER
  simple_url_loader_start_time_ = base::TimeTicks::Now();
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&FamilyInfoFetcher::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void FamilyInfoFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  std::string body;
  if (response_body) {
    body = std::move(*response_body);
  }
  OnSimpleLoaderCompleteInternal(simple_url_loader_->NetError(), response_code,
                                 body);
}

void FamilyInfoFetcher::OnSimpleLoaderCompleteInternal(
    int net_error,
    int response_code,
    const std::string& response_body) {
  if (response_code == net::HTTP_UNAUTHORIZED && !access_token_expired_) {
    DVLOG(1) << "Access token expired, retrying";
    access_token_expired_ = true;
    OAuth2AccessTokenManager::ScopeSet scopes;
    scopes.insert(GaiaConstants::kKidFamilyReadonlyOAuth2Scope);
    CoreAccountId primary_account_id =
        identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
    if (primary_account_id.empty()) {
      DLOG(WARNING) << "Primary account removed";
      RecordMetrics(KidsExternalFetcherStatus::State::GOOGLE_SERVICE_AUTH_ERROR,
                    simple_url_loader_start_time_, request_url_);
      consumer_->OnFailure(ErrorCode::kTokenError);
      return;
    }

    identity_manager_->RemoveAccessTokenFromCache(primary_account_id, scopes,
                                                  access_token_);
    StartFetchingAccessToken();
    return;
  }

  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "HTTP error " << response_code;
    RecordMetricsForNetOrHttpErrorState(
        response_code, simple_url_loader_start_time_, request_url_);
    consumer_->OnFailure(ErrorCode::kNetworkError);
    return;
  }

  if (net_error != net::OK) {
    DLOG(WARNING) << "NetError " << net_error;
    RecordMetricsForNetOrHttpErrorState(
        net_error, simple_url_loader_start_time_, request_url_);
    consumer_->OnFailure(ErrorCode::kNetworkError);
    return;
  }

  if (request_url_ == supervised_user::KidsManagementGetFamilyProfileURL()) {
    FamilyProfileFetched(response_body);
  } else if (request_url_ ==
             supervised_user::KidsManagementGetFamilyMembersURL()) {
    FamilyMembersFetched(response_body);
  } else {
    NOTREACHED();
  }
}

// static
bool FamilyInfoFetcher::ParseMembers(const base::Value::List& list,
                                     std::vector<FamilyMember>* members) {
  for (const auto& entry : list) {
    FamilyMember member;
    if (!entry.is_dict()) {
      return false;
    }
    const base::Value::Dict& dict = entry.GetDict();
    if (!ParseMember(dict, &member)) {
      return false;
    }
    members->push_back(member);
  }
  return true;
}

// static
bool FamilyInfoFetcher::ParseMember(const base::Value::Dict& dict,
                                    FamilyMember* member) {
  const std::string* obfuscated_gaia_id = dict.FindString(kIdUserId);
  if (!obfuscated_gaia_id) {
    return false;
  }
  member->obfuscated_gaia_id = *obfuscated_gaia_id;
  const std::string* role_str = dict.FindString(kIdRole);
  if (!role_str) {
    return false;
  }
  if (!StringToRole(*role_str, &member->role)) {
    return false;
  }
  const base::Value::Dict* profile_dict = dict.FindDict(kIdProfile);
  if (profile_dict) {
    ParseProfile(*profile_dict, member);
  }
  return true;
}

// static
void FamilyInfoFetcher::ParseProfile(const base::Value::Dict& dict,
                                     FamilyMember* member) {
  const std::string* display_name = dict.FindString(kIdDisplayName);
  if (display_name) {
    member->display_name = *display_name;
  }
  const std::string* email = dict.FindString(kIdEmail);
  if (email) {
    member->email = *email;
  }
  const std::string* profile_url = dict.FindString(kIdProfileUrl);
  if (profile_url) {
    member->profile_url = *profile_url;
  }
  const std::string* profile_image_url = dict.FindString(kIdProfileImageUrl);
  if (profile_image_url) {
    member->profile_image_url = *profile_image_url;
  }
  if (member->profile_image_url.empty()) {
    const std::string* def_profile_image_url =
        dict.FindString(kIdDefaultProfileImageUrl);
    if (def_profile_image_url) {
      member->profile_image_url = *def_profile_image_url;
    }
  }
}

void FamilyInfoFetcher::FamilyProfileFetched(const std::string& response) {
  absl::optional<base::Value> value = base::JSONReader::Read(response);
  if (!value || !value->is_dict()) {
    consumer_->OnFailure(ErrorCode::kServiceError);
    return;
  }
  const base::Value::Dict& dict = value->GetDict();
  const base::Value::Dict* family_dict = dict.FindDict(kIdFamily);
  if (!family_dict) {
    consumer_->OnFailure(ErrorCode::kServiceError);
    return;
  }
  FamilyProfile family;
  const std::string* id = family_dict->FindString(kIdFamilyId);
  if (!id) {
    consumer_->OnFailure(ErrorCode::kServiceError);
    return;
  }
  family.id = *id;
  const base::Value::Dict* profile_dict = family_dict->FindDict(kIdProfile);
  if (!profile_dict) {
    consumer_->OnFailure(ErrorCode::kServiceError);
    return;
  }
  const std::string* name = profile_dict->FindString(kIdFamilyName);
  if (!name) {
    consumer_->OnFailure(ErrorCode::kServiceError);
    return;
  }
  family.name = *name;
  consumer_->OnGetFamilyProfileSuccess(family);
}

void FamilyInfoFetcher::FamilyMembersFetched(const std::string& response) {
  absl::optional<base::Value> value = base::JSONReader::Read(response);
  if (!value || !value->is_dict()) {
    RecordMetrics(KidsExternalFetcherStatus::State::INVALID_RESPONSE,
                  simple_url_loader_start_time_, request_url_);
    consumer_->OnFailure(ErrorCode::kServiceError);
    return;
  }
  const base::Value::Dict& dict = value->GetDict();
  const base::Value::List* members_list = dict.FindList(kIdMembers);
  if (!members_list) {
    RecordMetrics(KidsExternalFetcherStatus::State::DATA_ERROR,
                  simple_url_loader_start_time_, request_url_);
    consumer_->OnFailure(ErrorCode::kServiceError);
    return;
  }
  std::vector<FamilyMember> members;
  if (!ParseMembers(*members_list, &members)) {
    RecordMetrics(KidsExternalFetcherStatus::State::DATA_ERROR,
                  simple_url_loader_start_time_, request_url_);
    consumer_->OnFailure(ErrorCode::kServiceError);
    return;
  }
  RecordMetrics(KidsExternalFetcherStatus::State::NO_ERROR,
                simple_url_loader_start_time_, request_url_);
  consumer_->OnGetFamilyMembersSuccess(members);
}
