// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_AIDA_CLIENT_H_
#define CHROME_BROWSER_DEVTOOLS_AIDA_CLIENT_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

// The possible values for the DevTools AI enterprise policy.
enum class DevToolsGenAiEnterprisePolicyValue {
  kAllow = 0,
  kAllowWithoutLogging = 1,
  kDisable = 2,
};

class AidaClient {
 public:
  using ScopedOverride = std::unique_ptr<base::ScopedClosureRunner>;

  explicit AidaClient(Profile* profile);
  ~AidaClient();

  void PrepareRequestOrFail(
      base::OnceCallback<
          void(absl::variant<network::ResourceRequest, std::string>)> callback);
  void RemoveAccessToken();

  // Needed because VariationsService is not available for unit tests.
  static ScopedOverride OverrideCountryForTesting(std::string country_code);

  void OverrideAidaEndpointAndScopeForTesting(const std::string& aida_endpoint,
                                              const std::string& aida_scope);

  static constexpr std::string_view kDoConversationUrlPath =
      "/v1/aida:doConversation";
  static constexpr std::string_view kRegisterClientEventUrlPath =
      "/v1:registerClientEvent";

  struct Availability {
    bool available = false;
    bool blocked = true;
    bool blocked_by_age = true;
    bool blocked_by_enterprise_policy = true;
    bool blocked_by_geo = true;
    bool blocked_by_rollout = false;
    bool disallow_logging = true;
  };

  static Availability CanUseAida(Profile* profile);

 private:
  void PrepareAidaRequest(
      base::OnceCallback<
          void(absl::variant<network::ResourceRequest, std::string>)> callback);
  void AccessTokenFetchFinished(
      base::OnceCallback<
          void(absl::variant<network::ResourceRequest, std::string>)> callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  const raw_ref<Profile> profile_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  std::string aida_endpoint_;
  std::string aida_scope_;
  std::string access_token_;
  base::Time access_token_expiration_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_AIDA_CLIENT_H_
