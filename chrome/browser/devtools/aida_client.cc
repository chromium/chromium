// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/aida_client.h"
#include <string>
#include "base/check_is_test.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/string_escape.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/service/variations_service.h"
#include "net/base/load_flags.h"

std::string GetAidaEndpoint() {
  if (base::FeatureList::IsEnabled(
          ::features::kDevToolsConsoleInsightsDogfood)) {
    return features::kDevToolsConsoleInsightsDogfoodAidaEndpoint.Get();
  }
  return features::kDevToolsConsoleInsightsAidaEndpoint.Get();
}

std::string GetAidaScope() {
  if (base::FeatureList::IsEnabled(
          ::features::kDevToolsConsoleInsightsDogfood)) {
    return features::kDevToolsConsoleInsightsDogfoodAidaScope.Get();
  }
  return features::kDevToolsConsoleInsightsAidaScope.Get();
}

AidaClient::AidaClient(Profile* profile)
    : profile_(*profile),
      aida_endpoint_(GetAidaEndpoint()),
      aida_scope_(GetAidaScope()) {}

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

AidaClient::BlockedReason AidaClient::CanUseAida(Profile* profile) {
  struct BlockedReason result;
  // Console insights is only available on branded builds
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  result.blocked = true;
  result.blocked_by_feature_flag = true;
  return result;
#else
  // Console insights is always available for Google dogfooders
  if (base::FeatureList::IsEnabled(
          ::features::kDevToolsConsoleInsightsDogfood)) {
    result.blocked = false;
    return result;
  }
  // If `SettingVisible` is disabled, DevTools does not show a blocked reason
  if (!base::FeatureList::IsEnabled(
          ::features::kDevToolsConsoleInsightsSettingVisible)) {
    result.blocked = true;
    result.blocked_by_feature_flag = true;
    return result;
  }
  // Console insights is not available if the feature flag is off
  if (!base::FeatureList::IsEnabled(::features::kDevToolsConsoleInsights)) {
    result.blocked = true;
    auto blocked_by =
        ::features::kDevToolsConsoleInsightsSettingVisibleBlockedReason.Get();
    if (blocked_by == "rollout") {
      result.blocked_by_rollout = true;
      return result;
    }
    if (blocked_by == "region") {
      result.blocked_by_geo = true;
      return result;
    }
    result.blocked_by_feature_flag = true;
    return result;
  }
  // If the feature flag is on, evaluate other restriction reasons
  result.blocked_by_feature_flag = false;
  auto account_info = AccountInfoForProfile(profile);
  result.blocked_by_age = IsAidaBlockedByAge(account_info);
  result.blocked_by_enterprise_policy =
      profile->GetPrefs()->GetInteger(prefs::kDevToolsGenAiSettings) ==
      static_cast<int>(DevToolsGenAiEnterprisePolicyValue::kDisable);
  result.disallow_logging =
      profile->GetPrefs()->GetInteger(prefs::kDevToolsGenAiSettings) ==
      static_cast<int>(
          DevToolsGenAiEnterprisePolicyValue::kAllowWithoutLogging);
  result.blocked = result.blocked_by_age || result.blocked_by_enterprise_policy;
  return result;
#endif
}

void AidaClient::OverrideAidaEndpointAndScopeForTesting(
    const std::string& aida_endpoint,
    const std::string& aida_scope) {
  aida_endpoint_ = aida_endpoint;
  aida_scope_ = aida_scope;
}

void AidaClient::PrepareRequestOrFail(
    base::OnceCallback<
        void(absl::variant<network::ResourceRequest, std::string>)> callback) {
  if (aida_scope_.empty()) {
    std::move(callback).Run(R"({"error": "AIDA scope is not configured"})");
    return;
  }
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
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync);
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

  if (aida_endpoint_.empty()) {
    std::move(callback).Run(R"({"error": "AIDA endpoint is not configured"})");
    return;
  }

  network::ResourceRequest aida_request;
  aida_request.url = GURL(aida_endpoint_);
  aida_request.load_flags = net::LOAD_DISABLE_CACHE;
  aida_request.credentials_mode = network::mojom::CredentialsMode::kOmit;
  aida_request.method = "POST";
  aida_request.headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                 std::string("Bearer ") + access_token_);
  std::move(callback).Run(std::move(aida_request));
}
