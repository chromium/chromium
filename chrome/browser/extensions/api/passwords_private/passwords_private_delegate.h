// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/password_manager/core/browser/export/export_progress_status.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "extensions/browser/extension_function.h"

namespace content {
class WebContents;
}

namespace extensions {

// Delegate used by the chrome.passwordsPrivate API to facilitate working with
// saved passwords and password exceptions (reading, adding, changing, removing,
// import/export) and to notify listeners when these values have changed.
class PasswordsPrivateDelegate
    : public base::RefCounted<PasswordsPrivateDelegate> {
 public:
  using ImportResultsCallback =
      base::OnceCallback<void(const api::passwords_private::ImportResults&)>;

  using FetchFamilyResultsCallback = base::OnceCallback<void(
      const api::passwords_private::FamilyFetchResults&)>;

  using ShareRecipients = std::vector<api::passwords_private::RecipientInfo>;

  using PlaintextPasswordCallback =
      base::OnceCallback<void(std::optional<std::u16string>)>;

  using StartPasswordCheckCallback =
      base::OnceCallback<void(password_manager::BulkLeakCheckService::State)>;

  using AuthenticationCallback = base::OnceCallback<void(bool)>;

  // Gets the saved passwords list.
  using UiEntries = std::vector<api::passwords_private::PasswordUiEntry>;
  using UiEntriesCallback = base::OnceCallback<void(const UiEntries&)>;
  virtual void GetSavedPasswordsList(UiEntriesCallback callback) = 0;

  using CredentialsGroups =
      std::vector<api::passwords_private::CredentialGroup>;
  virtual CredentialsGroups GetCredentialGroups() = 0;

  // Gets the password exceptions list.
  using ExceptionEntries = std::vector<api::passwords_private::ExceptionEntry>;
  using ExceptionEntriesCallback =
      base::OnceCallback<void(const ExceptionEntries&)>;
  virtual void GetPasswordExceptionsList(ExceptionEntriesCallback callback) = 0;

  // Checks whether the given |url| meets the requirements to save a password
  // for it (e.g. valid, has proper scheme etc.) and returns the corresponding
  // UrlCollection on success and std::nullopt otherwise.
  virtual std::optional<api::passwords_private::UrlCollection> GetUrlCollection(
      const std::string& url) = 0;

  // Returns whether the account store is a default location for saving
  // passwords. False means the device store is a default one. Must be called
  // when account storage is enabled.
  virtual bool IsAccountStoreDefault(content::WebContents* web_contents) = 0;

  // Adds the |username| and |password| corresponding to the |url| to the
  // specified store and returns true if the operation succeeded. Fails and
  // returns false if the data is invalid or an entry with such origin and
  // username already exists. Updates the default store to the used one on
  // success if account storage is enabled.
  // |url|: The url of the password entry, must be a valid http(s) ip/web
  //        address as is or after adding http(s) scheme.
  // |username|: The username to save, can be empty.
  // |password|: The password to save, must not be empty.
  // |use_account_store|: True for account store, false for device store.
  virtual bool AddPassword(const std::string& url,
                           const std::u16string& username,
                           const std::u16string& password,
                           const std::u16string& note,
                           bool use_account_store,
                           content::WebContents* web_contents) = 0;

  // Updates a credential. Not all attributes can be updated.
  // |credential|: The credential to be updated. Matched to an existing
  // credential by id.
  // Returns std::nullopt if the credential could not be found or updated.
  // Otherwise, returns the newly updated credential. Note that the new
  // credential may have a different ID, so it should replace the old one.
  virtual bool ChangeCredential(
      const api::passwords_private::PasswordUiEntry& credential) = 0;

  // Removes the credential entry corresponding to the |id| in the specified
  // |from_stores|. Any invalid id will be ignored.
  virtual void RemoveCredential(
      int id,
      api::passwords_private::PasswordStoreSet from_stores) = 0;

  // Removes the password exception entry corresponding to |id|. Any invalid id
  // will be ignored.
  virtual void RemovePasswordException(int id) = 0;

  // Undoes the last removal of a saved password or exception.
  virtual void UndoRemoveSavedPasswordOrException() = 0;

  // Requests the plain text password for entry corresponding to the |id|
  // generated for each entry of the password list.
  // |id| the id created when going over the list of saved passwords.
  // |reason| The reason why the plaintext password is requested.
  // |callback| The callback that gets invoked with the saved password if it
  // could be obtained successfully, or std::nullopt otherwise.
  // |web_contents| The web content object used as the UI; will be used to show
  //     an OS-level authentication dialog if necessary.
  virtual void RequestPlaintextPassword(
      int id,
      api::passwords_private::PlaintextReason reason,
      PlaintextPasswordCallback callback,
      content::WebContents* web_contents) = 0;

  // Requests the full PasswordUiEntry (with filled password) with the given id.
  // Returns the full PasswordUiEntry with |callback|. Returns |std::nullopt|
  // if no matching credential with |id| is found.
  // |id| the id created when going over the list of saved passwords.
  // |reason| The reason why the full PasswordUiEntry is requested.
  // |callback| The callback that gets invoked with the PasswordUiEntry if it
  // could be obtained successfully, or std::nullopt otherwise.
  // |web_contents| The web content object used as the UI; will be used to show
  //     an OS-level authentication dialog if necessary.
  virtual void RequestCredentialsDetails(
      const std::vector<int>& ids,
      UiEntriesCallback callback,
      content::WebContents* web_contents) = 0;

  // Moves a list of passwords currently stored on the device to being stored in
  // the signed-in, non-syncing Google Account. The result of any password is a
  // no-op if any of these is true: |id| is invalid; |id| corresponds to a
  // password already stored in the account; or the user is not using the
  // account-scoped password storage.
  virtual void MovePasswordsToAccount(const std::vector<int>& ids,
                                      content::WebContents* web_contents) = 0;

  // Fetches family members of the current user for the password sharing flow.
  // |callback|: Used to communicate the status of a request to fetch family
  //  members, as well as the data returned in the response.
  virtual void FetchFamilyMembers(FetchFamilyResultsCallback callback) = 0;

  // Sends sharing invitations for a credential with given |id| to the
  // |recipients|.
  virtual void SharePassword(int id, const ShareRecipients& recipients) = 0;

  // Trigger the password import procedure, allowing the user to select a file
  // containing passwords to import.
  // |to_store|: destination store (Device or Account) for imported passwords.
  // |results_callback|: Used to communicate the status and summary of the
  // import process.
  virtual void ImportPasswords(
      api::passwords_private::PasswordStoreSet to_store,
      ImportResultsCallback results_callback,
      content::WebContents* web_contents) = 0;

  // Resumes the password import process when user has selected which passwords
  // to replace.
  // |selected_ids|: The ids of passwords that need to be replaced.
  // |results_callback|: Used to communicate the status and summary of the
  // import process.
  virtual void ContinueImport(const std::vector<int>& selected_ids,
                              ImportResultsCallback results_callback,
                              content::WebContents* web_contents) = 0;

  // Resets the PasswordImporter if it is in the CONFLICTS/FINISHED state and
  // the user closes the dialog. Only when the PasswordImporter is in FINISHED
  // state, |deleteFile| option is taken into account.
  // |delete_file|: whether to trigger deletion of the last imported file.
  virtual void ResetImporter(bool delete_file) = 0;

  // Trigger the password export procedure, allowing the user to save a file
  // containing their passwords. |callback| will be called with an error
  // message if the request is rejected, because another export is in progress.
  virtual void ExportPasswords(
      base::OnceCallback<void(const std::string&)> callback,
      content::WebContents* web_contents) = 0;

  // Get the most recent progress status.
  virtual api::passwords_private::ExportProgressStatus
  GetExportProgressStatus() = 0;

  // Whether the current signed-in user (aka unconsented primary account) has
  // the Google account storage for passwords is enabled (as opposed to
  // local/profile storage).
  virtual bool IsAccountStorageEnabled() = 0;

  // Enables/disables use of the Google account storage for passwords. If
  // ExplicitBrowserSigninUIOnDesktop is off, enabling triggers a reauth flow.
  virtual void SetAccountStorageEnabled(bool enabled,
                                        content::WebContents* web_contents) = 0;

  // Obtains information about insecure credentials. This includes the last
  // time a check was run, as well as all insecure credentials that are present
  // in the password store. Credential is considered insecure if it is
  // compromised (leaked or phished) or has reused or weak password.
  virtual std::vector<api::passwords_private::PasswordUiEntry>
  GetInsecureCredentials() = 0;

  // Obtains all credentials which reuse passwords.
  virtual std::vector<api::passwords_private::PasswordUiEntryList>
  GetCredentialsWithReusedPassword() = 0;

  // Attempts to mute |credential| from the password store. Returns whether
  // the mute succeeded.
  virtual bool MuteInsecureCredential(
      const api::passwords_private::PasswordUiEntry& credential) = 0;

  // Attempts to unmute |credential| from the password store. Returns whether
  // the unmute succeeded.
  virtual bool UnmuteInsecureCredential(
      const api::passwords_private::PasswordUiEntry& credential) = 0;

  // Requests to start a check for insecure passwords. Invokes |callback|
  // once a check is running or the request was stopped via StopPasswordCheck().
  virtual void StartPasswordCheck(StartPasswordCheckCallback callback) = 0;

  // Returns the current status of the password check.
  virtual api::passwords_private::PasswordCheckStatus
  GetPasswordCheckStatus() = 0;

  // Returns a pointer to the current instance of InsecureCredentialsManager.
  // Needed to get notified when compromised credentials are written out to
  // disk, since BulkLeakCheckService does not know about that step.
  virtual password_manager::InsecureCredentialsManager*
  GetInsecureCredentialsManager() = 0;

  // Restarts the authentication timer if it is running.
  virtual void RestartAuthTimer() = 0;

  // Switches Biometric authentication before filling state after
  // successful authentication.  Invokes `callback` with true if the
  // authentication was successful, with false otherwise.
  virtual void SwitchBiometricAuthBeforeFillingState(
      content::WebContents* web_contents,
      AuthenticationCallback callback) = 0;

  // Triggers a dialog for installing the shortcut for PasswordManager page.
  virtual void ShowAddShortcutDialog(content::WebContents* web_contents) = 0;

  // Shows the file with the exported passwords in OS shell.
  virtual void ShowExportedFileInShell(content::WebContents* web_contents,
                                       std::string file_path) = 0;

  // Starts the flow for changing the password manager PIN.
  virtual void ChangePasswordManagerPin(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> success_callback) = 0;

  // Replies true if it's allowed to change the password manager PIN, if it
  // exists.
  virtual void IsPasswordManagerPinAvailable(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> pin_available_callback) = 0;

  // Starts the flow for disconnecting a Desktop Chrome client from the cloud
  // authenticator.
  virtual void DisconnectCloudAuthenticator(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> success_callback) = 0;

  virtual bool IsConnectedToCloudAuthenticator(
      content::WebContents* web_contents) = 0;

  virtual void DeleteAllPasswordManagerData(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> success_callback) = 0;

  virtual base::WeakPtr<PasswordsPrivateDelegate> AsWeakPtr() = 0;

 protected:
  virtual ~PasswordsPrivateDelegate() = default;

  friend class base::RefCounted<PasswordsPrivateDelegate>;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_
