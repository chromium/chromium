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
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/ui/passwords/settings/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/ui/export_progress_status.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "extensions/browser/extension_function.h"

namespace content {
class WebContents;
}

namespace extensions {

// Delegate used by the chrome.passwordsPrivate API to facilitate working with
// saved passwords and password exceptions (reading, changing, removing,
// import/export) and to notify listeners when these values have changed.
class PasswordsPrivateDelegate : public KeyedService {
 public:
  using PlaintextPasswordCallback =
      base::OnceCallback<void(base::Optional<std::u16string>)>;

  using StartPasswordCheckCallback =
      base::OnceCallback<void(password_manager::BulkLeakCheckService::State)>;

  using PlaintextInsecurePasswordCallback = base::OnceCallback<void(
      base::Optional<api::passwords_private::InsecureCredential>)>;

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

  // Changes the username and password corresponding to |ids|.
  // |ids|: The ids for the password entries being updated.
  // |new_username|: The new username.
  // |new_password|: The new password.
  virtual bool ChangeSavedPassword(const std::vector<int>& ids,
                                   const std::u16string& new_username,
                                   const std::u16string& new_password) = 0;

  // Removes the saved password entries corresponding to the |ids| generated for
  // each entry of the password list. Any invalid id will be ignored.
  virtual void RemoveSavedPasswords(const std::vector<int>& ids) = 0;

  // Removes the password exceptions entries corresponding corresponding to
  // |ids|. Any invalid id will be ignored.
  virtual void RemovePasswordExceptions(const std::vector<int>& ids) = 0;

  // Undoes the last removal of a saved password or exception.
  virtual void UndoRemoveSavedPasswordOrException() = 0;

  // Requests the plain text password for entry corresponding to the |id|
  // generated for each entry of the password list.
  // |id| the id created when going over the list of saved passwords.
  // |reason| The reason why the plaintext password is requested.
  // |callback| The callback that gets invoked with the saved password if it
  // could be obtained successfully, or base::nullopt otherwise.
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

  // Requests to start a check for insecure passwords. Invokes |callback|
  // once a check is running or the request was stopped via StopPasswordCheck().
  virtual void StartPasswordCheck(StartPasswordCheckCallback callback) = 0;
  // Stops a check for insecure passwords.
  virtual void StopPasswordCheck() = 0;

  // Returns the current status of the password check.
  virtual api::passwords_private::PasswordCheckStatus
  GetPasswordCheckStatus() = 0;

  // Returns a pointer to the current instance of InsecureCredentialsManager.
  // Needed to get notified when compromised credentials are written out to
  // disk, since BulkLeakCheckService does not know about that step.
  virtual password_manager::InsecureCredentialsManager*
  GetInsecureCredentialsManager() = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_
