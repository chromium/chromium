// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_MOCK_PASSWORDS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_MOCK_PASSWORDS_PRIVATE_DELEGATE_H_

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordsPrivateDelegate
    : public extensions::PasswordsPrivateDelegate {
 public:
  MockPasswordsPrivateDelegate();

  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(password_manager::SavedPasswordsPresenter*,
              GetSavedPasswordsPresenter,
              (),
              (override));
  MOCK_METHOD(void,
              GetSavedPasswordsList,
              (UiEntriesCallback callback),
              (override));
  MOCK_METHOD(CredentialsGroups, GetCredentialGroups, (), (override));
  MOCK_METHOD(void,
              GetPasswordExceptionsList,
              (ExceptionEntriesCallback callback),
              (override));
  MOCK_METHOD(std::optional<extensions::api::passwords_private::UrlCollection>,
              GetUrlCollection,
              (const std::string& url),
              (override));
  MOCK_METHOD(bool,
              AddPassword,
              (const std::string& url,
               const std::u16string& username,
               const std::u16string& password,
               const std::u16string& note,
               bool use_account_store),
              (override));
  MOCK_METHOD(
      bool,
      ChangeCredential,
      (const extensions::api::passwords_private::PasswordUiEntry& credential),
      (override));
  MOCK_METHOD(
      void,
      RemoveCredential,
      (int id,
       extensions::api::passwords_private::PasswordStoreSet from_stores),
      (override));
  MOCK_METHOD(void, RemoveBackupPassword, (int id), (override));
  MOCK_METHOD(void, RemovePasswordException, (int id), (override));
  MOCK_METHOD(void, UndoRemoveSavedPasswordOrException, (), (override));
  MOCK_METHOD(void,
              RequestPlaintextPassword,
              (int id,
               extensions::api::passwords_private::PlaintextReason reason,
               PlaintextPasswordCallback callback),
              (override));
  MOCK_METHOD(void,
              CopyPlaintextBackupPassword,
              (int id, base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              RequestCredentialsDetails,
              (const std::vector<int>& ids,
               UiEntriesCallback callback,
               content::WebContents* web_contents),
              (override));
  MOCK_METHOD(void,
              MovePasswordsToAccount,
              (const std::vector<int>& ids),
              (override));
  MOCK_METHOD(void,
              FetchFamilyMembers,
              (FetchFamilyResultsCallback callback),
              (override));
  MOCK_METHOD(void,
              SharePassword,
              (int id, const ShareRecipients& recipients),
              (override));
  MOCK_METHOD(void,
              ImportPasswords,
              (extensions::api::passwords_private::PasswordStoreSet to_store,
               ImportResultsCallback results_callback,
               content::WebContents* web_contents),
              (override));
  MOCK_METHOD(void,
              ContinueImport,
              (const std::vector<int>& selected_ids,
               ImportResultsCallback results_callback),
              (override));
  MOCK_METHOD(void, ResetImporter, (bool delete_file), (override));
  MOCK_METHOD(
      void,
      ExportPasswords,
      (base::OnceCallback<void(ExportPasswordsResult)> accepted_callback,
       content::WebContents* web_contents),
      (override));
  MOCK_METHOD(extensions::api::passwords_private::ExportProgressStatus,
              GetExportProgressStatus,
              (),
              (override));
  MOCK_METHOD(bool, IsAccountStorageActive, (), (override));
  MOCK_METHOD(void, SetAccountStorageEnabled, (bool enabled), (override));
  MOCK_METHOD(bool, ShouldShowAccountStorageSettingToggle, (), (override));
  MOCK_METHOD(std::vector<extensions::api::passwords_private::PasswordUiEntry>,
              GetInsecureCredentials,
              (),
              (override));
  MOCK_METHOD(
      std::vector<extensions::api::passwords_private::PasswordUiEntryList>,
      GetCredentialsWithReusedPassword,
      (),
      (override));
  MOCK_METHOD(
      bool,
      MuteInsecureCredential,
      (const extensions::api::passwords_private::PasswordUiEntry& credential),
      (override));
  MOCK_METHOD(
      bool,
      UnmuteInsecureCredential,
      (const extensions::api::passwords_private::PasswordUiEntry& credential),
      (override));
  MOCK_METHOD(void,
              StartPasswordCheck,
              (StartPasswordCheckCallback callback),
              (override));

  MOCK_METHOD(extensions::api::passwords_private::PasswordCheckStatus,
              GetPasswordCheckStatus,
              (),
              (override));
  MOCK_METHOD(password_manager::InsecureCredentialsManager*,
              GetInsecureCredentialsManager,
              (),
              (override));
  MOCK_METHOD(void, RestartAuthTimer, (), (override));
  MOCK_METHOD(void,
              SwitchBiometricAuthBeforeFillingState,
              (AuthenticationCallback callback),
              (override));
  MOCK_METHOD(void,
              ShowAddShortcutDialog,
              (content::WebContents * web_contents),
              (override));
  MOCK_METHOD(void,
              ShowLastExportedFileInShell,
              (content::WebContents * web_contents),
              (override));
  MOCK_METHOD(void,
              ChangePasswordManagerPin,
              (content::WebContents * web_contents,
               base::OnceCallback<void(bool)> success_callback),
              (override));
  MOCK_METHOD(void,
              IsPasswordManagerPinAvailable,
              (content::WebContents * web_contents,
               base::OnceCallback<void(bool)> pin_available_callback),
              (override));
  MOCK_METHOD(void,
              DisconnectCloudAuthenticator,
              (base::OnceCallback<void(bool)> success_callback),
              (override));
  MOCK_METHOD(bool, IsConnectedToCloudAuthenticator, (), (override));
  MOCK_METHOD(password_manager::ActionableError,
              GetActionableError,
              (),
              (override));
  MOCK_METHOD(void,
              DeleteAllPasswordManagerData,
              (base::OnceCallback<void(bool)> success_callback),
              (override));
  MOCK_METHOD(std::optional<password_manager::CredentialUIEntry>,
              GetCredentialFromId,
              (int credential_id),
              (override));
  MOCK_METHOD(base::WeakPtr<PasswordsPrivateDelegate>,
              AsWeakPtr,
              (),
              (override));

 protected:
  ~MockPasswordsPrivateDelegate() override;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_MOCK_PASSWORDS_PRIVATE_DELEGATE_H_
