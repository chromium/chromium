// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/aida_client.h"
#include <string>
#include "base/json/json_string_value_serializer.h"
#include "base/json/string_escape.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/load_flags.h"

AidaClient::AidaClient(Profile* profile)
    : profile_(*profile),
      aida_endpoint_(features::kDevToolsConsoleInsightsAidaEndpoint.Get()),
      aida_scope_(features::kDevToolsConsoleInsightsAidaScope.Get()) {}

AidaClient::~AidaClient() = default;

bool AidaClient::CanUseAida(Profile* profile) {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#else
  if (base::FeatureList::IsEnabled(
          ::features::kDevToolsConsoleInsightsDogfood)) {
    return true;
  }
  return base::FeatureList::IsEnabled(::features::kDevToolsConsoleInsights) &&
         profile->GetPrefs()->GetInteger(prefs::kDevToolsGenAiSettings) ==
             static_cast<int>(DevToolsGenAiEnterprisePolicyValue::kAllow);
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
  // TODO(dsv): remove clearing path once the config is updated
  GURL::Replacements clear_path;
  clear_path.ClearPath();
  aida_request.url = GURL(aida_endpoint_).ReplaceComponents(clear_path);
  aida_request.load_flags = net::LOAD_DISABLE_CACHE;
  aida_request.credentials_mode = network::mojom::CredentialsMode::kOmit;
  aida_request.method = "POST";
  aida_request.headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                 std::string("Bearer ") + access_token_);
  std::move(callback).Run(std::move(aida_request));
}
