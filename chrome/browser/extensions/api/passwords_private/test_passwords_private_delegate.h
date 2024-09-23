// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_TEST_PASSWORDS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_TEST_PASSWORDS_PRIVATE_DELEGATE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/passwords_private.h"

namespace extensions {
// A test PasswordsPrivateDelegate implementation which uses mock data.
// TestDelegate starts out with kNumMocks mocks of each type (saved password
// and password exception) and removes one mock each time RemoveCredential()
// or RemovePasswordException() is called.
class TestPasswordsPrivateDelegate : public PasswordsPrivateDelegate {
 public:
  TestPasswordsPrivateDelegate();

  // PasswordsPrivateDelegate implementation.
  void GetSavedPasswordsList(UiEntriesCallback callback) override;
  CredentialsGroups GetCredentialGroups() override;
  void GetPasswordExceptionsList(ExceptionEntriesCallback callback) override;
  // Fake implementation of `GetUrlCollection`. This returns a value if `url` is
  // not empty.
  std::optional<api::passwords_private::UrlCollection> GetUrlCollection(
      const std::string& url) override;
  // Fake implementation. This returns the value set by
  // `SetIsAccountStoreDefault`.
  bool IsAccountStoreDefault(content::WebContents* web_contents) override;
  // Fake implementation of AddPassword. This returns true if `url` and
  // `password` aren't empty.
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
      api::passwords_private::PasswordStoreSet from_store) override;
  void RemovePasswordException(int id) override;
  // Simplified version of undo logic, only use for testing.
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
  void ExportPasswords(base::OnceCallback<void(const std::string&)> callback,
                       content::WebContents* web_contents) override;
  api::passwords_private::ExportProgressStatus GetExportProgressStatus()
      override;
  bool IsAccountStorageEnabled() override;
  void SetAccountStorageEnabled(bool enabled,
                                content::WebContents* web_contents) override;
  std::vector<api::passwords_private::PasswordUiEntry> GetInsecureCredentials()
      override;
  std::vector<api::passwords_private::PasswordUiEntryList>
  GetCredentialsWithReusedPassword() override;
  // Fake implementation of `MuteInsecureCredential`. This succeeds if the
  // delegate knows of a insecure credential with the same id.
  bool MuteInsecureCredential(
      const api::passwords_private::PasswordUiEntry& credential) override;
  // Fake implementation of `UnmuteInsecureCredential`. This succeeds if the
  // delegate knows of a insecure credential with the same id.
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

  void SetProfile(Profile* profile);
  void SetAccountStorageEnabled(bool enabled);
  void SetIsAccountStoreDefault(bool is_default);
  void AddCompromisedCredential(int id);

  void ClearSavedPasswordsList() { current_entries_.clear(); }
  void ResetPlaintextPassword() { plaintext_password_.reset(); }
  bool ImportPasswordsTriggered() const { return import_passwords_triggered_; }
  bool ContinueImportTriggered() const { return continue_import_triggered_; }
  bool ResetImporterTriggered() const { return reset_importer_triggered_; }
  bool ExportPasswordsTriggered() const { return export_passwords_triggered_; }
  bool FetchFamilyMembersTriggered() const {
    return fetch_family_members_triggered_;
  }
  bool SharePasswordTriggered() const { return share_password_triggered_; }
  bool StartPasswordCheckTriggered() const {
    return start_password_check_triggered_;
  }
  void SetStartPasswordCheckState(
      password_manager::BulkLeakCheckService::State state) {
    start_password_check_state_ = state;
  }

  const std::vector<int>& last_moved_passwords() const {
    return last_moved_passwords_;
  }

  bool get_authenticator_interaction_status() const {
    return authenticator_interacted_;
  }

  bool get_add_shortcut_dialog_shown() const {
    return add_shortcut_dialog_shown_;
  }

  bool get_exported_file_shown_in_shell() const {
    return exported_file_shown_in_shell_;
  }

  bool get_change_password_manager_pin_called() const {
    return change_password_manager_pin_called_;
  }

  bool get_disconnect_cloud_authenticator_called() const {
    return disconnect_cloud_authenticator_called_;
  }

  bool get_delete_all_password_manager_data_called() const {
    return delete_all_password_manager_data_called_;
  }

 protected:
  ~TestPasswordsPrivateDelegate() override;

 private:
  void SendSavedPasswordsList();
  void SendPasswordExceptionsList();
  bool IsCredentialPresentInInsecureCredentialsList(
      const api::passwords_private::PasswordUiEntry& credential);
  // The current list of entries/exceptions. Cached here so that when new
  // observers are added, this delegate can send the current lists without
  // having to request them from |password_manager_presenter_| again.
  std::vector<api::passwords_private::PasswordUiEntry> current_entries_;
  std::vector<api::passwords_private::ExceptionEntry> current_exceptions_;

  // Simplified version of an undo manager that only allows undoing and redoing
  // the very last deletion. When the entries are nullopt, this means there is
  // no previous deletion to undo.
  std::optional<api::passwords_private::PasswordUiEntry> last_deleted_entry_;
  std::optional<api::passwords_private::ExceptionEntry> last_deleted_exception_;

  std::optional<std::u16string> plaintext_password_ = u"plaintext";

  api::passwords_private::ImportResults import_results_;

  api::passwords_private::FamilyFetchResults family_fetch_results_;

  // List of insecure credentials.
  std::vector<api::passwords_private::PasswordUiEntry> insecure_credentials_;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;

  bool is_account_storage_enabled_ = false;
  bool is_account_store_default_ = false;

  // Flags for detecting whether password sharing operations have been invoked.
  bool fetch_family_members_triggered_ = false;
  bool share_password_triggered_ = false;

  // Flags for detecting whether import/export operations have been invoked.
  bool import_passwords_triggered_ = false;
  bool continue_import_triggered_ = false;
  bool reset_importer_triggered_ = false;
  bool export_passwords_triggered_ = false;

  // Flags for detecting whether password check operations have been invoked.
  bool start_password_check_triggered_ = false;
  password_manager::BulkLeakCheckService::State start_password_check_state_ =
      password_manager::BulkLeakCheckService::State::kRunning;

  // Records the ids of the passwords that were last moved.
  std::vector<int> last_moved_passwords_;

  // Used to track whether user interacted with the ExtendAuthValidity API.
  bool authenticator_interacted_ = false;

  // Used to track whether shortcut creation dialog was shown.
  bool add_shortcut_dialog_shown_ = false;

  // Used to track whether the exported file was shown in shell.
  bool exported_file_shown_in_shell_ = false;

  // Used for checking whether `ChangePasswordManagerPin` is called.
  bool change_password_manager_pin_called_ = false;

  // Used to track whether `DisconnectCloudAuthenticator` was called.
  bool disconnect_cloud_authenticator_called_ = false;

  // Used to track whether `DeleteAllPasswordManagerData` was called.
  bool delete_all_password_manager_data_called_ = false;

  base::WeakPtrFactory<TestPasswordsPrivateDelegate> weak_ptr_factory_{this};
};
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_TEST_PASSWORDS_PRIVATE_DELEGATE_H_
