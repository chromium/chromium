// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/ui/export_progress_status.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace extensions {

// Delegate used by the chrome.passwordsPrivate API to facilitate working with
// saved passwords and password exceptions (reading, adding, changing, removing,
// import/export) and to notify listeners when these values have changed.
class PasswordsPrivateDelegate : public KeyedService {
 public:
  using PlaintextPasswordCallback =
      base::OnceCallback<void(absl::optional<std::u16string>)>;

  using RefreshScriptsIfNecessaryCallback = base::OnceClosure;

  using StartPasswordCheckCallback =
      base::OnceCallback<void(password_manager::BulkLeakCheckService::State)>;

  using PlaintextInsecurePasswordCallback = base::OnceCallback<void(
      absl::optional<api::passwords_private::InsecureCredential>)>;

  using StartAutomatedPasswordChangeCallback = base::OnceCallback<void(bool)>;

  ~PasswordsPrivateDelegate() override = default;

  // Gets the saved passwords list.
  using UiEntries = std::vector<api::passwords_private::PasswordUiEntry>;
  using UiEntriesCallback = base::OnceCallback<void(const UiEntries&)>;
  virtual void GetSavedPasswordsList(UiEntriesCallback callback) = 0;

  // Gets the password exceptions list.
  using ExceptionEntries = std::vector<api::passwords_private::ExceptionEntry>;
  using ExceptionEntriesCallback =
      base::OnceCallback<void(const ExceptionEntries&)>;
  virtual void GetPasswordExceptionsList(ExceptionEntriesCallback callback) = 0;

  // Checks whether the given |url| meets the requirements to save a password
  // for it (e.g. valid, has proper scheme etc.) and returns the corresponding
  // UrlCollection on success and absl::nullopt otherwise.
  virtual absl::optional<api::passwords_private::UrlCollection>
  GetUrlCollection(const std::string& url) = 0;

  // Returns whether the account store is a default location for saving
  // passwords. False means the device store is a default one. Must be called
  // when the current user has already opted-in for account storage.
  virtual bool IsAccountStoreDefault(content::WebContents* web_contents) = 0;

  // Adds the |username| and |password| corresponding to the |url| to the
  // specified store and returns true if the operation succeeded. Fails and
  // returns false if the data is invalid or an entry with such origin and
  // username already exists. Updates the default store to the used one on
  // success if the user has opted-in for account storage.
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

  // Changes the username and password corresponding to |ids|.
  // |ids|: The ids for the password entries being updated.
  // |params|: The struct which holds the new username, password and note.
  // Returns the ids if the change was successful (can be the same ids if the
  // username and the password didn't change), nullopt otherwise.
  virtual absl::optional<api::passwords_private::CredentialIds>
  ChangeSavedPassword(
      const std::vector<int>& ids,
      const api::passwords_private::ChangeSavedPasswordParams& params) = 0;

  // Removes the saved password entry corresponding to the |id| in the
  // specified |from_stores|. Any invalid id will be ignored.
  virtual void RemoveSavedPassword(
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
  // could be obtained successfully, or absl::nullopt otherwise.
  // |web_contents| The web content object used as the UI; will be used to show
  //     an OS-level authentication dialog if necessary.
  virtual void RequestPlaintextPassword(
      int id,
      api::passwords_private::PlaintextReason reason,
      PlaintextPasswordCallback callback,
      content::WebContents* web_contents) = 0;

  // Moves a list of passwords currently stored on the device to being stored in
  // the signed-in, non-syncing Google Account. The result of any password is a
  // no-op if any of these is true: |id| is invalid; |id| corresponds to a
  // password already stored in the account; or the user is not using the
  // account-scoped password storage.
  virtual void MovePasswordsToAccount(const std::vector<int>& ids,
                                      content::WebContents* web_contents) = 0;

  // Trigger the password import procedure, allowing the user to select a file
  // containing passwords to import.
  virtual void ImportPasswords(content::WebContents* web_contents) = 0;

  // Trigger the password export procedure, allowing the user to save a file
  // containing their passwords. |callback| will be called with an error
  // message if the request is rejected, because another export is in progress.
  virtual void ExportPasswords(
      base::OnceCallback<void(const std::string&)> callback,
      content::WebContents* web_contents) = 0;

  // Cancel any ongoing export.
  virtual void CancelExportPasswords() = 0;

  // Get the most recent progress status.
  virtual api::passwords_private::ExportProgressStatus
  GetExportProgressStatus() = 0;

  // Whether the current signed-in user (aka unconsented primary account) has
  // opted in to use the Google account storage for passwords (as opposed to
  // local/profile storage).
  virtual bool IsOptedInForAccountStorage() = 0;

  // Sets whether the user is opted in to use the Google account storage for
  // passwords. If |opt_in| is true and the user is not currently opted in,
  // will trigger a reauth flow.
  virtual void SetAccountStorageOptIn(bool opt_in,
                                      content::WebContents* web_contents) = 0;

  // Obtains information about compromised credentials. This includes the last
  // time a check was run, as well as all compromised credentials that are
  // present in the password store.
  virtual std::vector<api::passwords_private::InsecureCredential>
  GetCompromisedCredentials() = 0;

  // Obtains information about weak credentials.
  virtual std::vector<api::passwords_private::InsecureCredential>
  GetWeakCredentials() = 0;

  // Requests the plaintext password for |credential| due to |reason|. If
  // successful, |callback| gets invoked with the same |credential|, whose
  // |password| field will be set.
  virtual void GetPlaintextInsecurePassword(
      api::passwords_private::InsecureCredential credential,
      api::passwords_private::PlaintextReason reason,
      content::WebContents* web_contents,
      PlaintextInsecurePasswordCallback callback) = 0;

  // Attempts to change the stored password of |credential| to |new_password|.
  // Returns whether the change succeeded.
  virtual bool ChangeInsecureCredential(
      const api::passwords_private::InsecureCredential& credential,
      base::StringPiece new_password) = 0;

  // Attempts to remove |credential| from the password store. Returns whether
  // the remove succeeded.
  virtual bool RemoveInsecureCredential(
      const api::passwords_private::InsecureCredential& credential) = 0;

  // Attempts to mute |credential| from the password store. Returns whether
  // the mute succeeded.
  virtual bool MuteInsecureCredential(
      const api::passwords_private::InsecureCredential& credential) = 0;

  // Attempts to unmute |credential| from the password store. Returns whether
  // the unmute succeeded.
  virtual bool UnmuteInsecureCredential(
      const api::passwords_private::InsecureCredential& credential) = 0;

  // Records that a change password flow was started for |credential| and
  // whether |is_manual_flow| applies to the flow.
  virtual void RecordChangePasswordFlowStarted(
      const api::passwords_private::InsecureCredential& credential,
      bool is_manual_flow) = 0;

  // Refreshes the cache for automatic password change scripts if that is stale
  // and runs `callback` once that is complete.
  virtual void RefreshScriptsIfNecessary(
      RefreshScriptsIfNecessaryCallback callback) = 0;

  // Requests to start a check for insecure passwords. Invokes |callback|
  // once a check is running or the request was stopped via StopPasswordCheck().
  virtual void StartPasswordCheck(StartPasswordCheckCallback callback) = 0;
  // Stops a check for insecure passwords.
  virtual void StopPasswordCheck() = 0;

  // Returns the current status of the password check.
  virtual api::passwords_private::PasswordCheckStatus
  GetPasswordCheckStatus() = 0;

  // Starts an automated password change flow for `credential` and returns
  // whether the credential was changed successfully by calling `callback` with
  // a boolean parameter.
  virtual void StartAutomatedPasswordChange(
      const api::passwords_private::InsecureCredential& credential,
      StartAutomatedPasswordChangeCallback callback) = 0;

  // Returns a pointer to the current instance of InsecureCredentialsManager.
  // Needed to get notified when compromised credentials are written out to
  // disk, since BulkLeakCheckService does not know about that step.
  virtual password_manager::InsecureCredentialsManager*
  GetInsecureCredentialsManager() = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_
