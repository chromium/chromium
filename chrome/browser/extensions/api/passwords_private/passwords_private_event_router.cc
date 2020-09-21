// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
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
  if (!cached_saved_password_parameters_.get())
    // If there is nothing to send, return early.
    return;

  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_SAVED_PASSWORDS_LIST_CHANGED,
      api::passwords_private::OnSavedPasswordsListChanged::kEventName,
      cached_saved_password_parameters_->CreateDeepCopy());
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
  if (!cached_password_exception_parameters_.get())
    // If there is nothing to send, return early.
    return;

  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_PASSWORD_EXCEPTIONS_LIST_CHANGED,
      api::passwords_private::OnPasswordExceptionsListChanged::kEventName,
      cached_password_exception_parameters_->CreateDeepCopy());
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnPasswordsExportProgress(
    api::passwords_private::ExportProgressStatus status,
    const std::string& folder_name) {
  api::passwords_private::PasswordExportProgress params;
  params.status = status;
  params.folder_name = std::make_unique<std::string>(std::move(folder_name));

  auto event_value = std::make_unique<base::ListValue>();
  event_value->Append(params.ToValue());

  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_PASSWORDS_FILE_EXPORT_PROGRESS,
      api::passwords_private::OnPasswordsFileExportProgress::kEventName,
      std::move(event_value));
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnAccountStorageOptInStateChanged(
    bool opted_in) {
  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_ACCOUNT_STORAGE_OPT_IN_STATE_CHANGED,
      api::passwords_private::OnAccountStorageOptInStateChanged::kEventName,
      api::passwords_private::OnAccountStorageOptInStateChanged::Create(
          opted_in));
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnCompromisedCredentialsChanged(
    std::vector<api::passwords_private::InsecureCredential>
        compromised_credentials) {
  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_COMPROMISED_CREDENTIALS_INFO_CHANGED,
      api::passwords_private::OnCompromisedCredentialsChanged::kEventName,
      api::passwords_private::OnCompromisedCredentialsChanged::Create(
          compromised_credentials));
  event_router_->BroadcastEvent(std::move(extension_event));
}

void PasswordsPrivateEventRouter::OnWeakCredentialsChanged(
    std::vector<api::passwords_private::InsecureCredential> weak_credentials) {
  auto extension_event = std::make_unique<Event>(
      events::PASSWORDS_PRIVATE_ON_WEAK_CREDENTIALS_CHANGED,
      api::passwords_private::OnWeakCredentialsChanged::kEventName,
      api::passwords_private::OnWeakCredentialsChanged::Create(
          weak_credentials));
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

PasswordsPrivateEventRouter* PasswordsPrivateEventRouter::Create(
    content::BrowserContext* context) {
  return new PasswordsPrivateEventRouter(context);
}

}  // namespace extensions
