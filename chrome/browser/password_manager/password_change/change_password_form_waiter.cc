// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#include "components/password_manager/core/browser/features/password_features.h"
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

bool FieldFocusable(autofill::FieldRendererId renderer_id,
                    const autofill::FormData& form_data) {
  const auto& fields = form_data.fields();
  auto field = std::ranges::find(fields, renderer_id,
                                 &autofill::FormFieldData::renderer_id);
  if (field == fields.end()) {
    return false;
  }
  return field->is_focusable();
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

  // Either password confirmation field or old password field is enough to
  // assume this is a change password form.
  if (parsed_form->confirmation_password_element_renderer_id ||
      parsed_form->password_element_renderer_id) {
    return true;
  }

  // If there is a username field, it can't be empty. Websites where username is
  // part of change password form usually have it prefilled.
  if (parsed_form->username_element_renderer_id &&
      parsed_form->username_value.empty() &&
      FieldFocusable(parsed_form->username_element_renderer_id,
                     parsed_form->form_data)) {
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
  if (base::FeatureList::IsEnabled(
          password_manager::features::kDownloadModelForPasswordChange)) {
    form_waiter_->WaitForLocalMLModelAvailability();
  } else {
    form_waiter_->Init();
  }
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
  model_loaded_subscription_ = {};
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

void ChangePasswordFormWaiter::WaitForLocalMLModelAvailability() {
  if (auto* client =
          autofill::ContentAutofillClient::FromWebContents(web_contents())) {
    auto* model_handler =
        client->GetPasswordManagerFieldClassificationModelHandler();

    if (model_handler && !model_handler->ModelAvailable()) {
      model_loaded_subscription_ =
          model_handler->RegisterModelChangeCallback(base::BindRepeating(
              &ChangePasswordFormWaiter::Init, weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }

  // No downloading is required. Initialize waiter immediately.
  Init();
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

  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kCheckVisibilityInChangePasswordFormWaiter)) {
    if (!form_manager->GetDriver()) {
      return;
    }

    auto new_field_id =
        form_manager->GetParsedObservedForm()->new_password_element_renderer_id;
    form_manager->GetDriver()->CheckViewAreaVisible(
        new_field_id,
        base::BindOnce(
            &ChangePasswordFormWaiter::OnCheckViewAreaVisibleCallback,
            weak_ptr_factory_.GetWeakPtr(), new_field_id));
    return;
  }

  if (ignore_hidden_forms_ &&
      !FieldFocusable(form_manager->GetParsedObservedForm()
                          ->new_password_element_renderer_id,
                      form_manager->GetParsedObservedForm()->form_data)) {
    return;
  }

  std::move(callback_).Run(form_manager);
}

void ChangePasswordFormWaiter::OnCheckViewAreaVisibleCallback(
    autofill::FieldRendererId new_password_element_id,
    bool is_visible) {
  if (!is_visible) {
    return;
  }

  if (auto* form_manager = GetCorrespondingFormManager(
          weak_ptr_factory_.GetWeakPtr(), new_password_element_id)) {
    std::move(callback_).Run(form_manager);
  }
}

void ChangePasswordFormWaiter::DidStartLoading() {
  if (timeout_timer_.IsRunning()) {
    // Page is still loading, stop the timer.
    timeout_timer_.Stop();
  }
}

void ChangePasswordFormWaiter::DidStopLoading() {
  if (web_contents()->IsLoading() || model_loaded_subscription_) {
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
