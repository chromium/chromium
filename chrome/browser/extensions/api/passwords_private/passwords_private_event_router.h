// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// An event router that observes changes to saved passwords and password
// exceptions and notifies listeners to the onSavedPasswordsListChanged and
// onPasswordExceptionsListChanged events of changes.
class PasswordsPrivateEventRouter : public KeyedService {
 public:
  explicit PasswordsPrivateEventRouter(content::BrowserContext* context);

  PasswordsPrivateEventRouter(const PasswordsPrivateEventRouter&) = delete;
  PasswordsPrivateEventRouter& operator=(const PasswordsPrivateEventRouter&) =
      delete;

  ~PasswordsPrivateEventRouter() override;

  // Notifies listeners of updated passwords.
  // |entries| The new list of saved passwords.
  void OnSavedPasswordsListChanged(
      const std::vector<api::passwords_private::PasswordUiEntry>& entries);

  // Notifies listeners of updated exceptions.
  // |exceptions| The new list of password exceptions.
  void OnPasswordExceptionsListChanged(
      const std::vector<api::passwords_private::ExceptionEntry>& exceptions);

  // Notifies listeners after the passwords have been written to the export
  // destination.
  // |file_path| In case of successful export, this will describe the path
  // to the written file.
  // |folder_name| In case of failure to export, this will describe destination
  // we tried to write on.
  void OnPasswordsExportProgress(
      api::passwords_private::ExportProgressStatus status,
      const std::string& file_path,
      const std::string& folder_name);

  // Notifies listeners about a (possible) change to the enabled state for the
  // account-scoped password storage.
  void OnAccountStorageEnabledStateChanged(bool enabled);

  // Notifies listeners about a change to the information about insecure
  // credentials.
  void OnInsecureCredentialsChanged(
      std::vector<api::passwords_private::PasswordUiEntry>
          insecure_credentials);

  // Notifies listeners about a change to the status of the password check.
  void OnPasswordCheckStatusChanged(
      const api::passwords_private::PasswordCheckStatus& status);

  // Notifies listeners about the timeout for password manager access.
  void OnPasswordManagerAuthTimeout();

 private:
  void SendSavedPasswordListToListeners();
  void SendPasswordExceptionListToListeners();

  raw_ptr<content::BrowserContext> context_;

  raw_ptr<EventRouter> event_router_;

  // Cached parameters which are saved so that when new listeners are added, the
  // most up-to-date lists can be sent to them immediately.
  std::optional<base::Value::List> cached_saved_password_parameters_;
  std::optional<base::Value::List> cached_password_exception_parameters_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_
