// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/glic_password_change_actuator.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/grit/browser_resources.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/actor_login/password_change_from_checkup_actor_login_service.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"

namespace {

using Logger = password_manager::BrowserSavePasswordProgressLogger;

constexpr char kDefaultPostSubmissionPrompt[] =
    "Verify password change submission.";

std::unique_ptr<Logger> GetLoggerIfAvailable(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  auto* client = static_cast<password_manager::PasswordManagerClient*>(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  if (!client) {
    return nullptr;
  }
  if (password_manager_util::IsLoggingActive(client)) {
    return std::make_unique<Logger>(client->GetCurrentLogManager());
  }
  return nullptr;
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

  if (!json_data.empty()) {
    std::optional<base::Value> parsed_json =
        base::JSONReader::Read(json_data, base::JSON_PARSE_RFC);

    if (parsed_json.has_value() && parsed_json->is_dict()) {
      const std::string* system_prompt =
          parsed_json->GetDict().FindStringByDottedPath(
              "prompts.verify_password_change_submission.system_prompt");

      if (system_prompt && !system_prompt->empty()) {
        return *system_prompt;
      }
    }
  }
#endif

  return kDefaultPostSubmissionPrompt;
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

GlicPasswordChangeActuator::GlicPasswordChangeActuator(
    password_manager::StoredCredential credential,
    content::WebContents* originator,
    Profile* profile,
    GURL change_password_url)
    : change_password_url_(std::move(change_password_url)),
      credential_(std::move(credential)),
      originator_(originator ? originator->GetWeakPtr() : nullptr),
      profile_(profile) {}

GlicPasswordChangeActuator::~GlicPasswordChangeActuator() {
  ResetInternalState(actor::ActorTask::StoppedReason::kShutdown);
}

void GlicPasswordChangeActuator::Start() {
  if (!originator_) {
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  GURL target_url =
      change_password_url_.is_empty() ? credential_.url : change_password_url_;

  std::string site_domain(target_url.host());
  std::string reach_form_prompt = GetReachFormPrompt(
      site_domain, base::UTF16ToUTF8(credential_.username_value));

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(originator_.get());
  if (!tab_interface) {
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  glic::GlicKeyedService* glic_service = GetGlicService();
  if (!glic_service) {
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  content::OpenURLParams open_url_params(
      target_url.GetWithEmptyPath(), content::Referrer(),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false);

  content::WebContents* new_contents =
      originator_->OpenURL(open_url_params, /*navigation_handle_callback=*/{});

  if (!new_contents) {
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  tabs::TabInterface* new_tab_interface =
      tabs::TabInterface::MaybeGetFromContents(new_contents);

  if (!new_tab_interface) {
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  tab_will_detach_subscription_ = new_tab_interface->RegisterWillDetach(
      base::BindRepeating(&GlicPasswordChangeActuator::OnTabWillDetach,
                          weak_ptr_factory_.GetWeakPtr()));

  glic::GlicInvokeOptions options(
      glic::Target(*new_tab_interface),
      glic::mojom::InvocationSource::kPasswordChange);
  options.feature_mode = glic::mojom::FeatureMode::kPasswordChange;
  options.prompts.push_back(std::move(reach_form_prompt));
  options.target.actuation_target =
      glic::mojom::ActuationTarget::kTargetSurface;
  glic::GlicInvokeWithAutoSubmitOptions auto_submit_options;
  auto_submit_options.show_panel = false;
  glic_instance_ = glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      std::move(options), std::move(auto_submit_options));

  actor::ActorKeyedService* actor_service = actor::ActorKeyedService::Get(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  if (!actor_service) {
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  actuation_web_contents_ = new_contents->GetWeakPtr();
  if (auto logger = GetLoggerIfAvailable(originator_.get())) {
    logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_START_FLOW);
  }
  actor_task_state_subscription_ =
      actor_service->AddTaskStateChangedCallback(base::BindRepeating(
          &GlicPasswordChangeActuator::OnFindFormTaskStateChanged,
          base::Unretained(this)));
  NotifyStateChanged(
      PasswordChangeActuator::State::kWaitingForChangePasswordForm);
}

void GlicPasswordChangeActuator::Cancel() {
  ResetInternalState(actor::ActorTask::StoppedReason::kShutdown);
}

content::WebContents* GlicPasswordChangeActuator::GetExecutorWebContents()
    const {
  return actuation_web_contents_.get();
}

void GlicPasswordChangeActuator::OpenPasswordChangeTab(
    content::WebContents* originator) {
  if (saved_form_manager_) {
    saved_form_manager_->OnUpdateUsernameFromPrompt(credential_.username_value);
    saved_form_manager_->Save();
    saved_form_manager_.reset();
  }

  if (actuation_web_contents_) {
    tabs::TabInterface* tab_interface =
        tabs::TabInterface::MaybeGetFromContents(actuation_web_contents_.get());
    if (!tab_interface) {
      return;
    }

    auto* browser_window = tab_interface->GetBrowserWindowInterface();
    if (!browser_window) {
      return;
    }

    auto* tab_strip_model = browser_window->GetTabStripModel();
    if (!tab_strip_model) {
      return;
    }

    int index =
        tab_strip_model->GetIndexOfWebContents(actuation_web_contents_.get());
    if (index != TabStripModel::kNoTab) {
      tab_strip_model->ActivateTabAt(index);
    }
    return;
  }

  if (originator) {
    GURL target_url = change_password_url_.is_empty() ? credential_.url
                                                      : change_password_url_;
    originator->OpenURL(
        content::OpenURLParams(target_url, content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK,
                               /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
  }
}

std::u16string GlicPasswordChangeActuator::GetGeneratedPassword() const {
  return generated_password_;
}

void GlicPasswordChangeActuator::AddObserver(
    PasswordChangeActuator::Observer* observer) {
  observers_.AddObserver(observer);
}

void GlicPasswordChangeActuator::RemoveObserver(
    PasswordChangeActuator::Observer* observer) {
  observers_.RemoveObserver(observer);
}

glic::GlicKeyedService* GlicPasswordChangeActuator::GetGlicService() {
  if (profile_) {
    return glic::GlicKeyedService::Get(profile_);
  }
  if (originator_) {
    return glic::GlicKeyedService::Get(
        Profile::FromBrowserContext(originator_->GetBrowserContext()));
  }
  return nullptr;
}

void GlicPasswordChangeActuator::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    Cancel();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
  }
}

void GlicPasswordChangeActuator::OnFindFormTaskStateChanged(
    actor::ActorTask& task) {
  tabs::TabInterface* actuation_tab =
      tabs::TabInterface::MaybeGetFromContents(actuation_web_contents_.get());
  if (!actuation_tab) {
    return;
  }

  if (!find_form_task_id_ &&
      task.GetTabs().contains(actuation_tab->GetHandle())) {
    find_form_task_id_ = task.id();
    task.GetExecutionEngine().SetActorLoginService(
        std::make_unique<
            actor_login::PasswordChangeFromCheckupActorLoginService>(
            password_manager::CloneStoredCredential(credential_)));

    if (auto logger = GetLoggerIfAvailable(originator_.get())) {
      logger->LogMessage(
          Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_FIND_FORM_TASK_FOUND);
    }
  }

  if (find_form_task_id_ != task.id()) {
    // Ignore unrelated tasks.
    return;
  }

  const actor::ActorTask::State new_state = task.GetState();
  if (IsTaskInterrupted(new_state)) {
    if (auto logger = GetLoggerIfAvailable(originator_.get())) {
      logger->LogMessage(
          Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_CANCEL_FLOW);
    }
    task.Stop(actor::ActorTask::StoppedReason::kShutdown);
    actor_task_state_subscription_ = {};
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kOtpDetected);
    return;
  }

  if (new_state == actor::ActorTask::State::kFinished) {
    actor_task_state_subscription_ = {};
    form_waiter_ =
        ChangePasswordFormWaiter::Builder(
            actuation_web_contents_.get(),
            ChromePasswordManagerClient::FromWebContents(
                actuation_web_contents_.get()),
            base::BindOnce(
                &GlicPasswordChangeActuator::OnChangePasswordFormManagerFound,
                weak_ptr_factory_.GetWeakPtr()))
            .Build();
  }
}

void GlicPasswordChangeActuator::OnChangePasswordFormManagerFound(
    password_manager::PasswordFormManager* form_manager) {
  form_waiter_.reset();

  if (!actuation_web_contents_ || !form_manager) {
    CloseGlicSession();
    NotifyStateChanged(
        PasswordChangeActuator::State::kChangePasswordFormNotFound);
    return;
  }

  generated_password_ = GeneratePassword(
      *form_manager->GetParsedObservedForm(),
      form_manager->GetDriver()->GetPasswordGenerationHelper());

  if (auto logger = GetLoggerIfAvailable(originator_.get())) {
    logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_FORM_FOUND);
  }

  NotifyStateChanged(PasswordChangeActuator::State::kChangingPassword);

  form_filler_ = std::make_unique<ChangePasswordFormFiller>(
      actuation_web_contents_.get(),
      ChromePasswordManagerClient::FromWebContents(
          actuation_web_contents_.get()),
      /*logs_uploader=*/nullptr);

  form_filler_->FillForm(
      form_manager, credential_.username_value,
      credential_.password_value.value(), generated_password_,
      base::BindOnce(&GlicPasswordChangeActuator::OnChangePasswordFormFilled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlicPasswordChangeActuator::OnChangePasswordFormFilled(
    ChangePasswordFormFiller::FillingResult result) {
  form_filler_.reset();

  if (!result.has_value()) {
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  saved_form_manager_ = std::move(result).value();
  if (!actuation_web_contents_) {
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  if (auto logger = GetLoggerIfAvailable(originator_.get())) {
    logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_FORM_FILLED);
  }

  glic::GlicKeyedService* glic_service = GetGlicService();
  if (!glic_service) {
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(actuation_web_contents_.get());
  if (!tab_interface) {
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  verification_task_id_ = std::nullopt;
  verification_task_created_ = false;

  std::string post_submission_prompt = GetPostSubmissionPrompt();

  if (post_submission_prompt.empty()) {
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  CloseGlicSession();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GlicPasswordChangeActuator::InvokeVerificationFlow,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(post_submission_prompt)));
}

void GlicPasswordChangeActuator::InvokeVerificationFlow(
    std::string post_submission_prompt) {
  glic::GlicKeyedService* glic_service = GetGlicService();
  if (!glic_service || !actuation_web_contents_) {
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(actuation_web_contents_.get());
  if (!tab_interface) {
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  glic::Target target = glic::Target(*tab_interface, glic::NewConversation());
  glic::GlicInvokeOptions options(
      std::move(target), glic::mojom::InvocationSource::kPasswordChange);
  options.feature_mode = glic::mojom::FeatureMode::kPasswordChange;
  options.prompts.push_back(std::move(post_submission_prompt));
  options.target.actuation_target =
      glic::mojom::ActuationTarget::kTargetSurface;
  glic::GlicInvokeWithAutoSubmitOptions auto_submit_options;
  auto_submit_options.show_panel = false;
  glic_instance_ = glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      std::move(options), std::move(auto_submit_options));

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(Profile::FromBrowserContext(
          actuation_web_contents_->GetBrowserContext()));
  if (actor_service) {
    actor_task_state_subscription_ =
        actor_service->AddTaskStateChangedCallback(base::BindRepeating(
            &GlicPasswordChangeActuator::OnVerificationTaskStateChanged,
            base::Unretained(this)));
  }

  verification_timer_.Start(
      FROM_HERE, base::Seconds(90),
      base::BindOnce(&GlicPasswordChangeActuator::OnVerificationTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlicPasswordChangeActuator::OnVerificationTaskStateChanged(
    actor::ActorTask& task) {
  const actor::ActorTask::State new_state = task.GetState();
  tabs::TabInterface* actuation_tab =
      tabs::TabInterface::MaybeGetFromContents(actuation_web_contents_.get());
  if (!actuation_tab) {
    return;
  }

  if (!verification_task_id_ &&
      task.GetTabs().contains(actuation_tab->GetHandle())) {
    verification_task_id_ = task.id();
    verification_task_created_ = true;
    task.GetExecutionEngine().SetActorLoginService(
        std::make_unique<
            actor_login::PasswordChangeFromCheckupActorLoginService>(
            password_manager::CloneStoredCredential(credential_)));
    if (auto logger = GetLoggerIfAvailable(originator_.get())) {
      logger->LogMessage(
          Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_VERIFICATION_CREATED);
    }
    // A task was created, so stopping the timer to not trigger
    // the password being saved.
    verification_timer_.Stop();
  }

  // Ignore unrelated tasks.
  if (verification_task_id_ != task.id()) {
    return;
  }

  if (IsTaskInterrupted(new_state)) {
    if (auto logger = GetLoggerIfAvailable(originator_.get())) {
      logger->LogMessage(
          Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_CANCEL_FLOW);
    }
    task.Stop(actor::ActorTask::StoppedReason::kShutdown);
    actor_task_state_subscription_ = {};
    saved_form_manager_.reset();
    verification_timer_.Stop();
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kOtpDetected);
    return;
  }

  // If the task for verification finishes, we assume success.
  if (new_state == actor::ActorTask::State::kFinished) {
    actor_task_state_subscription_ = {};
    if (auto logger = GetLoggerIfAvailable(originator_.get())) {
      logger->LogMessage(
          Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_VERIFICATION_FINISHED);
    }
    CloseGlicSession();
    HandleMaybeSuccessfulPasswordChange();
  }
}

void GlicPasswordChangeActuator::OnVerificationTimeout() {
  if (auto logger = GetLoggerIfAvailable(originator_.get())) {
    logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_TIMEOUT);
  }
  actor_task_state_subscription_ = {};
  CloseGlicSession();
  HandleMaybeSuccessfulPasswordChange();
}

void GlicPasswordChangeActuator::HandleMaybeSuccessfulPasswordChange() {
  if (saved_form_manager_) {
    saved_form_manager_->OnUpdateUsernameFromPrompt(credential_.username_value);
    saved_form_manager_->Save();
    saved_form_manager_.reset();
    NotifyStateChanged(
        PasswordChangeActuator::State::kPasswordSuccessfullyChanged);
  } else {
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
  }
}

void GlicPasswordChangeActuator::CloseGlicSession() {
  if (glic_instance_) {
    glic_instance_->CancelInvoke();
  }
}

void GlicPasswordChangeActuator::ResetInternalState(
    actor::ActorTask::StoppedReason stop_reason) {
  if (actuation_web_contents_) {
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(Profile::FromBrowserContext(
            actuation_web_contents_->GetBrowserContext()));
    if (actor_service) {
      if (find_form_task_id_.has_value() &&
          actor_service->GetTask(*find_form_task_id_)) {
        actor_service->StopTask(*find_form_task_id_, stop_reason);
      }
      if (verification_task_id_.has_value() &&
          actor_service->GetTask(*verification_task_id_)) {
        actor_service->StopTask(*verification_task_id_, stop_reason);
      }
    }
  }

  CloseGlicSession();

  tab_will_detach_subscription_ = {};
  form_filler_.reset();
  form_waiter_.reset();
  saved_form_manager_.reset();
  verification_timer_.Stop();
  actor_task_state_subscription_ = {};

  find_form_task_id_ = std::nullopt;
  verification_task_id_ = std::nullopt;
}

void GlicPasswordChangeActuator::NotifyStateChanged(
    PasswordChangeActuator::State new_state) {
  observers_.Notify(&PasswordChangeActuator::Observer::OnActuationStateChanged,
                    new_state);
}
