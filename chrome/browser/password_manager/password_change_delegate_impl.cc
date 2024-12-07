// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_delegate_impl.h"

#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/generation/password_generator.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

using password_manager::PasswordForm;
using password_manager::PasswordFormCache;
using password_manager::PasswordFormManager;

PasswordFormCache& GetFormCache(content::WebContents* web_contents) {
  auto* client = static_cast<password_manager::PasswordManagerClient*>(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  CHECK(client);
  CHECK(client->GetPasswordManager());

  auto* cache = client->GetPasswordManager()->GetPasswordFormCache();
  CHECK(cache);
  return *cache;
}

const PasswordForm* GetChangePasswordForm(PasswordFormManager* form_manager) {
  CHECK(form_manager);

  const PasswordForm* parsed_form = form_manager->GetParsedObservedForm();
  CHECK(parsed_form);

  // New password field and password confirmation fields are indicators of
  // a change password form.
  return (parsed_form->new_password_element_renderer_id &&
          parsed_form->confirmation_password_element_renderer_id)
             ? parsed_form
             : nullptr;
}

std::u16string GeneratePassword(
    const PasswordForm& form,
    password_manager::PasswordGenerationFrameHelper* generation_helper) {
  auto iter = base::ranges::find(form.form_data.fields(),
                                 form.new_password_element_renderer_id,
                                 &autofill::FormFieldData::renderer_id);
  CHECK(iter != form.form_data.fields().end());

  return generation_helper->GeneratePassword(
      form.url,
      autofill::password_generation::PasswordGenerationType::kAutomatic,
      autofill::CalculateFormSignature(form.form_data),
      autofill::CalculateFieldSignatureForField(*iter), iter->max_length());
}

}  // namespace

PasswordChangeDelegateImpl::PasswordChangeDelegateImpl(
    GURL change_password_url,
    std::u16string username,
    std::u16string password,
    content::WebContents* originator,
    base::RepeatingCallback<
        content::WebContents*(const GURL&, content::WebContents*)> callback)
    : change_password_url_(std::move(change_password_url)),
      username_(std::move(username)),
      original_password_(std::move(password)),
      originator_(originator->GetWeakPtr()) {
  content::WebContents* new_tab =
      std::move(callback).Run(change_password_url_, originator);
  if (new_tab) {
    executor_ = new_tab->GetWeakPtr();
    GetFormCache(new_tab).SetObserver(weak_ptr_factory_.GetWeakPtr());

    Observe(new_tab);
  }
}

PasswordChangeDelegateImpl::~PasswordChangeDelegateImpl() = default;

void PasswordChangeDelegateImpl::OnPasswordFormParsed(
    PasswordFormManager* form_manager) {
  const PasswordForm* form = GetChangePasswordForm(form_manager);
  if (!form) {
    return;
  }

  CHECK(executor_);
  // Change password form is detected - no need to continue observing.
  GetFormCache(executor_.get()).ResetObserver();
  form_manager_ = form_manager->Clone();

  // Post task is required because when PasswordFormManager parses a form
  // SendFillInformationToRenderer is invoked after OnPasswordFormParsed,
  // potentially clearing agent state and preventing successful login detection.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordChangeDelegateImpl::FillChangePasswordForm,
                     weak_ptr_factory_.GetWeakPtr(), *form,
                     form_manager->GetDriver()));
}

void PasswordChangeDelegateImpl::WebContentsDestroyed() {
  // PasswordFormManager keeps raw pointers to PasswordManagerClient reset it
  // immediately to avoid keeping dangling pointer.
  form_manager_.reset();
}

bool PasswordChangeDelegateImpl::IsPasswordChangeOngoing(
    content::WebContents* web_contents) {
  return (originator_ && originator_.get() == web_contents) ||
         (executor_ && executor_.get() == web_contents);
}

PasswordChangeDelegate::State PasswordChangeDelegateImpl::GetCurrentState()
    const {
  return State::kWaitingForChangePasswordForm;
}

void PasswordChangeDelegateImpl::Stop() {
  observers_.Notify(&PasswordChangeDelegate::Observer::OnPasswordChangeStopped,
                    this);
}

void PasswordChangeDelegateImpl::SuccessfulSubmissionDetected(
    content::WebContents* web_contents) {
  if (executor_ && executor_.get() == web_contents && form_manager_) {
    // TODO(crbug.com/377878716): Do it only after verification of successful
    // update.
    form_manager_->OnUpdateUsernameFromPrompt(username_);
    form_manager_->Save();
    // TODO(crbug.com/375565171): Transition to a password successfully updated
    // state.
  }
}

void PasswordChangeDelegateImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordChangeDelegateImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PasswordChangeDelegateImpl::UpdateState(
    PasswordChangeDelegate::State new_state) {
  if (new_state != current_state_) {
    current_state_ = new_state;
    observers_.Notify(&PasswordChangeDelegate::Observer::OnStateChanged,
                      current_state_);
  }
}

void PasswordChangeDelegateImpl::ChangePasswordFormFilled(
    const autofill::FormData& submitted_form) {
  form_manager_->ProvisionallySave(
      submitted_form, form_manager_->GetDriver().get(),
      base::LRUCache<password_manager::PossibleUsernameFieldIdentifier,
                     password_manager::PossibleUsernameData>(
          password_manager::kMaxSingleUsernameFieldsToStore));
}

void PasswordChangeDelegateImpl::FillChangePasswordForm(
    password_manager::PasswordForm form,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver) {
  CHECK(form_manager_);

  if (!driver || !driver->GetPasswordGenerationHelper()) {
    return;
  }

  generated_password_ =
      GeneratePassword(form, driver->GetPasswordGenerationHelper());

  driver->SubmitChangePasswordForm(
      form.password_element_renderer_id, form.new_password_element_renderer_id,
      form.confirmation_password_element_renderer_id, original_password_,
      generated_password_,
      base::BindOnce(&PasswordChangeDelegateImpl::ChangePasswordFormFilled,
                     weak_ptr_factory_.GetWeakPtr()));

  form_manager_->PresaveGeneratedPassword(form.form_data, generated_password_);
  UpdateState(PasswordChangeDelegate::State::kChangingPassword);
}
