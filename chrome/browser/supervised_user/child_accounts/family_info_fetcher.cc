// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/family_info_fetcher.h"

#include <stddef.h>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/child_accounts/kids_management_api.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

const char kGetFamilyProfileApiPath[] = "families/mine?alt=json";
const char kGetFamilyMembersApiPath[] = "families/mine/members?alt=json";
const char kScope[] = "https://www.googleapis.com/auth/kid.family.readonly";
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

// These correspond to enum FamilyInfoFetcher::FamilyMemberRole, in order.
const char* const kFamilyMemberRoleStrings[] = {"headOfHousehold", "parent",
                                                "member", "child"};

FamilyInfoFetcher::FamilyProfile::FamilyProfile() {
}

FamilyInfoFetcher::FamilyProfile::FamilyProfile(const std::string& id,
                                                const std::string& name)
    : id(id), name(name) {
}

FamilyInfoFetcher::FamilyProfile::~FamilyProfile() {
}

FamilyInfoFetcher::FamilyMember::FamilyMember() {
}

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
      profile_image_url(profile_image_url) {
}

FamilyInfoFetcher::FamilyMember::FamilyMember(const FamilyMember& other) =
    default;

FamilyInfoFetcher::FamilyMember::~FamilyMember() {
}

FamilyInfoFetcher::FamilyInfoFetcher(
    Consumer* consumer,
    const std::string& account_id,
    OAuth2TokenService* token_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : OAuth2TokenService::Consumer("family_info_fetcher"),
      consumer_(consumer),
      account_id_(account_id),
      token_service_(token_service),
      url_loader_factory_(std::move(url_loader_factory)),
      access_token_expired_(false) {}

FamilyInfoFetcher::~FamilyInfoFetcher() {
  // Ensures O2TS observation is cleared when FamilyInfoFetcher is destructed
  // before refresh token is available.
  token_service_->RemoveObserver(this);
}

// static
std::string FamilyInfoFetcher::RoleToString(FamilyMemberRole role) {
  return kFamilyMemberRoleStrings[role];
}

// static
bool FamilyInfoFetcher::StringToRole(
    const std::string& str,
    FamilyInfoFetcher::FamilyMemberRole* role) {
  for (size_t i = 0; i < arraysize(kFamilyMemberRoleStrings); i++) {
    if (str == kFamilyMemberRoleStrings[i]) {
      *role = FamilyMemberRole(i);
      return true;
    }
  }
  return false;
}

void FamilyInfoFetcher::StartGetFamilyProfile() {
  request_path_ = kGetFamilyProfileApiPath;
  StartFetching();
}

void FamilyInfoFetcher::StartGetFamilyMembers() {
  request_path_ = kGetFamilyMembersApiPath;
  StartFetching();
}

void FamilyInfoFetcher::StartFetching() {
  if (token_service_->RefreshTokenIsAvailable(account_id_)) {
    StartFetchingAccessToken();
  } else {
    // Wait until we get a refresh token.
    token_service_->AddObserver(this);
  }
}

void FamilyInfoFetcher::StartFetchingAccessToken() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kScope);
  access_token_request_ =
      token_service_->StartRequest(account_id_, scopes, this);
}

void FamilyInfoFetcher::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // Wait until we get a refresh token for the requested account.
  if (account_id != account_id_)
    return;

  token_service_->RemoveObserver(this);

  StartFetchingAccessToken();
}

void FamilyInfoFetcher::OnRefreshTokensLoaded() {
  token_service_->RemoveObserver(this);

  // The PO2TS has loaded all tokens, but we didn't get one for the account we
  // want. We probably won't get one any time soon, so report an error.
  DLOG(WARNING) << "Did not get a refresh token for account " << account_id_;
  consumer_->OnFailure(TOKEN_ERROR);
}

void FamilyInfoFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK_EQ(access_token_request_.get(), request);
  access_token_ = token_response.access_token;

  GURL url = kids_management_api::GetURL(request_path_);

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
  resource_request->url = url;
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(supervised_users::kAuthorizationHeaderFormat,
                         access_token_.c_str()));
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->SetRetryOptions(
      kNumFamilyInfoFetcherRetries,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  // TODO re-add data use measurement once SimpleURLLoader supports it
  // data_use_measurement::DataUseUserData::SUPERVISED_USER
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&FamilyInfoFetcher::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void FamilyInfoFetcher::OnGetTokenFailure(
  const OAuth2TokenService::Request* request,
  const GoogleServiceAuthError& error) {
  DCHECK_EQ(access_token_request_.get(), request);
  DLOG(WARNING) << "Failed to get an access token: " << error.ToString();
  consumer_->OnFailure(TOKEN_ERROR);
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
  if (response_body)
    body = std::move(*response_body);
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
    OAuth2TokenService::ScopeSet scopes;
    scopes.insert(kScope);
    token_service_->InvalidateAccessToken(account_id_, scopes, access_token_);
    StartFetching();
    return;
  }

  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "HTTP error " << response_code;
    consumer_->OnFailure(NETWORK_ERROR);
    return;
  }

  if (net_error != net::OK) {
    DLOG(WARNING) << "NetError " << net_error;
    consumer_->OnFailure(NETWORK_ERROR);
    return;
  }

  if (request_path_ == kGetFamilyProfileApiPath) {
    FamilyProfileFetched(response_body);
  } else if (request_path_ == kGetFamilyMembersApiPath) {
    FamilyMembersFetched(response_body);
  } else {
    NOTREACHED();
  }
}

// static
bool FamilyInfoFetcher::ParseMembers(const base::ListValue* list,
                                     std::vector<FamilyMember>* members) {
  for (auto it = list->begin(); it != list->end(); ++it) {
    FamilyMember member;
    const base::DictionaryValue* dict = NULL;
    if (!it->GetAsDictionary(&dict) || !ParseMember(dict, &member)) {
      return false;
    }
    members->push_back(member);
  }
  return true;
}

// static
bool FamilyInfoFetcher::ParseMember(const base::DictionaryValue* dict,
                                    FamilyMember* member) {
  if (!dict->GetString(kIdUserId, &member->obfuscated_gaia_id))
    return false;
  std::string role_str;
  if (!dict->GetString(kIdRole, &role_str))
    return false;
  if (!StringToRole(role_str, &member->role))
    return false;
  const base::DictionaryValue* profile_dict = NULL;
  if (dict->GetDictionary(kIdProfile, &profile_dict))
    ParseProfile(profile_dict, member);
  return true;
}

// static
void FamilyInfoFetcher::ParseProfile(const base::DictionaryValue* dict,
                                     FamilyMember* member) {
  dict->GetString(kIdDisplayName, &member->display_name);
  dict->GetString(kIdEmail, &member->email);
  dict->GetString(kIdProfileUrl, &member->profile_url);
  dict->GetString(kIdProfileImageUrl, &member->profile_image_url);
  if (member->profile_image_url.empty())
    dict->GetString(kIdDefaultProfileImageUrl, &member->profile_image_url);
}

void FamilyInfoFetcher::FamilyProfileFetched(const std::string& response) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(response);
  const base::DictionaryValue* dict = NULL;
  if (!value || !value->GetAsDictionary(&dict)) {
    consumer_->OnFailure(SERVICE_ERROR);
    return;
  }
  const base::DictionaryValue* family_dict = NULL;
  if (!dict->GetDictionary(kIdFamily, &family_dict)) {
    consumer_->OnFailure(SERVICE_ERROR);
    return;
  }
  FamilyProfile family;
  if (!family_dict->GetStringWithoutPathExpansion(kIdFamilyId, &family.id)) {
    consumer_->OnFailure(SERVICE_ERROR);
    return;
  }
  const base::DictionaryValue* profile_dict = NULL;
  if (!family_dict->GetDictionary(kIdProfile, &profile_dict)) {
    consumer_->OnFailure(SERVICE_ERROR);
    return;
  }
  if (!profile_dict->GetStringWithoutPathExpansion(kIdFamilyName,
                                                   &family.name)) {
    consumer_->OnFailure(SERVICE_ERROR);
    return;
  }
  consumer_->OnGetFamilyProfileSuccess(family);
}

void FamilyInfoFetcher::FamilyMembersFetched(const std::string& response) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(response);
  const base::DictionaryValue* dict = NULL;
  if (!value || !value->GetAsDictionary(&dict)) {
    consumer_->OnFailure(SERVICE_ERROR);
    return;
  }
  const base::ListValue* members_list = NULL;
  if (!dict->GetList(kIdMembers, &members_list)) {
    consumer_->OnFailure(SERVICE_ERROR);
    return;
  }
  std::vector<FamilyMember> members;
  if (!ParseMembers(members_list, &members)){
    consumer_->OnFailure(SERVICE_ERROR);
    return;
  }
  consumer_->OnGetFamilyMembersSuccess(members);
}
