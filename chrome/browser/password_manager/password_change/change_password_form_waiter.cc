// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"

#include "base/containers/adapters.h"
#include "base/task/single_thread_task_runner.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "content/public/browser/web_contents.h"

namespace {

using password_manager::PasswordFormCache;

PasswordFormCache* GetPasswordFormCache(
    password_manager::PasswordManagerClient* client) {
  CHECK(client);
  if (!client->GetPasswordManager()) {
    return nullptr;
  }

  auto* cache = client->GetPasswordManager()->GetPasswordFormCache();
  CHECK(cache);
  return cache;
}

bool IsNewPasswordFieldVisible(
    const password_manager::PasswordForm* parsed_form) {
  CHECK(parsed_form);

  const std::vector<autofill::FormFieldData>& fields =
      parsed_form->form_data.fields();
  if (parsed_form->new_password_element_renderer_id) {
    auto field =
        std::ranges::find(fields, parsed_form->new_password_element_renderer_id,
                          &autofill::FormFieldData::renderer_id);
    return field != fields.end() ? field->is_focusable() : false;
  }
  // No new password field found, this is not a password change form
  return false;
}

bool IsLikelyChangePasswordForm(
    const password_manager::PasswordFormManager* form_manager) {
  auto* parsed_form = form_manager->GetParsedObservedForm();

  if (!parsed_form) {
    return false;
  }

  // New password field must be present in a change password form.
  if (!parsed_form->new_password_element_renderer_id) {
    return false;
  }

  // If there are multiple fields, either confirmation password or the old
  // password must be present in a change password form.
  if (parsed_form->form_data.fields().size() > 1 &&
      !parsed_form->confirmation_password_element_renderer_id &&
      !parsed_form->password_element_renderer_id) {
    return false;
  }

  return true;
}

}  // namespace

ChangePasswordFormWaiter::Builder::Builder(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    PasswordFormFoundCallback callback) {
  CHECK(web_contents);
  CHECK(client);
  CHECK(callback);
  form_waiter_ = absl::WrapUnique(
      new ChangePasswordFormWaiter(web_contents, client, std::move(callback)));
}

ChangePasswordFormWaiter::Builder::~Builder() = default;

ChangePasswordFormWaiter::Builder&
ChangePasswordFormWaiter::Builder::SetTimeoutCallback(
    base::OnceClosure timeout_callback) {
  form_waiter_->timeout_ =
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout;
  form_waiter_->timeout_callback_ = std::move(timeout_callback);
  return *this;
}

ChangePasswordFormWaiter::Builder&
ChangePasswordFormWaiter::Builder::IgnoreHiddenForms() {
  form_waiter_->ignore_hidden_forms_ = true;
  return *this;
}

ChangePasswordFormWaiter::Builder&
ChangePasswordFormWaiter::Builder::SetFieldsToIgnore(
    const std::vector<autofill::FieldRendererId>& fields_to_ignore) {
  form_waiter_->fields_to_ignore_ = fields_to_ignore;
  return *this;
}

std::unique_ptr<ChangePasswordFormWaiter>
ChangePasswordFormWaiter::Builder::Build() {
  form_waiter_->Init();
  return std::move(form_waiter_);
}

ChangePasswordFormWaiter::ChangePasswordFormWaiter(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    PasswordFormFoundCallback callback)
    : content::WebContentsObserver(web_contents),
      client_(client),
      callback_(std::move(callback)) {}

ChangePasswordFormWaiter::~ChangePasswordFormWaiter() {
  CHECK(client_);
  if (auto* cache = GetPasswordFormCache(client_)) {
    cache->RemoveObserver(this);
  }
}

void ChangePasswordFormWaiter::Init() {
  if (PasswordFormCache* cache = GetPasswordFormCache(client_)) {
    for (const auto& manager : cache->GetFormManagers()) {
      if (!IsLikelyChangePasswordForm(manager.get())) {
        continue;
      }

      // There is no control over the lifetime of PasswordFormManager. Use a
      // helper function which checks the cache again.
      auto callback = base::BindOnce(
          &ChangePasswordFormWaiter::GetCorrespondingFormManager,
          weak_ptr_factory_.GetWeakPtr(),
          manager->GetParsedObservedForm()->new_password_element_renderer_id);

      // The form has been already parsed. Invoke OnPasswordFormParsed to check
      // if the form is eligible.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(callback).Then(base::BindOnce(
                         &ChangePasswordFormWaiter::OnPasswordFormParsed,
                         weak_ptr_factory_.GetWeakPtr())));
    }
    cache->AddObserver(this);
  }
  if (!web_contents()->IsLoading()) {
    DidStopLoading();
  }
}

void ChangePasswordFormWaiter::OnPasswordFormParsed(
    password_manager::PasswordFormManager* form_manager) {
  CHECK(callback_);

  if (!form_manager) {
    return;
  }

  if (!IsLikelyChangePasswordForm(form_manager)) {
    return;
  }

  if (std::ranges::count(fields_to_ignore_,
                         form_manager->GetParsedObservedForm()
                             ->new_password_element_renderer_id)) {
    return;
  }

  if (ignore_hidden_forms_ &&
      !IsNewPasswordFieldVisible(form_manager->GetParsedObservedForm())) {
    return;
  }

  std::move(callback_).Run(form_manager);
}

void ChangePasswordFormWaiter::DidStartLoading() {
  if (timeout_timer_.IsRunning()) {
    // Page is still loading, stop the timer.
    timeout_timer_.Stop();
  }
}

void ChangePasswordFormWaiter::DidStopLoading() {
  if (web_contents()->IsLoading()) {
    return;
  }
  timeout_timer_.Start(FROM_HERE, timeout_, this,
                       &ChangePasswordFormWaiter::OnTimeout);
}

void ChangePasswordFormWaiter::OnTimeout() {
  if (timeout_callback_) {
    std::move(timeout_callback_).Run();
  }
}

// static
password_manager::PasswordFormManager*
ChangePasswordFormWaiter::GetCorrespondingFormManager(
    base::WeakPtr<ChangePasswordFormWaiter> waiter,
    autofill::FieldRendererId new_password_element_id) {
  if (!waiter) {
    return nullptr;
  }

  if (auto* cache = GetPasswordFormCache(waiter->client_)) {
    for (const auto& manager : cache->GetFormManagers()) {
      if (manager->GetParsedObservedForm() &&
          manager->GetParsedObservedForm()->new_password_element_renderer_id ==
              new_password_element_id) {
        return manager.get();
      }
    }
  }
  return nullptr;
}
