// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experimental_actor/experimental_actor_api.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "base/version_info/channel.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/aggregated_journal_file_serializer.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/extensions/api/experimental_actor.h"
#include "chrome/common/extensions/api/tabs.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "components/tabs/public/tab_interface.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

namespace {

// Converts a session tab id to a tab handle.
int32_t ConvertSessionTabIdToTabHandle(
    int32_t session_tab_id,
    content::BrowserContext* browser_context) {
  return tabs::SessionMappedTabHandleFactory::GetInstance()
      .GetHandleForSessionId(session_tab_id);
}

// Converts a tab handle to a session tab id.
int32_t ConvertTabHandleToSessionTabId(
    int32_t tab_handle,
    content::BrowserContext* browser_context) {
  return tabs::SessionMappedTabHandleFactory::GetInstance()
      .GetSessionIdForHandle(tab_handle)
      .value_or(api::tabs::TAB_ID_NONE);
}

// Helper function to convert the session tab id to a tab handle for any action
// that has a `tab_id` field.
template <typename T>
void ConvertActionTabId(T* action_payload,
                        content::BrowserContext* browser_context) {
  action_payload->set_tab_id(ConvertSessionTabIdToTabHandle(
      action_payload->tab_id(), browser_context));
}

const void* const kSerializerKey = &kSerializerKey;

// File location that the actor journal should be serialized to.
const char kExperimentalActorJournalLog[] = "experimental-actor-journal";

class Serializer : public base::SupportsUserData::Data {
 public:
  explicit Serializer(actor::AggregatedJournal& journal) {
    base::FilePath path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            kExperimentalActorJournalLog);
    if (!path.empty()) {
      serializer_ =
          std::make_unique<actor::AggregatedJournalFileSerializer>(journal);
      serializer_->Init(
          path, base::BindOnce(&Serializer::InitDone, base::Unretained(this)));
    }
  }

  static void EnsureInitialized(content::BrowserContext* context,
                                actor::AggregatedJournal& journal) {
    if (!context->GetUserData(kSerializerKey)) {
      context->SetUserData(kSerializerKey,
                           std::make_unique<Serializer>(journal));
    }
  }

 private:
  void InitDone(bool success) {
    if (!success) {
      serializer_.reset();
    }
  }

  std::unique_ptr<actor::AggregatedJournalFileSerializer> serializer_;
};

}  // namespace

ExperimentalActorApiFunction::ExperimentalActorApiFunction() = default;

ExperimentalActorApiFunction::~ExperimentalActorApiFunction() = default;

bool ExperimentalActorApiFunction::PreRunValidation(std::string* error) {
  if (GetCurrentChannel() == version_info::Channel::STABLE &&
      !AiDataKeyedService::IsExtensionAllowlistedForStable(extension_id())) {
    *error = "API access not allowed on this channel.";
    return false;
  }

  if (!AiDataKeyedService::IsExtensionAllowlistedForActions(extension_id())) {
    *error = "Actions API access restricted for this extension.";
    return false;
  }

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  if (!actor_service) {
    *error = "Incognito profile not supported.";
    return false;
  }

  Serializer::EnsureInitialized(browser_context(), actor_service->GetJournal());
  return true;
}

ExperimentalActorStartTaskFunction::ExperimentalActorStartTaskFunction() =
    default;

ExperimentalActorStartTaskFunction::~ExperimentalActorStartTaskFunction() =
    default;

ExtensionFunction::ResponseAction ExperimentalActorStartTaskFunction::Run() {
  auto params = api::experimental_actor::StartTask::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  optimization_guide::proto::BrowserStartTask task;
  if (!task.ParseFromArray(params->start_task_proto.data(),
                           params->start_task_proto.size())) {
    return RespondNow(
        Error("Parsing optimization_guide::proto::BrowserStartTask failed."));
  }

  // Convert from extension tab ids to TabHandles.
  int32_t tab_handle =
      ConvertSessionTabIdToTabHandle(task.tab_id(), browser_context());

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());

  actor::TaskId task_id = actor_service->CreateTask();

  // If a tab_id wasn't specified, create a new one.
  // TODO(crbug.com/411462297): The client of this API should create a new tab
  // themselves using the CreateTabAction and this code can be removed.
  if (!tab_handle) {
    // Get the most recently active browser for this profile.
    Browser* browser = chrome::FindTabbedBrowser(
        Profile::FromBrowserContext(browser_context()),
        /*match_original_profiles=*/false);
    // If no browser exists create one.
    if (!browser) {
      browser = Browser::Create(
          Browser::CreateParams(Profile::FromBrowserContext(browser_context()),
                                /*user_gesture=*/false));
    }

    std::unique_ptr<actor::ToolRequest> create_tab =
        std::make_unique<actor::CreateTabToolRequest>(
            browser->session_id().id(),
            WindowOpenDisposition::NEW_FOREGROUND_TAB);
    std::vector<std::unique_ptr<actor::ToolRequest>> actions;
    actions.push_back(std::move(create_tab));
    actor_service->PerformActions(
        task_id, std::move(actions),
        base::BindOnce(&ExperimentalActorStartTaskFunction::OnTabCreated, this,
                       browser->AsWeakPtr(), task_id));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExperimentalActorStartTaskFunction::OnTaskStarted, this,
                       task_id, tab_handle));
  }

  return RespondLater();
}

void ExperimentalActorStartTaskFunction::OnTaskStarted(actor::TaskId task_id,
                                                       int32_t tab_id) {
  optimization_guide::proto::BrowserStartTaskResult result;
  result.set_task_id(task_id.value());
  result.set_tab_id(tab_id);
  result.set_status(optimization_guide::proto::BrowserStartTaskResult::SUCCESS);

  std::vector<uint8_t> data_buffer(result.ByteSizeLong());
  result.SerializeToArray(&data_buffer[0], result.ByteSizeLong());
  Respond(ArgumentList(api::experimental_actor::StartTask::Results::Create(
      std::move(data_buffer))));
}

void ExperimentalActorStartTaskFunction::OnTabCreated(
    base::WeakPtr<Browser> browser,
    actor::TaskId task_id,
    actor::mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action) {
  int32_t tab_id = 0;
  // CreateTask assumes it always succeeds but we won't have a tab if the
  // browser is closed during creation.
  if (browser) {
    tab_id =
        browser->tab_strip_model()->GetActiveTab()->GetHandle().raw_value();
  }
  OnTaskStarted(actor::TaskId(task_id), tab_id);
}

ExperimentalActorStopTaskFunction::ExperimentalActorStopTaskFunction() =
    default;

ExperimentalActorStopTaskFunction::~ExperimentalActorStopTaskFunction() =
    default;

ExtensionFunction::ResponseAction ExperimentalActorStopTaskFunction::Run() {
  auto params = api::experimental_actor::StopTask::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());

  actor_service->StopTask(actor::TaskId(params->task_id));
  return RespondNow(
      ArgumentList(api::experimental_actor::StopTask::Results::Create()));
}

ExperimentalActorExecuteActionFunction::
    ExperimentalActorExecuteActionFunction() = default;

ExperimentalActorExecuteActionFunction::
    ~ExperimentalActorExecuteActionFunction() = default;

ExtensionFunction::ResponseAction
ExperimentalActorExecuteActionFunction::Run() {
#if !BUILDFLAG(ENABLE_GLIC)
  return RespondNow(
      Error("Execute action not supported for this build configuration."));
#else
  auto params = api::experimental_actor::ExecuteAction::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  optimization_guide::proto::BrowserAction action;
  if (!action.ParseFromArray(params->browser_action_proto.data(),
                             params->browser_action_proto.size())) {
    return RespondNow(
        Error("Parsing optimization_guide::proto::BrowserAction failed."));
  }

  int32_t tab_handle =
      ConvertSessionTabIdToTabHandle(action.tab_id(), browser_context());
  action.set_tab_id(tab_handle);

  auto* actor_service =
      actor::ActorKeyedServiceFactory::GetActorKeyedService(browser_context());

  actor_service->GetJournal().Log(
      GURL(), actor::TaskId(action.task_id()),
      actor::mojom::JournalTrack::kActor, "ExperimentalActorExecutAction",
      absl::StrFormat("Proto: %s", actor::ToBase64(action)));

  // BuildToolRequest looks for tab_ids on the individual action structs since
  // that's where Glic puts them. However, the extension puts the tab_id on the
  // BrowserAction itself. Use the BrowserAction's tab_id as the fallback tab so
  // that, if Action doesn't provide a tab_id we'll use the
  // BrowserAction.tab_id. This path should go away once extension clients are
  // migrated to PerformActions.
  tabs::TabInterface* browser_action_tab =
      action.has_tab_id() ? tabs::TabHandle(action.tab_id()).Get() : nullptr;

  actor::BuildToolRequestResult requests =
      actor::BuildToolRequest(action, browser_action_tab);

  if (!requests.has_value()) {
    return RespondNow(
        Error("Failed to convert BrowserAction to ToolRequests."));
  }

  actor_service->ExecuteAction(
      actor::TaskId(action.task_id()), std::move(requests.value()),
      base::BindOnce(
          &ExperimentalActorExecuteActionFunction::OnResponseReceived, this));

  return RespondLater();
#endif
}

void ExperimentalActorExecuteActionFunction::OnResponseReceived(
    optimization_guide::proto::BrowserActionResult response) {
  // Convert from tab handle to session tab id.
  int32_t session_tab_id =
      ConvertTabHandleToSessionTabId(response.tab_id(), browser_context());
  response.set_tab_id(session_tab_id);

  std::vector<uint8_t> data_buffer(response.ByteSizeLong());
  if (!data_buffer.empty()) {
    response.SerializeToArray(&data_buffer[0], response.ByteSizeLong());
  }
  Respond(ArgumentList(api::experimental_actor::ExecuteAction::Results::Create(
      std::move(data_buffer))));
}

ExperimentalActorCreateTaskFunction::ExperimentalActorCreateTaskFunction() =
    default;
ExperimentalActorCreateTaskFunction::~ExperimentalActorCreateTaskFunction() =
    default;

ExtensionFunction::ResponseAction ExperimentalActorCreateTaskFunction::Run() {
  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  actor::TaskId task_id = actor_service->CreateTask();

  return RespondNow(ArgumentList(
      api::experimental_actor::CreateTask::Results::Create(task_id.value())));
}

ExperimentalActorPerformActionsFunction::
    ExperimentalActorPerformActionsFunction() = default;
ExperimentalActorPerformActionsFunction::
    ~ExperimentalActorPerformActionsFunction() = default;

ExtensionFunction::ResponseAction
ExperimentalActorPerformActionsFunction::Run() {
  auto params = api::experimental_actor::PerformActions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  optimization_guide::proto::Actions actions;
  if (!actions.ParseFromArray(params->actions_proto.data(),
                              params->actions_proto.size())) {
    return RespondNow(
        Error("Parsing optimization_guide::proto::Actions failed."));
  }

  // Convert from extension tab ids to TabHandles.
  for (auto& action : *actions.mutable_actions()) {
    switch (action.action_case()) {
      case optimization_guide::proto::Action::kClick:
        ConvertActionTabId(action.mutable_click(), browser_context());
        break;
      case optimization_guide::proto::Action::kType:
        ConvertActionTabId(action.mutable_type(), browser_context());
        break;
      case optimization_guide::proto::Action::kScroll:
        ConvertActionTabId(action.mutable_scroll(), browser_context());
        break;
      case optimization_guide::proto::Action::kMoveMouse:
        ConvertActionTabId(action.mutable_move_mouse(), browser_context());
        break;
      case optimization_guide::proto::Action::kDragAndRelease:
        ConvertActionTabId(action.mutable_drag_and_release(),
                           browser_context());
        break;
      case optimization_guide::proto::Action::kSelect:
        ConvertActionTabId(action.mutable_select(), browser_context());
        break;
      case optimization_guide::proto::Action::kNavigate:
        ConvertActionTabId(action.mutable_navigate(), browser_context());
        break;
      case optimization_guide::proto::Action::kBack:
        ConvertActionTabId(action.mutable_back(), browser_context());
        break;
      case optimization_guide::proto::Action::kForward:
        ConvertActionTabId(action.mutable_forward(), browser_context());
        break;
      case optimization_guide::proto::Action::kCloseTab:
        ConvertActionTabId(action.mutable_close_tab(), browser_context());
        break;
      case optimization_guide::proto::Action::kActivateTab:
        ConvertActionTabId(action.mutable_activate_tab(), browser_context());
        break;
      case optimization_guide::proto::Action::kAttemptLogin:
        ConvertActionTabId(action.mutable_attempt_login(), browser_context());
        break;
      case optimization_guide::proto::Action::kScriptTool:
        ConvertActionTabId(action.mutable_script_tool(), browser_context());
        break;
      case optimization_guide::proto::Action::kScrollTo:
        ConvertActionTabId(action.mutable_scroll_to(), browser_context());
        break;
      case optimization_guide::proto::Action::kWait:
      case optimization_guide::proto::Action::kCreateTab:
      case optimization_guide::proto::Action::kCreateWindow:
      case optimization_guide::proto::Action::kCloseWindow:
      case optimization_guide::proto::Action::kActivateWindow:
      case optimization_guide::proto::Action::kYieldToUser:
      case optimization_guide::proto::Action::ACTION_NOT_SET:
        // No tab id to convert.
        break;
    }
  }

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  actor_service->GetJournal().Log(
      GURL(), actor::TaskId(actions.task_id()),
      actor::mojom::JournalTrack::kActor, "ExperimentalActorExecutAction",
      absl::StrFormat("Proto: %s", actor::ToBase64(actions)));

  actor::TaskId task_id(actions.task_id());

  // If the client didn't create a task or passed in the wrong task id, return
  // failure.
  actor::ActorTask* task = actor_service->GetTask(task_id);
  if (!task) {
    return RespondNow(
        Error(absl::StrFormat("Invalid task_id[%d].", task_id.value())));
  }

  actor::BuildToolRequestResult requests = actor::BuildToolRequest(actions);

  if (!requests.has_value()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ExperimentalActorPerformActionsFunction::OnActionsFinished, this,
            task_id, actor::mojom::ActionResultCode::kArgumentsInvalid,
            requests.error()));
    return RespondLater();
  }

  actor_service->PerformActions(
      task_id, std::move(requests.value()),
      base::BindOnce(
          &ExperimentalActorPerformActionsFunction::OnActionsFinished, this,
          task_id));

  return RespondLater();
}

void ExperimentalActorPerformActionsFunction::OnActionsFinished(
    actor::TaskId task_id,
    actor::mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action) {
  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  actor::ActorTask* task = actor_service->GetTask(task_id);

  // Task is checked when calling PerformActions and it cannot be removed once
  // added (a stopped task is no longer active but will still be retrieved by
  // GetTask).
  CHECK(task);

  actor::BuildActionsResultWithObservations(
      *browser_context(), result_code, index_of_failed_action, *task,
      base::BindOnce(
          &ExperimentalActorPerformActionsFunction::OnObservationResult, this));
}

void ExperimentalActorPerformActionsFunction::OnObservationResult(
    std::unique_ptr<optimization_guide::proto::ActionsResult> response) {
  CHECK(response);

  // Convert back from tab handle to session tab id.
  for (optimization_guide::proto::TabObservation& observation :
       *response->mutable_tabs()) {
    // Note: session_tab_id will be -1 if the tab if couldn't be mapped.
    int32_t session_tab_id =
        ConvertTabHandleToSessionTabId(observation.id(), browser_context());
    observation.set_id(session_tab_id);
  }

  std::vector<uint8_t> data_buffer(response->ByteSizeLong());
  if (!data_buffer.empty()) {
    response->SerializeToArray(data_buffer.data(), response->ByteSizeLong());
  }

  Respond(ArgumentList(api::experimental_actor::PerformActions::Results::Create(
      std::move(data_buffer))));
}

ExperimentalActorRequestTabObservationFunction::
    ExperimentalActorRequestTabObservationFunction() = default;
ExperimentalActorRequestTabObservationFunction::
    ~ExperimentalActorRequestTabObservationFunction() = default;

ExtensionFunction::ResponseAction
ExperimentalActorRequestTabObservationFunction::Run() {
  auto params =
      api::experimental_actor::RequestTabObservation::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params->tab_id, browser_context(),
                                    include_incognito_information(),
                                    &web_contents)) {
    return RespondNow(Error(
        ErrorUtils::FormatErrorMessage(ExtensionTabUtil::kTabNotFoundError,
                                       base::NumberToString(params->tab_id))));
  }
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  // Can be null for pre-render web-contents.
  // TODO(crbug.com/369319589): Remove this logic.
  if (!tab) {
    return RespondNow(Error(
        ErrorUtils::FormatErrorMessage(ExtensionTabUtil::kTabNotFoundError,
                                       base::NumberToString(params->tab_id))));
  }

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  actor_service->RequestTabObservation(
      *tab, base::BindOnce(&ExperimentalActorRequestTabObservationFunction::
                               OnObservationFinished,
                           this));

  return RespondLater();
}

void ExperimentalActorRequestTabObservationFunction::OnObservationFinished(
    actor::ActorKeyedService::TabObservationResult observation_result) {
  if (!observation_result.has_value()) {
    Respond(Error(observation_result.error()));
    return;
  }

  // TODO(bokan): This doesn't set the (tab) `id` field, maybe unneeded in this
  // case but would be good for consistency.
  optimization_guide::proto::TabObservation tab_observation =
      actor::ConvertToTabObservation(**observation_result);
  std::vector<uint8_t> data_buffer(tab_observation.ByteSizeLong());
  if (!data_buffer.empty()) {
    tab_observation.SerializeToArray(&data_buffer[0], data_buffer.size());
  }
  Respond(ArgumentList(
      api::experimental_actor::RequestTabObservation::Results::Create(
          std::move(data_buffer))));
}

}  // namespace extensions
