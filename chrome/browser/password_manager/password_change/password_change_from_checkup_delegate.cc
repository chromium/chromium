// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/password_manager/core/browser/password_form_manager.h"
// TODO(crbug.com/485620841): Make delegate not dependent on client and move
// this back to /password_change.
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

std::string GetPromptToReachForm(std::string_view domain,
                                 std::string_view username) {
  return base::StrCat(
      {"I want to change my password for ", domain,
       ". Please help me complete the password change flow starting from "
       "this current page. You can login using ",
       username,
       ". The task is finished when the change password form is visible."});
}

std::u16string GeneratePassword(
    const password_manager::PasswordForm& form,
    password_manager::PasswordGenerationFrameHelper* generation_helper) {
  auto iter = std::ranges::find(form.form_data.fields(),
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

PasswordChangeFromCheckupDelegate::PasswordChangeFromCheckupDelegate() =
    default;

PasswordChangeFromCheckupDelegate::~PasswordChangeFromCheckupDelegate() =
    default;

void PasswordChangeFromCheckupDelegate::StartPasswordChangeFlow(
    const password_manager::CredentialUIEntry& credential,
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return;
  }
  originator_ = std::move(web_contents);

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(originator_.get());
  if (!tab_interface) {
    return;
  }

  glic::GlicKeyedService* glic_service = GetGlicService();
  if (!glic_service) {
    return;
  }

  // TODO(crbug.com/485620841): Handle non-web URLs for Android passwords.
  const GURL& credential_url = credential.GetURL();
  std::string site_domain(credential_url.host());

  // TODO(crbug.com/485620841): Replace with internal prompt.
  std::string prompt =
      GetPromptToReachForm(site_domain, base::UTF16ToUTF8(credential.username));
  content::OpenURLParams open_url_params(
      credential_url.GetWithEmptyPath(), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false);

  content::WebContents* new_contents =
      originator_->OpenURL(open_url_params, /*navigation_handle_callback=*/{});

  if (!new_contents) {
    return;
  }

  glic::GlicInvokeOptions options(glic::mojom::InvocationSource::kSharedTab);
  options.prompts.push_back(std::move(prompt));
  options.additional_context = glic::mojom::AdditionalContext::New();

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(new_contents);
  if (session_tab_helper) {
    options.additional_context->tab_id = session_tab_helper->session_id().id();
  }

  tabs::TabInterface* new_tab_interface =
      tabs::TabInterface::MaybeGetFromContents(new_contents);

  if (!new_tab_interface) {
    return;
  }

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      new_tab_interface, std::move(options));

  actor::ActorKeyedService* actor_service = actor::ActorKeyedService::Get(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  if (actor_service) {
    username_ = credential.username;
    current_password_ = credential.password;
    actuation_web_contents_ = new_contents->GetWeakPtr();
    actor_task_state_subscription_ =
        actor_service->AddTaskStateChangedCallback(base::BindRepeating(
            &PasswordChangeFromCheckupDelegate::OnActorTaskStateChanged,
            base::Unretained(this)));
  }
}

glic::GlicKeyedService* PasswordChangeFromCheckupDelegate::GetGlicService() {
  if (!originator_) {
    return nullptr;
  }
  return glic::GlicKeyedService::Get(
      Profile::FromBrowserContext(originator_->GetBrowserContext()));
}

void PasswordChangeFromCheckupDelegate::OnActorTaskStateChanged(
    actor::TaskId task_id,
    actor::ActorTask::State state) {
  if (!actor_task_id_ && state == actor::ActorTask::State::kCreated) {
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(Profile::FromBrowserContext(
            actuation_web_contents_->GetBrowserContext()));
    CHECK(actor_service);

    actor::ActorTask* actor_task_for_actuation =
        actor_service->GetTaskFromTab(*tabs::TabInterface::MaybeGetFromContents(
            actuation_web_contents_.get()));
    if (!actor_task_for_actuation) {
      return;
    }
    actor_task_id_ = actor_task_for_actuation->id();
    return;
  }

  if (actor_task_id_ && *actor_task_id_ != task_id) {
    // Ignore tasks unrelated to the password change flow.
    return;
  }

  // TODO(crbug.com/485620841): Handle interruption states.
  if (state == actor::ActorTask::State::kFinished) {
    actor_task_state_subscription_ = {};
    if (!actuation_web_contents_) {
      return;
    }

    auto* client = ChromePasswordManagerClient::FromWebContents(
        actuation_web_contents_.get());
    if (!client) {
      return;
    }

    form_waiter_ = ChangePasswordFormWaiter::Builder(
                       actuation_web_contents_.get(), client,
                       base::BindOnce(&PasswordChangeFromCheckupDelegate::
                                          OnChangePasswordFormManagerFound,
                                      weak_ptr_factory_.GetWeakPtr()))
                       .Build();
  }
}

void PasswordChangeFromCheckupDelegate::OnChangePasswordFormManagerFound(
    password_manager::PasswordFormManager* form_manager) {
  form_waiter_.reset();

  if (!actuation_web_contents_ || !form_manager) {
    return;
  }

  std::u16string new_password = GeneratePassword(
      *form_manager->GetParsedObservedForm(),
      form_manager->GetDriver()->GetPasswordGenerationHelper());

  auto* client = ChromePasswordManagerClient::FromWebContents(
      actuation_web_contents_.get());

  submission_helper_ =
      std::make_unique<ChangePasswordFormFillingSubmissionHelper>(
          actuation_web_contents_.get(), client,
          base::BindOnce(
              &PasswordChangeFromCheckupDelegate::OnChangePasswordFormSubmitted,
              weak_ptr_factory_.GetWeakPtr()));

  submission_helper_->FillChangePasswordForm(form_manager, username_,
                                             current_password_, new_password);
}

void PasswordChangeFromCheckupDelegate::OnChangePasswordFormSubmitted(
    ChangePasswordFormFillingSubmissionHelper::SubmissionResult result) {
  submission_helper_.reset();
}
