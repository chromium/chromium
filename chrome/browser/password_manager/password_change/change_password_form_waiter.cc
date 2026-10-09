// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/password_manager/password_change/features.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "content/public/browser/web_contents.h"

namespace {

using password_change::DiscardedForm;
using password_change::FormDiscardReason;
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

std::optional<FormDiscardReason> GetDiscardReason(
    const password_manager::PasswordFormManager* form_manager) {
  auto* parsed_form = form_manager->GetParsedObservedForm();
  if (!parsed_form) {
    return FormDiscardReason::kUnknown;
  }

  if (!form_manager->GetDriver() ||
      !form_manager->GetDriver()->IsInPrimaryMainFrame()) {
    return FormDiscardReason::kNotInPrimaryMainFrame;
  }

  // New password field must be present in a change password form.
  if (!parsed_form->new_password_element_renderer_id) {
    return FormDiscardReason::kNoNewPasswordField;
  }

  // The new password field must be enabled to be considered a change password
  // form.
  if (base::FeatureList::IsEnabled(
          password_change::features::
              kCheckFieldEnabledInChangePasswordFormWaiter) &&
      !FieldEnabled(parsed_form->new_password_element_renderer_id,
                    parsed_form->form_data)) {
    return FormDiscardReason::kNewPasswordFieldDisabled;
  }

  // Either password confirmation field or old password field is enough to
  // assume this is a change password form.
  if (parsed_form->confirmation_password_element_renderer_id ||
      parsed_form->password_element_renderer_id) {
    return std::nullopt;
  }

  // If there is a username field, it can't be empty. Websites where username is
  // part of change password form usually have it prefilled.
  if (parsed_form->username_element_renderer_id &&
      parsed_form->username_value.empty() &&
      FieldFocusable(parsed_form->username_element_renderer_id,
                     parsed_form->form_data)) {
    return FormDiscardReason::kUsernameFieldEmptyAndFocusable;
  }

  return std::nullopt;
}

}  // namespace

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

bool FieldEnabled(autofill::FieldRendererId renderer_id,
                  const autofill::FormData& form_data) {
  const auto& fields = form_data.fields();
  auto field = std::ranges::find(fields, renderer_id,
                                 &autofill::FormFieldData::renderer_id);
  if (field == fields.end()) {
    return false;
  }
  return field->is_enabled() && !field->is_readonly();
}

ChangePasswordFormWaiter::Result::Result(
    password_manager::PasswordFormManager* form_manager,
    std::vector<DiscardedForm> discarded_forms)
    : form_manager(form_manager), discarded_forms(std::move(discarded_forms)) {}

ChangePasswordFormWaiter::Result::~Result() = default;

ChangePasswordFormWaiter::Result::Result(const Result&) = default;

ChangePasswordFormWaiter::Result& ChangePasswordFormWaiter::Result::operator=(
    const Result&) = default;

ChangePasswordFormWaiter::Result::Result(Result&&) = default;

ChangePasswordFormWaiter::Result& ChangePasswordFormWaiter::Result::operator=(
    Result&&) = default;

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
ChangePasswordFormWaiter::Builder::EnableTimeout() {
  form_waiter_->should_timeout_ = true;
  return *this;
}

ChangePasswordFormWaiter::Builder&
ChangePasswordFormWaiter::Builder::IgnoreHiddenForms() {
  form_waiter_->ignore_hidden_forms_ = true;
  return *this;
}

ChangePasswordFormWaiter::Builder&
ChangePasswordFormWaiter::Builder::SetFieldsToIgnore(
    const std::vector<autofill::FieldGlobalId>& fields_to_ignore) {
  form_waiter_->fields_to_ignore_ = fields_to_ignore;
  return *this;
}

std::unique_ptr<ChangePasswordFormWaiter>
ChangePasswordFormWaiter::Builder::Build() {
  form_waiter_->WaitForLocalMLModelAvailability();
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
    cache->AddObserver(this);
  }
  RecheckForms();

  if (!web_contents()->IsLoading()) {
    DidStopLoading();
  }
}

void ChangePasswordFormWaiter::RecheckForms() {
  if (auto* cache = GetPasswordFormCache(client_)) {
    for (const auto& manager : cache->GetFormManagers()) {
      std::optional<FormDiscardReason> discard_reason =
          GetDiscardReason(manager.get());
      if (discard_reason.has_value()) {
        RecordDiscardedForm(manager.get(), *discard_reason);
        continue;
      }

      // There is no control over the lifetime of PasswordFormManager. Use a
      // helper function which checks the cache again.
      auto callback = base::BindOnce(
          &ChangePasswordFormWaiter::GetCorrespondingFormManager,
          weak_ptr_factory_.GetWeakPtr(),
          autofill::FieldGlobalId{
              manager->GetParsedObservedForm()->form_data.host_frame(),
              manager->GetParsedObservedForm()
                  ->new_password_element_renderer_id});

      // The form has been already parsed. Invoke OnPasswordFormParsed to check
      // if the form is eligible.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(callback).Then(base::BindOnce(
                         &ChangePasswordFormWaiter::OnPasswordFormParsed,
                         weak_ptr_factory_.GetWeakPtr())));
    }
  }

  if (base::FeatureList::IsEnabled(
          password_change::features::
              kRecheckFormsExponentiallyInChangePasswordFormWaiter)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ChangePasswordFormWaiter::RecheckForms,
                       weak_ptr_factory_.GetWeakPtr()),
        forms_recheck_delay_);
    forms_recheck_delay_ *= 2;
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
      if (base::FeatureList::IsEnabled(
              password_change::features::
                  kTimeoutLocalMLModelDownloadInChangePasswordFormWaiter)) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &ChangePasswordFormWaiter::OnLocalMLModelDownloadTimeout,
                weak_ptr_factory_.GetWeakPtr()),
            kLocalMLModelDownloadTimeout);
      }
      return;
    }
  }

  // No downloading is required. Initialize waiter immediately.
  Init();
}

void ChangePasswordFormWaiter::OnLocalMLModelDownloadTimeout() {
  // If the subscription is still valid, the model did not finish loading
  // before the timeout expired. Proceed with initialization without waiting.
  if (model_loaded_subscription_) {
    Init();
  }
}

void ChangePasswordFormWaiter::OnPasswordFormParsed(
    password_manager::PasswordFormManager* form_manager) {
  if (!callback_ || !form_manager) {
    return;
  }

  std::optional<FormDiscardReason> discard_reason =
      GetDiscardReason(form_manager);
  if (discard_reason.has_value()) {
    RecordDiscardedForm(form_manager, *discard_reason);
    return;
  }

  if (std::ranges::count(
          fields_to_ignore_,
          autofill::FieldGlobalId{
              form_manager->GetParsedObservedForm()->form_data.host_frame(),
              form_manager->GetParsedObservedForm()
                  ->new_password_element_renderer_id})) {
    RecordDiscardedForm(form_manager, FormDiscardReason::kFieldToIgnore);
    return;
  }

  if (!form_manager->GetDriver()) {
    RecordDiscardedForm(form_manager, FormDiscardReason::kNoDriver);
    return;
  }

  auto new_field_id =
      form_manager->GetParsedObservedForm()->new_password_element_renderer_id;
  auto field_global_id = autofill::FieldGlobalId{
      form_manager->GetParsedObservedForm()->form_data.host_frame(),
      new_field_id};
  form_manager->GetDriver()->CheckViewAreaVisible(
      new_field_id,
      base::BindOnce(&ChangePasswordFormWaiter::OnCheckViewAreaVisibleCallback,
                     weak_ptr_factory_.GetWeakPtr(), field_global_id));
  return;
}

void ChangePasswordFormWaiter::OnCheckViewAreaVisibleCallback(
    autofill::FieldGlobalId field_global_id,
    bool is_visible) {
  auto* form_manager = GetCorrespondingFormManager(
      weak_ptr_factory_.GetWeakPtr(), field_global_id);
  if (!form_manager) {
    return;
  }

  if (!is_visible) {
    RecordDiscardedForm(form_manager, FormDiscardReason::kFormNotVisible);
    return;
  }

  NotifyResult(form_manager);
}

void ChangePasswordFormWaiter::DidStartLoading() {
  if (timeout_timer_.IsRunning()) {
    // Page is still loading, stop the timer.
    timeout_timer_.Stop();
  }
}

void ChangePasswordFormWaiter::DidStopLoading() {
  if (!should_timeout_ || !callback_ || !web_contents() ||
      web_contents()->IsLoading() || model_loaded_subscription_) {
    return;
  }
  timeout_timer_.Start(FROM_HERE, kChangePasswordFormWaitingTimeout, this,
                       &ChangePasswordFormWaiter::OnTimeout);
}

void ChangePasswordFormWaiter::OnTimeout() {
  NotifyResult(nullptr);
}

void ChangePasswordFormWaiter::NotifyResult(
    password_manager::PasswordFormManager* form_manager) {
  CHECK(callback_);
  timeout_timer_.Stop();
  model_loaded_subscription_ = {};
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (auto* cache = GetPasswordFormCache(client_)) {
    cache->RemoveObserver(this);
  }
  Observe(nullptr);
  std::move(callback_).Run(
      Result(form_manager, std::exchange(discarded_forms_, {})));
}

void ChangePasswordFormWaiter::RecordDiscardedForm(
    const password_manager::PasswordFormManager* form_manager,
    FormDiscardReason discard_reason) {
  const auto* parsed_form = form_manager->GetParsedObservedForm();
  if (!parsed_form) {
    return;
  }
  autofill::FormSignature form_signature =
      autofill::CalculateFormSignature(parsed_form->form_data);
  if (std::ranges::any_of(discarded_forms_, [&](const DiscardedForm& existing) {
        return existing.reason == discard_reason &&
               autofill::CalculateFormSignature(existing.form.form_data) ==
                   form_signature;
      })) {
    return;
  }
  discarded_forms_.emplace_back(*parsed_form, discard_reason);
}

// static
password_manager::PasswordFormManager*
ChangePasswordFormWaiter::GetCorrespondingFormManager(
    base::WeakPtr<ChangePasswordFormWaiter> waiter,
    autofill::FieldGlobalId field_global_id) {
  if (!waiter) {
    return nullptr;
  }

  if (auto* cache = GetPasswordFormCache(waiter->client_)) {
    for (const auto& manager : cache->GetFormManagers()) {
      if (manager->GetParsedObservedForm() &&
          autofill::FieldGlobalId{
              manager->GetParsedObservedForm()->form_data.host_frame(),
              manager->GetParsedObservedForm()
                  ->new_password_element_renderer_id} == field_global_id) {
        return manager.get();
      }
    }
  }
  return nullptr;
}
