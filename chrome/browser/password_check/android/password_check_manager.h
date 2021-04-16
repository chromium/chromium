// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_MANAGER_H_
#define CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/password_check/android/password_check_ui_status.h"
#include "chrome/browser/password_entry_edit/android/credential_edit_bridge.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/bulk_leak_check_service_interface.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class PasswordCheckManager
    : public password_manager::SavedPasswordsPresenter::Observer,
      public password_manager::InsecureCredentialsManager::Observer,
      public password_manager::BulkLeakCheckServiceInterface::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnSavedPasswordsFetched(int count) = 0;
    virtual void OnCompromisedCredentialsChanged(int count) = 0;
    virtual void OnPasswordCheckStatusChanged(
        password_manager::PasswordCheckUIStatus status) = 0;
    virtual void OnPasswordCheckProgressChanged(int already_processed,
                                                int remaining_in_queue) = 0;
  };

  struct CompromisedCredentialForUI : password_manager::CredentialWithPassword {
    explicit CompromisedCredentialForUI(
        const password_manager::CredentialWithPassword& credential);

    CompromisedCredentialForUI(const CompromisedCredentialForUI& other);
    CompromisedCredentialForUI(CompromisedCredentialForUI&& other);
    CompromisedCredentialForUI& operator=(
        const CompromisedCredentialForUI& other);
    CompromisedCredentialForUI& operator=(CompromisedCredentialForUI&& other);
    ~CompromisedCredentialForUI();

    std::u16string display_username;
    std::u16string display_origin;
    std::string package_name;
    std::string change_password_url;
    bool has_startable_script = false;
    bool has_auto_change_button = false;
  };

  // `observer` must outlive `this`.
  PasswordCheckManager(Profile* profile, Observer* observer);
  ~PasswordCheckManager() override;

  // Requests to start the password check.
  void StartCheck();

  // Stops a running check.
  void StopCheck();

  // Called by java to retireve the timestamp of the last password check.
  base::Time GetLastCheckTimestamp();

  // Called by java to retrieve the number of compromised credentials. If the
  // credentials haven't been fetched yet, this will return 0.
  int GetCompromisedCredentialsCount() const;

  // Called by java to retrieve the number of saved passwords.
  // If the saved passwords haven't been fetched yet, this will return 0.
  int GetSavedPasswordsCount() const;

  // Called by java to retrieve the compromised credentials.
  std::vector<CompromisedCredentialForUI> GetCompromisedCredentials() const;

  // Called by java to update the given compromised `credential` and set its
  // password to `new_password`.
  void UpdateCredential(const password_manager::CredentialView& credential,
                        base::StringPiece new_password);

  // Called by java to launch the edit credential UI for `credential`.
  void OnEditCredential(
      const password_manager::CredentialView& credential,
      const base::android::JavaParamRef<jobject>& context,
      const base::android::JavaParamRef<jobject>& settings_launcher);

  // Called by java to remove the given compromised `credential` and trigger a
  // UI update on completion.
  void RemoveCredential(const password_manager::CredentialView& credential);

  // Invokes `PasswordScriptsFetcher`'s scripts refreshment.
  void RefreshScripts();

  // Returns if scripts refreshment is finished.
  bool AreScriptsRefreshed() const;

  // Not copyable or movable
  PasswordCheckManager(const PasswordCheckManager&) = delete;
  PasswordCheckManager& operator=(const PasswordCheckManager&) = delete;
  PasswordCheckManager(PasswordCheckManager&&) = delete;
  PasswordCheckManager& operator=(PasswordCheckManager&&) = delete;

 private:
  // Helps to track which preconditions are fulfilled.
  enum CheckPreconditions {
    // No preconditions have been fulfilled.
    kNone = 0,
    // Saved passwords can be accessed.
    kSavedPasswordsAvailable = 1 << 0,
    // Sites with available Scripts are fetched.
    kScriptsCachePrewarmed = 1 << 1,
    // Already known compromised credentials were loaded.
    kKnownCredentialsFetched = 1 << 2,
    // All preconditions have been fulfilled.
    kAll = kSavedPasswordsAvailable | kScriptsCachePrewarmed |
           kKnownCredentialsFetched,
  };

  // Class remembering the state required to update the progress of an ongoing
  // Password Check.
  class PasswordCheckProgress {
   public:
    PasswordCheckProgress();
    ~PasswordCheckProgress();

    size_t remaining_in_queue() const { return remaining_in_queue_; }
    size_t already_processed() const { return already_processed_; }

    // Increments the counts corresponding to `password`. Intended to be called
    // for each credential that is passed to the bulk check.
    void IncrementCounts(const password_manager::PasswordForm& password);

    // Updates the counts after a `credential` has been processed by the bulk
    // check.
    void OnProcessed(const password_manager::LeakCheckCredential& credential);

   private:
    // Count variables needed to correctly show the progress of the check to the
    // user. `already_processed_` contains the number of credentials that have
    // been checked already, while `remaining_in_queue_` remembers how many
    // passwords still need to be checked.
    // Since the bulk leak check tries to be as efficient as possible, it
    // performs a deduplication step before starting to check passwords. In this
    // step it canonicalizes each credential, and only processes the
    // combinations that are unique. Since this number likely does not match the
    // total number of saved passwords, we remember in `counts_` how many saved
    // passwords a given canonicalized credential corresponds to.
    size_t already_processed_ = 0;
    size_t remaining_in_queue_ = 0;
    std::map<password_manager::CanonicalizedCredential, size_t> counts_;
  };

  // password_manager::SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      password_manager::SavedPasswordsPresenter::SavedPasswordsView passwords)
      override;

  // InsecureCredentialsManager::Observer
  void OnInsecureCredentialsChanged(
      password_manager::InsecureCredentialsManager::CredentialsView credentials)
      override;

  // BulkLeakCheckServiceInterface::Observer
  void OnStateChanged(
      password_manager::BulkLeakCheckServiceInterface::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;
  void OnBulkCheckServiceShutDown() override;

  // Turns a `CredentialWithPassword` into a `CompromisedCredentialForUI`,
  // getting suitable strings for all display elements (e.g. url, app name,
  // app package, username, etc.).
  CompromisedCredentialForUI MakeUICredential(
      const password_manager::CredentialWithPassword& credential) const;

  // Converts the state retrieved from the check service into a state that
  // can be used by the UI to display appropriate messages.
  password_manager::PasswordCheckUIStatus GetUIStatus(
      password_manager::BulkLeakCheckServiceInterface::State state) const;

  // Returns true if the user has their passwords available in their Google
  // Account. Used to determine whether the user could use the password check
  // in the account if the quota limit was reached.
  bool CanUseAccountCheck() const;

  // Returns true if the password scripts fetching (kPasswordScriptsFetching) is
  // enabled. To have precise metrics about user actions on credentials with
  // scripts, scripts are fetched only for the users who can start a script,
  // i.e. sync users.
  bool ShouldFetchPasswordScripts() const;

  // Callback when PasswordScriptsFetcher's cache has been warmed up.
  void OnScriptsFetched();

  // Returns true if the passed |condition| was already met.
  bool IsPreconditionFulfilled(CheckPreconditions condition) const;

  // Marks the passed |condition| as fulfilled and runs a check if applicable.
  void FulfillPrecondition(CheckPreconditions condition);

  // Resets the passed |condition| so that it's expected to happen again.
  void ResetPrecondition(CheckPreconditions condition);

  // Destroys the edit ui bridge.
  void OnEditUIDismissed();

  // Obsever being notified of UI-relevant events.
  // It must outlive `this`.
  Observer* observer_ = nullptr;

  // The profile for which the passwords are checked.
  Profile* profile_ = nullptr;

  // Object storing the progress of a running password check.
  std::unique_ptr<PasswordCheckProgress> progress_;

  // Handle to the password store, powering both `saved_passwords_presenter_`
  // and `insecure_credentials_manager_`.
  scoped_refptr<password_manager::PasswordStore> password_store_ =
      PasswordStoreFactory::GetForProfile(profile_,
                                          ServiceAccessType::EXPLICIT_ACCESS);

  // Used to check whether autofill assistant scripts are available for
  // the specified domain.
  password_manager::PasswordScriptsFetcher* password_script_fetcher_ =
      PasswordScriptsFetcherFactory::GetInstance()->GetForBrowserContext(
          profile_);

  // Used by `insecure_credentials_manager_` to obtain the list of saved
  // passwords.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_{
      password_store_};

  // Used to obtain the list of insecure credentials.
  password_manager::InsecureCredentialsManager insecure_credentials_manager_{
      &saved_passwords_presenter_, password_store_};

  // Adapter used to start, monitor and stop a bulk leak check.
  password_manager::BulkLeakCheckServiceAdapter
      bulk_leak_check_service_adapter_{
          &saved_passwords_presenter_,
          BulkLeakCheckServiceFactory::GetForProfile(profile_),
          profile_->GetPrefs()};

  // The check can be run only of this is CheckPreconditions::kAll;
  int fulfilled_preconditions_ = CheckPreconditions::kNone;

  // Whether the check start was requested.
  bool was_start_requested_ = false;

  // Whether a check is currently running.
  bool is_check_running_ = false;

  // Latest number of changed compromised credentials while script fetching
  // was running. If `credentials_count_to_notify_` has value, after scripts are
  // fetched `onCompromisedCredentials` should be called.
  base::Optional<size_t> credentials_count_to_notify_;

  // Used to open the view/edit/delete UI.
  std::unique_ptr<CredentialEditBridge> credential_edit_bridge_;

  // A scoped observer for `saved_passwords_presenter_`.
  ScopedObserver<password_manager::SavedPasswordsPresenter,
                 password_manager::SavedPasswordsPresenter::Observer>
      observed_saved_passwords_presenter_{this};

  // A scoped observer for `insecure_credentials_manager_`.
  ScopedObserver<password_manager::InsecureCredentialsManager,
                 password_manager::InsecureCredentialsManager::Observer>
      observed_insecure_credentials_manager_{this};

  // A scoped observer for the BulkLeakCheckService.
  ScopedObserver<password_manager::BulkLeakCheckServiceInterface,
                 password_manager::BulkLeakCheckServiceInterface::Observer>
      observed_bulk_leak_check_service_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_CHECK_ANDROID_PASSWORD_CHECK_MANAGER_H_
