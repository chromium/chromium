// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"

#include "base/containers/adapters.h"
#include "base/task/single_thread_task_runner.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents.h"

namespace {

using password_manager::PasswordFormCache;

PasswordFormCache* GetFormCache(
    password_manager::PasswordManagerClient* client) {
  CHECK(client);
  if (!client->GetPasswordManager()) {
    return nullptr;
  }

  auto* cache = client->GetPasswordManager()->GetPasswordFormCache();
  CHECK(cache);
  return cache;
}

bool IsElementVisible(autofill::FieldRendererId renderer_id,
                      const autofill::FormData& form_data) {
  auto field = std::ranges::find(form_data.fields(), renderer_id,
                                 &autofill::FormFieldData::renderer_id);
  CHECK(field != form_data.fields().end());
  return field->is_focusable();
}

bool IsLikelyLoginForm(const password_manager::PasswordForm* parsed_form) {
  CHECK(parsed_form);

  // New password field can't be present in a login form.
  if (parsed_form->new_password_element_renderer_id &&
      IsElementVisible(parsed_form->new_password_element_renderer_id,
                       parsed_form->form_data)) {
    return false;
  }

  // Confirm password field can't be present in a login form.
  if (parsed_form->confirmation_password_element_renderer_id &&
      IsElementVisible(parsed_form->confirmation_password_element_renderer_id,
                       parsed_form->form_data)) {
    return false;
  }

  // Login form needs at least password or username field.
  return parsed_form->password_element_renderer_id ||
         parsed_form->username_element_renderer_id;
}

bool IsLikelyChangePasswordForm(
    const password_manager::PasswordForm* parsed_form) {
  CHECK(parsed_form);

  // Change password form shouldn't contain username field. This doesn't apply
  // to <form>-less forms.
  if (parsed_form->form_data.renderer_id() &&
      parsed_form->username_element_renderer_id &&
      IsElementVisible(parsed_form->username_element_renderer_id,
                       parsed_form->form_data)) {
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

PasswordFormWaiter::PasswordFormWaiter(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    PasswordFormFoundCallback callback)
    : web_contents_(web_contents),
      client_(client),
      callback_(std::move(callback)) {
  if (PasswordFormCache* cache = GetFormCache(client_)) {
    auto managers = cache->GetFormManagers();
    // Check form managers in reversed order to process newly added managers
    // first.
    for (const auto& manager : base::Reversed(managers)) {
      if (manager->GetParsedObservedForm() &&
          IsLikelyChangePasswordForm(manager->GetParsedObservedForm())) {
        // Change password form is already present on a page. Simply post a
        // callback with result.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback_),
                                      Result{.change_password_form_manager =
                                                 manager.get()}));
        return;
      }
    }
    cache->AddObserver(this);
  }
  if (web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    DocumentOnLoadCompletedInPrimaryMainFrame();
  } else {
    Observe(web_contents);
  }
}

PasswordFormWaiter::~PasswordFormWaiter() {
  CHECK(client_);
  if (auto* cache = GetFormCache(client_)) {
    cache->RemoveObserver(this);
  }
}

void PasswordFormWaiter::OnPasswordFormParsed(
    password_manager::PasswordFormManager* form_manager) {
  CHECK(callback_);
  CHECK(form_manager);

  if (IsLikelyChangePasswordForm(form_manager->GetParsedObservedForm())) {
    // Do not invoke anything after calling the `callback_` as object might be
    // destroyed immediately after.
    std::move(callback_).Run({.change_password_form_manager = form_manager});
    return;
  }

  if (IsLikelyLoginForm(form_manager->GetParsedObservedForm())) {
    login_form_manager_ = form_manager;
  }
}

void PasswordFormWaiter::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (timeout_timer_.IsRunning()) {
    // Page is still loading, reset the timer.
    timeout_timer_.Reset();
  }
  timeout_timer_.Start(FROM_HERE, kChangePasswordFormWaitingTimeout, this,
                       &PasswordFormWaiter::OnTimeout);
}

void PasswordFormWaiter::OnTimeout() {
  CHECK(callback_);
  std::move(callback_).Run({.login_form_manager = login_form_manager_});
}
