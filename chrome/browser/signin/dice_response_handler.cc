// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler.h"

#include <optional>
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/binding_key_registration_token_helper.h"
#include "components/embedder_support/user_agent_utils.h"
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
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/structured_headers.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

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
const char kDiceEnableSyncHeaderAccountInfoIsPresent[] =
    "Signin.DiceEnableSyncHeaderAccountInfoIsPresent";

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
  // The token fetch was aborted. Deprecated.
  // kFetchAbort = 1,
  // The token fetch failed because Gaia responsed with an error.
  kFetchFailure = 2,
  // The token fetch failed because no response was received from Gaia.
  kFetchTimeout = 3,

  kDiceTokenFetchResultCount
};

void RecordDiceResponseHeader(DiceResponseHeader header) {
  base::UmaHistogramEnumeration(kDiceResponseHeaderHistogram, header,
                                kDiceResponseHeaderCount);
}

void RecordDiceFetchTokenResult(DiceTokenFetchResult result) {
  base::UmaHistogramEnumeration(kDiceTokenFetchResultHistogram, result,
                                kDiceTokenFetchResultCount);
}

// Creates a serialized string header value out of the input type, using
// structured headers.
template <typename T>
std::string SerializeHeaderString(const T& value) {
  return net::structured_headers::SerializeItem(
             net::structured_headers::Item(value))
      .value_or(std::string());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DiceTokenFetcher
////////////////////////////////////////////////////////////////////////////////

DiceResponseHandler::DiceTokenFetcher::DiceTokenFetcher(
    const GaiaId& gaia_id,
    const std::string& email,
    const std::string& authorization_code,
    bool mtls_token_binding,
    SigninClient* signin_client,
    AccountReconcilor* account_reconcilor,
    base::expected<raw_ref<BindingKeyRegistrationTokenHelper>,
                   TokenBindingOutcome> registration_token_helper_or_error,
    DiceSigninSession* session)
    : gaia_id_(gaia_id),
      email_(email),
      authorization_code_(authorization_code),
      mtls_token_binding_(mtls_token_binding),
      session_(session),
      signin_client_(signin_client),
      timeout_closure_(
          base::BindOnce(&DiceResponseHandler::DiceTokenFetcher::OnTimeout,
                         base::Unretained(this))),
      should_enable_sync_(false) {
  CHECK(session_);
  account_reconcilor_lock_ =
      std::make_unique<AccountReconcilor::Lock>(account_reconcilor);
  if (registration_token_helper_or_error.has_value()) {
    StartBindingKeyGeneration(registration_token_helper_or_error->get());
    // Wait until the binding key is generated before fetching a token.
    return;
  } else {
    token_binding_outcome_ = registration_token_helper_or_error.error();
  }
  StartTokenFetch();
}

DiceResponseHandler::DiceTokenFetcher::~DiceTokenFetcher() = default;

void DiceResponseHandler::DiceTokenFetcher::OnTimeout() {
  RecordDiceFetchTokenResult(kFetchTimeout);
  gaia_auth_fetcher_.reset();
  timeout_closure_.Cancel();
  session_->OnTokenExchangeFailure(
      this, GoogleServiceAuthError::CreateRequestCanceled());
  // |this| may be deleted at this point.
}

void DiceResponseHandler::DiceTokenFetcher::OnClientOAuthSuccess(
    const GaiaAuthConsumer::ClientOAuthResult& result) {
  RecordDiceFetchTokenResult(kFetchSuccess);
  gaia_auth_fetcher_.reset();
  timeout_closure_.Cancel();
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
  session_->OnTokenExchangeSuccess(this, result.refresh_token,
                                   result.is_under_advanced_protection,
                                   wrapped_binding_key_);
  // |this| may be deleted at this point.
}

void DiceResponseHandler::DiceTokenFetcher::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  RecordDiceFetchTokenResult(kFetchFailure);
  gaia_auth_fetcher_.reset();
  timeout_closure_.Cancel();
  session_->OnTokenExchangeFailure(this, error);
  // |this| may be deleted at this point.
}

void DiceResponseHandler::DiceTokenFetcher::StartTokenFetch() {
  VLOG(1) << "Start fetching token for account: " << email_;
  gaia_auth_fetcher_ =
      signin_client_->CreateGaiaAuthFetcher(this, gaia::GaiaSource::kChrome);
  blink::UserAgentMetadata ua_metadata =
      embedder_support::GetUserAgentMetadata();
  // `binding_registration_token_` is empty if the binding key was not
  // generated.
  gaia_auth_fetcher_->StartAuthCodeForOAuth2TokenExchange(
      authorization_code_, binding_registration_token_,
      {.full_version_list = ua_metadata.SerializeBrandFullVersionList(),
       .platform = SerializeHeaderString(ua_metadata.platform)},
      mtls_token_binding_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, timeout_closure_.callback(),
      base::Seconds(kDiceTokenFetchTimeoutSeconds));
}

void DiceResponseHandler::DiceTokenFetcher::StartBindingKeyGeneration(
    BindingKeyRegistrationTokenHelper& registration_token_helper) {
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
    std::optional<BindingKeyRegistrationTokenHelper::Result> result) {
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

// DiceSigninSession

DiceResponseHandler::DiceSigninSession::DiceSigninSession(
    DiceResponseHandler* handler,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate,
    signin::DiceResponseParams::SigninInfo signin_info)
    : handler_(handler),
      delegate_(std::move(delegate)),
      signin_info_(std::move(signin_info)) {
  CHECK(handler_);
  CHECK(delegate_);
}

DiceResponseHandler::DiceSigninSession::~DiceSigninSession() = default;

void DiceResponseHandler::DiceSigninSession::StartTokenFetch() {
  const auto* initiator = signin_info_.GetInitiator();
  CHECK(initiator);

  // The user is signing in, which means that account fetching will shortly be
  // triggered.
  //
  // Notify identity manager. This will trigger pre-connecting the network
  // socket to the AccountCapabilities endpoint, in parallel with the LST and
  // access token requests (instead of waiting for these to complete).
  handler_->identity_manager_->PrepareForAddingNewAccount();

  base::expected<raw_ref<BindingKeyRegistrationTokenHelper>,
                 TokenBindingOutcome>
      registration_token_helper_or_error =
          handler_->MaybeGetBindingRegistrationTokenHelper(
              initiator->supported_algorithms_for_token_binding);

  token_fetcher_ = std::make_unique<DiceTokenFetcher>(
      initiator->account_info.gaia_id, initiator->account_info.email,
      initiator->authorization_code, initiator->mtls_token_binding,
      handler_->signin_client_, handler_->account_reconcilor_,
      registration_token_helper_or_error, this);
}

void DiceResponseHandler::DiceSigninSession::OnTokenExchangeSuccess(
    DiceTokenFetcher* fetcher,
    const std::string& refresh_token,
    bool is_under_advanced_protection,
    const std::vector<uint8_t>& wrapped_binding_key) {
  const std::string& email = fetcher->email();
  const GaiaId& gaia_id = fetcher->gaia_id();
  // Log is consumed by E2E tests. Please CC potassium-engprod@google.com if you
  // have to change this log.
  VLOG(1) << "[Dice] OAuth success for email " << email;
  CoreAccountId account_id =
      handler_->identity_manager_->PickAccountIdForAccount(gaia_id, email);
  bool is_new_account =
      !handler_->identity_manager_->HasAccountWithRefreshToken(account_id);

  handler_->identity_manager_->GetAccountsMutator()->AddOrUpdateAccount(
      gaia_id, email, refresh_token, is_under_advanced_protection,
      delegate_->GetAccessPoint(),
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin,
      signin::TokenBindingInfo(wrapped_binding_key,
                               fetcher->mtls_token_binding()));

  handler_->about_signin_internals_->OnRefreshTokenReceived(
      base::StringPrintf("Successful (%s)", account_id.ToString().c_str()));

  delegate_->HandleTokenExchangeSuccess(account_id, is_new_account);

  if (fetcher->should_enable_sync()) {
    delegate_->CompleteChromeSignInAfterGaiaSignin(
        handler_->identity_manager_->FindExtendedAccountInfoByAccountId(
            account_id));
  }

  CHECK_EQ(token_fetcher_.get(), fetcher);
  token_fetcher_.reset();
  handler_->DeleteSession(this);
}

void DiceResponseHandler::DiceSigninSession::OnTokenExchangeFailure(
    DiceTokenFetcher* fetcher,
    const GoogleServiceAuthError& error) {
  const std::string& email = fetcher->email();
  CoreAccountId account_id =
      handler_->identity_manager_->PickAccountIdForAccount(fetcher->gaia_id(),
                                                           email);
  handler_->about_signin_internals_->OnRefreshTokenReceived(
      base::StringPrintf("Failure (%s)", account_id.ToString().c_str()));

  delegate_->HandleTokenExchangeFailure(email, error);
  VLOG(1) << "Initiator fetch failed. Aborting session.";

  CHECK_EQ(token_fetcher_.get(), fetcher);
  token_fetcher_.reset();
  handler_->DeleteSession(this);
}

bool DiceResponseHandler::DiceSigninSession::IsFetchingForAccount(
    const CoreAccountId& account_id) const {
  if (!token_fetcher_) {
    return false;
  }
  return handler_->identity_manager_->PickAccountIdForAccount(
             token_fetcher_->gaia_id(), token_fetcher_->email()) == account_id;
}

bool DiceResponseHandler::DiceSigninSession::CancelFetchForAccount(
    const CoreAccountId& account_id) {
  if (!token_fetcher_) {
    return false;
  }
  CoreAccountId fetcher_account_id =
      handler_->identity_manager_->PickAccountIdForAccount(
          token_fetcher_->gaia_id(), token_fetcher_->email());
  if (fetcher_account_id == account_id) {
    token_fetcher_.reset();
    handler_->DeleteSession(this);
    return true;
  }
  return false;
}

bool DiceResponseHandler::DiceSigninSession::MarkEnableSyncIfFetching(
    const GaiaId& gaia_id,
    const std::string& email) {
  if (token_fetcher_ && token_fetcher_->gaia_id() == gaia_id) {
    DCHECK(gaia::AreEmailsSame(token_fetcher_->email(), email));
    token_fetcher_->set_should_enable_sync(true);
    return true;
  }
  return false;
}

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

DiceResponseHandler::~DiceResponseHandler() = default;

void DiceResponseHandler::ProcessDiceHeader(
    signin::DiceResponseParams dice_params,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate) {
  if (!dice_params.IsValid()) {
    return;
  }

  CHECK(delegate);
  switch (dice_params.user_intention()) {
    case signin::DiceAction::SIGNIN: {
      signin::DiceResponseParams::SigninInfo* signin_info =
          dice_params.signin_info();
      CHECK(signin_info);
      ProcessDiceSigninHeader(std::move(*signin_info), std::move(delegate));
      return;
    }
    case signin::DiceAction::ENABLE_SYNC: {
      const signin::DiceResponseParams::EnableSyncInfo* enable_sync_info =
          dice_params.enable_sync_info();
      CHECK(enable_sync_info);
      const signin::DiceResponseParams::AccountInfo& info =
          enable_sync_info->account_info;
      ProcessEnableSyncHeader(info.gaia_id, info.email, std::move(delegate));
      return;
    }
    case signin::DiceAction::SIGNOUT: {
      const signin::DiceResponseParams::SignoutInfo* signout_info =
          dice_params.signout_info();
      CHECK(signout_info);
      DCHECK_GT(signout_info->account_infos.size(), 0u);
      ProcessDiceSignoutHeader(signout_info->account_infos);
      return;
    }
    case signin::DiceAction::NONE:
      NOTREACHED() << "Invalid Dice response parameters.";
  }
  NOTREACHED();
}

void DiceResponseHandler::ProcessDiceSigninHeader(
    signin::DiceResponseParams::SigninInfo signin_info,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate) {
  VLOG(1) << "Start processing Dice signin response";
  RecordDiceResponseHeader(kSignin);

  delegate->OnDiceSigninHeaderReceived();

  const signin::DiceResponseParams::SigninInfo::SigninAccount* initiator =
      signin_info.GetInitiator();
  CHECK(initiator);

  if (initiator->no_authorization_code) {
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

  auto session = std::make_unique<DiceSigninSession>(this, std::move(delegate),
                                                     std::move(signin_info));
  sessions_.push_back(std::move(session));
  sessions_.back()->StartTokenFetch();
}

size_t DiceResponseHandler::GetPendingDiceTokenFetchersCountForTesting() const {
  size_t count = 0;
  for (const auto& session : sessions_) {
    count += session->GetPendingDiceTokenFetchersCountForTesting();  // IN-TEST
  }
  return count;
}

void DiceResponseHandler::OnTimeoutUnlockReconcilor() {
  lock_.reset();
}

void DiceResponseHandler::SetTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
}

void DiceResponseHandler::
    SetRegistrationTokenHelperFactoryForTesting(  // IN-TEST
        RegistrationTokenHelperFactory factory) {
  CHECK(
      switches::IsChromeRefreshTokenBindingEnabled(signin_client_->GetPrefs()));
  registration_token_helper_factory_ = std::move(factory);
}

void DiceResponseHandler::ProcessEnableSyncHeader(
    const GaiaId& gaia_id,
    const std::string& email,
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate) {
  VLOG(1) << "Start processing Dice enable sync response";
  RecordDiceResponseHeader(kEnableSync);
  for (const auto& session : sessions_) {
    if (session->MarkEnableSyncIfFetching(gaia_id, email)) {
      return;  // There is already a request in flight with the same parameters.
    }
  }
  AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id);
  base::UmaHistogramBoolean(kDiceEnableSyncHeaderAccountInfoIsPresent,
                            !account_info.IsEmpty());
  if (account_info.IsEmpty()) {
    return;
  }
  delegate->CompleteChromeSignInAfterGaiaSignin(account_info);
}

void DiceResponseHandler::ProcessDiceSignoutHeader(
    const std::vector<signin::DiceResponseParams::AccountInfo>& account_infos) {
  VLOG(1) << "Start processing Dice signout response";

  CoreAccountId primary_account =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  bool primary_account_signed_out = false;
  auto* accounts_mutator = identity_manager_->GetAccountsMutator();
  for (const auto& account_info : account_infos) {
    CoreAccountId signed_out_account =
        identity_manager_->PickAccountIdForAccount(account_info.gaia_id,
                                                   account_info.email);

    // The primary account can only be invalidated. Removing the primary accouny
    // requires explicit user action from the Chrome UI.
    if (!primary_account.empty() && signed_out_account == primary_account) {
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
    // Note: This might lead to the session being deleted synchronously inside
    // `CancelFetchForAccount()`, invalidating the iterator. This is safe as we
    // do not use the iterator after this call.
    auto it =
        std::find_if(sessions_.begin(), sessions_.end(),
                     [&signed_out_account](const auto& session) {
                       return session->IsFetchingForAccount(signed_out_account);
                     });
    if (it != sessions_.end()) {
      (*it)->CancelFetchForAccount(signed_out_account);
    }
  }

  if (sessions_.empty()) {
    registration_token_helper_.reset();
  }

  if (!primary_account_signed_out) {
    RecordDiceResponseHeader(kSignoutSecondary);
  }
}

void DiceResponseHandler::DeleteSession(DiceSigninSession* session) {
  size_t delete_count = std::erase_if(
      sessions_,
      [session](const auto& current) { return current.get() == session; });
  CHECK_EQ(delete_count, 1U);

  if (sessions_.empty()) {
    registration_token_helper_.reset();
  }
}

base::expected<raw_ref<BindingKeyRegistrationTokenHelper>,
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

  if (!identity_manager_->AreRefreshTokensLoaded()) {
    // We cannot determine the right binding key to reuse if tokens haven't been
    // loaded yet. This is a very unlikely event, so prefer to not bind at all
    // instead of binding to an incorrect key.
    // TODO(crbug.com/428138073): properly wait for the tokens to be loaded if
    // the number of affected users is high.
    return base::unexpected(
        TokenBindingOutcome::kNotBoundRefreshTokensNotLoaded);
  }

  // If `registration_token_helper_` doesn't exist, create it.
  if (!registration_token_helper_) {
    std::vector<uint8_t> wrapped_binding_key_to_reuse =
        identity_manager_->GetWrappedBindingKey();
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
  return raw_ref<BindingKeyRegistrationTokenHelper>(
      *registration_token_helper_);
}
