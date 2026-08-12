// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_

#include <string>
#include <vector>

#include "chrome/common/extensions/api/passwords_private.h"
#include "components/keyed_service/core/keyed_service.h"

namespace extensions {

// An event router that observes changes to saved passwords and password
// exceptions and notifies listeners to the onSavedPasswordsListChanged and
// onPasswordExceptionsListChanged events of changes.
class PasswordsPrivateEventRouter : public KeyedService {
 public:
  PasswordsPrivateEventRouter() = default;

  ~PasswordsPrivateEventRouter() override = default;

  // Notifies listeners of updated passwords.
  // |entries| The new list of saved passwords.
  virtual void OnSavedPasswordsListChanged(
      const std::vector<api::passwords_private::PasswordUiEntry>& entries) = 0;

  // Notifies listeners of updated exceptions.
  // |exceptions| The new list of password exceptions.
  virtual void OnPasswordExceptionsListChanged(
      const std::vector<api::passwords_private::ExceptionEntry>&
          exceptions) = 0;

  // Notifies listeners after the passwords have been written to the export
  // destination.
  // |file_path| In case of successful export, this will describe the path
  // to the written file.
  // |folder_name| In case of failure to export, this will describe destination
  // we tried to write on.
  virtual void OnPasswordsExportProgress(
      api::passwords_private::ExportProgressStatus status,
      const std::string& file_path,
      const std::string& folder_name) = 0;

  // Notifies listeners about a (possible) change to the active state for the
  // account-scoped password storage.
  virtual void OnAccountStorageActiveStateChanged(bool active) = 0;

  // Notifies listeners about a change to the information about insecure
  // credentials.
  virtual void OnInsecureCredentialsChanged(
      std::vector<api::passwords_private::PasswordUiEntry>
          insecure_credentials) = 0;

  // Notifies listeners about a change to the status of the password check.
  virtual void OnPasswordCheckStatusChanged(
      const api::passwords_private::PasswordCheckStatus& status) = 0;

  // Notifies listeners about the timeout for password manager access.
  virtual void OnPasswordManagerAuthTimeout() = 0;

  // Notifies listeners about a change to the password manager actionable error.
  virtual void OnPasswordManagerActionableErrorChanged(
      api::passwords_private::PasswordManagerActionableError error) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_
