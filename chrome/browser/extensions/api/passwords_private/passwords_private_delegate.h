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
#include "base/observer_list_threadsafe.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/ui/export_progress_status.h"
#include "extensions/browser/extension_function.h"

namespace content {
class WebContents;
}

namespace extensions {

// Delegate used by the chrome.passwordsPrivate API to facilitate removing saved
// passwords and password exceptions and to notify listeners when these values
// have changed.
class PasswordsPrivateDelegate : public KeyedService {
 public:
  ~PasswordsPrivateDelegate() override {}

  // Sends the saved passwords list to the event router.
  virtual void SendSavedPasswordsList() = 0;

  // Gets the saved passwords list.
  using UiEntries = std::vector<api::passwords_private::PasswordUiEntry>;
  using UiEntriesCallback = base::Callback<void(const UiEntries&)>;
  virtual void GetSavedPasswordsList(const UiEntriesCallback& callback) = 0;

  // Sends the password exceptions list to the event router.
  virtual void SendPasswordExceptionsList() = 0;

  // Gets the password exceptions list.
  using ExceptionEntries = std::vector<api::passwords_private::ExceptionEntry>;
  using ExceptionEntriesCallback =
      base::Callback<void(const ExceptionEntries&)>;
  virtual void GetPasswordExceptionsList(
      const ExceptionEntriesCallback& callback) = 0;

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
  // |web_contents| The web content object used as the UI; will be used to show
  //     an OS-level authentication dialog if necessary.
  virtual void RequestShowPassword(int id,
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
