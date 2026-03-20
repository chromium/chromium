// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/browser_resources.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "ui/base/resource/resource_bundle.h"
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

std::string GetFallbackPromptToReachForm(std::string_view domain,
                                         std::string_view username) {
  return base::StrCat(
      {"I want to change my password for ", domain,
       ". Please help me complete the password change flow starting from "
       "this current page. You can login using ",
       username,
       ". The task is finished when the change password form is visible."});
}

std::string GetReachChangeFormPrompt(const std::string& domain,
                                     const std::string& username) {
#if defined(IDR_APC_PROMPTS_JSON)
  std::string json_data =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_APC_PROMPTS_JSON);

  if (json_data.empty()) {
    return std::string();
  }

  std::optional<base::Value> parsed_json =
      base::JSONReader::Read(json_data, base::JSON_PARSE_RFC);

  if (!parsed_json.has_value() || !parsed_json->is_dict()) {
    return std::string();
  }

  const std::string* system_prompt =
      parsed_json->GetDict().FindStringByDottedPath(
          "prompts.reach_change_password_form.system_prompt");

  if (!system_prompt) {
    return std::string();
  }

  std::string final_prompt = *system_prompt;
  base::ReplaceSubstringsAfterOffset(&final_prompt, 0, "{url_spec}", domain);
  base::ReplaceSubstringsAfterOffset(&final_prompt, 0, "{username}", username);

  return final_prompt;

#else
  return std::string();
#endif
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

bool IsTaskInterrupted(actor::ActorTask::State new_state) {
  return (new_state == actor::ActorTask::State::kWaitingOnUser ||
          new_state == actor::ActorTask::State::kPausedByActor ||
          new_state == actor::ActorTask::State::kPausedByUser);
}

bool IsTaskResumed(actor::ActorTask::State old_state,
                   actor::ActorTask::State new_state) {
  return IsTaskInterrupted(old_state) &&
         new_state == actor::ActorTask::State::kActing;
}

void ActivateTabForWebContents(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab_interface) {
    return;
  }

  BrowserWindowInterface* browser_window =
      tab_interface->GetBrowserWindowInterface();
  if (!browser_window) {
    return;
  }

  TabStripModel* tab_strip = browser_window->GetTabStripModel();
  int target_index = tab_strip->GetIndexOfWebContents(web_contents);

  if (target_index != TabStripModel::kNoTab) {
    tab_strip->ActivateTabAt(target_index);
  }
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

  // TODO(crbug.com/485620841): Handle non-web URLs for Android passwords.
  const GURL& credential_url = credential.GetURL();
  std::string site_domain(credential_url.host());
  username_ = credential.username;
  current_password_ = credential.password;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetReachChangeFormPrompt, std::move(site_domain),
                     base::UTF16ToUTF8(username_)),
      base::BindOnce(&PasswordChangeFromCheckupDelegate::OnPromptReady,
                     weak_ptr_factory_.GetWeakPtr(), credential_url));
}

void PasswordChangeFromCheckupDelegate::OnPromptReady(GURL credential_url,
                                                      std::string prompt) {
  if (prompt.empty()) {
    std::string site_domain(credential_url.host());
    prompt =
        GetFallbackPromptToReachForm(site_domain, base::UTF16ToUTF8(username_));
  }

  if (prompt.empty() || !originator_) {
    return;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(originator_.get());
  if (!tab_interface) {
    return;
  }

  glic::GlicKeyedService* glic_service = GetGlicService();
  if (!glic_service) {
    return;
  }

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

  // Invoking it in a new tab ensures that the settings page is not shared.
  // It also expects that the actor uses the current tab instead of attempting
  // to open a new one for completing the flow.
  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      new_tab_interface, std::move(options));

  actor::ActorKeyedService* actor_service = actor::ActorKeyedService::Get(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  if (actor_service) {
    actuation_web_contents_ = new_contents->GetWeakPtr();
    actor_task_state_subscription_ =
        actor_service->AddTaskStateChangedCallback(base::BindRepeating(
            &PasswordChangeFromCheckupDelegate::OnFindFormTaskStateChanged,
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

void PasswordChangeFromCheckupDelegate::OnFindFormTaskStateChanged(
    actor::TaskId task_id,
    actor::ActorTask::State new_state) {
  if (!find_form_task_id_ && new_state == actor::ActorTask::State::kCreated) {
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

    find_form_task_id_ = actor_task_for_actuation->id();
    return;
  }

  if (find_form_task_id_ && *find_form_task_id_ != task_id) {
    return;  // Ignore unrelated tasks
  }

  if (IsTaskInterrupted(new_state)) {
    ActivateTabForWebContents(actuation_web_contents_.get());
  } else if (find_form_task_state_.has_value() &&
             IsTaskResumed(find_form_task_state_.value(), new_state)) {
    ActivateTabForWebContents(originator_.get());
  }

  if (new_state == actor::ActorTask::State::kFinished) {
    actor_task_state_subscription_ = {};
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

  find_form_task_state_ = new_state;
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

  // If the form submission failed, do not trigger a verification task.
  if (!result.has_value()) {
    return;
  }

  saved_form_manager_ = std::move(result).value();
  if (!actuation_web_contents_) {
    return;
  }

  glic::GlicKeyedService* glic_service = GetGlicService();
  if (!glic_service) {
    return;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(actuation_web_contents_.get());
  if (!tab_interface) {
    return;
  }

  verification_task_id_ = std::nullopt;
  verification_task_created_ = false;

  glic::GlicInvokeOptions options(glic::mojom::InvocationSource::kSharedTab);
  // TODO(crbug.com/485620841): Read this from internal.
  options.prompts.push_back(
      "I just filled and submitted a change password form in order to change "
      "my password. Tell me if this was successful or not, if it was not "
      "successful and there are extra steps needed, complete the extra steps "
      "in my behalf.");

  options.additional_context = glic::mojom::AdditionalContext::New();
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(
          actuation_web_contents_.get());
  if (session_tab_helper) {
    options.additional_context->tab_id = session_tab_helper->session_id().id();
  }

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(), tab_interface,
      std::move(options));

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(Profile::FromBrowserContext(
          actuation_web_contents_->GetBrowserContext()));
  if (actor_service) {
    actor_task_state_subscription_ =
        actor_service->AddTaskStateChangedCallback(base::BindRepeating(
            &PasswordChangeFromCheckupDelegate::OnVerificationTaskStateChanged,
            base::Unretained(this)));
  }

  // If no task is created after 5 seconds, we assume that it was a successful
  // change since and no extra steps are needed.
  verification_timer_.Start(
      FROM_HERE, base::Seconds(5),
      base::BindOnce(&PasswordChangeFromCheckupDelegate::OnVerificationTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeFromCheckupDelegate::OnVerificationTaskStateChanged(
    actor::TaskId task_id,
    actor::ActorTask::State new_state) {
  if (!verification_task_id_) {
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(Profile::FromBrowserContext(
            actuation_web_contents_->GetBrowserContext()));
    CHECK(actor_service);

    actor::ActorTask* actor_task =
        actor_service->GetTaskFromTab(*tabs::TabInterface::MaybeGetFromContents(
            actuation_web_contents_.get()));

    if (!actor_task) {
      return;
    }

    verification_task_id_ = actor_task->id();
    verification_task_created_ = true;
    // A task was created, so stopping the timer to not trigger
    // the password being saved.
    verification_timer_.Stop();
    return;
  }

  // Ignore unrelated tasks.
  if (verification_task_id_ && *verification_task_id_ != task_id) {
    return;
  }

  // If the task for verifification finishes, we assume success.
  if (new_state == actor::ActorTask::State::kFinished) {
    actor_task_state_subscription_ = {};
    HandleMaybeSuccessfulPasswordChange();
  }
}

void PasswordChangeFromCheckupDelegate::OnVerificationTimeout() {
  if (!verification_task_created_) {
    actor_task_state_subscription_ = {};
    HandleMaybeSuccessfulPasswordChange();
  }
}

void PasswordChangeFromCheckupDelegate::HandleMaybeSuccessfulPasswordChange() {
  if (saved_form_manager_) {
    saved_form_manager_->Save();
    saved_form_manager_.reset();
  }
}
