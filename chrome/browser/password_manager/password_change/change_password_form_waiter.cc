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

bool IsLikelyChangePasswordForm(
    const password_manager::PasswordForm* parsed_form) {
  CHECK(parsed_form);

  // Change password form shouldn't contain username field. This doesn't apply
  // to <form>-less forms.
  if (parsed_form->form_data.renderer_id() &&
      parsed_form->username_element_renderer_id) {
    auto username_element =
        std::ranges::find(parsed_form->form_data.fields(),
                          parsed_form->username_element_renderer_id,
                          &autofill::FormFieldData::renderer_id);
    CHECK(username_element != parsed_form->form_data.fields().end());
    // Username must be focusable, otherwise it's hidden or non-editable which
    // isn't an issue.
    if (username_element->is_focusable()) {
      return false;
    }
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

ChangePasswordFormWaiter::ChangePasswordFormWaiter(
    content::WebContents* web_contents,
    password_manager::PasswordManagerClient* client,
    PasswordFormFoundCallback callback,
    base::TimeDelta timeout)
    : timeout_(timeout),
      web_contents_(web_contents),
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
            FROM_HERE, base::BindOnce(std::move(callback_), manager.get()));
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

ChangePasswordFormWaiter::~ChangePasswordFormWaiter() {
  CHECK(client_);
  if (auto* cache = GetFormCache(client_)) {
    cache->RemoveObserver(this);
  }
}

void ChangePasswordFormWaiter::OnPasswordFormParsed(
    password_manager::PasswordFormManager* form_manager) {
  CHECK(callback_);
  CHECK(form_manager);

  if (IsLikelyChangePasswordForm(form_manager->GetParsedObservedForm())) {
    // Do not invoke anything after calling the `callback_` as object might be
    // destroyed immediately after.
    std::move(callback_).Run(form_manager);
  }
}

void ChangePasswordFormWaiter::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (timeout_timer_.IsRunning()) {
    // Page is still loading, reset the timer.
    timeout_timer_.Reset();
  }
  timeout_timer_.Start(FROM_HERE, timeout_, this,
                       &ChangePasswordFormWaiter::OnTimeout);
}

void ChangePasswordFormWaiter::OnTimeout() {
  CHECK(callback_);
  std::move(callback_).Run(nullptr);
}
