// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

const int kDiceTokenFetchTimeoutSeconds = 10;
// Timeout for locking the account reconcilor when
// there was OAuth outage in Dice.
const int kLockAccountReconcilorTimeoutHours = 12;

namespace {

// The UMA histograms that logs events related to Dice responses.
const char kDiceResponseHeaderHistogram[] = "Signin.DiceResponseHeader";
const char kDiceTokenFetchResultHistogram[] = "Signin.DiceTokenFetchResult";

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

class DiceResponseHandlerFactory : public ProfileKeyedServiceFactory {
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
      : ProfileKeyedServiceFactory("DiceResponseHandler") {
    DependsOn(AboutSigninInternalsFactory::GetInstance());
    DependsOn(AccountReconcilorFactory::GetInstance());
    DependsOn(ChromeSigninClientFactory::GetInstance());
    DependsOn(IdentityManagerFactory::GetInstance());
  }

  ~DiceResponseHandlerFactory() override {}

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = static_cast<Profile*>(context);
    return new DiceResponseHandler(
        ChromeSigninClientFactory::GetForProfile(profile),
        IdentityManagerFactory::GetForProfile(profile),
        AccountReconcilorFactory::GetForProfile(profile),
        AboutSigninInternalsFactory::GetForProfile(profile),
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
          base::BindOnce(&DiceResponseHandler::DiceTokenFetcher::OnTimeout,
                         base::Unretained(this))),
      should_enable_sync_(false) {
  DCHECK(dice_response_handler_);
  account_reconcilor_lock_ =
      std::make_unique<AccountReconcilor::Lock>(account_reconcilor);
  gaia_auth_fetcher_ =
      signin_client->CreateGaiaAuthFetcher(this, gaia::GaiaSource::kChrome);
  VLOG(1) << "Start fetching token for account: " << email;
  gaia_auth_fetcher_->StartAuthCodeForOAuth2TokenExchange(authorization_code_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, timeout_closure_.callback(),
      base::Seconds(kDiceTokenFetchTimeoutSeconds));
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
    signin::IdentityManager* identity_manager,
    AccountReconcilor* account_reconcilor,
    AboutSigninInternals* about_signin_internals,
    const base::FilePath& profile_path)
    : signin_client_(signin_client),
      identity_manager_(identity_manager),
      account_reconcilor_(account_reconcilor),
      about_signin_internals_(about_signin_internals),
      profile_path_(profile_path) {
  DCHECK(signin_client_);
  DCHECK(identity_manager_);
  DCHECK(account_reconcilor_);
  DCHECK(about_signin_internals_);
}

DiceResponseHandler::~DiceResponseHandler() {}

void DiceResponseHandler::ProcessDiceHeader(
    const signin::DiceResponseParams& dice_params,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate) {
  DCHECK(delegate);
  switch (dice_params.user_intention) {
    case signin::DiceAction::SIGNIN: {
      const signin::DiceResponseParams::AccountInfo& info =
          dice_params.signin_info->account_info;
      ProcessDiceSigninHeader(
          info.gaia_id, info.email, dice_params.signin_info->authorization_code,
          dice_params.signin_info->no_authorization_code, std::move(delegate));
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

void DiceResponseHandler::OnTimeoutUnlockReconcilor() {
  lock_.reset();
}

void DiceResponseHandler::SetTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
}

void DiceResponseHandler::ProcessDiceSigninHeader(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& authorization_code,
    bool no_authorization_code,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate) {
  if (no_authorization_code) {
    lock_ = std::make_unique<AccountReconcilor::Lock>(account_reconcilor_);
    about_signin_internals_->OnRefreshTokenReceived(
        "Missing authorization code due to OAuth outage in Dice.");
    if (!timer_) {
      timer_ = std::make_unique<base::OneShotTimer>();
      if (task_runner_)
        timer_->SetTaskRunner(task_runner_);
    }
    // If there is already another lock, the timer will be reset and
    // we'll wait another full timeout.
    timer_->Start(
        FROM_HERE, base::Hours(kLockAccountReconcilorTimeoutHours),
        base::BindOnce(&DiceResponseHandler::OnTimeoutUnlockReconcilor,
                       base::Unretained(this)));
    return;
  }

  DCHECK(!gaia_id.empty());
  DCHECK(!email.empty());
  DCHECK(!authorization_code.empty());
  VLOG(1) << "Start processing Dice signin response";
  RecordDiceResponseHeader(kSignin);

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
  CoreAccountId account_id =
      identity_manager_->PickAccountIdForAccount(gaia_id, email);
  delegate->EnableSync(account_id);
}

void DiceResponseHandler::ProcessDiceSignoutHeader(
    const std::vector<signin::DiceResponseParams::AccountInfo>& account_infos) {
  VLOG(1) << "Start processing Dice signout response";

  // If there is a restriction on removing the primary account. Do not remove
  // the account regardless of the consent level. Else, the sync account can
  // only be invalidated.
  signin::ConsentLevel level =
      signin_client_->IsClearPrimaryAccountAllowed(
          identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync))
          ? signin::ConsentLevel::kSync
          : signin::ConsentLevel::kSignin;

  CoreAccountId primary_account = identity_manager_->GetPrimaryAccountId(level);
  bool primary_account_signed_out = false;
  auto* accounts_mutator = identity_manager_->GetAccountsMutator();
  for (const auto& account_info : account_infos) {
    CoreAccountId signed_out_account =
        identity_manager_->PickAccountIdForAccount(account_info.gaia_id,
                                                   account_info.email);
    if (signed_out_account == primary_account) {
      primary_account_signed_out = true;
      RecordDiceResponseHeader(kSignoutPrimary);

      // Put the account in error state.
      accounts_mutator->InvalidateRefreshTokenForPrimaryAccount(
          signin_metrics::SourceForRefreshTokenOperation::
              kDiceResponseHandler_Signout);
    } else {
      accounts_mutator->RemoveAccount(
          signed_out_account, signin_metrics::SourceForRefreshTokenOperation::
                                  kDiceResponseHandler_Signout);
    }

    // If a token fetch is in flight for the same account, cancel it.
    for (auto it = token_fetchers_.begin(); it != token_fetchers_.end(); ++it) {
      CoreAccountId token_fetcher_account_id =
          identity_manager_->PickAccountIdForAccount(it->get()->gaia_id(),
                                                     it->get()->email());
      if (token_fetcher_account_id == signed_out_account) {
        token_fetchers_.erase(it);
        break;
      }
    }
  }

  if (!primary_account_signed_out)
    RecordDiceResponseHeader(kSignoutSecondary);
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
  VLOG(1) << "[Dice] OAuth success for email " << email;
  bool should_enable_sync = token_fetcher->should_enable_sync();
  CoreAccountId account_id =
      identity_manager_->PickAccountIdForAccount(gaia_id, email);
  bool is_new_account =
      !identity_manager_->HasAccountWithRefreshToken(account_id);
  identity_manager_->GetAccountsMutator()->AddOrUpdateAccount(
      gaia_id, email, refresh_token, is_under_advanced_protection,
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin);
  about_signin_internals_->OnRefreshTokenReceived(
      base::StringPrintf("Successful (%s)", account_id.ToString().c_str()));
  token_fetcher->delegate()->HandleTokenExchangeSuccess(account_id,
                                                        is_new_account);
  if (should_enable_sync)
    token_fetcher->delegate()->EnableSync(account_id);

  DeleteTokenFetcher(token_fetcher);
}

void DiceResponseHandler::OnTokenExchangeFailure(
    DiceTokenFetcher* token_fetcher,
    const GoogleServiceAuthError& error) {
  const std::string& email = token_fetcher->email();
  const std::string& gaia_id = token_fetcher->gaia_id();
  CoreAccountId account_id =
      identity_manager_->PickAccountIdForAccount(gaia_id, email);
  about_signin_internals_->OnRefreshTokenReceived(
      base::StringPrintf("Failure (%s)", account_id.ToString().c_str()));
  token_fetcher->delegate()->HandleTokenExchangeFailure(email, error);

  DeleteTokenFetcher(token_fetcher);
}

// static
void DiceResponseHandler::EnsureFactoryBuilt() {
  DiceResponseHandlerFactory::GetInstance();
}
