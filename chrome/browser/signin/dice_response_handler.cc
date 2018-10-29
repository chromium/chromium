// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/mutable_profile_oauth2_token_service_delegate.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

const int kDiceTokenFetchTimeoutSeconds = 10;

namespace {

// The UMA histograms that logs events related to Dice responses.
const char kDiceResponseHeaderHistogram[] = "Signin.DiceResponseHeader";
const char kDiceTokenFetchResultHistogram[] = "Signin.DiceTokenFetchResult";
const char kChromePrimaryAccountStateOnWebSignoutHistogram[] =
    "Signin.ChromePrimaryAccountStateOnWebSignout";

// Used for UMA. Do not reorder, append new values at the end.
enum DiceResponseHeader {
  // Received a signin header.
  kSignin = 0,
  // Received a signout header including the Chrome primary account.
  kSignoutPrimary = 1,
  // Received a signout header for other account(s).
  kSignoutSecondary = 2,
  // Received a "EnableSync" header.
  kEnableSync = 3,

  kDiceResponseHeaderCount
};

// Used for UMA. Do not reorder, append new values at the end.
enum DiceTokenFetchResult {
  // The token fetch succeeded.
  kFetchSuccess = 0,
  // The token fetch was aborted. For example, if another request for the same
  // account is already in flight.
  kFetchAbort = 1,
  // The token fetch failed because Gaia responsed with an error.
  kFetchFailure = 2,
  // The token fetch failed because no response was received from Gaia.
  kFetchTimeout = 3,

  kDiceTokenFetchResultCount
};

// Used for UMA. Do not reorder, append new values at the end.
enum ChromePrimaryAccountStateInGaiaCookies {
  // The user is not authenticated in Chrome.
  kNoChromePrimaryAccount = 0,
  // The user is authenticated in Chrome with the first Gaia account.
  kChromePrimaryAccountIsFirstGaiaAccount = 1,
  // The user is authenticated in Chrome with another Gaia account.
  kChromePrimaryAccountIsSecondaryGaiaAccount = 2,
  // The user is authenticated in Chrome with an account that is not in Gaia
  // cookies.
  kChromePrimaryAccountIsNotInGaiaAccounts = 3,

  kChromePrimaryAccountStateInGaiaCookiesCount
};

class DiceResponseHandlerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns an instance of the factory singleton.
  static DiceResponseHandlerFactory* GetInstance() {
    return base::Singleton<DiceResponseHandlerFactory>::get();
  }

  static DiceResponseHandler* GetForProfile(Profile* profile) {
    return static_cast<DiceResponseHandler*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

 private:
  friend struct base::DefaultSingletonTraits<DiceResponseHandlerFactory>;

  DiceResponseHandlerFactory()
      : BrowserContextKeyedServiceFactory(
            "DiceResponseHandler",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(AboutSigninInternalsFactory::GetInstance());
    DependsOn(AccountReconcilorFactory::GetInstance());
    DependsOn(AccountTrackerServiceFactory::GetInstance());
    DependsOn(ChromeSigninClientFactory::GetInstance());
    DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
    DependsOn(SigninManagerFactory::GetInstance());
  }

  ~DiceResponseHandlerFactory() override {}

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    if (context->IsOffTheRecord())
      return nullptr;

    Profile* profile = static_cast<Profile*>(context);
    return new DiceResponseHandler(
        ChromeSigninClientFactory::GetForProfile(profile),
        SigninManagerFactory::GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        AccountTrackerServiceFactory::GetForProfile(profile),
        AccountReconcilorFactory::GetForProfile(profile),
        AboutSigninInternalsFactory::GetForProfile(profile),
        AccountConsistencyModeManager::GetMethodForProfile(profile),
        profile->GetPath());
  }
};

// Histogram macros expand to a lot of code, so it is better to wrap them in
// functions.

void RecordDiceResponseHeader(DiceResponseHeader header) {
  UMA_HISTOGRAM_ENUMERATION(kDiceResponseHeaderHistogram, header,
                            kDiceResponseHeaderCount);
}

void RecordDiceFetchTokenResult(DiceTokenFetchResult result) {
  UMA_HISTOGRAM_ENUMERATION(kDiceTokenFetchResultHistogram, result,
                            kDiceTokenFetchResultCount);
}

void RecordGaiaSignoutMetrics(ChromePrimaryAccountStateInGaiaCookies state) {
  UMA_HISTOGRAM_ENUMERATION(kChromePrimaryAccountStateOnWebSignoutHistogram,
                            state,
                            kChromePrimaryAccountStateInGaiaCookiesCount);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DiceTokenFetcher
////////////////////////////////////////////////////////////////////////////////

DiceResponseHandler::DiceTokenFetcher::DiceTokenFetcher(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& authorization_code,
    SigninClient* signin_client,
    AccountReconcilor* account_reconcilor,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate,
    DiceResponseHandler* dice_response_handler)
    : gaia_id_(gaia_id),
      email_(email),
      authorization_code_(authorization_code),
      delegate_(std::move(delegate)),
      dice_response_handler_(dice_response_handler),
      timeout_closure_(
          base::Bind(&DiceResponseHandler::DiceTokenFetcher::OnTimeout,
                     base::Unretained(this))),
      should_enable_sync_(false) {
  DCHECK(dice_response_handler_);
  account_reconcilor_lock_ =
      std::make_unique<AccountReconcilor::Lock>(account_reconcilor);
  gaia_auth_fetcher_ = signin_client->CreateGaiaAuthFetcher(
      this, GaiaConstants::kChromeSource, signin_client->GetURLLoaderFactory());
  VLOG(1) << "Start fetching token for account: " << email;
  gaia_auth_fetcher_->StartAuthCodeForOAuth2TokenExchange(authorization_code_);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, timeout_closure_.callback(),
      base::TimeDelta::FromSeconds(kDiceTokenFetchTimeoutSeconds));
}

DiceResponseHandler::DiceTokenFetcher::~DiceTokenFetcher() {}

void DiceResponseHandler::DiceTokenFetcher::OnTimeout() {
  RecordDiceFetchTokenResult(kFetchTimeout);
  gaia_auth_fetcher_.reset();
  timeout_closure_.Cancel();
  dice_response_handler_->OnTokenExchangeFailure(
      this, GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
  // |this| may be deleted at this point.
}

void DiceResponseHandler::DiceTokenFetcher::OnClientOAuthSuccess(
    const GaiaAuthConsumer::ClientOAuthResult& result) {
  RecordDiceFetchTokenResult(kFetchSuccess);
  gaia_auth_fetcher_.reset();
  timeout_closure_.Cancel();
  dice_response_handler_->OnTokenExchangeSuccess(
      this, result.refresh_token, result.is_under_advanced_protection);
  // |this| may be deleted at this point.
}

void DiceResponseHandler::DiceTokenFetcher::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  RecordDiceFetchTokenResult(kFetchFailure);
  gaia_auth_fetcher_.reset();
  timeout_closure_.Cancel();
  dice_response_handler_->OnTokenExchangeFailure(this, error);
  // |this| may be deleted at this point.
}

////////////////////////////////////////////////////////////////////////////////
// DiceResponseHandler
////////////////////////////////////////////////////////////////////////////////

// static
DiceResponseHandler* DiceResponseHandler::GetForProfile(Profile* profile) {
  return DiceResponseHandlerFactory::GetForProfile(profile);
}

DiceResponseHandler::DiceResponseHandler(
    SigninClient* signin_client,
    SigninManager* signin_manager,
    ProfileOAuth2TokenService* profile_oauth2_token_service,
    AccountTrackerService* account_tracker_service,
    AccountReconcilor* account_reconcilor,
    AboutSigninInternals* about_signin_internals,
    signin::AccountConsistencyMethod account_consistency,
    const base::FilePath& profile_path)
    : signin_manager_(signin_manager),
      signin_client_(signin_client),
      token_service_(profile_oauth2_token_service),
      account_tracker_service_(account_tracker_service),
      account_reconcilor_(account_reconcilor),
      about_signin_internals_(about_signin_internals),
      account_consistency_(account_consistency),
      profile_path_(profile_path) {
  DCHECK(signin_client_);
  DCHECK(signin_manager_);
  DCHECK(token_service_);
  DCHECK(account_tracker_service_);
  DCHECK(account_reconcilor_);
  DCHECK(about_signin_internals_);
}

DiceResponseHandler::~DiceResponseHandler() {}

void DiceResponseHandler::ProcessDiceHeader(
    const signin::DiceResponseParams& dice_params,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate) {
  DCHECK(signin::DiceMethodGreaterOrEqual(
      account_consistency_,
      signin::AccountConsistencyMethod::kDiceFixAuthErrors));
  DCHECK(delegate);
  switch (dice_params.user_intention) {
    case signin::DiceAction::SIGNIN: {
      const signin::DiceResponseParams::AccountInfo& info =
          dice_params.signin_info->account_info;
      ProcessDiceSigninHeader(info.gaia_id, info.email,
                              dice_params.signin_info->authorization_code,
                              std::move(delegate));
      return;
    }
    case signin::DiceAction::ENABLE_SYNC: {
      const signin::DiceResponseParams::AccountInfo& info =
          dice_params.enable_sync_info->account_info;
      ProcessEnableSyncHeader(info.gaia_id, info.email, std::move(delegate));
      return;
    }
    case signin::DiceAction::SIGNOUT:
      DCHECK_GT(dice_params.signout_info->account_infos.size(), 0u);
      ProcessDiceSignoutHeader(dice_params.signout_info->account_infos);
      return;
    case signin::DiceAction::NONE:
      NOTREACHED() << "Invalid Dice response parameters.";
      return;
  }
  NOTREACHED();
}

size_t DiceResponseHandler::GetPendingDiceTokenFetchersCountForTesting() const {
  return token_fetchers_.size();
}

bool DiceResponseHandler::CanGetTokenForAccount(const std::string& gaia_id,
                                                const std::string& email) {
  if (signin::DiceMethodGreaterOrEqual(
          account_consistency_,
          signin::AccountConsistencyMethod::kDiceMigration)) {
    return true;
  }

  // When using kDiceFixAuthErrors, only get a token if the account matches
  // the current Chrome account.
  DCHECK_EQ(signin::AccountConsistencyMethod::kDiceFixAuthErrors,
            account_consistency_);
  std::string account =
      account_tracker_service_->PickAccountIdForAccount(gaia_id, email);
  std::string chrome_account = signin_manager_->GetAuthenticatedAccountId();
  bool can_get_token = (chrome_account == account);
  VLOG_IF(1, !can_get_token)
      << "[Dice] Dropping Dice signin response for " << account;
  return can_get_token;
}

void DiceResponseHandler::ProcessDiceSigninHeader(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& authorization_code,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate) {
  DCHECK(!gaia_id.empty());
  DCHECK(!email.empty());
  DCHECK(!authorization_code.empty());
  VLOG(1) << "Start processing Dice signin response";
  RecordDiceResponseHeader(kSignin);

  if (!CanGetTokenForAccount(gaia_id, email)) {
    RecordDiceFetchTokenResult(kFetchAbort);
    return;
  }

  for (auto it = token_fetchers_.begin(); it != token_fetchers_.end(); ++it) {
    if ((it->get()->gaia_id() == gaia_id) && (it->get()->email() == email) &&
        (it->get()->authorization_code() == authorization_code)) {
      RecordDiceFetchTokenResult(kFetchAbort);
      return;  // There is already a request in flight with the same parameters.
    }
  }
  token_fetchers_.push_back(std::make_unique<DiceTokenFetcher>(
      gaia_id, email, authorization_code, signin_client_, account_reconcilor_,
      std::move(delegate), this));
}

void DiceResponseHandler::ProcessEnableSyncHeader(
    const std::string& gaia_id,
    const std::string& email,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate) {
  VLOG(1) << "Start processing Dice enable sync response";
  RecordDiceResponseHeader(kEnableSync);
  for (auto it = token_fetchers_.begin(); it != token_fetchers_.end(); ++it) {
    DiceTokenFetcher* fetcher = it->get();
    if (fetcher->gaia_id() == gaia_id) {
      DCHECK(gaia::AreEmailsSame(fetcher->email(), email));
      // If there is a fetch in progress for a resfresh token for the given
      // account, then simply mark it to enable sync after the refresh token is
      // available.
      fetcher->set_should_enable_sync(true);
      return;  // There is already a request in flight with the same parameters.
    }
  }
  std::string account_id =
      account_tracker_service_->PickAccountIdForAccount(gaia_id, email);
  delegate->EnableSync(account_id);
}

void DiceResponseHandler::ProcessDiceSignoutHeader(
    const std::vector<signin::DiceResponseParams::AccountInfo>& account_infos) {
  VLOG(1) << "Start processing Dice signout response";
  if (account_consistency_ ==
      signin::AccountConsistencyMethod::kDiceFixAuthErrors) {
    // Ignore signout responses when using kDiceFixAuthErrors.
    return;
  }

  std::string primary_account = signin_manager_->GetAuthenticatedAccountId();
  bool primary_account_signed_out = false;
  for (const auto& account_info : account_infos) {
    std::string signed_out_account =
        account_tracker_service_->PickAccountIdForAccount(account_info.gaia_id,
                                                          account_info.email);
    if (signed_out_account == primary_account) {
      primary_account_signed_out = true;
      RecordDiceResponseHeader(kSignoutPrimary);
      RecordGaiaSignoutMetrics(
          (account_info.session_index == 0)
              ? kChromePrimaryAccountIsFirstGaiaAccount
              : kChromePrimaryAccountIsSecondaryGaiaAccount);

      if (account_consistency_ == signin::AccountConsistencyMethod::kDice) {
        // Put the account in error state.
        token_service_->UpdateCredentials(
            primary_account,
            MutableProfileOAuth2TokenServiceDelegate::kInvalidRefreshToken);
      } else {
        // If Dice migration is not complete, the token for the main account
        // must not be deleted when signing out of the web.
        continue;
      }
    } else {
      token_service_->RevokeCredentials(signed_out_account);
    }

    // If a token fetch is in flight for the same account, cancel it.
    for (auto it = token_fetchers_.begin(); it != token_fetchers_.end(); ++it) {
      std::string token_fetcher_account_id =
          account_tracker_service_->PickAccountIdForAccount(
              it->get()->gaia_id(), it->get()->email());
      if (token_fetcher_account_id == signed_out_account) {
        token_fetchers_.erase(it);
        break;
      }
    }
  }

  if (!primary_account_signed_out) {
    RecordDiceResponseHeader(kSignoutSecondary);
    RecordGaiaSignoutMetrics(primary_account.empty()
                                 ? kNoChromePrimaryAccount
                                 : kChromePrimaryAccountIsNotInGaiaAccounts);
  }
}

void DiceResponseHandler::DeleteTokenFetcher(DiceTokenFetcher* token_fetcher) {
  for (auto it = token_fetchers_.begin(); it != token_fetchers_.end(); ++it) {
    if (it->get() == token_fetcher) {
      token_fetchers_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void DiceResponseHandler::OnTokenExchangeSuccess(
    DiceTokenFetcher* token_fetcher,
    const std::string& refresh_token,
    bool is_under_advanced_protection) {
  const std::string& email = token_fetcher->email();
  const std::string& gaia_id = token_fetcher->gaia_id();
  if (!CanGetTokenForAccount(gaia_id, email))
    return;
  VLOG(1) << "[Dice] OAuth success for email " << email;
  bool should_enable_sync = token_fetcher->should_enable_sync();
  std::string account_id =
      account_tracker_service_->SeedAccountInfo(gaia_id, email);
  account_tracker_service_->SetIsAdvancedProtectionAccount(
      account_id, is_under_advanced_protection);
  token_service_->UpdateCredentials(account_id, refresh_token);
  about_signin_internals_->OnRefreshTokenReceived(
      base::StringPrintf("Successful (%s)", account_id.c_str()));
  if (should_enable_sync)
    token_fetcher->delegate()->EnableSync(account_id);

  DeleteTokenFetcher(token_fetcher);
}

void DiceResponseHandler::OnTokenExchangeFailure(
    DiceTokenFetcher* token_fetcher,
    const GoogleServiceAuthError& error) {
  const std::string& email = token_fetcher->email();
  const std::string& gaia_id = token_fetcher->gaia_id();
  std::string account_id =
      account_tracker_service_->PickAccountIdForAccount(gaia_id, email);
  about_signin_internals_->OnRefreshTokenReceived(
      base::StringPrintf("Failure (%s)", account_id.c_str()));
  token_fetcher->delegate()->HandleTokenExchangeFailure(email, error);

  DeleteTokenFetcher(token_fetcher);
}
