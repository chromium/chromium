// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_
#define CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

class AboutSigninInternals;
class GaiaAuthFetcher;
class GoogleServiceAuthError;
class SigninClient;
class Profile;

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

  // Asks the delegate to enable sync for the |account_id|.
  // Called after the account was seeded in the account tracker service and
  // after the refresh token was fetched and updated in the token service.
  virtual void EnableSync(const CoreAccountId& account_id) = 0;

  // Handles a failure in the token exchange (i.e. shows the error to the user).
  virtual void HandleTokenExchangeFailure(
      const std::string& email,
      const GoogleServiceAuthError& error) = 0;
};

// Processes the Dice responses from Gaia.
class DiceResponseHandler : public KeyedService {
 public:
  // Returns the DiceResponseHandler associated with this profile.
  // May return nullptr if there is none (e.g. in incognito).
  static DiceResponseHandler* GetForProfile(Profile* profile);

  DiceResponseHandler(SigninClient* signin_client,
                      signin::IdentityManager* identity_manager,
                      AccountReconcilor* account_reconcilor,
                      AboutSigninInternals* about_signin_internals,
                      const base::FilePath& profile_path_);

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

  static void EnsureFactoryBuilt();

 private:
  // Helper class to fetch a refresh token from an authorization code.
  class DiceTokenFetcher : public GaiaAuthConsumer {
   public:
    DiceTokenFetcher(const std::string& gaia_id,
                     const std::string& email,
                     const std::string& authorization_code,
                     SigninClient* signin_client,
                     AccountReconcilor* account_reconcilor,
                     std::unique_ptr<ProcessDiceHeaderDelegate> delegate,
                     DiceResponseHandler* dice_response_handler);

    DiceTokenFetcher(const DiceTokenFetcher&) = delete;
    DiceTokenFetcher& operator=(const DiceTokenFetcher&) = delete;

    ~DiceTokenFetcher() override;

    const std::string& gaia_id() const { return gaia_id_; }
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

    // Lock the account reconcilor while tokens are being fetched.
    std::unique_ptr<AccountReconcilor::Lock> account_reconcilor_lock_;

    std::string gaia_id_;
    std::string email_;
    std::string authorization_code_;
    std::unique_ptr<ProcessDiceHeaderDelegate> delegate_;
    raw_ptr<DiceResponseHandler> dice_response_handler_;
    base::CancelableOnceClosure timeout_closure_;
    bool should_enable_sync_;
    std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  };

  // Deletes the token fetcher.
  void DeleteTokenFetcher(DiceTokenFetcher* token_fetcher);

  // Process the Dice signin action.
  void ProcessDiceSigninHeader(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& authorization_code,
      bool no_authorization_code,
      std::unique_ptr<ProcessDiceHeaderDelegate> delegate);

  // Process the Dice enable sync action.
  void ProcessEnableSyncHeader(
      const std::string& gaia_id,
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
                              bool is_under_advanced_protection);
  void OnTokenExchangeFailure(DiceTokenFetcher* token_fetcher,
                              const GoogleServiceAuthError& error);
  // Called to unlock the reconcilor after a SLO outage.
  void OnTimeoutUnlockReconcilor();

  raw_ptr<SigninClient> signin_client_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<AccountReconcilor> account_reconcilor_;
  raw_ptr<AboutSigninInternals> about_signin_internals_;
  base::FilePath profile_path_;
  std::vector<std::unique_ptr<DiceTokenFetcher>> token_fetchers_;
  // Lock the account reconcilor for kLockAccountReconcilorTimeoutHours
  // when there was OAuth outage in Dice.
  std::unique_ptr<AccountReconcilor::Lock> lock_;
  std::unique_ptr<base::OneShotTimer> timer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_H_
