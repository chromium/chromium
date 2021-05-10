// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_TEST_PASSWORDS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_TEST_PASSWORDS_PRIVATE_DELEGATE_H_

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {
// A test PasswordsPrivateDelegate implementation which uses mock data.
// TestDelegate starts out with kNumMocks mocks of each type (saved password
// and password exception) and removes one mock each time RemoveSavedPassword()
// or RemovePasswordException() is called.
class TestPasswordsPrivateDelegate : public PasswordsPrivateDelegate {
 public:
  TestPasswordsPrivateDelegate();
  ~TestPasswordsPrivateDelegate() override;

  // PasswordsPrivateDelegate implementation.
  void GetSavedPasswordsList(UiEntriesCallback callback) override;
  void GetPasswordExceptionsList(ExceptionEntriesCallback callback) override;
  // Fake implementation of ChangeSavedPassword. This succeeds if the current
  // list of entries has each of the ids, vector of ids isn't empty and if the
  // new password isn't empty.
  bool ChangeSavedPassword(const std::vector<int>& ids,
                           const std::u16string& new_username,
                           const std::u16string& new_password) override;
  void RemoveSavedPasswords(const std::vector<int>& id) override;
  void RemovePasswordExceptions(const std::vector<int>& ids) override;
  // Simplified version of undo logic, only use for testing.
  void UndoRemoveSavedPasswordOrException() override;
  void RequestPlaintextPassword(int id,
                                api::passwords_private::PlaintextReason reason,
                                PlaintextPasswordCallback callback,
                                content::WebContents* web_contents) override;
  void MovePasswordsToAccount(const std::vector<int>& ids,
                              content::WebContents* web_contents) override;
  void ImportPasswords(content::WebContents* web_contents) override;
  void ExportPasswords(base::OnceCallback<void(const std::string&)> callback,
                       content::WebContents* web_contents) override;
  void CancelExportPasswords() override;
  api::passwords_private::ExportProgressStatus GetExportProgressStatus()
      override;
  bool IsOptedInForAccountStorage() override;
  void SetAccountStorageOptIn(bool opt_in,
                              content::WebContents* web_contents) override;
  std::vector<api::passwords_private::InsecureCredential>
  GetCompromisedCredentials() override;
  std::vector<api::passwords_private::InsecureCredential> GetWeakCredentials()
      override;
  void GetPlaintextInsecurePassword(
      api::passwords_private::InsecureCredential credential,
      api::passwords_private::PlaintextReason reason,
      content::WebContents* web_contents,
      PlaintextInsecurePasswordCallback callback) override;
  // Fake implementation of ChangeInsecureCredential. This succeeds if the
  // delegate knows of a insecure credential with the same id.
  bool ChangeInsecureCredential(
      const api::passwords_private::InsecureCredential& credential,
      base::StringPiece new_password) override;
  // Fake implementation of RemoveInsecureCredential. This succeeds if the
  // delegate knows of a insecure credential with the same id.
  bool RemoveInsecureCredential(
      const api::passwords_private::InsecureCredential& credential) override;
  void StartPasswordCheck(StartPasswordCheckCallback callback) override;
  void StopPasswordCheck() override;
  api::passwords_private::PasswordCheckStatus GetPasswordCheckStatus() override;
  password_manager::InsecureCredentialsManager* GetInsecureCredentialsManager()
      override;

  void SetProfile(Profile* profile);
  void SetOptedInForAccountStorage(bool opted_in);
  void AddCompromisedCredential(int id);

  void ClearSavedPasswordsList() { current_entries_.clear(); }
  void ResetPlaintextPassword() { plaintext_password_.reset(); }
  bool ImportPasswordsTriggered() const { return import_passwords_triggered_; }
  bool ExportPasswordsTriggered() const { return export_passwords_triggered_; }
  bool CancelExportPasswordsTriggered() const {
    return cancel_export_passwords_triggered_;
  }
  bool StartPasswordCheckTriggered() const {
    return start_password_check_triggered_;
  }
  bool StopPasswordCheckTriggered() const {
    return stop_password_check_triggered_;
  }
  void SetStartPasswordCheckState(
      password_manager::BulkLeakCheckService::State state) {
    start_password_check_state_ = state;
  }

  const std::vector<int>& last_moved_passwords() const {
    return last_moved_passwords_;
  }

 private:
  void SendSavedPasswordsList();
  void SendPasswordExceptionsList();

  // The current list of entries/exceptions. Cached here so that when new
  // observers are added, this delegate can send the current lists without
  // having to request them from |password_manager_presenter_| again.
  std::vector<api::passwords_private::PasswordUiEntry> current_entries_;
  std::vector<api::passwords_private::ExceptionEntry> current_exceptions_;

  // Simplified version of an undo manager that only allows undoing and redoing
  // the very last deletion. When the batches are *empty*, this means there is
  // no previous deletion to undo.
  std::vector<api::passwords_private::PasswordUiEntry>
      last_deleted_entries_batch_;
  std::vector<api::passwords_private::ExceptionEntry>
      last_deleted_exceptions_batch_;

  base::Optional<std::u16string> plaintext_password_ = u"plaintext";

  // List of insecure credentials.
  std::vector<api::passwords_private::InsecureCredential> insecure_credentials_;
  Profile* profile_ = nullptr;

  bool is_opted_in_for_account_storage_ = false;

  // Flags for detecting whether import/export operations have been invoked.
  bool import_passwords_triggered_ = false;
  bool export_passwords_triggered_ = false;
  bool cancel_export_passwords_triggered_ = false;

  // Flags for detecting whether password check operations have been invoked.
  bool start_password_check_triggered_ = false;
  bool stop_password_check_triggered_ = false;
  password_manager::BulkLeakCheckService::State start_password_check_state_ =
      password_manager::BulkLeakCheckService::State::kRunning;

  // Records the ids of the passwords that were last moved.
  std::vector<int> last_moved_passwords_;
};
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_TEST_PASSWORDS_PRIVATE_DELEGATE_H_
