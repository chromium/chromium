// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
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
  static PasswordsPrivateEventRouter* Create(
      content::BrowserContext* browser_context);
  ~PasswordsPrivateEventRouter() override;

  // Notifies listeners of updated passwords.
  // |entries| The new list of saved passwords.
  void OnSavedPasswordsListChanged(
      const std::vector<api::passwords_private::PasswordUiEntry>& entries);

  // Notifies listeners of updated exceptions.
  // |exceptions| The new list of password exceptions.
  void OnPasswordExceptionsListChanged(
      const std::vector<api::passwords_private::ExceptionEntry>& exceptions);

  // Notifies listeners after fetching a plain-text password.
  // |id| the id for the password entry being shown.
  // |plaintext_password| The human-readable password.
  void OnPlaintextPasswordFetched(int id,
                                  const std::string& plaintext_password);

  // Notifies listeners after the passwords have been written to the export
  // destination.
  // |folder_name| In case of failure to export, this will describe destination
  // we tried to write on.
  void OnPasswordsExportProgress(
      api::passwords_private::ExportProgressStatus status,
      const std::string& folder_name);

 protected:
  explicit PasswordsPrivateEventRouter(content::BrowserContext* context);

 private:
  void SendSavedPasswordListToListeners();
  void SendPasswordExceptionListToListeners();

  content::BrowserContext* context_;

  EventRouter* event_router_;

  // Cached parameters which are saved so that when new listeners are added, the
  // most up-to-date lists can be sent to them immediately.
  std::unique_ptr<base::ListValue> cached_saved_password_parameters_;
  std::unique_ptr<base::ListValue> cached_password_exception_parameters_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_
