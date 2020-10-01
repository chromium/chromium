// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/secondary_account_consent_logger.h"

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/child_accounts/kids_management_api.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

constexpr char kConsentApiPath[] =
    "people/me/consentsForSecondaryAccounts:create";

// Version of the parental consent text. Must be updated when consent text is
// changed. Format of the text version is "v<GERRIT_CL_NUMBER>" with number of
// the last CL where strings with information for parents were changed.
constexpr char kConsentScreenTextVersion[] = "v2353089";
// The text version which requires invalidation of the secondary accounts added
// before consent text changes. Format of the invalidation version is
// "iv<GERRIT_CL_NUMBER>".
// =============================================================================
// WARNING: change of the current version will result in invalidation of
// secondary accounts added with previous versions.
// =============================================================================
constexpr char kSecondaryAccountsInvalidationVersion[] = "iv2353089";

constexpr int kNumConsentLogRetries = 1;
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("secondary_account_consent_logger",
                                        R"(
        semantics {
          sender: "Consent logger for EDU account login flow"
          description:
            "Logs parent consent for addition of secondary accounts"
          trigger:
            "Triggered when child user adds account via EDU account addition"
            "flow."
          data:
            "The request is authenticated with an OAuth2 access token "
            "identifying the Google account. Secondary account email, parent"
            "RAPT and obfuscated gaia id of the parent are sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        })");

std::string GetOrCreateEduCoexistenceId(PrefService* pref_service) {
  std::string id = pref_service->GetString(chromeos::prefs::kEduCoexistenceId);
  if (id.empty()) {
    id = base::GenerateGUID();
    pref_service->SetString(chromeos::prefs::kEduCoexistenceId, id);
  }
  return id;
}

}  // namespace

// static
void SecondaryAccountConsentLogger::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(chromeos::prefs::kEduCoexistenceId,
                               std::string() /* default_value */);
  registry->RegisterStringPref(
      chromeos::prefs::kEduCoexistenceSecondaryAccountsInvalidationVersion,
      "iv2153049" /* default_value, the first invalidation version */);

  // |kEduCoexistenceToSVersion| is derived from Google3 cl that introduced new
  // ToS version. We use string here for the ToS version to be more future
  // proof. In the future we might add a prefix to indicate the flow where the
  // ToS were accepted (OOBE or Settings flow).
  registry->RegisterStringPref(chromeos::prefs::kEduCoexistenceToSVersion,
                               std::string());
}

// static
std::string
SecondaryAccountConsentLogger::GetSecondaryAccountsInvalidationVersion() {
  return kSecondaryAccountsInvalidationVersion;
}

SecondaryAccountConsentLogger::SecondaryAccountConsentLogger(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    const std::string& secondary_account_email,
    const std::string& parent_obfuscated_gaia_id,
    const std::string& re_auth_proof_token,
    base::OnceCallback<void(Result)> callback)
    : primary_account_id_(identity_manager->GetPrimaryAccountId(
          signin::ConsentLevel::kNotRequired)),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      pref_service_(pref_service),
      secondary_account_email_(secondary_account_email),
      parent_obfuscated_gaia_id_(parent_obfuscated_gaia_id),
      re_auth_proof_token_(re_auth_proof_token),
      callback_(std::move(callback)) {}

SecondaryAccountConsentLogger::~SecondaryAccountConsentLogger() = default;

void SecondaryAccountConsentLogger::StartLogging() {
  OAuth2AccessTokenManager::ScopeSet scopes{
      GaiaConstants::kKidManagementPrivilegedOAuth2Scope};
  DCHECK(!access_token_fetcher_);
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "secondary_account_consent_logger", identity_manager_, scopes,
          base::BindOnce(
              &SecondaryAccountConsentLogger::OnAccessTokenFetchComplete,
              base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::
              kWaitUntilAvailable /*mode*/,
          signin::ConsentLevel::kNotRequired /*consent*/);
}

void SecondaryAccountConsentLogger::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    DLOG(WARNING) << "Failed to get an access token: " << error.ToString();
    std::move(callback_).Run(Result::kTokenError);
    return;
  }
  access_token_ = access_token_info.token;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = kids_management_api::GetURL(kConsentApiPath);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(supervised_users::kAuthorizationHeaderFormat,
                         access_token_.c_str()));

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  std::string upload_data;
  base::JSONWriter::Write(CreateRequestBody(), &upload_data);
  simple_url_loader_->AttachStringForUpload(upload_data,
                                            "application/json; charset=UTF-8");
  simple_url_loader_->SetRetryOptions(
      kNumConsentLogRetries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&SecondaryAccountConsentLogger::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

base::DictionaryValue SecondaryAccountConsentLogger::CreateRequestBody() const {
  // See google3/google/internal/kidsmanagement/v1/kidsmanagement_messages.proto
  // > CreateConsentForSecondaryAccountRequest
  base::DictionaryValue request_body;
  request_body.SetStringKey("person_id", "me");
  base::DictionaryValue consent;
  consent.SetStringKey("chrome_os_unicorn_edu_coexistence_id",
                       GetOrCreateEduCoexistenceId(pref_service_));
  consent.SetStringKey("secondary_account_email", secondary_account_email_);
  consent.SetStringKey("parent_id", parent_obfuscated_gaia_id_);
  consent.SetStringKey("parent_rapt", re_auth_proof_token_);
  consent.SetStringKey("text_version", kConsentScreenTextVersion);
  request_body.SetKey("chrome_os_consent", std::move(consent));
  return request_body;
}

void SecondaryAccountConsentLogger::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  OnSimpleLoaderCompleteInternal(simple_url_loader_->NetError(), response_code);
}

void SecondaryAccountConsentLogger::OnSimpleLoaderCompleteInternal(
    int net_error,
    int response_code) {
  if (response_code == net::HTTP_UNAUTHORIZED && !access_token_expired_) {
    DVLOG(1) << "Access token expired, retrying";
    access_token_expired_ = true;
    OAuth2AccessTokenManager::ScopeSet scopes{
        GaiaConstants::kKidManagementPrivilegedOAuth2Scope};
    identity_manager_->RemoveAccessTokenFromCache(primary_account_id_, scopes,
                                                  access_token_);
    StartLogging();
    return;
  }

  if (response_code != net::HTTP_OK || net_error != net::OK) {
    DLOG(WARNING) << "HTTP error " << response_code << ", NetError "
                  << net_error;
    std::move(callback_).Run(Result::kNetworkError);
    return;
  }

  std::move(callback_).Run(Result::kSuccess);
}
