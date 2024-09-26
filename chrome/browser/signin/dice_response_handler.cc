// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include <optional>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/bound_session_credentials/registration_token_helper.h"  // nogncheck
#include "components/embedder_support/user_agent_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/unexportable_keys/unexportable_key_id.h"       // nogncheck
#include "components/unexportable_keys/unexportable_key_service.h"  // nogncheck
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

const int kDiceTokenFetchTimeoutSeconds = 10;
// Timeout for locking the account reconcilor when
// there was OAuth outage in Dice.
const int kLockAccountReconcilorTimeoutHours = 12;

namespace {

// The UMA histograms that logs events related to Dice responses.
const char kDiceResponseHeaderHistogram[] = "Signin.DiceResponseHeader";
const char kDiceTokenFetchResultHistogram[] = "Signin.DiceTokenFetchResult";
const char kDiceTokenBindingOutcomeHistogram[] =
    "Signin.DiceTokenBindingOutcome";

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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
// If any of existing accounts is already bound, returns its binding key.
// Otherwise, returns an empty key indicating that a new key needs to be
// generated.
std::vector<uint8_t> GetWrappedBindingKeyToReuse(
    const signin::IdentityManager& identity_manager) {
  std::vector<CoreAccountInfo> accounts =
      identity_manager.GetAccountsWithRefreshTokens();
  for (const auto& account : accounts) {
    std::vector<uint8_t> account_binding_key =
        identity_manager.GetWrappedBindingKeyOfRefreshTokenForAccount(
            account.account_id);
    if (!account_binding_key.empty()) {
      // All bound tokens are supposed to use the same key, so return the first
      // non-empty key. Having two different keys should be considered a bug.
      return account_binding_key;
    }
  }

  return {};
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

void RecordDiceResponseHeader(DiceResponseHeader header) {
  base::UmaHistogramEnumeration(kDiceResponseHeaderHistogram, header,
                                kDiceResponseHeaderCount);
}

void RecordDiceFetchTokenResult(DiceTokenFetchResult result) {
  base::UmaHistogramEnumeration(kDiceTokenFetchResultHistogram, result,
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
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    base::expected<raw_ref<RegistrationTokenHelper>, TokenBindingOutcome>
        registration_token_helper_or_error,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    DiceResponseHandler* dice_response_handler)
    : gaia_id_(gaia_id),
      email_(email),
      authorization_code_(authorization_code),
      delegate_(std::move(delegate)),
      dice_response_handler_(dice_response_handler),
      signin_client_(signin_client),
      timeout_closure_(
          base::BindOnce(&DiceResponseHandler::DiceTokenFetcher::OnTimeout,
                         base::Unretained(this))),
      should_enable_sync_(false) {
  DCHECK(dice_response_handler_);
  account_reconcilor_lock_ =
      std::make_unique<AccountReconcilor::Lock>(account_reconcilor);
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (registration_token_helper_or_error.has_value()) {
    StartBindingKeyGeneration(registration_token_helper_or_error->get());
    // Wait until the binding key is generated before fetching a token.
    return;
  } else {
    token_binding_outcome_ = registration_token_helper_or_error.error();
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  StartTokenFetch();
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
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (!wrapped_binding_key_.empty()) {
    CHECK(switches::IsChromeRefreshTokenBindingEnabled(
        signin_client_->GetPrefs()));
    if (!result.is_bound_to_key) {
      wrapped_binding_key_.clear();
      token_binding_outcome_ = TokenBindingOutcome::kNotBoundServerRejectedKey;
    } else {
      token_binding_outcome_ = TokenBindingOutcome::kBound;
    }
  }
  base::UmaHistogramEnumeration(kDiceTokenBindingOutcomeHistogram,
                                token_binding_outcome_);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  dice_response_handler_->OnTokenExchangeSuccess(
      this, result.refresh_token, result.is_under_advanced_protection
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      wrapped_binding_key_
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  );
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

void DiceResponseHandler::DiceTokenFetcher::StartTokenFetch() {
  VLOG(1) << "Start fetching token for account: " << email_;
  gaia_auth_fetcher_ =
      signin_client_->CreateGaiaAuthFetcher(this, gaia::GaiaSource::kChrome);
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  // `binding_registration_token_` is empty if the binding key was not
  // generated.
  gaia_auth_fetcher_->StartAuthCodeForOAuth2TokenExchange(
      authorization_code_,
      embedder_support::GetUserAgentMetadata(g_browser_process->local_state())
          .SerializeBrandFullVersionList(),
      binding_registration_token_);
#else
  gaia_auth_fetcher_->StartAuthCodeForOAuth2TokenExchange(authorization_code_);
#endif
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, timeout_closure_.callback(),
      base::Seconds(kDiceTokenFetchTimeoutSeconds));
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
void DiceResponseHandler::DiceTokenFetcher::StartBindingKeyGeneration(
    RegistrationTokenHelper& registration_token_helper) {
  CHECK(
      switches::IsChromeRefreshTokenBindingEnabled(signin_client_->GetPrefs()));
  // `base::Unretained()` is safe because `DiceResponseHandler` guarantees that
  // `registration_token_helper` outlives `this`.
  registration_token_helper.GenerateForTokenBinding(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(), authorization_code_,
      GURL("https://accounts.google.com/accountmanager"),
      base::BindOnce(&DiceTokenFetcher::OnRegistrationTokenGenerated,
                     base::Unretained(this)));
}

void DiceResponseHandler::DiceTokenFetcher::OnRegistrationTokenGenerated(
    std::optional<RegistrationTokenHelper::Result> result) {
  CHECK(
      switches::IsChromeRefreshTokenBindingEnabled(signin_client_->GetPrefs()));
  if (result.has_value()) {
    binding_registration_token_ = std::move(result->registration_token);
    wrapped_binding_key_ = std::move(result->wrapped_binding_key);
  } else {
    token_binding_outcome_ =
        TokenBindingOutcome::kNotBoundRegistrationTokenGenerationFailed;
  }
  StartTokenFetch();
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

////////////////////////////////////////////////////////////////////////////////
// DiceResponseHandler
////////////////////////////////////////////////////////////////////////////////

DiceResponseHandler::DiceResponseHandler(
    SigninClient* signin_client,
    signin::IdentityManager* identity_manager,
    AccountReconcilor* account_reconcilor,
    AboutSigninInternals* about_signin_internals,
    RegistrationTokenHelperFactory registration_token_helper_factory)
    : signin_client_(signin_client),
      identity_manager_(identity_manager),
      account_reconcilor_(account_reconcilor),
      about_signin_internals_(about_signin_internals),
      registration_token_helper_factory_(
          std::move(registration_token_helper_factory)) {
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
          dice_params.signin_info->no_authorization_code,
          dice_params.signin_info->supported_algorithms_for_token_binding,
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
      NOTREACHED_IN_MIGRATION() << "Invalid Dice response parameters.";
      return;
  }
  NOTREACHED_IN_MIGRATION();
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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
void DiceResponseHandler::SetRegistrationTokenHelperFactoryForTesting(
    RegistrationTokenHelperFactory factory) {
  CHECK(
      switches::IsChromeRefreshTokenBindingEnabled(signin_client_->GetPrefs()));
  registration_token_helper_factory_ = std::move(factory);
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

void DiceResponseHandler::ProcessDiceSigninHeader(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& authorization_code,
    bool no_authorization_code,
    const std::string& supported_algorithms_for_token_binding,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate) {
  if (no_authorization_code) {
    lock_ = std::make_unique<AccountReconcilor::Lock>(account_reconcilor_);
    about_signin_internals_->OnRefreshTokenReceived(
        "Missing authorization code due to OAuth outage in Dice.");
    if (!timer_) {
      timer_ = std::make_unique<base::OneShotTimer>();
      if (task_runner_) {
        timer_->SetTaskRunner(task_runner_);
      }
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

  delegate->OnDiceSigninHeaderReceived();

  for (auto it = token_fetchers_.begin(); it != token_fetchers_.end(); ++it) {
    if ((it->get()->gaia_id() == gaia_id) && (it->get()->email() == email) &&
        (it->get()->authorization_code() == authorization_code)) {
      RecordDiceFetchTokenResult(kFetchAbort);
      return;  // There is already a request in flight with the same parameters.
    }
  }

  if (base::FeatureList::IsEnabled(
          ::switches::kPreconnectAccountCapabilitiesPostSignin)) {
    // The user is signing in, which means that account fetching will shortly be
    // triggered.
    //
    // Notify identity manager. This will trigger pre-connecting the network
    // socket to the AccountCapabilities endpoint, in parallel with the LST and
    // access token requests (instead of waiting for these to complete).
    identity_manager_->PrepareForAddingNewAccount();
  }

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  base::expected<raw_ref<RegistrationTokenHelper>, TokenBindingOutcome>
      registration_token_helper_or_error =
          MaybeGetBindingRegistrationTokenHelper(
              supported_algorithms_for_token_binding);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  token_fetchers_.push_back(std::make_unique<DiceTokenFetcher>(
      gaia_id, email, authorization_code, signin_client_, account_reconcilor_,
      std::move(delegate),
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      registration_token_helper_or_error,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      this));
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
  delegate->EnableSync(
      identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id));
}

void DiceResponseHandler::ProcessDiceSignoutHeader(
    const std::vector<signin::DiceResponseParams::AccountInfo>& account_infos) {
  VLOG(1) << "Start processing Dice signout response";

  // In some cases, the primary account can only be invalidated:
  // - There is a sync primary account
  // - Browser explicit sign in is enabled, setting/clearing the primary account
  //   requires explicit user action through chrome UI.
  // - If there is a policy restriction on removing the primary account.
  bool invalidate_only_primary_account =
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync) ||
      !signin::IsImplicitBrowserSigninOrExplicitDisabled(
          identity_manager_, signin_client_->GetPrefs()) ||
      !signin_client_->IsClearPrimaryAccountAllowed(
          /*has_sync_account=*/false);

  CoreAccountId primary_account =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  bool primary_account_signed_out = false;
  auto* accounts_mutator = identity_manager_->GetAccountsMutator();
  for (const auto& account_info : account_infos) {
    CoreAccountId signed_out_account =
        identity_manager_->PickAccountIdForAccount(account_info.gaia_id,
                                                   account_info.email);
    if (invalidate_only_primary_account &&
        signed_out_account == primary_account) {
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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (token_fetchers_.empty()) {
    registration_token_helper_.reset();
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  if (!primary_account_signed_out) {
    RecordDiceResponseHeader(kSignoutSecondary);
  }
}

void DiceResponseHandler::DeleteTokenFetcher(DiceTokenFetcher* token_fetcher) {
  size_t delete_count =
      std::erase_if(token_fetchers_, [token_fetcher](const auto& current) {
        return current.get() == token_fetcher;
      });
  CHECK_EQ(delete_count, 1U);

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (token_fetchers_.empty()) {
    registration_token_helper_.reset();
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
}

void DiceResponseHandler::OnTokenExchangeSuccess(
    DiceTokenFetcher* token_fetcher,
    const std::string& refresh_token,
    bool is_under_advanced_protection
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    ,
    const std::vector<uint8_t>& wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
) {
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
      token_fetcher->delegate()->GetAccessPoint(),
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  );
  about_signin_internals_->OnRefreshTokenReceived(
      base::StringPrintf("Successful (%s)", account_id.ToString().c_str()));
  token_fetcher->delegate()->HandleTokenExchangeSuccess(account_id,
                                                        is_new_account);
  if (should_enable_sync) {
    token_fetcher->delegate()->EnableSync(
        identity_manager_->FindExtendedAccountInfoByAccountId(account_id));
  }

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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
base::expected<raw_ref<RegistrationTokenHelper>,
               DiceResponseHandler::TokenBindingOutcome>
DiceResponseHandler::MaybeGetBindingRegistrationTokenHelper(
    std::string_view supported_algorithms) {
  if (registration_token_helper_factory_.is_null()) {
    return base::unexpected(TokenBindingOutcome::kNotBoundNotSupported);
  }

  if (supported_algorithms.empty()) {
    return base::unexpected(TokenBindingOutcome::kNotBoundNotEligible);
  }

  CHECK(
      switches::IsChromeRefreshTokenBindingEnabled(signin_client_->GetPrefs()));

  // If `registration_token_helper_` doesn't exist, create it.
  if (!registration_token_helper_) {
    std::vector<uint8_t> wrapped_binding_key_to_reuse =
        GetWrappedBindingKeyToReuse(*identity_manager_);
    if (!wrapped_binding_key_to_reuse.empty()) {
      // Ignore the value of `supported_algorithms` in favor of an existing
      // binding key.
      registration_token_helper_ = registration_token_helper_factory_.Run(
          std::move(wrapped_binding_key_to_reuse));
    } else {
      registration_token_helper_ = registration_token_helper_factory_.Run(
          signin::ParseSignatureAlgorithmList(supported_algorithms));
    }
  }

  // If `registration_token_helper_` was reused, its supported algorithm
  // list may mismatch `supported_algorithms`. We ignore this because it's more
  // important to reuse the same key.
  CHECK(registration_token_helper_);
  return raw_ref<RegistrationTokenHelper>(*registration_token_helper_);
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
