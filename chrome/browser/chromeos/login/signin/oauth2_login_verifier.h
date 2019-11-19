// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_OAUTH2_LOGIN_VERIFIER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_OAUTH2_LOGIN_VERIFIER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace chromeos {

// Given GCMS and primary account id, this class verifies GAIA credentials
// (SAPISID) and rebuild current session's cookie jar for the primary account.
class OAuth2LoginVerifier : public signin::IdentityManager::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Invoked when cookie session is successfully merged.
    virtual void OnSessionMergeSuccess() = 0;

    // Invoked when cookie session can not be merged.
    virtual void OnSessionMergeFailure(bool connection_error) = 0;

    // Invoked when account list is retrieved during post-merge session
    // verification.
    virtual void OnListAccountsSuccess(
        const std::vector<gaia::ListedAccount>& accounts) = 0;

    // Invoked when post-merge session verification fails.
    virtual void OnListAccountsFailure(bool connection_error) = 0;
  };

  OAuth2LoginVerifier(OAuth2LoginVerifier::Delegate* delegate,
                      signin::IdentityManager* identity_manager,
                      const CoreAccountId& primary_account_id,
                      const std::string& oauthlogin_access_token);
  ~OAuth2LoginVerifier() override;

  // Initiates verification of GAIA cookies in the cookie jar.
  void VerifyUserCookies();

  // Attempts to restore session from OAuth2 refresh token minting all necessary
  // tokens along the way (OAuth2 access token, SID/LSID, GAIA service token).
  void VerifyProfileTokens();

 private:
  // IdentityManager::Observer
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  void OnAddAccountToCookieCompleted(const CoreAccountId& account_id,
                                     const GoogleServiceAuthError& error);

  OAuth2LoginVerifier::Delegate* delegate_;
  signin::IdentityManager* identity_manager_;
  const CoreAccountId primary_account_id_;
  const std::string access_token_;

  base::WeakPtrFactory<OAuth2LoginVerifier> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OAuth2LoginVerifier);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_OAUTH2_LOGIN_VERIFIER_H_
