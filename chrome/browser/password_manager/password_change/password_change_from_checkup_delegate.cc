// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"

#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
  base::FilePath source_root;
  if (!base::PathService::Get(base::DIR_CURRENT, &source_root)) {
    return std::string();
  }

  base::FilePath json_file_path = source_root.AppendASCII("internal")
                                      .AppendASCII("password_manager")
                                      .AppendASCII("apc_prompts.json");

  std::string json_data;
  if (!base::ReadFileToString(json_file_path, &json_data)) {
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

  glic::GlicKeyedService* glic_service = glic::GlicKeyedService::Get(
      Profile::FromBrowserContext(originator_->GetBrowserContext()));
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
            &PasswordChangeFromCheckupDelegate::OnActorTaskStateChanged,
            base::Unretained(this)));

    // TODO(crbug.com/485620841): Return focus to the settings tab right after
    // invoking in the newly opened tab. Do this when the Invoke API fully wires
    // up `additional_context`. Currently, if Invoke is called on a background
    // tab, it will not start the flow unless the user goes to the tab.
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
    actor::ActorTask::State new_state) {
  if (!actor_task_id_ && new_state == actor::ActorTask::State::kCreated) {
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

  if (IsTaskInterrupted(new_state)) {
    // Focus the actuation tab when there is an interruption so the user can
    // complete the flow.
    ActivateTabForWebContents(actuation_web_contents_.get());
  } else if (actor_state_.has_value() &&
             IsTaskResumed(actor_state_.value(), new_state)) {
    // Return focus to the original tab.
    ActivateTabForWebContents(originator_.get());
  }

  // When the task reaches `kFinished` it is assumed that the change password
  // form was found.
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

  // Set the new state.
  actor_state_ = new_state;
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
