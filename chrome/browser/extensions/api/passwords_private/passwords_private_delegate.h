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
#include "base/strings/string16.h"
#include "chrome/browser/ui/passwords/settings/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/ui/export_progress_status.h"
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
      base::OnceCallback<void(base::Optional<base::string16>)>;

  ~PasswordsPrivateDelegate() override {}

  // Gets the saved passwords list.
  using UiEntries = std::vector<api::passwords_private::PasswordUiEntry>;
  using UiEntriesCallback = base::OnceCallback<void(const UiEntries&)>;
  virtual void GetSavedPasswordsList(UiEntriesCallback callback) = 0;

  // Gets the password exceptions list.
  using ExceptionEntries = std::vector<api::passwords_private::ExceptionEntry>;
  using ExceptionEntriesCallback =
      base::Callback<void(const ExceptionEntries&)>;
  virtual void GetPasswordExceptionsList(
      const ExceptionEntriesCallback& callback) = 0;

  // Changes the username and password corresponding to |id|.
  // |id|: The id for the password entry being updated.
  // |new_username|: The new username.
  // |new_password|: The new password.
  virtual void ChangeSavedPassword(
      int id,
      base::string16 new_username,
      base::Optional<base::string16> new_password) = 0;

  // Removes the saved password entry corresponding to the |id| generated for
  // each entry of the password list.
  // |id| the id created when going over the list of saved passwords.
  virtual void RemoveSavedPassword(int id) = 0;

  // Removes the saved password exception entry corresponding set in the
  // given |id|
  // |id| The id for the exception url entry being removed.
  virtual void RemovePasswordException(int id) = 0;

  // Undoes the last removal of a saved password or exception.
  virtual void UndoRemoveSavedPasswordOrException() = 0;

  // Requests the plain text password for entry corresponding to the |id|
  // generated for each entry of the password list.
  // |id| the id created when going over the list of saved passwords.
  // |callback| The callback that gets invoked with the saved password if it
  // could be obtained successfully, or base::nullopt otherwise.
  // |web_contents| The web content object used as the UI; will be used to show
  //     an OS-level authentication dialog if necessary.
  virtual void RequestShowPassword(int id,
                                   PlaintextPasswordCallback callback,
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
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_DELEGATE_H_
