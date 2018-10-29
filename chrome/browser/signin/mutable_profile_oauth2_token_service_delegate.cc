// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/mutable_profile_oauth2_token_service_delegate.h"

#include <stddef.h>

#include <map>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/webdata/token_web_data.h"
#include "components/webdata/common/web_data_service_base.h"
#include "content/public/browser/network_service_instance.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const char kAccountIdPrefix[] = "AccountId-";
const size_t kAccountIdPrefixLength = 10;

// Used to record token state transitions in histograms.
// Do not change existing values, new values can only be added at the end.
enum class TokenStateTransition {
  // Update events.
  kNoneToInvalid = 0,
  kNoneToRegular,
  kInvalidToRegular,
  kRegularToInvalid,
  kRegularToRegular,

  // Revocation events.
  kInvalidToNone,
  kRegularToNone,

  // Load events.
  kLoadRegular,
  kLoadInvalid,
  kLoadInvalidNoTokenForPrimaryAccount,

  kCount
};

// Enum for the Signin.LoadTokenFromDB histogram.
// Do not modify, or add or delete other than directly before
// NUM_LOAD_TOKEN_FROM_DB_STATUS.
enum class LoadTokenFromDBStatus {
  // Token was loaded.
  TOKEN_LOADED = 0,
  // Token was revoked as part of Dice migration.
  TOKEN_REVOKED_DICE_MIGRATION = 1,
  // Token was revoked because it is a secondary account and account consistency
  // is disabled.
  TOKEN_REVOKED_SECONDARY_ACCOUNT = 2,
  // Token was revoked on load due to cookie settings.
  TOKEN_REVOKED_ON_LOAD = 3,

  NUM_LOAD_TOKEN_FROM_DB_STATUS
};

// Used to record events related to token revocation requests in histograms.
// Do not change existing values, new values can only be added at the end.
enum class TokenRevocationRequestProgress {
  // The request was created.
  kRequestCreated = 0,
  // The request was sent over the network.
  kRequestStarted = 1,
  // The network request completed with a failure.
  kRequestFailed = 2,
  // The network request completed with a success.
  kRequestSucceeded = 3,

  kMaxValue = kRequestSucceeded
};

// Adds a sample to the TokenStateTransition histogram. Encapsuled in a function
// to reduce executable size, because histogram macros may generate a lot of
// code.
void RecordTokenStateTransition(TokenStateTransition transition) {
  UMA_HISTOGRAM_ENUMERATION("Signin.TokenStateTransition", transition,
                            TokenStateTransition::kCount);
}

// Adds a sample to the TokenRevocationRequestProgress histogram. Encapsuled in
// a function to reduce executable size, because histogram macros may generate a
// lot of code.
void RecordRefreshTokenRevocationRequestEvent(
    TokenRevocationRequestProgress event) {
  UMA_HISTOGRAM_ENUMERATION("Signin.RefreshTokenRevocationRequestProgress",
                            event);
}

// Record metrics when a token was updated.
void RecordTokenChanged(const std::string& existing_token,
                        const std::string& new_token) {
  DCHECK_NE(existing_token, new_token);
  DCHECK(!new_token.empty());
  TokenStateTransition transition = TokenStateTransition::kCount;
  if (existing_token.empty()) {
    transition = (new_token == OAuth2TokenServiceDelegate::kInvalidRefreshToken)
                     ? TokenStateTransition::kNoneToInvalid
                     : TokenStateTransition::kNoneToRegular;
  } else if (existing_token ==
             OAuth2TokenServiceDelegate::kInvalidRefreshToken) {
    transition = TokenStateTransition::kInvalidToRegular;
  } else {
    // Existing token is a regular token.
    transition = (new_token == OAuth2TokenServiceDelegate::kInvalidRefreshToken)
                     ? TokenStateTransition::kRegularToInvalid
                     : TokenStateTransition::kRegularToRegular;
  }
  DCHECK_NE(TokenStateTransition::kCount, transition);
  RecordTokenStateTransition(transition);
}

// Record metrics when a token was loaded.
void RecordTokenLoaded(const std::string& token) {
  RecordTokenStateTransition(
      (token == OAuth2TokenServiceDelegate::kInvalidRefreshToken)
          ? TokenStateTransition::kLoadInvalid
          : TokenStateTransition::kLoadRegular);
}

// Record metrics when a token was revoked.
void RecordTokenRevoked(const std::string& token) {
  RecordTokenStateTransition(
      (token == OAuth2TokenServiceDelegate::kInvalidRefreshToken)
          ? TokenStateTransition::kInvalidToNone
          : TokenStateTransition::kRegularToNone);
}

std::string ApplyAccountIdPrefix(const std::string& account_id) {
  return kAccountIdPrefix + account_id;
}

bool IsLegacyRefreshTokenId(const std::string& service_id) {
  return service_id == GaiaConstants::kGaiaOAuth2LoginRefreshToken;
}

bool IsLegacyServiceId(const std::string& account_id) {
  return account_id.compare(0u, kAccountIdPrefixLength, kAccountIdPrefix) != 0;
}

std::string RemoveAccountIdPrefix(const std::string& prefixed_account_id) {
  return prefixed_account_id.substr(kAccountIdPrefixLength);
}

OAuth2TokenServiceDelegate::LoadCredentialsState
LoadCredentialsStateFromTokenResult(TokenServiceTable::Result token_result) {
  switch (token_result) {
    case TokenServiceTable::TOKEN_DB_RESULT_SQL_INVALID_STATEMENT:
    case TokenServiceTable::TOKEN_DB_RESULT_BAD_ENTRY:
      return OAuth2TokenServiceDelegate::
          LOAD_CREDENTIALS_FINISHED_WITH_DB_ERRORS;
    case TokenServiceTable::TOKEN_DB_RESULT_DECRYPT_ERROR:
      return OAuth2TokenServiceDelegate::
          LOAD_CREDENTIALS_FINISHED_WITH_DECRYPT_ERRORS;
    case TokenServiceTable::TOKEN_DB_RESULT_SUCCESS:
      return OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS;
  }
  NOTREACHED();
  return OAuth2TokenServiceDelegate::
      LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS;
}

// Returns whether the token service should be migrated to Dice.
// Migration can happen if the following conditions are met:
// - Token service Dice migration is not already done,
// - AccountTrackerService migration is done,
// - All accounts in the AccountTrackerService are valid,
// - Account consistency is DiceMigration or greater.
// TODO(droger): Remove this code once Dice is fully enabled.
bool ShouldMigrateToDice(signin::AccountConsistencyMethod account_consistency,
                         PrefService* prefs,
                         AccountTrackerService* account_tracker,
                         const std::map<std::string, std::string>& db_tokens) {
  AccountTrackerService::AccountIdMigrationState migration_state =
      account_tracker->GetMigrationState();
  if ((account_consistency == signin::AccountConsistencyMethod::kMirror) ||
      !signin::DiceMethodGreaterOrEqual(
          account_consistency,
          signin::AccountConsistencyMethod::kDiceMigration) ||
      (migration_state != AccountTrackerService::MIGRATION_DONE) ||
      prefs->GetBoolean(prefs::kTokenServiceDiceCompatible)) {
    return false;
  }

  // Do not migrate if some accounts are not valid.
  for (auto iter = db_tokens.begin(); iter != db_tokens.end(); ++iter) {
    const std::string& prefixed_account_id = iter->first;
    std::string account_id = RemoveAccountIdPrefix(prefixed_account_id);
    AccountInfo account_info = account_tracker->GetAccountInfo(account_id);
    if (!account_info.IsValid()) {
      return false;
    }
  }
  return true;
}

}  // namespace

// This class sends a request to GAIA to revoke the given refresh token from
// the server.  This is a best effort attempt only.  This class deletes itself
// when done successfully or otherwise.
class MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken
    : public GaiaAuthConsumer {
 public:
  RevokeServerRefreshToken(
      MutableProfileOAuth2TokenServiceDelegate* token_service_delegate,
      SigninClient* client,
      const std::string& refresh_token);
  ~RevokeServerRefreshToken() override;

  // Starts the network request.
  static void Start(base::WeakPtr<RevokeServerRefreshToken> rsrt,
                    const std::string& refresh_token);

 private:
  // GaiaAuthConsumer overrides:
  void OnOAuth2RevokeTokenCompleted(
      GaiaAuthConsumer::TokenRevocationStatus status) override;

  MutableProfileOAuth2TokenServiceDelegate* token_service_delegate_;
  GaiaAuthFetcher fetcher_;
  base::WeakPtrFactory<RevokeServerRefreshToken> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RevokeServerRefreshToken);
};

MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    RevokeServerRefreshToken(
        MutableProfileOAuth2TokenServiceDelegate* token_service_delegate,
        SigninClient* client,
        const std::string& refresh_token)
    : token_service_delegate_(token_service_delegate),
      fetcher_(this,
               GaiaConstants::kChromeSource,
               token_service_delegate_->GetURLLoaderFactory()),
      weak_ptr_factory_(this) {
  RecordRefreshTokenRevocationRequestEvent(
      TokenRevocationRequestProgress::kRequestCreated);
  client->DelayNetworkCall(
      base::Bind(&MutableProfileOAuth2TokenServiceDelegate::
                     RevokeServerRefreshToken::Start,
                 weak_ptr_factory_.GetWeakPtr(), refresh_token));
}

// static
void MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::Start(
    base::WeakPtr<RevokeServerRefreshToken> rsrt,
    const std::string& refresh_token) {
  if (!rsrt)
    return;
  RecordRefreshTokenRevocationRequestEvent(
      TokenRevocationRequestProgress::kRequestStarted);
  rsrt->fetcher_.StartRevokeOAuth2Token(refresh_token);
}

MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    ~RevokeServerRefreshToken() {
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeServerRefreshToken::
    OnOAuth2RevokeTokenCompleted(
        GaiaAuthConsumer::TokenRevocationStatus status) {
  RecordRefreshTokenRevocationRequestEvent(
      (status == GaiaAuthConsumer::TokenRevocationStatus::kSuccess)
          ? TokenRevocationRequestProgress::kRequestSucceeded
          : TokenRevocationRequestProgress::kRequestFailed);
  UMA_HISTOGRAM_ENUMERATION("Signin.RefreshTokenRevocationStatus", status);
  // |this| pointer will be deleted when removed from the vector, so don't
  // access any members after call to erase().
  token_service_delegate_->server_revokes_.erase(std::find_if(
      token_service_delegate_->server_revokes_.begin(),
      token_service_delegate_->server_revokes_.end(),
      [this](const std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate::
                                       RevokeServerRefreshToken>& item) {
        return item.get() == this;
      }));
}

MutableProfileOAuth2TokenServiceDelegate::AccountStatus::AccountStatus(
    SigninErrorController* signin_error_controller,
    const std::string& account_id,
    const std::string& refresh_token)
    : signin_error_controller_(signin_error_controller),
      account_id_(account_id),
      refresh_token_(refresh_token),
      last_auth_error_(GoogleServiceAuthError::NONE) {
  DCHECK(signin_error_controller_);
  DCHECK(!account_id_.empty());
  DCHECK(!refresh_token.empty());
}

void MutableProfileOAuth2TokenServiceDelegate::AccountStatus::Initialize() {
  signin_error_controller_->AddProvider(this);
}

MutableProfileOAuth2TokenServiceDelegate::AccountStatus::~AccountStatus() {
  signin_error_controller_->RemoveProvider(this);
}

void MutableProfileOAuth2TokenServiceDelegate::AccountStatus::SetLastAuthError(
    const GoogleServiceAuthError& error) {
  last_auth_error_ = error;
  signin_error_controller_->AuthStatusChanged();
}

std::string
MutableProfileOAuth2TokenServiceDelegate::AccountStatus::GetAccountId() const {
  return account_id_;
}

GoogleServiceAuthError
MutableProfileOAuth2TokenServiceDelegate::AccountStatus::GetAuthStatus() const {
  return last_auth_error_;
}

MutableProfileOAuth2TokenServiceDelegate::
    MutableProfileOAuth2TokenServiceDelegate(
        SigninClient* client,
        SigninErrorController* signin_error_controller,
        AccountTrackerService* account_tracker_service,
        scoped_refptr<TokenWebData> token_web_data,
        signin::AccountConsistencyMethod account_consistency,
        bool revoke_all_tokens_on_load,
        bool can_revoke_credentials)
    : web_data_service_request_(0),
      backoff_entry_(&backoff_policy_),
      backoff_error_(GoogleServiceAuthError::NONE),
      client_(client),
      signin_error_controller_(signin_error_controller),
      account_tracker_service_(account_tracker_service),
      token_web_data_(token_web_data),
      account_consistency_(account_consistency),
      revoke_all_tokens_on_load_(revoke_all_tokens_on_load),
      can_revoke_credentials_(can_revoke_credentials) {
  VLOG(1) << "MutablePO2TS::MutablePO2TS";
  DCHECK(client);
  DCHECK(signin_error_controller);
  DCHECK(account_tracker_service_);
  // It's okay to fill the backoff policy after being used in construction.
  backoff_policy_.num_errors_to_ignore = 0;
  backoff_policy_.initial_delay_ms = 1000;
  backoff_policy_.multiply_factor = 2.0;
  backoff_policy_.jitter_factor = 0.2;
  backoff_policy_.maximum_backoff_ms = 15 * 60 * 1000;
  backoff_policy_.entry_lifetime_ms = -1;
  backoff_policy_.always_use_initial_delay = false;
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
}

MutableProfileOAuth2TokenServiceDelegate::
    ~MutableProfileOAuth2TokenServiceDelegate() {
  VLOG(1) << "MutablePO2TS::~MutablePO2TS";
  DCHECK(server_revokes_.empty());
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

// static
void MutableProfileOAuth2TokenServiceDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kTokenServiceDiceCompatible, false);
}

OAuth2AccessTokenFetcher*
MutableProfileOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  ValidateAccountId(account_id);
  // check whether the account has persistent error.
  if (refresh_tokens_[account_id]->GetAuthStatus().IsPersistentError()) {
    VLOG(1) << "Request for token has been rejected due to persistent error #"
            << refresh_tokens_[account_id]->GetAuthStatus().state();
    return new OAuth2AccessTokenFetcherImmediateError(
        consumer, refresh_tokens_[account_id]->GetAuthStatus());
  }
  if (backoff_entry_.ShouldRejectRequest()) {
    VLOG(1) << "Request for token has been rejected due to backoff rules from"
            << " previous error #" << backoff_error_.state();
    return new OAuth2AccessTokenFetcherImmediateError(consumer, backoff_error_);
  }
  std::string refresh_token = GetRefreshToken(account_id);
  DCHECK(!refresh_token.empty());
  return new OAuth2AccessTokenFetcherImpl(consumer, url_loader_factory,
                                          refresh_token);
}

GoogleServiceAuthError MutableProfileOAuth2TokenServiceDelegate::GetAuthError(
    const std::string& account_id) const {
  auto it = refresh_tokens_.find(account_id);
  return (it == refresh_tokens_.end()) ? GoogleServiceAuthError::AuthErrorNone()
                                       : it->second->GetAuthStatus();
}

void MutableProfileOAuth2TokenServiceDelegate::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "MutablePO2TS::UpdateAuthError. Error: " << error.state()
          << " account_id=" << account_id;
  backoff_entry_.InformOfRequest(!error.IsTransientError());
  ValidateAccountId(account_id);

  // Do not report connection errors as these are not actually auth errors.
  // We also want to avoid masking a "real" auth error just because we
  // subsequently get a transient network error.  We do keep it around though
  // to report for future requests being denied for "backoff" reasons.
  if (error.IsTransientError()) {
    backoff_error_ = error;
    return;
  }

  if (refresh_tokens_.count(account_id) == 0) {
    // This could happen if the preferences have been corrupted (see
    // http://crbug.com/321370). In a Debug build that would be a bug, but in a
    // Release build we want to deal with it gracefully.
    NOTREACHED();
    return;
  }

  AccountStatus* status = refresh_tokens_[account_id].get();
  if (error != status->GetAuthStatus()) {
    status->SetLastAuthError(error);
    FireAuthErrorChanged(account_id, error);
  }
}

std::string MutableProfileOAuth2TokenServiceDelegate::GetTokenForMultilogin(
    const std::string& account_id) const {
  auto iter = refresh_tokens_.find(account_id);
  if (iter == refresh_tokens_.end() ||
      iter->second->GetAuthStatus() !=
          GoogleServiceAuthError::AuthErrorNone()) {
    return std::string();
  }
  const std::string& refresh_token = iter->second->refresh_token();
  DCHECK(!refresh_token.empty());
  return refresh_token;
}

bool MutableProfileOAuth2TokenServiceDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  VLOG(1) << "MutablePO2TS::RefreshTokenIsAvailable";
  return !GetRefreshToken(account_id).empty();
}

std::string MutableProfileOAuth2TokenServiceDelegate::GetRefreshToken(
    const std::string& account_id) const {
  auto iter = refresh_tokens_.find(account_id);
  if (iter != refresh_tokens_.end()) {
    const std::string refresh_token = iter->second->refresh_token();
    DCHECK(!refresh_token.empty());
    return refresh_token;
  }
  return std::string();
}

std::string MutableProfileOAuth2TokenServiceDelegate::GetRefreshTokenForTest(
    const std::string& account_id) const {
  return GetRefreshToken(account_id);
}

std::vector<std::string>
MutableProfileOAuth2TokenServiceDelegate::GetAccounts() {
  std::vector<std::string> account_ids;
  for (auto& token : refresh_tokens_) {
    account_ids.push_back(token.first);
  }
  return account_ids;
}

scoped_refptr<network::SharedURLLoaderFactory>
MutableProfileOAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return client_->GetURLLoaderFactory();
}

void MutableProfileOAuth2TokenServiceDelegate::InvalidateTokenForMultilogin(
    const std::string& failed_account) {
  UpdateAuthError(
      failed_account,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

void MutableProfileOAuth2TokenServiceDelegate::LoadCredentials(
    const std::string& primary_account_id) {
  if (load_credentials_state() == LOAD_CREDENTIALS_IN_PROGRESS) {
    VLOG(1) << "Load credentials operation already in progress";
    return;
  }

  set_load_credentials_state(LOAD_CREDENTIALS_IN_PROGRESS);

#if defined(OS_CHROMEOS)
  // ChromeOS OOBE loads credentials without a primary account and expects this
  // to be a no-op. See htttp://crbug.com/891818
  if (primary_account_id.empty()) {
    set_load_credentials_state(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
    FinishLoadingCredentials();
    return;
  }
#endif

  if (!primary_account_id.empty())
    ValidateAccountId(primary_account_id);
  DCHECK(loading_primary_account_id_.empty());
  DCHECK_EQ(0, web_data_service_request_);

  refresh_tokens_.clear();

  if (!token_web_data_) {
    // This case only exists in unit tests that do not care about loading
    // credentials.
    set_load_credentials_state(LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS);
    FinishLoadingCredentials();
    return;
  }

  // If |account_id| is an email address, then canonicalize it. This is needed
  // to support legacy account IDs, and will not be needed after switching to
  // gaia IDs.
  if (primary_account_id.find('@') != std::string::npos) {
    loading_primary_account_id_ = gaia::CanonicalizeEmail(primary_account_id);
  } else {
    loading_primary_account_id_ = primary_account_id;
  }

  web_data_service_request_ = token_web_data_->GetAllTokens(this);
}

void MutableProfileOAuth2TokenServiceDelegate::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  VLOG(1) << "MutablePO2TS::OnWebDataServiceRequestDone. Result type: "
          << (result.get() == nullptr ? -1
                                      : static_cast<int>(result->GetType()));

  DCHECK_EQ(web_data_service_request_, handle);
  web_data_service_request_ = 0;

  if (result) {
    DCHECK(result->GetType() == TOKEN_RESULT);
    const WDResult<TokenResult>* token_result =
        static_cast<const WDResult<TokenResult>*>(result.get());
    LoadAllCredentialsIntoMemory(token_result->GetValue().tokens);
    set_load_credentials_state(LoadCredentialsStateFromTokenResult(
        token_result->GetValue().db_result));
  } else {
    set_load_credentials_state(LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS);
  }

  // Make sure that we have an entry for |loading_primary_account_id_| in the
  // map.  The entry could be missing if there is a corruption in the token DB
  // while this profile is connected to an account.
  if (!loading_primary_account_id_.empty() &&
      refresh_tokens_.count(loading_primary_account_id_) == 0) {
    if (load_credentials_state() == LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS) {
      set_load_credentials_state(
          LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT);
    }
    AddAccountStatus(loading_primary_account_id_, kInvalidRefreshToken,
                     GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                         GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                             CREDENTIALS_MISSING));
    RecordTokenStateTransition(
        TokenStateTransition::kLoadInvalidNoTokenForPrimaryAccount);
    FireRefreshTokenAvailable(loading_primary_account_id_);
  }

#ifndef NDEBUG
  for (auto& token : refresh_tokens_) {
    DCHECK(RefreshTokenIsAvailable(token.first))
        << "Missing token for " << token.first;
  }
#endif

  loading_primary_account_id_.clear();
  FinishLoadingCredentials();
}

void MutableProfileOAuth2TokenServiceDelegate::LoadAllCredentialsIntoMemory(
    const std::map<std::string, std::string>& db_tokens) {
  std::string old_login_token;
  bool migrate_to_dice =
      ShouldMigrateToDice(account_consistency_, client_->GetPrefs(),
                          account_tracker_service_, db_tokens);

  {
    ScopedBatchChange batch(this);

    VLOG(1) << "MutablePO2TS::LoadAllCredentialsIntoMemory; "
            << db_tokens.size() << " Credential(s).";
    AccountTrackerService::AccountIdMigrationState migration_state =
        account_tracker_service_->GetMigrationState();
    for (auto iter = db_tokens.begin(); iter != db_tokens.end(); ++iter) {
      std::string prefixed_account_id = iter->first;
      std::string refresh_token = iter->second;

      if (IsLegacyRefreshTokenId(prefixed_account_id) && !refresh_token.empty())
        old_login_token = refresh_token;

      if (IsLegacyServiceId(prefixed_account_id)) {
        if (token_web_data_) {
          VLOG(1) << "MutablePO2TS remove legacy refresh token for account id "
                  << prefixed_account_id;
          token_web_data_->RemoveTokenForService(prefixed_account_id);
        }
      } else {
        DCHECK(!refresh_token.empty());
        std::string account_id = RemoveAccountIdPrefix(prefixed_account_id);

        switch (migration_state) {
          case AccountTrackerService::MIGRATION_IN_PROGRESS: {
            // Migrate to gaia-ids.
            AccountInfo account_info =
                account_tracker_service_->FindAccountInfoByEmail(account_id);
            // |account_info.gaia| could be empty if |account_id| is already
            // gaia id. This could happen if the chrome was closed in the middle
            // of migration.
            if (!account_info.gaia.empty()) {
              ClearPersistedCredentials(account_id);
              PersistCredentials(account_info.gaia, refresh_token);
              account_id = account_info.gaia;
            }

            // Skip duplicate accounts, this could happen if migration was
            // crashed in the middle.
            if (refresh_tokens_.count(account_id) != 0)
              continue;
            break;
          }
          case AccountTrackerService::MIGRATION_NOT_STARTED:
            // If the account_id is an email address, then canonicalize it. This
            // is to support legacy account_ids, and will not be needed after
            // switching to gaia-ids.
            if (account_id.find('@') != std::string::npos) {
              // If the canonical account id is not the same as the loaded
              // account id, make sure not to overwrite a refresh token from
              // a canonical version.  If no canonical version was loaded, then
              // re-persist this refresh token with the canonical account id.
              std::string canon_account_id =
                  gaia::CanonicalizeEmail(account_id);
              if (canon_account_id != account_id) {
                ClearPersistedCredentials(account_id);
                if (db_tokens.count(ApplyAccountIdPrefix(canon_account_id)) ==
                    0)
                  PersistCredentials(canon_account_id, refresh_token);
              }
              account_id = canon_account_id;
            }
            break;
          case AccountTrackerService::MIGRATION_DONE:
            DCHECK_EQ(std::string::npos, account_id.find('@'));
            break;
          case AccountTrackerService::NUM_MIGRATION_STATES:
            NOTREACHED();
            break;
        }

        // Only load secondary accounts when account consistency is enabled.
        bool load_account =
            (account_id == loading_primary_account_id_) ||
            (account_consistency_ ==
             signin::AccountConsistencyMethod::kMirror) ||
            signin::DiceMethodGreaterOrEqual(
                account_consistency_,
                signin::AccountConsistencyMethod::kDiceMigration);
        LoadTokenFromDBStatus load_token_status =
            load_account
                ? LoadTokenFromDBStatus::TOKEN_LOADED
                : LoadTokenFromDBStatus::TOKEN_REVOKED_SECONDARY_ACCOUNT;

        if (migrate_to_dice) {
          // Revoke old hosted domain accounts as part of Dice migration.
          AccountInfo account_info =
              account_tracker_service_->GetAccountInfo(account_id);
          DCHECK(account_info.IsValid());
          if (account_info.hosted_domain !=
              AccountTrackerService::kNoHostedDomainFound) {
            load_account = false;
            load_token_status =
                LoadTokenFromDBStatus::TOKEN_REVOKED_DICE_MIGRATION;
          }
        }

        if (load_account && revoke_all_tokens_on_load_) {
          if (account_id == loading_primary_account_id_) {
            RevokeCredentialsOnServer(refresh_token);
            refresh_token = kInvalidRefreshToken;
            PersistCredentials(account_id, refresh_token);
          } else {
            load_account = false;
          }
          load_token_status = LoadTokenFromDBStatus::TOKEN_REVOKED_ON_LOAD;
        }

        UMA_HISTOGRAM_ENUMERATION(
            "Signin.LoadTokenFromDB", load_token_status,
            LoadTokenFromDBStatus::NUM_LOAD_TOKEN_FROM_DB_STATUS);

        if (load_account) {
          RecordTokenLoaded(refresh_token);
          UpdateCredentialsInMemory(account_id, refresh_token);
          FireRefreshTokenAvailable(account_id);
        } else {
          RecordTokenRevoked(refresh_token);
          RevokeCredentialsOnServer(refresh_token);
          ClearPersistedCredentials(account_id);
          FireRefreshTokenRevoked(account_id);
        }
      }
    }

    if (!old_login_token.empty()) {
      DCHECK(!loading_primary_account_id_.empty());
      if (refresh_tokens_.count(loading_primary_account_id_) == 0)
        UpdateCredentials(loading_primary_account_id_, old_login_token);
    }
  }

  if (migrate_to_dice)
    client_->GetPrefs()->SetBoolean(prefs::kTokenServiceDiceCompatible, true);
}

void MutableProfileOAuth2TokenServiceDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());

  ValidateAccountId(account_id);
  signin_metrics::LogSigninAddAccount();

  const std::string& existing_token = GetRefreshToken(account_id);
  if (existing_token != refresh_token) {
    ScopedBatchChange batch(this);
    RecordTokenChanged(existing_token, refresh_token);
    UpdateCredentialsInMemory(account_id, refresh_token);
    PersistCredentials(account_id, refresh_token);
    FireRefreshTokenAvailable(account_id);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::UpdateCredentialsInMemory(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());

  bool is_refresh_token_invalidated = refresh_token == kInvalidRefreshToken;
  GoogleServiceAuthError error =
      is_refresh_token_invalidated
          ? GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_CLIENT)
          : GoogleServiceAuthError::AuthErrorNone();

  bool refresh_token_present = refresh_tokens_.count(account_id) > 0;
  // If token present, and different from the new one, cancel its requests,
  // and clear the entries in cache related to that account.
  if (refresh_token_present) {
    DCHECK_NE(refresh_token, refresh_tokens_[account_id]->refresh_token());
    VLOG(1) << "MutablePO2TS::UpdateCredentials; Refresh Token was present. "
            << "account_id=" << account_id;

    // The old refresh token must be revoked on the server only when it is
    // invalidated.
    //
    // The refresh token is updated to a new valid one in case of reauth.
    // In the reauth case the old and the new refresh tokens have the same
    // device ID. When revoking a refresh token on the server, Gaia revokes
    // all the refresh tokens that have the same device ID.
    // Therefore, the old refresh token must not be revoked on the server
    // when it is updated to a new valid one (otherwise the new refresh token
    // would also be invalidated server-side).
    // See http://crbug.com/865189 for more information about this regression.
    if (is_refresh_token_invalidated)
      RevokeCredentialsOnServer(refresh_tokens_[account_id]->refresh_token());

    refresh_tokens_[account_id]->set_refresh_token(refresh_token);
    UpdateAuthError(account_id, error);
  } else {
    VLOG(1) << "MutablePO2TS::UpdateCredentials; Refresh Token was absent. "
            << "account_id=" << account_id;
    AddAccountStatus(account_id, refresh_token, error);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::PersistCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());
  if (token_web_data_) {
    VLOG(1) << "MutablePO2TS::PersistCredentials for account_id=" << account_id;
    token_web_data_->SetTokenForService(ApplyAccountIdPrefix(account_id),
                                        refresh_token);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeAllCredentials() {
  if (!can_revoke_credentials_)
    return;
  DCHECK(thread_checker_.CalledOnValidThread());

  ScopedBatchChange batch(this);

  VLOG(1) << "MutablePO2TS::RevokeAllCredentials";
  CancelWebTokenFetch();

  // Make a temporary copy of the account ids.
  std::vector<std::string> accounts;
  for (const auto& token : refresh_tokens_)
    accounts.push_back(token.first);
  for (const std::string& account : accounts)
    RevokeCredentials(account);

  DCHECK_EQ(0u, refresh_tokens_.size());

  // Make sure all tokens are removed.
  if (token_web_data_)
    token_web_data_->RemoveAllTokens();
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeCredentials(
    const std::string& account_id) {
  ValidateAccountId(account_id);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (refresh_tokens_.count(account_id) > 0) {
    VLOG(1) << "MutablePO2TS::RevokeCredentials for account_id=" << account_id;
    ScopedBatchChange batch(this);
    const std::string& token = refresh_tokens_[account_id]->refresh_token();
    RecordTokenRevoked(token);
    RevokeCredentialsOnServer(token);
    refresh_tokens_.erase(account_id);
    ClearPersistedCredentials(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}

void MutableProfileOAuth2TokenServiceDelegate::ClearPersistedCredentials(
    const std::string& account_id) {
  DCHECK(!account_id.empty());
  if (token_web_data_) {
    VLOG(1) << "MutablePO2TS::ClearPersistedCredentials for account_id="
            << account_id;
    token_web_data_->RemoveTokenForService(ApplyAccountIdPrefix(account_id));
  }
}

void MutableProfileOAuth2TokenServiceDelegate::RevokeCredentialsOnServer(
    const std::string& refresh_token) {
  DCHECK(!refresh_token.empty());

  if (refresh_token == kInvalidRefreshToken)
    return;

  // Keep track or all server revoke requests.  This way they can be deleted
  // before the token service is shutdown and won't outlive the profile.
  server_revokes_.push_back(
      std::make_unique<RevokeServerRefreshToken>(this, client_, refresh_token));
}

void MutableProfileOAuth2TokenServiceDelegate::CancelWebTokenFetch() {
  if (web_data_service_request_ != 0) {
    DCHECK(token_web_data_);
    token_web_data_->CancelRequest(web_data_service_request_);
    web_data_service_request_ = 0;
  }
}

void MutableProfileOAuth2TokenServiceDelegate::Shutdown() {
  VLOG(1) << "MutablePO2TS::Shutdown";
  server_revokes_.clear();
  CancelWebTokenFetch();
  refresh_tokens_.clear();
}

void MutableProfileOAuth2TokenServiceDelegate::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  // If our network has changed, reset the backoff timer so that errors caused
  // by a previous lack of network connectivity don't prevent new requests.
  backoff_entry_.Reset();
}

const net::BackoffEntry*
    MutableProfileOAuth2TokenServiceDelegate::BackoffEntry() const {
  return &backoff_entry_;
}

void MutableProfileOAuth2TokenServiceDelegate::AddAccountStatus(
    const std::string& account_id,
    const std::string& refresh_token,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(0u, refresh_tokens_.count(account_id));
  AccountStatus* status =
      new AccountStatus(signin_error_controller_, account_id, refresh_token);
  refresh_tokens_[account_id].reset(status);
  status->Initialize();
  status->SetLastAuthError(error);
  FireAuthErrorChanged(account_id, status->GetAuthStatus());
}

void MutableProfileOAuth2TokenServiceDelegate::FinishLoadingCredentials() {
  FireRefreshTokensLoaded();
}
