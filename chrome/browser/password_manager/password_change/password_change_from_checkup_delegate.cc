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
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor_webui.mojom.h"
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

bool IsValidUrl(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

void CreateDummyTaskAndTiedTab(glic::GlicKeyedService* glic_service,
                               content::WebContents* web_contents) {
  if (!glic_service || !web_contents) {
    return;
  }
  actor::ActorKeyedService* actor_service = actor::ActorKeyedService::Get(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!actor_service) {
    return;
  }
  actor::TaskId dummy_task_id = actor_service->CreateTask(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic, std::nullopt),
      reinterpret_cast<const actor::EnterprisePolicyChecker*>(
          &glic_service->actor_policy_checker()));
  actor::ActorTask* dummy_task = actor_service->GetTask(dummy_task_id);
  tabs::TabInterface* actuation_tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  CHECK(dummy_task && actuation_tab);
  dummy_task->AddTab(actuation_tab->GetHandle(), /*stop_task_on_detach=*/true,
                     base::DoNothing());
}

bool IsSameOrigin(const std::u16string& credential_source_site_or_app,
                  const GURL& credential_target_url) {
  GURL source_url(credential_source_site_or_app);

  if (!IsValidUrl(credential_target_url) || !IsValidUrl(source_url)) {
    return false;
  }

  return url::Origin::Create(source_url)
      .IsSameOriginWith(url::Origin::Create(credential_target_url));
}

std::string GetReachFormPrompt(const std::string& domain,
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

std::string GetPostSubmissionPrompt() {
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
          "prompts.verify_password_change_submission.system_prompt");

  if (!system_prompt) {
    return std::string();
  }

  return *system_prompt;

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
  credential_url_ = credential.GetURL();
  std::string site_domain(credential_url_.host());
  username_ = credential.username;
  current_password_ = credential.password;

  std::string reach_form_prompt =
      GetReachFormPrompt(site_domain, base::UTF16ToUTF8(username_));

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
      credential_url_.GetWithEmptyPath(), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false);

  content::WebContents* new_contents =
      originator_->OpenURL(open_url_params, /*navigation_handle_callback=*/{});

  if (!new_contents) {
    return;
  }

  tabs::TabInterface* new_tab_interface =
      tabs::TabInterface::MaybeGetFromContents(new_contents);

  if (!new_tab_interface) {
    return;
  }

  glic::GlicInvokeOptions options(glic::Target(new_tab_interface),
                                  glic::mojom::InvocationSource::kSharedTab);
  options.prompts.push_back(std::move(reach_form_prompt));
  options.additional_context = glic::mojom::AdditionalContext::New();

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(new_contents);
  if (session_tab_helper) {
    options.additional_context->tab_id = session_tab_helper->session_id().id();
  }

  // Invoking it in a new tab ensures that the settings page is not shared.
  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      std::move(options));

  actor::ActorKeyedService* actor_service = actor::ActorKeyedService::Get(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  if (actor_service) {
    actuation_web_contents_ = new_contents->GetWeakPtr();
    CreateDummyTaskAndTiedTab(glic_service, new_contents);
    actor_task_state_subscription_ =
        actor_service->AddTaskStateChangedCallback(base::BindRepeating(
            &PasswordChangeFromCheckupDelegate::OnFindFormTaskStateChanged,
            base::Unretained(this)));
  }
}

void PasswordChangeFromCheckupDelegate::AutoSelectCredential(
    const std::vector<actor_login::Credential>& credentials,
    actor::ToolDelegate::CredentialSelectedCallback callback) {
  if (!actuation_web_contents_) {
    std::move(callback).Run(
        actor::webui::mojom::SelectCredentialDialogResponse::New());
    return;
  }

  for (const auto& cred : credentials) {
    // Discard credentials that are not passwords.
    if (cred.type != actor_login::CredentialType::kPassword ||
        cred.username != username_) {
      continue;
    }

    if (IsSameOrigin(cred.source_site_or_app, credential_url_)) {
      auto response =
          actor::webui::mojom::SelectCredentialDialogResponse::New();
      response->selected_credential_id = cred.id.value();
      response->permission_duration =
          actor::webui::mojom::UserGrantedPermissionDuration::kOneTime;
      std::move(callback).Run(std::move(response));
      return;
    }
  }

  std::move(callback).Run(
      actor::webui::mojom::SelectCredentialDialogResponse::New());
}

glic::GlicKeyedService* PasswordChangeFromCheckupDelegate::GetGlicService() {
  if (!originator_) {
    return nullptr;
  }
  return glic::GlicKeyedService::Get(
      Profile::FromBrowserContext(originator_->GetBrowserContext()));
}

void PasswordChangeFromCheckupDelegate::OnFindFormTaskStateChanged(
    actor::ActorTask& task) {
  tabs::TabInterface* actuation_tab =
      tabs::TabInterface::MaybeGetFromContents(actuation_web_contents_.get());
  if (!actuation_tab) {
    return;
  }

  if (!find_form_task_id_) {
    if (task.GetTabs().contains(actuation_tab->GetHandle())) {
      find_form_task_id_ = task.id();
      RegisterAutoSelectCredential(task);
    } else {
      return;
    }
  }

  if (task.id() != *find_form_task_id_) {
    // Ignore unrelated tasks.
    return;
  }

  const actor::ActorTask::State new_state = task.GetState();
  if (IsTaskInterrupted(new_state)) {
    task.Stop(actor::ActorTask::StoppedReason::kShutdown);
    actor_task_state_subscription_ = {};
    return;
  }

  if (new_state == actor::ActorTask::State::kFinished) {
    actor_task_state_subscription_ = {};
    CreateDummyTaskAndTiedTab(GetGlicService(), actuation_web_contents_.get());

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

  std::string post_submission_prompt = GetPostSubmissionPrompt();

  if (post_submission_prompt.empty()) {
    return;
  }

  glic_service->CloseAndShutdown(
      actuation_web_contents_->GetPrimaryMainFrame());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasswordChangeFromCheckupDelegate::InvokeVerificationFlow,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(post_submission_prompt)),
      base::Seconds(1));
}

void PasswordChangeFromCheckupDelegate::OnVerificationTaskStateChanged(
    actor::ActorTask& task) {
  const actor::ActorTask::State new_state = task.GetState();
  tabs::TabInterface* actuation_tab =
      tabs::TabInterface::MaybeGetFromContents(actuation_web_contents_.get());
  if (!actuation_tab) {
    return;
  }

  if (!verification_task_id_) {
    if (task.GetTabs().contains(actuation_tab->GetHandle())) {
      verification_task_id_ = task.id();
      verification_task_created_ = true;
      verification_timer_.Stop();
    } else {
      return;
    }
  }

  // Ignore unrelated tasks.
  if (verification_task_id_ && *verification_task_id_ != task.id()) {
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
void PasswordChangeFromCheckupDelegate::RegisterAutoSelectCredential(
    actor::ActorTask& task) {
  task.GetExecutionEngine().PreHandleCredentialSelectionDialog(
      base::BindOnce(&PasswordChangeFromCheckupDelegate::AutoSelectCredential,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordChangeFromCheckupDelegate::InvokeVerificationFlow(
    std::string post_submission_prompt) {
  glic::GlicKeyedService* glic_service = GetGlicService();
  if (!glic_service || !actuation_web_contents_) {
    return;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(actuation_web_contents_.get());
  if (!tab_interface) {
    return;
  }

  glic::GlicInvokeOptions options(
      glic::Target(tab_interface, glic::NewConversation()),
      glic::mojom::InvocationSource::kSharedTab);
  options.prompts.push_back(std::move(post_submission_prompt));
  options.additional_context = glic::mojom::AdditionalContext::New();
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(
          actuation_web_contents_.get());
  if (session_tab_helper) {
    options.additional_context->tab_id = session_tab_helper->session_id().id();
  }

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
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

  // TODO(crbug.com/485620841): Replace this timeout signal with
  // InvokeWithUpdates once fully functional. Currently this assumes that if no
  // task is created within 90 seconds, Bluedog was not triggered which means no
  // extra steps are required for completion of the password change flow and
  // assumes success.
  verification_timer_.Start(
      FROM_HERE, base::Seconds(90),
      base::BindOnce(&PasswordChangeFromCheckupDelegate::OnVerificationTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}
