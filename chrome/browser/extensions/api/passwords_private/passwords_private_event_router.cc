// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "url/gurl.h"

namespace extensions {

PasswordsPrivateEventRouter::PasswordsPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context), event_router_(nullptr) {
  event_router_ = EventRouter::Get(context_);
}

PasswordsPrivateEventRouter::~PasswordsPrivateEventRouter() {}

void PasswordsPrivateEventRouter::OnSavedPasswordsListChanged(
    const std::vector<api::passwords_private::PasswordUiEntry>& entries) {
  cached_saved_password_parameters_ =
      api::passwords_private::OnSavedPasswordsListChanged::Create(entries);
  SendSavedPasswordListToListeners();
}

void PasswordsPrivateEventRouter::SendSavedPasswordListToListeners() {
  if (!cached_saved_password_parameters_.has_value())
    // If there is nothing to send, return early.
    return;

  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_SAVED_PASSWORDS_LIST_CHANGED,
      api::passwords_private::OnSavedPasswordsListChanged::kEventName,
      std::move(cached_saved_password_parameters_).value());
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnPasswordExceptionsListChanged(
    const std::vector<api::passwords_private::ExceptionEntry>& exceptions) {
  cached_password_exception_parameters_ =
      api::passwords_private::OnPasswordExceptionsListChanged::Create(
          exceptions);
  SendPasswordExceptionListToListeners();
}

void PasswordsPrivateEventRouter::SendPasswordExceptionListToListeners() {
  if (!cached_password_exception_parameters_.has_value())
    // If there is nothing to send, return early.
    return;

  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_PASSWORD_EXCEPTIONS_LIST_CHANGED,
      api::passwords_private::OnPasswordExceptionsListChanged::kEventName,
      std::move(cached_password_exception_parameters_).value());
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnPasswordsExportProgress(
    api::passwords_private::ExportProgressStatus status,
    const std::string& file_path,
    const std::string& folder_name) {
  api::passwords_private::PasswordExportProgress params;
  params.status = status;
  params.file_path = file_path;
  params.folder_name = folder_name;

  base::Value::List event_value;
  event_value.Append(params.ToValue());

  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_PASSWORDS_FILE_EXPORT_PROGRESS,
      api::passwords_private::OnPasswordsFileExportProgress::kEventName,
      std::move(event_value));
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnAccountStorageEnabledStateChanged(
    bool enabled) {
  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_ACCOUNT_STORAGE_ENABLED_STATE_CHANGED,
      api::passwords_private::OnAccountStorageEnabledStateChanged::kEventName,
      api::passwords_private::OnAccountStorageEnabledStateChanged::Create(
          enabled));
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnInsecureCredentialsChanged(
    std::vector<api::passwords_private::PasswordUiEntry> insecure_credentials) {
  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_INSECURE_CREDENTIALS_CHANGED,
      api::passwords_private::OnInsecureCredentialsChanged::kEventName,
      api::passwords_private::OnInsecureCredentialsChanged::Create(
          insecure_credentials));
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnPasswordCheckStatusChanged(
    const api::passwords_private::PasswordCheckStatus& status) {
  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
      api::passwords_private::OnPasswordCheckStatusChanged::kEventName,
      api::passwords_private::OnPasswordCheckStatusChanged::Create(status));
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnPasswordManagerAuthTimeout() {
  event_router_->BroadcastEvent(std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_PASSWORD_MANAGER_AUTH_TIMEOUT,
      api::passwords_private::OnPasswordManagerAuthTimeout::kEventName,
      api::passwords_private::OnPasswordManagerAuthTimeout::Create()));
}

}  // namespace extensions
