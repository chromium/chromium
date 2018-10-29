// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_OAUTH2_TOKEN_SERVICE_DELEGATE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/account_manager/account_manager.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class AccountTrackerService;
class SigninErrorController;

namespace chromeos {

class AccountMapperUtil;

class ChromeOSOAuth2TokenServiceDelegate
    : public OAuth2TokenServiceDelegate,
      public AccountManager::Observer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Accepts non-owning pointers to |AccountTrackerService|, |AccountManager|
  // and |SigninErrorController|. |AccountTrackerService| and
  // |SigninErrorController| are |KeyedService|s. |AccountManager| transitively
  // belongs to |g_browser_process|. They outlive (as they must) |this|
  // delegate.
  ChromeOSOAuth2TokenServiceDelegate(
      AccountTrackerService* account_tracker_service,
      AccountManager* account_manager,
      SigninErrorController* signin_error_controller);
  ~ChromeOSOAuth2TokenServiceDelegate() override;

  // OAuth2TokenServiceDelegate overrides.
  OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override;
  bool RefreshTokenIsAvailable(const std::string& account_id) const override;
  void UpdateAuthError(const std::string& account_id,
                       const GoogleServiceAuthError& error) override;
  GoogleServiceAuthError GetAuthError(
      const std::string& account_id) const override;
  std::vector<std::string> GetAccounts() override;
  void LoadCredentials(const std::string& primary_account_id) override;
  void UpdateCredentials(const std::string& account_id,
                         const std::string& refresh_token) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;
  void RevokeCredentials(const std::string& account_id) override;
  void RevokeAllCredentials() override;
  const net::BackoffEntry* BackoffEntry() const override;

  // |AccountManager::Observer| overrides.
  void OnTokenUpserted(const AccountManager::AccountKey& account_key) override;
  void OnAccountRemoved(const AccountManager::AccountKey& account_key) override;

  // |NetworkConnectionTracker::NetworkConnectionObserver| overrides.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CrOSOAuthDelegateTest,
                           BackOffIsTriggerredForTransientErrors);
  FRIEND_TEST_ALL_PREFIXES(CrOSOAuthDelegateTest,
                           BackOffIsResetOnNetworkChange);

  // A utility class to keep track of |GoogleServiceAuthError|s for an account.
  // This is used for providing account error status reports to
  // |SigninErrorController| and for firing
  // |OAuth2TokenService::Observer::OnAuthErrorChanged|.
  class AccountErrorStatus;

  // Callback handler for |AccountManager::GetAccounts|.
  void GetAccountsCallback(
      std::vector<AccountManager::AccountKey> account_keys);

  std::unique_ptr<AccountMapperUtil> account_mapper_util_;

  // A non-owning pointer to |AccountManager|. |AccountManager| is available
  // throughout the lifetime of a user session.
  AccountManager* account_manager_;

  // A non-owning pointer to |SigninErrorController|.
  SigninErrorController* const signin_error_controller_;

  // A cache of AccountKeys.
  std::set<AccountManager::AccountKey> account_keys_;

  // A map from account id to the last seen error for that account.
  std::map<std::string, std::unique_ptr<AccountErrorStatus>> errors_;

  // Used to rate-limit token fetch requests so as to not overload the server.
  net::BackoffEntry backoff_entry_;
  GoogleServiceAuthError backoff_error_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ChromeOSOAuth2TokenServiceDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSOAuth2TokenServiceDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
