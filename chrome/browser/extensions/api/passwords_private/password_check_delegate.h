// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORD_CHECK_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORD_CHECK_DELEGATE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class Profile;

namespace extensions {

extern const char kPasswordCheckDataKey[];

class PasswordCheckProgress;
class PasswordsPrivateEventRouter;

// This class handles the part of the passwordsPrivate extension API that deals
// with the bulk password check feature.
class PasswordCheckDelegate
    : public password_manager::SavedPasswordsPresenter::Observer,
      public password_manager::InsecureCredentialsManager::Observer,
      public password_manager::BulkLeakCheckServiceInterface::Observer {
 public:
  using StartPasswordCheckCallback =
      PasswordsPrivateDelegate::StartPasswordCheckCallback;

  PasswordCheckDelegate(Profile* profile,
                        password_manager::SavedPasswordsPresenter* presenter,
                        IdGenerator* id_generator,
                        PasswordsPrivateEventRouter* event_router = nullptr);
  PasswordCheckDelegate(const PasswordCheckDelegate&) = delete;
  PasswordCheckDelegate& operator=(const PasswordCheckDelegate&) = delete;
  ~PasswordCheckDelegate() override;

  // Obtains information about insecure credentials. This includes the last
  // time a check was run, as well as all insecure credentials that are
  // present in the password store.
  // TODO:(crbug.com/1350947) - Rename to GetInsecureCredentialsUiEntry.
  std::vector<api::passwords_private::PasswordUiEntry> GetInsecureCredentials();

  // Returns a list of vectors. Each vector contains all credentials that share
  // the same password.
  std::vector<api::passwords_private::PasswordUiEntryList>
  GetCredentialsWithReusedPassword();

  // Attempts to mute `credential` from the password store. Returns whether
  // the mute succeeded.
  bool MuteInsecureCredential(
      const api::passwords_private::PasswordUiEntry& credential);

  // Attempts to unmute `credential` from the password store. Returns whether
  // the unmute succeeded.
  bool UnmuteInsecureCredential(
      const api::passwords_private::PasswordUiEntry& credential);

  // Checks that all preconditions for running a password check are fulfilled
  // and, once that is the case, launches the password check. Invokes `callback`
  // once a check is running or the request was stopped via
  // `StopPasswordCheck()`.
  void StartPasswordCheck(
      password_manager::LeakDetectionInitiator initiator,
      StartPasswordCheckCallback callback = base::DoNothing());

  // Returns the current status of the password check.
  api::passwords_private::PasswordCheckStatus GetPasswordCheckStatus() const;

  // Returns a pointer to the current instance of InsecureCredentialsManager.
  // Needed to get notified when compromised credentials are written out to
  // disk, since BulkLeakCheckService does not know about that step.
  password_manager::InsecureCredentialsManager* GetInsecureCredentialsManager();

 private:
  // password_manager::SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // password_manager::InsecureCredentialsManager::Observer:
  // Invokes PasswordsPrivateEventRouter::OnInsecureCredentialsChanged if
  // a valid pointer can be obtained.
  void OnInsecureCredentialsChanged() override;

  // password_manager::BulkLeakCheckService::Observer:
  void OnStateChanged(
      password_manager::BulkLeakCheckService::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;
  void OnBulkCheckServiceShutDown() override;

  // Starts the analyses of whether credentials are compromised and/or weak.
  // Assumes that `StartPasswordCheck()` was called prior.
  void StartPasswordAnalyses(StartPasswordCheckCallback callback);

  // Invoked when a compromised password check completes. Records the current
  // timestamp in `kLastTimePasswordCheckCompleted` pref.
  void RecordAndNotifyAboutCompletedCompromisedPasswordCheck();

  // Invoked when a weak password check completes. Records the current timestamp
  // in `last_completed_weak_check_`.
  void RecordAndNotifyAboutCompletedWeakPasswordCheck();

  // Tries to notify the `PasswordsPrivateEventRouter` that the password check
  // status has changed. Invoked after `OnSavedPasswordsChanged` and
  // `OnStateChanged`.
  void NotifyPasswordCheckStatusChanged();

  // Constructs `PasswordUiEntry` from `CredentialUIEntry`.
  api::passwords_private::PasswordUiEntry ConstructInsecureCredentialUiEntry(
      password_manager::CredentialUIEntry entry);

  // Raw pointer to the underlying profile. Needs to outlive this instance.
  raw_ptr<Profile> profile_ = nullptr;

  // Used by `insecure_credentials_manager_` to obtain the list of saved
  // passwords.
  raw_ptr<password_manager::SavedPasswordsPresenter>
      saved_passwords_presenter_ = nullptr;

  // Used to obtain the list of insecure credentials.
  password_manager::InsecureCredentialsManager insecure_credentials_manager_;

  // Adapter used to start, monitor and stop a bulk leak check.
  password_manager::BulkLeakCheckServiceAdapter
      bulk_leak_check_service_adapter_;

  // Boolean that remembers whether the delegate is initialized. This is done
  // when the delegate obtains the list of saved passwords for the first time.
  bool is_initialized_ = false;

  // List of callbacks that were passed to `StartPasswordCheck()` prior to the
  // delegate being initialized. These will be run when either initialization
  // finishes.
  std::vector<StartPasswordCheckCallback> start_check_callbacks_;

  // Remembers the progress of the ongoing check. Null if no check is currently
  // running.
  base::WeakPtr<PasswordCheckProgress> password_check_progress_;

  // Remembers whether a password check is running right now.
  bool is_check_running_ = false;

  // Store when the last weak check was completed.
  base::Time last_completed_weak_check_;

  // A scoped observer for `saved_passwords_presenter_`.
  base::ScopedObservation<password_manager::SavedPasswordsPresenter,
                          password_manager::SavedPasswordsPresenter::Observer>
      observed_saved_passwords_presenter_{this};

  // A scoped observer for `insecure_credentials_manager_`.
  base::ScopedObservation<
      password_manager::InsecureCredentialsManager,
      password_manager::InsecureCredentialsManager::Observer>
      observed_insecure_credentials_manager_{this};

  // A scoped observer for the `BulkLeakCheckService`.
  base::ScopedObservation<
      password_manager::BulkLeakCheckServiceInterface,
      password_manager::BulkLeakCheckServiceInterface::Observer>
      observed_bulk_leak_check_service_{this};

  // An id generator for insecure credentials. Required to match
  // `api::passwords_private::PasswordUiEntry` instances passed to the UI
  // with the underlying `CredentialUIEntry` they are based on.
  raw_ptr<IdGenerator> id_generator_;

  raw_ptr<PasswordsPrivateEventRouter> event_router_;

  // This indicate what was the reason to start the password check.
  password_manager::LeakDetectionInitiator password_check_initiator_ =
      password_manager::LeakDetectionInitiator::kClientUseCaseUnspecified;

  base::WeakPtrFactory<PasswordCheckDelegate> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORD_CHECK_DELEGATE_H_
