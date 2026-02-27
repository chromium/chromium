// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_AI_PRIVATE_AI_SERVICE_H_
#define CHROME_BROWSER_PRIVATE_AI_PRIVATE_AI_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/private_ai/phosphor/oauth_token_provider.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

class Profile;
class PrefService;

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}

namespace private_ai {

namespace phosphor {
class BlindSignAuthFactory;
class TokenFetcherImpl;
class TokenManager;
}  // namespace phosphor

class Client;

// The `PrivateAiService` is a KeyedService responsible for managing
// authentication tokens for the PrivateAI feature. It observes the user's
// sign-in state and, when a primary account is available, it can fetch OAuth2
// access tokens. These access tokens are then used by the underlying
// `phosphor::TokenManager` and `phosphor::TokenFetcher` to acquire and manage
// authentication tokens for PrivateAI. This service also creates and provides
// the `Client`, which serves as the primary interface for interacting with the
// PrivateAI feature.
class PrivateAiService : public KeyedService,
                         public phosphor::OAuthTokenProvider,
                         public signin::IdentityManager::Observer {
 public:
  explicit PrivateAiService(
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      Profile* profile,
      std::unique_ptr<phosphor::BlindSignAuthFactory> bsa_factory);
  ~PrivateAiService() override;

  // KeyedService override:
  void Shutdown() override;

  static bool CanPrivateAiBeEnabled();

  // Returns `nullptr` if `PrivateAiService` is shutting down.
  phosphor::TokenManager* GetTokenManager();

  Client* GetClient();

  void SetClientForTesting(std::unique_ptr<Client> client);

  // phosphor::OAuthTokenProvider override:
  bool IsTokenFetchEnabled() override;
  void RequestOAuthToken(RequestOAuthTokenCallback callback) override;

 private:
  void OnRequestOAuthTokenCompleted(
      std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> fetcher,
      RequestOAuthTokenCallback callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // signin::IdentityManager::Observer override:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<Profile> profile_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<PrefService> pref_service_;

  std::unique_ptr<phosphor::BlindSignAuthFactory> bsa_factory_;

  std::unique_ptr<phosphor::TokenManager> token_manager_;
  // Owned by `token_manager_`.
  raw_ptr<phosphor::TokenFetcherImpl> token_fetcher_;

  std::unique_ptr<Client> client_;

  bool is_shutting_down_ = false;

  base::WeakPtrFactory<PrivateAiService> weak_ptr_factory_{this};
};

}  // namespace private_ai

#endif  // CHROME_BROWSER_PRIVATE_AI_PRIVATE_AI_SERVICE_H_
