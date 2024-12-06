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
    const PasswordForm* form,
    password_manager::PasswordGenerationFrameHelper* generation_helper) {
  CHECK(form);

  auto iter = base::ranges::find(form->form_data.fields(),
                                 form->new_password_element_renderer_id,
                                 &autofill::FormFieldData::renderer_id);
  CHECK(iter != form->form_data.fields().end());

  return generation_helper->GeneratePassword(
      form->url,
      autofill::password_generation::PasswordGenerationType::kAutomatic,
      autofill::CalculateFormSignature(form->form_data),
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
  }
}

PasswordChangeDelegateImpl::~PasswordChangeDelegateImpl() = default;

void PasswordChangeDelegateImpl::OnPasswordFormParsed(
    PasswordFormManager* form_manager) {
  if (!form_manager->GetDriver() ||
      !form_manager->GetDriver()->GetPasswordGenerationHelper()) {
    return;
  }

  const PasswordForm* form = GetChangePasswordForm(form_manager);
  if (!form) {
    return;
  }

  CHECK(executor_);
  // Change password form is detected - no need to continue observing.
  GetFormCache(executor_.get()).ResetObserver();

  generated_password_ = GeneratePassword(
      form, form_manager->GetDriver()->GetPasswordGenerationHelper());

  form_manager->GetDriver()->SubmitChangePasswordForm(
      form->password_element_renderer_id,
      form->new_password_element_renderer_id,
      form->confirmation_password_element_renderer_id, original_password_,
      generated_password_,
      // TODO(crbug.com/375565171): Add handling for completion.
      base::DoNothing());

  UpdateState(PasswordChangeDelegate::State::kChangingPassword);
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

void PasswordChangeDelegateImpl::Cancel() {}

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
