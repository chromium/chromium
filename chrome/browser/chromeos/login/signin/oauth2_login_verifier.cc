// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/oauth2_login_verifier.h"

#include <vector>

#include "base/logging.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"

using content::BrowserThread;

namespace chromeos {

OAuth2LoginVerifier::OAuth2LoginVerifier(
    OAuth2LoginVerifier::Delegate* delegate,
    signin::IdentityManager* identity_manager,
    const CoreAccountId& primary_account_id,
    const std::string& oauthlogin_access_token)
    : delegate_(delegate),
      identity_manager_(identity_manager),
      primary_account_id_(primary_account_id),
      access_token_(oauthlogin_access_token) {
  DCHECK(delegate);
  identity_manager_->AddObserver(this);
}

OAuth2LoginVerifier::~OAuth2LoginVerifier() {
  identity_manager_->RemoveObserver(this);
}

void OAuth2LoginVerifier::VerifyUserCookies() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<gaia::ListedAccount> accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_manager_->GetAccountsInCookieJar();
  if (accounts_in_cookie_jar_info.accounts_are_fresh) {
    OnAccountsInCookieUpdated(
        accounts_in_cookie_jar_info,
        GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  }
}

void OAuth2LoginVerifier::VerifyProfileTokens() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  signin::AccountsCookieMutator::AddAccountToCookieCompletedCallback
      completion_callback =
          base::BindOnce(&OAuth2LoginVerifier::OnAddAccountToCookieCompleted,
                         weak_ptr_factory_.GetWeakPtr());
  if (access_token_.empty()) {
    identity_manager_->GetAccountsCookieMutator()->AddAccountToCookie(
        primary_account_id_, gaia::GaiaSource::kOAuth2LoginVerifier,
        std::move(completion_callback));
  } else {
    identity_manager_->GetAccountsCookieMutator()->AddAccountToCookieWithToken(
        primary_account_id_, access_token_,
        gaia::GaiaSource::kOAuth2LoginVerifier, std::move(completion_callback));
  }
}

void OAuth2LoginVerifier::OnAddAccountToCookieCompleted(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  if (account_id != primary_account_id_)
    return;

  if (error.state() == GoogleServiceAuthError::State::NONE) {
    VLOG(1) << "MergeSession successful.";
    delegate_->OnSessionMergeSuccess();
    return;
  }

  LOG(WARNING) << "Failed MergeSession request,"
               << " error: " << error.state();
  delegate_->OnSessionMergeFailure(error.IsTransientError());
}

void OAuth2LoginVerifier::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if (error.state() == GoogleServiceAuthError::State::NONE) {
    VLOG(1) << "ListAccounts successful.";
    delegate_->OnListAccountsSuccess(
        accounts_in_cookie_jar_info.signed_in_accounts);
    return;
  }

  LOG(WARNING) << "Failed to get list of session accounts, "
               << " error: " << error.state();
  delegate_->OnListAccountsFailure(error.IsTransientError());
}

}  // namespace chromeos
