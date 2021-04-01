// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SECONDARY_ACCOUNT_CONSENT_LOGGER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SECONDARY_ACCOUNT_CONSENT_LOGGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "google_apis/gaia/core_account_id.h"

class GoogleServiceAuthError;
class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
}

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// Logs parental consent for addition of secondary EDU account.
// Firstly fetches access token with "kid.management.privileged" scope. Then
// logs consent with provided parent id, rapt, coexistence id and text version.
// EduCoexistenceId identifies the profile and is created if it's not present
// yet. Text version specifies the version of the text on the information page.
// TODO(crbug.com/1145246): Remove this code when the educoexistence v2 flow is
// stable.
class SecondaryAccountConsentLogger {
 public:
  enum class Result : int {
    kSuccess = 0,
    kTokenError,   // Failed to get OAuth2 token.
    kNetworkError  // Network failure.
  };

  // Create a new instance to log the consent. To start logging call
  // |StartLogging|.
  SecondaryAccountConsentLogger(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      const std::string& secondary_account_email,
      const std::string& parent_obfuscated_gaia_id,
      const std::string& re_auth_proof_token,
      base::OnceCallback<void(Result)> callback);
  SecondaryAccountConsentLogger(const SecondaryAccountConsentLogger&) = delete;
  SecondaryAccountConsentLogger& operator=(
      const SecondaryAccountConsentLogger&) = delete;
  ~SecondaryAccountConsentLogger();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the text version which reqires invalidation of the secondary
  // accounts added before the consent text changes.
  static std::string GetSecondaryAccountsInvalidationVersion();

  // Logs the consent.
  void StartLogging();

 private:
  friend class SecondaryAccountConsentLoggerTest;

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);
  // Returns a request body which contains secondary account email, parent id,
  // rapt and other info.
  base::DictionaryValue CreateRequestBody() const;
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnSimpleLoaderCompleteInternal(int net_error, int response_code);

  const CoreAccountId primary_account_id_;
  // Unowned pointer to identity manager.
  signin::IdentityManager* const identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Unowned pointer to pref service.
  PrefService* const pref_service_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::string access_token_;
  bool access_token_expired_ = false;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  const std::string secondary_account_email_;
  const std::string parent_obfuscated_gaia_id_;
  const std::string re_auth_proof_token_;

  base::OnceCallback<void(Result)> callback_;
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_SECONDARY_ACCOUNT_CONSENT_LOGGER_H_
