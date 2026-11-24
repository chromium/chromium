// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/glic_password_change_actuator.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_manager.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/grit/browser_resources.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/origin_gating/core/task_policy_config.h"
#include "components/password_manager/core/browser/actor_login/password_change_from_checkup_actor_login_service.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_change/change_password_generation_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "net/base/schemeful_site.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

using Logger = password_manager::BrowserSavePasswordProgressLogger;

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

std::string GetReachFormPrompt(std::string_view url_spec) {
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
  base::ReplaceSubstringsAfterOffset(&final_prompt, 0, "{url_spec}", url_spec);

  return final_prompt;

#else
  return std::string();
#endif
}

void LogTriggeringUpdate(
    content::WebContents* originator,
    const glic::mojom::ExperimentalTriggeringUpdate* update,
    glic::mojom::SubscriberObservationType observation) {
  std::unique_ptr<Logger> logger = GetLoggerIfAvailable(originator);
  if (!logger) {
    return;
  }
  logger->LogNumber(Logger::STRING_PASSWORD_CHANGE_STATUS_UPDATE,
                    static_cast<int>(observation));
  if (update &&
      update->type == glic::mojom::ExperimentalTriggeringUpdateType::kWorklog) {
    logger->LogString(Logger::STRING_PASSWORD_CHANGE_WORKLOG, update->data);
  }
}

std::optional<PasswordChangeActuator::State> ParseStateFromUpdateData(
    std::string_view response_text) {
  using State = PasswordChangeActuator::State;
  static constexpr std::pair<std::string_view, State> kResponseStates[] = {
      {"PASSWORD_CHANGE_FINISHED_SUCCESSFULLY",
       State::kPasswordSuccessfullyChanged},
      {"FAILED_TO_FIND_CHANGE_PASSWORD_FORM",
       State::kChangePasswordFormNotFound},
      {"FAILED_TO_CHANGE_PASSWORD", State::kPasswordChangeFailed},
      {"USER_INTERVENTION_REQUIRED", State::kOtpDetected},
      {"USER_LOGIN_REQUIRED", State::kChangePasswordFormNotFound},
      {"UNKNOWN_FAILURE", State::kPasswordChangeFailed},
  };

  std::string clean_text = base::CollapseWhitespaceASCII(
      response_text, /*trim_sequences_with_line_breaks=*/true);
  for (const auto& [token, state] : kResponseStates) {
    if (clean_text.contains(token)) {
      return state;
    }
  }
  return std::nullopt;
}

std::optional<PasswordChangeActuator::State> GetTerminalState(
    const glic::mojom::ExperimentalTriggeringUpdate* update,
    glic::mojom::SubscriberObservationType observation) {
  using State = PasswordChangeActuator::State;
  if (observation == glic::mojom::SubscriberObservationType::kError) {
    return State::kPasswordChangeFailed;
  }
  if (!update) {
    return std::nullopt;
  }

  switch (update->type) {
    case glic::mojom::ExperimentalTriggeringUpdateType::kTerminalFailed:
    case glic::mojom::ExperimentalTriggeringUpdateType::kTerminalStopped:
      return State::kPasswordChangeFailed;
    case glic::mojom::ExperimentalTriggeringUpdateType::kYieldToUser:
    case glic::mojom::ExperimentalTriggeringUpdateType::kPaused:
      return State::kOtpDetected;
    case glic::mojom::ExperimentalTriggeringUpdateType::kUnknown:
    case glic::mojom::ExperimentalTriggeringUpdateType::kWorklog:
    case glic::mojom::ExperimentalTriggeringUpdateType::kTerminalCompletion:
    case glic::mojom::ExperimentalTriggeringUpdateType::kResumed:
      return ParseStateFromUpdateData(update->data);
  }
}

origin_gating::TaskPolicyConfig BuildPasswordChangeContainerConfig(
    const std::set<net::SchemefulSite>& allowed_origins) {
  origin_gating::TaskPolicyConfig::LocationRules rules;

  origin_gating::TaskPolicyConfig::Rule rule(
      /*navigation_sources=*/{},
      /*resources=*/
      {origin_gating::TaskPolicyConfig::Rule::Resource::kSession},
      /*allowed_tools=*/actor::GetAllActorTools());

  for (const auto& origin : allowed_origins) {
    rules.emplace(origin_gating::TaskPolicyConfig::Location(origin), rule);
  }

  return origin_gating::TaskPolicyConfig(
      std::move(rules),
      origin_gating::ClientTool(
          optimization_guide::proto::RuleMetadata::CAPABILITY_NAVIGATE));
}

}  // namespace

GlicPasswordChangeActuator::GlicPasswordChangeActuator(
    password_manager::LeakedPasswordDetails leak_details,
    content::WebContents* originator)
    : credential_(password_manager::FromPasswordForm(
          std::move(leak_details.credentials))),
      change_password_url_match_(
          std::move(leak_details.change_password_url_match).value()),
      originator_(originator ? originator->GetWeakPtr() : nullptr) {
  CHECK(change_password_url_match_.change_password_url.is_valid());
}

GlicPasswordChangeActuator::GlicPasswordChangeActuator(
    password_manager::StoredCredential credential,
    content::WebContents* originator)
    : credential_(std::move(credential)),
      change_password_url_match_(credential_.url),
      originator_(originator ? originator->GetWeakPtr() : nullptr) {
  CHECK(change_password_url_match_.change_password_url.is_valid());
}

GlicPasswordChangeActuator::~GlicPasswordChangeActuator() {
  ResetInternalState(actor::ActorTask::StoppedReason::kShutdown);
}

void GlicPasswordChangeActuator::Start() {
  if (!originator_) {
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  const GURL& target_url = change_password_url_match_.change_password_url;
  std::string reach_form_prompt = GetReachFormPrompt(target_url.spec());

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

  content::OpenURLParams open_url_params =
      content::OpenURLParams::CreateBrowserInitiated(
          target_url, ui::mojom::WindowOpenDisposition::kNewBackgroundTab,
          ui::PAGE_TRANSITION_AUTO_TOPLEVEL);

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
  options.on_client_connected =
      base::BindOnce(&GlicPasswordChangeActuator::SubscribeForTriggeringUpdates,
                     weak_ptr_factory_.GetWeakPtr());
  glic::GlicInvokeWithAutoSubmitOptions auto_submit_options;
  auto_submit_options.show_panel = false;
  glic_instance_ =
      glic_service
          ->InvokeWithAutoSubmit(
              glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
              std::move(options), std::move(auto_submit_options))
          .instance();

  actuation_web_contents_ = new_contents->GetWeakPtr();
  if (auto logger = GetLoggerIfAvailable(originator_.get())) {
    logger->LogMessage(Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_START_FLOW);
  }

  actor::ActorKeyedService* actor_service = actor::ActorKeyedService::Get(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  if (!actor_service) {
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  actor_task_state_subscription_ = actor_service->AddTaskStateChangedCallback(
      base::BindRepeating(&GlicPasswordChangeActuator::OnActorTaskStateChanged,
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
    originator->OpenURL(content::OpenURLParams::CreateBrowserInitiated(
                            change_password_url_match_.change_password_url,
                            ui::mojom::WindowOpenDisposition::kNewForegroundTab,
                            ui::PAGE_TRANSITION_LINK),
                        /*navigation_handle_callback=*/{});
  }
}

PasswordChangeActuator::NewPasswordResult
GlicPasswordChangeActuator::GetOrGenerateNewPassword(
    password_manager::PasswordFormManager* form_manager) {
  using Error = password_manager::PasswordChangeCredentialsProvider::Error;
  if (generated_password_.has_value()) {
    return *generated_password_;
  }
  if (!form_manager) {
    return base::unexpected(Error::kGenerationFailed);
  }

  const password_manager::PasswordForm* parsed_form =
      form_manager->GetParsedObservedForm();
  CHECK(parsed_form);
  base::WeakPtr<password_manager::PasswordManagerDriver> driver =
      form_manager->GetDriver();
  password_manager::PasswordGenerationFrameHelper* generation_helper =
      driver ? driver->GetPasswordGenerationHelper() : nullptr;
  if (!generation_helper) {
    return base::unexpected(Error::kGenerationFailed);
  }

  generated_password_ = password_manager::GeneratePasswordForChangePasswordForm(
                            *parsed_form, *generation_helper)
                            .password;

  // Keep a copy of the form manager to promote the backup password once the
  // password change succeeds.
  form_manager_ = form_manager->Clone();

  // Save the generated password as a backup of the credential being changed,
  // so that it isn't lost if the website accepts it but Chrome doesn't save
  // it.
  parsed_form_ = std::make_unique<password_manager::PasswordForm>(*parsed_form);
  parsed_form_->username_value = credential_.username_value;
  // TODO(crbug.com/422125487): Fix metrics duplication.
  form_manager_->Clone()->SaveWithBackupPassword(
      *parsed_form_, credential_.password_value, *generated_password_);
  // Refetch the credentials so that `form_manager_` is aware of the backup.
  form_manager_->GetFormFetcher()->Fetch();

  return *generated_password_;
}

bool GlicPasswordChangeActuator::SaveGeneratedPassword() {
  if (!form_manager_) {
    return false;
  }
  CHECK(parsed_form_);
  // TODO(crbug.com/581337075): Figure out if waiting for `OnFetchCompleted()`
  // is worth it.
  form_manager_->SaveWithBackupPassword(std::move(*parsed_form_),
                                        *generated_password_,
                                        credential_.password_value);
  // Load-bearing: `form_manager_` stays alive so any save scheduled for
  // `OnFetchCompleted()` is not dropped, so resetting `parsed_form_` enforces
  // that `SaveGeneratedPassword()` is called at most once.
  parsed_form_.reset();
  return true;
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

void GlicPasswordChangeActuator::OnActorTaskStateChanged(
    actor::ActorTask& task) {
  if (!actuation_web_contents_) {
    return;
  }
  tabs::TabInterface* actuation_tab =
      tabs::TabInterface::MaybeGetFromContents(actuation_web_contents_.get());
  // Ignore updates coming from other tabs, then we initiated actuation in.
  if (!actuation_tab || !task.HasActorSurface(actor::ActorSurfaceHandle::From(
                            actuation_tab->GetHandle()))) {
    return;
  }

  if (task_id_.has_value()) {
    return;
  }
  task_id_ = task.id();

  task.GetExecutionEngine().SetActorLoginService(
      std::make_unique<actor_login::PasswordChangeFromCheckupActorLoginService>(
          password_manager::CloneStoredCredential(credential_)));

  // TODO(crbug.com/559497033): Inject TaskPolicyConfig through API when
  // it's ready
  CHECK(!task.GetExecutionEngine()
             .GetOriginGatingChecker()
             .task_policy_config_slot()
             .has_value());
  std::set<net::SchemefulSite> allowed_origins;
  allowed_origins.emplace(credential_.url);
  allowed_origins.emplace(change_password_url_match_.change_password_url);
  for (const affiliations::FacetURI& facet_uri :
       change_password_url_match_.affiliations) {
    if (facet_uri.IsValidWebFacetURI()) {
      allowed_origins.emplace(GURL(facet_uri.canonical_spec()));
    }
  }

  task.GetExecutionEngine()
      .GetOriginGatingChecker()
      .task_policy_config_slot()
      .Assign(BuildPasswordChangeContainerConfig(allowed_origins));

  if (auto logger = GetLoggerIfAvailable(originator_.get())) {
    logger->LogMessage(
        Logger::STRING_PASSWORD_CHANGE_FROM_CHECKUP_FIND_FORM_TASK_FOUND);
  }
}

void GlicPasswordChangeActuator::CloseGlicSession() {
  updates_receiver_.reset();
  if (glic_instance_) {
    glic_instance_->CancelInvoke();
  }
}

void GlicPasswordChangeActuator::ResetInternalState(
    actor::ActorTask::StoppedReason stop_reason) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (actuation_web_contents_) {
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(Profile::FromBrowserContext(
            actuation_web_contents_->GetBrowserContext()));
    if (actor_service && task_id_.has_value() &&
        actor_service->GetTask(*task_id_)) {
      actor_service->StopTask(*task_id_, stop_reason);
    }
  }

  CloseGlicSession();

  tab_will_detach_subscription_ = {};
  updates_receiver_.reset();
  actor_task_state_subscription_ = {};

  task_id_ = std::nullopt;
  parsed_form_.reset();
  form_manager_.reset();
}

void GlicPasswordChangeActuator::NotifyStateChanged(
    PasswordChangeActuator::State new_state) {
  observers_.Notify(&PasswordChangeActuator::Observer::OnActuationStateChanged,
                    new_state);
}

void GlicPasswordChangeActuator::SubscribeForTriggeringUpdates(
    base::WeakPtr<glic::GlicInstance> instance) {
  if (!instance) {
    if (auto logger = GetLoggerIfAvailable(originator_.get())) {
      logger->LogMessage(
          Logger::STRING_PASSWORD_CHANGE_GLIC_INSTANCE_NOT_AVAILABLE);
    }
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
    return;
  }

  if (auto* manager = instance->GetExperimentalTriggeringManager()) {
    mojo::PendingRemote<glic::mojom::ExperimentalTriggeringUpdatesHandler>
        remote;
    if (updates_receiver_.is_bound()) {
      updates_receiver_.reset();
    }
    updates_receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    updates_receiver_.set_disconnect_handler(base::BindOnce(
        &GlicPasswordChangeActuator::OnUpdatesReceiverDisconnected,
        weak_ptr_factory_.GetWeakPtr()));
    manager->GetExperimentalTriggeringUpdates(
        std::move(remote),
        base::BindOnce(
            &GlicPasswordChangeActuator::OnExperimentalTriggeringRegistered,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void GlicPasswordChangeActuator::OnExperimentalTriggeringRegistered(
    bool success) {
  if (!success) {
    if (auto logger = GetLoggerIfAvailable(originator_.get())) {
      logger->LogMessage(
          Logger::
              STRING_PASSWORD_CHANGE_FAILED_TO_REGISTER_EXPERIMENTAL_TRIGGERING);
    }
    CloseGlicSession();
    NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
  }
}

void GlicPasswordChangeActuator::OnUpdatesReceiverDisconnected() {
  CloseGlicSession();
  NotifyStateChanged(PasswordChangeActuator::State::kPasswordChangeFailed);
}

void GlicPasswordChangeActuator::OnUpdate(
    glic::mojom::ExperimentalTriggeringUpdatePtr update,
    glic::mojom::SubscriberObservationType observation) {
  LogTriggeringUpdate(originator_.get(), update.get(), observation);

  std::optional<State> terminal_state =
      GetTerminalState(update.get(), observation);
  if (!terminal_state.has_value()) {
    return;
  }

  CloseGlicSession();
  if (*terminal_state == State::kPasswordSuccessfullyChanged) {
    // Flow completed; try saving the new password as the main password.
    if (!SaveGeneratedPassword()) {
      terminal_state = State::kPasswordChangeFailed;
    }
  }
  NotifyStateChanged(*terminal_state);
}
