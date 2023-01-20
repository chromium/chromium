// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#include "chrome/browser/ui/passwords/settings/password_manager_porter.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"
#include "components/password_manager/core/browser/password_account_storage_settings_watcher.h"
#include "components/password_manager/core/browser/reauth_purpose.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/export_progress_status.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

// Concrete PasswordsPrivateDelegate implementation.
class PasswordsPrivateDelegateImpl
    : public PasswordsPrivateDelegate,
      public password_manager::SavedPasswordsPresenter::Observer {
 public:
  explicit PasswordsPrivateDelegateImpl(Profile* profile);

  PasswordsPrivateDelegateImpl(const PasswordsPrivateDelegateImpl&) = delete;
  PasswordsPrivateDelegateImpl& operator=(const PasswordsPrivateDelegateImpl&) =
      delete;

  ~PasswordsPrivateDelegateImpl() override;

  // PasswordsPrivateDelegate implementation.
  void GetSavedPasswordsList(UiEntriesCallback callback) override;
  CredentialsGroups GetCredentialGroups() override;
  void GetPasswordExceptionsList(ExceptionEntriesCallback callback) override;
  absl::optional<api::passwords_private::UrlCollection> GetUrlCollection(
      const std::string& url) override;
  bool IsAccountStoreDefault(content::WebContents* web_contents) override;
  bool AddPassword(const std::string& url,
                   const std::u16string& username,
                   const std::u16string& password,
                   const std::u16string& note,
                   bool use_account_store,
                   content::WebContents* web_contents) override;
  absl::optional<int> ChangeSavedPassword(
      int id,
      const api::passwords_private::ChangeSavedPasswordParams& params) override;
  void RemoveSavedPassword(
      int id,
      api::passwords_private::PasswordStoreSet from_stores) override;
  void RemovePasswordException(int id) override;
  void UndoRemoveSavedPasswordOrException() override;
  void RequestPlaintextPassword(int id,
                                api::passwords_private::PlaintextReason reason,
                                PlaintextPasswordCallback callback,
                                content::WebContents* web_contents) override;
  void RequestCredentialsDetails(const std::vector<int>& ids,
                                 UiEntriesCallback callback,
                                 content::WebContents* web_contents) override;
  void MovePasswordsToAccount(const std::vector<int>& ids,
                              content::WebContents* web_contents) override;
  void ImportPasswords(api::passwords_private::PasswordStoreSet to_store,
                       ImportResultsCallback results_callback,
                       content::WebContents* web_contents) override;
  void ExportPasswords(
      base::OnceCallback<void(const std::string&)> accepted_callback,
      content::WebContents* web_contents) override;
  void CancelExportPasswords() override;
  api::passwords_private::ExportProgressStatus GetExportProgressStatus()
      override;
  bool IsOptedInForAccountStorage() override;
  // TODO(crbug.com/1102294): Mimic the signature in PasswordFeatureManager.
  void SetAccountStorageOptIn(bool opt_in,
                              content::WebContents* web_contents) override;
  std::vector<api::passwords_private::PasswordUiEntry> GetInsecureCredentials()
      override;
  std::vector<api::passwords_private::PasswordUiEntryList>
  GetCredentialsWithReusedPassword() override;
  bool MuteInsecureCredential(
      const api::passwords_private::PasswordUiEntry& credential) override;
  bool UnmuteInsecureCredential(
      const api::passwords_private::PasswordUiEntry& credential) override;
  void RecordChangePasswordFlowStarted(
      const api::passwords_private::PasswordUiEntry& credential) override;
  void StartPasswordCheck(StartPasswordCheckCallback callback) override;
  void StopPasswordCheck() override;
  api::passwords_private::PasswordCheckStatus GetPasswordCheckStatus() override;
  password_manager::InsecureCredentialsManager* GetInsecureCredentialsManager()
      override;
  void ExtendAuthValidity() override;
  void SwitchBiometricAuthBeforeFillingState(
      content::WebContents* web_contents) override;
  void ShowAddShortcutDialog(content::WebContents* web_contents) override;

  // KeyedService overrides:
  void Shutdown() override;

#if defined(UNIT_TEST)
  int GetIdForCredential(
      const password_manager::CredentialUIEntry& credential) {
    return credential_id_generator_.GenerateId(credential);
  }

  // Use this in tests to mock the OS-level reauthentication.
  void set_os_reauth_call(
      password_manager::PasswordAccessAuthenticator::ReauthCallback
          os_reauth_call) {
    password_access_authenticator_.set_os_reauth_call(
        std::move(os_reauth_call));
  }

  void SetPorterForTesting(
      std::unique_ptr<PasswordManagerPorterInterface> porter) {
    password_manager_porter_ = std::move(porter);
  }
#endif  // defined(UNIT_TEST)

 private:
  // password_manager::SavedPasswordsPresenter::Observer implementation.
  void OnSavedPasswordsChanged() override;

  // Called after the lists are fetched. Once both lists have been set, the
  // class is considered initialized and any queued functions (which could
  // not be executed immediately due to uninitialized data) are invoked.
  void InitializeIfNecessary();

  // Executes a given callback by either invoking it immediately if the class
  // has been initialized or by deferring it until initialization has completed.
  void ExecuteFunction(base::OnceClosure callback);

  void SetCredentials(
      std::vector<password_manager::CredentialUIEntry> credentials);

  void RemoveEntryInternal(
      int id,
      api::passwords_private::PasswordStoreSet from_stores);
  void UndoRemoveSavedPasswordOrExceptionInternal();

  // Callback for when the password list has been written to the destination.
  void OnPasswordsExportProgress(password_manager::ExportProgressStatus status,
                                 const std::string& folder_name);

  // Callback for RequestPlaintextPassword() after authentication check.
  void OnRequestPlaintextPasswordAuthResult(
      int id,
      api::passwords_private::PlaintextReason reason,
      PlaintextPasswordCallback callback,
      bool authenticated);

  // Callback for RequestCredentialDetails() after authentication check.
  void OnRequestCredentialDetailsAuthResult(const std::vector<int>& ids,
                                            UiEntriesCallback callback,
                                            bool authenticated);

  // Callback for ExportPasswords() after authentication check.
  void OnExportPasswordsAuthResult(
      base::OnceCallback<void(const std::string&)> accepted_callback,
      content::WebContents* web_contents,
      bool authenticated);

  void OnAccountStorageOptInStateChanged();

  // Decides whether an authentication check is successful. Passes the result
  // to |callback|. True indicates that no extra work is needed. False
  // indicates that OS-dependent UI to present OS account login challenge
  // should be shown.
  void OsReauthCall(
      password_manager::ReauthPurpose purpose,
      password_manager::PasswordAccessAuthenticator::AuthResultCallback
          callback);

  // Records user action and emits histogram values for retrieving |entry|.
  void EmitHistogramsForCredentialAccess(
      const password_manager::CredentialUIEntry& entry,
      api::passwords_private::PlaintextReason reason);

  // Callback for biometric authentication after authentication check.
  void OnReauthCompleted();

  // Invokes PasswordsPrivateEventRouter::OnPasswordManagerAuthTimeout().
  void OsReauthTimeoutCall();

  void AuthenticateWithBiometrics(
      const std::u16string& message,
      password_manager::PasswordAccessAuthenticator::AuthResultCallback
          callback);

  extensions::api::passwords_private::PasswordUiEntry
  CreatePasswordUiEntryFromCredentialUiEntry(
      password_manager::CredentialUIEntry credential);

  // Not owned by this class.
  raw_ptr<Profile> profile_;

  // Used to add/edit passwords and to create |password_check_delegate_|.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_;

  // Used to control the export and import flows.
  std::unique_ptr<PasswordManagerPorterInterface> password_manager_porter_;

  password_manager::PasswordAccessAuthenticator password_access_authenticator_;

  std::unique_ptr<password_manager::PasswordAccountStorageSettingsWatcher>
      password_account_storage_settings_watcher_;

  PasswordCheckDelegate password_check_delegate_;

  // The current list of entries/exceptions. Cached here so that when new
  // observers are added, this delegate can send the current lists without
  // having to request them from |password_manager_presenter_| again.
  UiEntries current_entries_;
  ExceptionEntries current_exceptions_;

  // An id generator for saved passwords and blocked websites.
  IdGenerator credential_id_generator_;

  // Whether SetCredentials has been called, and whether this class has been
  // initialized.
  bool current_entries_initialized_;
  bool is_initialized_;

  // Vector of callbacks which are queued up before the password store has been
  // initialized. Once SetCredentials() has been called, this class is
  // considered initialized and can these callbacks are invoked.
  std::vector<base::OnceClosure> pre_initialization_callbacks_;
  std::vector<UiEntriesCallback> get_saved_passwords_list_callbacks_;
  std::vector<ExceptionEntriesCallback> get_password_exception_list_callbacks_;

  // The WebContents used when invoking this API. Used to fetch the
  // NativeWindow for the window where the API was called.
  raw_ptr<content::WebContents> web_contents_;

  // Biometric authenticator used to authenticate user on Mac in settings.
  scoped_refptr<device_reauth::BiometricAuthenticator> biometric_authenticator_;

  base::WeakPtrFactory<PasswordsPrivateDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_
