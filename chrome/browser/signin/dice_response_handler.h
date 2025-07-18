// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_
#define CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chrome/browser/signin/binding_key_registration_token_helper.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_id.h"

class AboutSigninInternals;
class GaiaAuthFetcher;
class GoogleServiceAuthError;
class SigninClient;

namespace signin {
class IdentityManager;
}

// Exposed for testing.
extern const int kDiceTokenFetchTimeoutSeconds;
// Exposed for testing.
extern const int kLockAccountReconcilorTimeoutHours;

// Delegate interface for processing a dice request.
class ProcessDiceHeaderDelegate {
 public:
  virtual ~ProcessDiceHeaderDelegate() = default;

  // Called when a token was successfully exchanged.
  // Called after the account was seeded in the account tracker service and
  // after the refresh token was fetched and updated in the token service.
  // |is_new_account| is true if the account was added to Chrome (it is not a
  // re-auth).
  virtual void HandleTokenExchangeSuccess(CoreAccountId account_id,
                                          bool is_new_account) = 0;

  // Asks the delegate to enable sync for the |account_info|.
  // Called after the account was seeded in the account tracker service and
  // after the refresh token was fetched and updated in the token service.
  virtual void EnableSync(const CoreAccountInfo& account_info) = 0;

  // Called when a Dice signin header is received. This is received before
  // navigating to the `continue_url`. Chrome has received the authorization
  // code, but has not exchanged it for a token yet.
  virtual void OnDiceSigninHeaderReceived() = 0;

  // Handles a failure in the token exchange (i.e. shows the error to the user).
  virtual void HandleTokenExchangeFailure(
      const std::string& email,
      const GoogleServiceAuthError& error) = 0;

  virtual signin_metrics::AccessPoint GetAccessPoint() = 0;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PrimaryAccountSettingGaiaIntegrationState)
enum class PrimaryAccountSettingGaiaIntegrationState {
  kOnTokenExchangeSuccess = 0,
  kOnSyncHeaderReceived = 1,
  kMaxValue = kOnSyncHeaderReceived
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:GaiaIntegrationState)

// Processes the Dice responses from Gaia.
class DiceResponseHandler : public KeyedService {
 public:
  using RegistrationTokenHelperFactory =
      base::RepeatingCallback<std::unique_ptr<BindingKeyRegistrationTokenHelper>(
          BindingKeyRegistrationTokenHelper::KeyInitParam key_init_param)>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Public for testing.
  // LINT.IfChange(TokenBindingOutcome)
  enum class TokenBindingOutcome {
    kBound = 0,
    kNotBoundUnknown = 1,
    kNotBoundNotSupported = 2,
    kNotBoundNotEligible = 3,
    kNotBoundRegistrationTokenGenerationFailed = 4,
    kNotBoundServerRejectedKey = 5,
    kNotBoundRefreshTokensNotLoaded = 6,
    kMaxValue = kNotBoundRefreshTokensNotLoaded,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:DiceTokenBindingOutcome)

  // `registration_token_helper_factory` might be null. If that's the case,
  // Chrome won't make an attempt to bind a refresh token.
  DiceResponseHandler(
      SigninClient* signin_client,
      signin::IdentityManager* identity_manager,
      AccountReconcilor* account_reconcilor,
      AboutSigninInternals* about_signin_internals,
      RegistrationTokenHelperFactory registration_token_helper_factory);

  DiceResponseHandler(const DiceResponseHandler&) = delete;
  DiceResponseHandler& operator=(const DiceResponseHandler&) = delete;

  ~DiceResponseHandler() override;

  // Must be called when receiving a Dice response header.
  void ProcessDiceHeader(const signin::DiceResponseParams& dice_params,
                         std::unique_ptr<ProcessDiceHeaderDelegate> delegate);

  // Returns the number of pending DiceTokenFetchers. Exposed for testing.
  size_t GetPendingDiceTokenFetchersCountForTesting() const;

  // Sets |task_runner_| for testing.
  void SetTaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Sets a `registration_token_helper_factory_` factory callback for testing.
  void SetRegistrationTokenHelperFactoryForTesting(
      RegistrationTokenHelperFactory factory);

 private:
  // Helper class to fetch a refresh token from an authorization code.
  class DiceTokenFetcher : public GaiaAuthConsumer {
   public:
    DiceTokenFetcher(
        const GaiaId& gaia_id,
        const std::string& email,
        const std::string& authorization_code,
        SigninClient* signin_client,
        AccountReconcilor* account_reconcilor,
        std::unique_ptr<ProcessDiceHeaderDelegate> delegate,
        base::expected<raw_ref<BindingKeyRegistrationTokenHelper>,
                       TokenBindingOutcome>
            registration_token_helper_or_error,
        DiceResponseHandler* dice_response_handler);

    DiceTokenFetcher(const DiceTokenFetcher&) = delete;
    DiceTokenFetcher& operator=(const DiceTokenFetcher&) = delete;

    ~DiceTokenFetcher() override;

    const GaiaId& gaia_id() const { return gaia_id_; }
    const std::string& email() const { return email_; }
    const std::string& authorization_code() const {
      return authorization_code_;
    }
    bool should_enable_sync() const { return should_enable_sync_; }
    void set_should_enable_sync(bool should_enable_sync) {
      should_enable_sync_ = should_enable_sync;
    }
    ProcessDiceHeaderDelegate* delegate() { return delegate_.get(); }

   private:
    // Called by |timeout_closure_| when the request times out.
    void OnTimeout();

    // GaiaAuthConsumer implementation:
    void OnClientOAuthSuccess(
        const GaiaAuthConsumer::ClientOAuthResult& result) override;
    void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;

    void StartTokenFetch();

    void StartBindingKeyGeneration(
        BindingKeyRegistrationTokenHelper& registration_token_helper);
    void OnRegistrationTokenGenerated(
        std::optional<BindingKeyRegistrationTokenHelper::Result> result);

    // Lock the account reconcilor while tokens are being fetched.
    std::unique_ptr<AccountReconcilor::Lock> account_reconcilor_lock_;

    const GaiaId gaia_id_;
    const std::string email_;
    const std::string authorization_code_;
    const std::unique_ptr<ProcessDiceHeaderDelegate> delegate_;
    const raw_ptr<DiceResponseHandler> dice_response_handler_;
    const raw_ptr<SigninClient> signin_client_;
    base::CancelableOnceClosure timeout_closure_;
    bool should_enable_sync_;
    std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
    TokenBindingOutcome token_binding_outcome_ =
        TokenBindingOutcome::kNotBoundUnknown;
    // The following fields are empty if the binding key wasn't generated.
    std::string binding_registration_token_;
    std::vector<uint8_t> wrapped_binding_key_;
  };

  // Deletes the token fetcher.
  void DeleteTokenFetcher(DiceTokenFetcher* token_fetcher);

  // Process the Dice signin action.
  void ProcessDiceSigninHeader(
      const GaiaId& gaia_id,
      const std::string& email,
      const std::string& authorization_code,
      bool no_authorization_code,
      const std::string& supported_algorithms_for_token_binding,
      std::unique_ptr<ProcessDiceHeaderDelegate> delegate);

  // Process the Dice enable sync action.
  void ProcessEnableSyncHeader(
      const GaiaId& gaia_id,
      const std::string& email,
      std::unique_ptr<ProcessDiceHeaderDelegate> delegate);

  // Process the Dice signout action.
  void ProcessDiceSignoutHeader(
      const std::vector<signin::DiceResponseParams::AccountInfo>&
          account_infos);

  // Called after exchanging an OAuth 2.0 authorization code for a refresh token
  // after DiceAction::SIGNIN.
  void OnTokenExchangeSuccess(DiceTokenFetcher* token_fetcher,
                              const std::string& refresh_token,
                              bool is_under_advanced_protection,
                              const std::vector<uint8_t>& wrapped_binding_key);
  void OnTokenExchangeFailure(DiceTokenFetcher* token_fetcher,
                              const GoogleServiceAuthError& error);
  // Called to unlock the reconcilor after a SLO outage.
  void OnTimeoutUnlockReconcilor();

  // Returns a `BindingKeyRegistrationTokenHelper` if `this` should attempt to
  // bind a refresh token given the configuration parameters and a list of
  // `supported_algorithms` provided by the server. Otherwise, returns the
  // reason for why the refresh token wasn't bound.
  // Returned `BindingKeyRegistrationTokenHelper` is owned by `this`. See
  // `registration_token_helper_` for the description of its lifetime.
  base::expected<raw_ref<BindingKeyRegistrationTokenHelper>, TokenBindingOutcome>
  MaybeGetBindingRegistrationTokenHelper(
      std::string_view supported_algorithms);

  const raw_ptr<SigninClient> signin_client_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<AccountReconcilor> account_reconcilor_;
  const raw_ptr<AboutSigninInternals> about_signin_internals_;
  // Shared between all fetches in `token_fetchers_` and must outlive them.
  // Must be cleaned up as soon as `token_fetchers_` becomes empty.
  std::unique_ptr<BindingKeyRegistrationTokenHelper>
      registration_token_helper_;
  std::vector<std::unique_ptr<DiceTokenFetcher>> token_fetchers_;
  // Lock the account reconcilor for kLockAccountReconcilorTimeoutHours
  // when there was OAuth outage in Dice.
  std::unique_ptr<AccountReconcilor::Lock> lock_;
  std::unique_ptr<base::OneShotTimer> timer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  RegistrationTokenHelperFactory registration_token_helper_factory_;
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_
