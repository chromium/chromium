// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/aida_client.h"

#include <string>

#include "base/check_is_test.h"
#include "base/containers/fixed_flat_set.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/string_escape.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_utils.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"

const char kAidaEndpointUrl[] =
    "https://aida.googleapis.com/v1/aida:doConversation";

constexpr auto kLoggingDisallowedCountries =
    base::MakeFixedFlatSet<std::string_view>(
        {"at", "be", "bg", "ch", "cy", "cz", "de", "dk", "ee", "es", "fi",
         "fr", "gb", "gr", "hr", "hu", "ie", "is", "it", "li", "lt", "lu",
         "lv", "mt", "nl", "no", "pl", "pt", "ro", "se", "si", "sk"});

constexpr auto kAidaSupportedCountries =
    base::MakeFixedFlatSet<std::string_view>(
        {"ae", "ag", "ai", "am", "ao", "ar", "as", "at", "au", "aw", "az", "bb",
         "bd", "be", "bf", "bg", "bh", "bi", "bj", "bl", "bm", "bn", "bo", "bq",
         "br", "bs", "bt", "bw", "bz", "ca", "cc", "cd", "cf", "cg", "ch", "ci",
         "ck", "cl", "cm", "co", "cr", "cv", "cw", "cx", "cy", "cz", "de", "dj",
         "dk", "dm", "do", "dz", "ec", "ee", "eg", "eh", "er", "es", "et", "fi",
         "fj", "fk", "fm", "fr", "ga", "gb", "gd", "ge", "gg", "gh", "gi", "gm",
         "gn", "gq", "gr", "gs", "gt", "gu", "gw", "gy", "hm", "hn", "hr", "ht",
         "hu", "id", "ie", "il", "im", "in", "io", "iq", "is", "it", "je", "jm",
         "jo", "jp", "ke", "kg", "kh", "ki", "km", "kn", "kr", "kw", "ky", "kz",
         "la", "lb", "lc", "li", "lk", "lr", "ls", "lt", "lu", "lv", "ly", "ma",
         "mg", "mh", "ml", "mn", "mp", "mr", "ms", "mt", "mu", "mv", "mw", "mx",
         "my", "mz", "na", "nc", "ne", "nf", "ng", "ni", "nl", "no", "np", "nr",
         "nu", "nz", "om", "pa", "pe", "pg", "ph", "pk", "pl", "pm", "pn", "pr",
         "ps", "pt", "pw", "py", "qa", "ro", "rw", "sa", "sb", "sc", "sd", "se",
         "sg", "sh", "si", "sk", "sl", "sn", "so", "sr", "ss", "st", "sv", "sz",
         "tc", "td", "tg", "th", "tj", "tk", "tl", "tm", "tn", "to", "tr", "tt",
         "tv", "tw", "tz", "ug", "um", "us", "uy", "uz", "vc", "ve", "vg", "vi",
         "vn", "vu", "wf", "ws", "ye", "za", "zm", "zw"});

AidaClient::AidaClient(Profile* profile)
    : profile_(*profile),
      aida_endpoint_(kAidaEndpointUrl),
      aida_scope_(GaiaConstants::kAidaOAuth2Scope) {}

AidaClient::~AidaClient() = default;

std::optional<AccountInfo> AccountInfoForProfile(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::nullopt;
  }
  const auto account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    return std::nullopt;
  }
  return identity_manager->FindExtendedAccountInfoByAccountId(account_id);
}

bool IsAidaBlockedByAge(std::optional<AccountInfo> account_info) {
  if (!account_info.has_value()) {
    return true;
  }
  return account_info.value()
             .capabilities.can_use_devtools_generative_ai_features() !=
         signin::Tribool::kTrue;
}

std::unique_ptr<std::string>& GetCountryCodeOverride() {
  static base::NoDestructor<std::unique_ptr<std::string>> country_code_override(
      nullptr);
  return *country_code_override;
}

std::string GetCountryCode() {
  if (GetCountryCodeOverride()) {
    return *GetCountryCodeOverride();
  }
  std::string country_code =
      base::ToLowerASCII(variations::GetCurrentCountryCode(
          g_browser_process->variations_service()));
  DLOG_IF(WARNING, country_code.empty()) << "Couldn't get country info.";
  return country_code;
}

bool IsLoggingDisabledByGeo(std::string country_code) {
  return kLoggingDisallowedCountries.contains(country_code);
}

bool IsAidaBlockedByGeo(std::string country_code) {
  return !kAidaSupportedCountries.contains(country_code);
}

AidaClient::Availability AidaClient::CanUseAida(Profile* profile) {
  struct Availability result;
  // AidaClient is only available on branded builds
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  result.available = true;
  auto account_info = AccountInfoForProfile(profile);
  result.blocked_by_age = IsAidaBlockedByAge(account_info);
  result.blocked_by_enterprise_policy =
      profile->GetPrefs()->GetInteger(prefs::kDevToolsGenAiSettings) ==
      static_cast<int>(DevToolsGenAiEnterprisePolicyValue::kDisable);
  std::string country_code = GetCountryCode();
  result.blocked_by_geo = IsAidaBlockedByGeo(country_code);
  result.disallow_logging =
      profile->GetPrefs()->GetInteger(prefs::kDevToolsGenAiSettings) ==
          static_cast<int>(
              DevToolsGenAiEnterprisePolicyValue::kAllowWithoutLogging) ||
      IsLoggingDisabledByGeo(country_code);
  result.blocked = result.blocked_by_age ||
                   result.blocked_by_enterprise_policy || result.blocked_by_geo;

  return result;
#else
  // AidaClient is only available on branded builds
  result.available = false;
  result.blocked = true;
  return result;
#endif
}

AidaClient::ScopedOverride AidaClient::OverrideCountryForTesting(
    std::string country_code) {
  CHECK(!GetCountryCodeOverride());
  GetCountryCodeOverride() = std::make_unique<std::string>(country_code);
  return std::make_unique<base::ScopedClosureRunner>(
      base::BindOnce([]() { GetCountryCodeOverride().reset(); }));
}

void AidaClient::OverrideAidaEndpointAndScopeForTesting(
    const std::string& aida_endpoint,
    const std::string& aida_scope) {
  aida_endpoint_ = aida_endpoint;
  aida_scope_ = aida_scope;
}

void AidaClient::RemoveAccessToken() {
  access_token_.clear();
}

void AidaClient::PrepareRequestOrFail(
    base::OnceCallback<
        void(absl::variant<network::ResourceRequest, std::string>)> callback) {
  if (!access_token_.empty() && base::Time::Now() < access_token_expiration_) {
    PrepareAidaRequest(std::move(callback));
    return;
  }
  auto* identity_manager = IdentityManagerFactory::GetForProfile(&*profile_);
  if (!identity_manager) {
    std::move(callback).Run(R"({"error": "IdentityManager is not available"})");
    return;
  }
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_id, "AIDA client", signin::ScopeSet{aida_scope_},
      base::BindOnce(&AidaClient::AccessTokenFetchFinished,
                     base::Unretained(this), std::move(callback)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void AidaClient::AccessTokenFetchFinished(
    base::OnceCallback<
        void(absl::variant<network::ResourceRequest, std::string>)> callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback).Run(base::ReplaceStringPlaceholders(
        R"({"error": "Cannot get OAuth credentials", "detail": $1})",
        {base::GetQuotedJSONString(error.ToString())}, nullptr));
    return;
  }

  access_token_ = access_token_info.token;
  access_token_expiration_ = access_token_info.expiration_time;
  PrepareAidaRequest(std::move(callback));
}

void AidaClient::PrepareAidaRequest(
    base::OnceCallback<
        void(absl::variant<network::ResourceRequest, std::string>)> callback) {
  CHECK(!access_token_.empty());

  network::ResourceRequest aida_request;
  aida_request.url = GURL(aida_endpoint_);
  aida_request.load_flags = net::LOAD_DISABLE_CACHE;
  aida_request.credentials_mode = network::mojom::CredentialsMode::kOmit;
  aida_request.method = "POST";
  aida_request.headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                 std::string("Bearer ") + access_token_);
  std::move(callback).Run(std::move(aida_request));
}
