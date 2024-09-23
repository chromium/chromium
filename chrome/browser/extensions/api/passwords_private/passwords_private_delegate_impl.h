// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/password_access_auth_timeout_handler.h"
#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#include "chrome/browser/ui/passwords/settings/password_manager_porter.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/export/export_progress_status.h"
#include "components/password_manager/core/browser/export/password_manager_exporter.h"
#include "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {
class WebAppInstallManager;
}

namespace password_manager {
class RecipientsFetcher;
}

namespace extensions {

// Concrete PasswordsPrivateDelegate implementation.
class PasswordsPrivateDelegateImpl
    : public PasswordsPrivateDelegate,
      public password_manager::SavedPasswordsPresenter::Observer,
      public syncer::SyncServiceObserver,
      public web_app::WebAppInstallManagerObserver {
 public:
  using AuthResultCallback = base::OnceCallback<void(bool)>;

  explicit PasswordsPrivateDelegateImpl(Profile* profile);

  PasswordsPrivateDelegateImpl(const PasswordsPrivateDelegateImpl&) = delete;
  PasswordsPrivateDelegateImpl& operator=(const PasswordsPrivateDelegateImpl&) =
      delete;

  // PasswordsPrivateDelegate implementation.
  void GetSavedPasswordsList(UiEntriesCallback callback) override;
  CredentialsGroups GetCredentialGroups() override;
  void GetPasswordExceptionsList(ExceptionEntriesCallback callback) override;
  std::optional<api::passwords_private::UrlCollection> GetUrlCollection(
      const std::string& url) override;
  bool IsAccountStoreDefault(content::WebContents* web_contents) override;
  bool AddPassword(const std::string& url,
                   const std::u16string& username,
                   const std::u16string& password,
                   const std::u16string& note,
                   bool use_account_store,
                   content::WebContents* web_contents) override;
  bool ChangeCredential(
      const api::passwords_private::PasswordUiEntry& credential) override;
  void RemoveCredential(
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
  void FetchFamilyMembers(FetchFamilyResultsCallback callback) override;
  void SharePassword(int id, const ShareRecipients& recipients) override;
  void ImportPasswords(api::passwords_private::PasswordStoreSet to_store,
                       ImportResultsCallback results_callback,
                       content::WebContents* web_contents) override;
  void ContinueImport(const std::vector<int>& selected_ids,
                      ImportResultsCallback results_callback,
                      content::WebContents* web_contents) override;
  void ResetImporter(bool delete_file) override;
  void ExportPasswords(
      base::OnceCallback<void(const std::string&)> accepted_callback,
      content::WebContents* web_contents) override;
  api::passwords_private::ExportProgressStatus GetExportProgressStatus()
      override;
  bool IsAccountStorageEnabled() override;
  // TODO(crbug.com/40138722): Mimic the signature in PasswordFeatureManager.
  void SetAccountStorageEnabled(bool enabled,
                                content::WebContents* web_contents) override;
  std::vector<api::passwords_private::PasswordUiEntry> GetInsecureCredentials()
      override;
  std::vector<api::passwords_private::PasswordUiEntryList>
  GetCredentialsWithReusedPassword() override;
  bool MuteInsecureCredential(
      const api::passwords_private::PasswordUiEntry& credential) override;
  bool UnmuteInsecureCredential(
      const api::passwords_private::PasswordUiEntry& credential) override;
  void StartPasswordCheck(StartPasswordCheckCallback callback) override;
  api::passwords_private::PasswordCheckStatus GetPasswordCheckStatus() override;
  password_manager::InsecureCredentialsManager* GetInsecureCredentialsManager()
      override;
  void RestartAuthTimer() override;
  void SwitchBiometricAuthBeforeFillingState(
      content::WebContents* web_contents,
      AuthenticationCallback callback) override;
  void ShowAddShortcutDialog(content::WebContents* web_contents) override;
  void ShowExportedFileInShell(content::WebContents* web_contents,
                               std::string file_path) override;
  void ChangePasswordManagerPin(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> success_callback) override;
  void IsPasswordManagerPinAvailable(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> pin_available_callback) override;
  void DisconnectCloudAuthenticator(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> success_callback) override;
  bool IsConnectedToCloudAuthenticator(
      content::WebContents* web_contents) override;
  void DeleteAllPasswordManagerData(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> success_callback) override;

  base::WeakPtr<PasswordsPrivateDelegate> AsWeakPtr() override;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator(
      content::WebContents* web_contents,
      base::TimeDelta auth_validity_period);
#endif

#if defined(UNIT_TEST)
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  void SetDeviceAuthenticatorForTesting(
      std::unique_ptr<device_reauth::DeviceAuthenticator>
          device_authenticator) {
    test_device_authenticator_ = std::move(device_authenticator);
  }
#endif

  int GetIdForCredential(
      const password_manager::CredentialUIEntry& credential) {
    return credential_id_generator_.GenerateId(credential);
  }

  void SetPorterForTesting(
      std::unique_ptr<PasswordManagerPorterInterface> porter) {
    password_manager_porter_ = std::move(porter);
  }

  void SetRecipientsFetcherForTesting(
      std::unique_ptr<password_manager::RecipientsFetcher>
          sharing_password_recipients_fetcher) {
    sharing_password_recipients_fetcher_ =
        std::move(sharing_password_recipients_fetcher);
  }

#endif  // defined(UNIT_TEST)

 private:
  ~PasswordsPrivateDelegateImpl() override;

  // password_manager::SavedPasswordsPresenter::Observer implementation.
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // web_app::WebAppInstallManagerObserver implementation.
  void OnWebAppInstalledWithOsHooks(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  void SetCredentials(
      std::vector<password_manager::CredentialUIEntry> credentials);

  void MaybeShowPasswordShareButtonIPH(
      base::WeakPtr<content::WebContents> web_contents);

  // Callback for when the password list has been written to the destination.
  void OnPasswordsExportProgress(
      const password_manager::PasswordExportInfo& progress);

  // Callback for RequestPlaintextPassword() after authentication check.
  void OnRequestPlaintextPasswordAuthResult(
      int id,
      api::passwords_private::PlaintextReason reason,
      PlaintextPasswordCallback callback,
      bool authenticated);

  // Callback for RequestCredentialDetails() after authentication check.
  void OnRequestCredentialDetailsAuthResult(
      const std::vector<int>& ids,
      UiEntriesCallback callback,
      base::WeakPtr<content::WebContents> web_contents,
      bool authenticated);

  // Callback for ExportPasswords() after authentication check.
  void OnExportPasswordsAuthResult(
      base::OnceCallback<void(const std::string&)> accepted_callback,
      base::WeakPtr<content::WebContents> web_contents,
      bool authenticated);

  // Callback for ContinueImport() after authentication check.
  void OnImportPasswordsAuthResult(ImportResultsCallback results_callback,
                                   const std::vector<int>& selected_ids,
                                   bool authenticated);

  // Callback for DeleteAllPasswordManagerData() after authentication check.
  void OnDeleteAllDataAuthResult(
      base::OnceCallback<void(bool)> success_callback,
      bool authenticated);

  // SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  void OnFetchingFamilyMembersCompleted(
      FetchFamilyResultsCallback callback,
      std::vector<password_manager::RecipientInfo> recipients_info,
      password_manager::FetchFamilyMembersRequestStatus request_status);

  // Records user action and emits histogram values for retrieving |entry|.
  void EmitHistogramsForCredentialAccess(
      const password_manager::CredentialUIEntry& entry,
      api::passwords_private::PlaintextReason reason);

  // Callback for biometric authentication after authentication check.
  void OnReauthCompleted(bool authenticated);

  // Invokes PasswordsPrivateEventRouter::OnPasswordManagerAuthTimeout().
  void OsReauthTimeoutCall();

  // Authenticate the user using os-authentication.
  void AuthenticateUser(content::WebContents* web_contents,
                        base::TimeDelta auth_validity_period,
                        const std::u16string& message,
                        AuthResultCallback auth_callback);

  extensions::api::passwords_private::PasswordUiEntry
  CreatePasswordUiEntryFromCredentialUiEntry(
      password_manager::CredentialUIEntry credential);

  // Not owned by this class.
  raw_ptr<Profile> profile_;

  // Used to add/edit passwords and to create |password_check_delegate_|.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_;

  // Used to control the export and import flows.
  std::unique_ptr<PasswordManagerPorterInterface> password_manager_porter_;

  PasswordAccessAuthTimeoutHandler auth_timeout_handler_;

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

  // Vectors of callbacks which are queued up before the password store has been
  // initialized.
  std::vector<UiEntriesCallback> get_saved_passwords_list_callbacks_;
  std::vector<ExceptionEntriesCallback> get_password_exception_list_callbacks_;

  // Device authenticator used to authenticate users in settings.
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  std::unique_ptr<password_manager::RecipientsFetcher>
      sharing_password_recipients_fetcher_;

  std::unique_ptr<device_reauth::DeviceAuthenticator>
      test_device_authenticator_;

  base::WeakPtrFactory<PasswordsPrivateDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_IMPL_H_
