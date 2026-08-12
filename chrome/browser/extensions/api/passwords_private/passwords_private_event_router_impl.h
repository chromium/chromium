// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class PasswordsPrivateEventRouterImpl : public PasswordsPrivateEventRouter {
 public:
  explicit PasswordsPrivateEventRouterImpl(content::BrowserContext* context);

  PasswordsPrivateEventRouterImpl(const PasswordsPrivateEventRouterImpl&) =
      delete;
  PasswordsPrivateEventRouterImpl& operator=(
      const PasswordsPrivateEventRouterImpl&) = delete;

  ~PasswordsPrivateEventRouterImpl() override;

  // PasswordsPrivateEventRouter overrides:
  void OnSavedPasswordsListChanged(
      const std::vector<api::passwords_private::PasswordUiEntry>& entries)
      override;
  void OnPasswordExceptionsListChanged(
      const std::vector<api::passwords_private::ExceptionEntry>& exceptions)
      override;
  void OnPasswordsExportProgress(
      api::passwords_private::ExportProgressStatus status,
      const std::string& file_path,
      const std::string& folder_name) override;
  void OnAccountStorageActiveStateChanged(bool active) override;
  void OnInsecureCredentialsChanged(
      std::vector<api::passwords_private::PasswordUiEntry> insecure_credentials)
      override;
  void OnPasswordCheckStatusChanged(
      const api::passwords_private::PasswordCheckStatus& status) override;
  void OnPasswordManagerAuthTimeout() override;
  void OnPasswordManagerActionableErrorChanged(
      api::passwords_private::PasswordManagerActionableError error) override;

 private:
  void SendSavedPasswordListToListeners();
  void SendPasswordExceptionListToListeners();

  raw_ptr<content::BrowserContext> context_;

  raw_ptr<EventRouter> event_router_;

  // Cached parameters which are saved so that when new listeners are added, the
  // most up-to-date lists can be sent to them immediately.
  std::optional<base::ListValue> cached_saved_password_parameters_;
  std::optional<base::ListValue> cached_password_exception_parameters_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_IMPL_H_
