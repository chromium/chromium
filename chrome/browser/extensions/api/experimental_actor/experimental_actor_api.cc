// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experimental_actor/experimental_actor_api.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/strings/string_split.h"
#include "base/version_info/channel.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/experimental_actor.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/sessions/content/session_tab_helper.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

namespace {

// Converts a session tab id to a tab handle.
int32_t ConvertSessionTabIdToTabHandle(
    int32_t session_tab_id,
    content::BrowserContext* browser_context) {
  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(session_tab_id, browser_context,
                                    /*include_incognito=*/true,
                                    &web_contents)) {
    return tabs::TabHandle::Null().raw_value();
  }
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  // Can be null for pre-render web-contents.
  // TODO(crbug.com/369319589): Remove this logic.
  if (!tab) {
    return tabs::TabHandle::Null().raw_value();
  }
  return tab->GetHandle().raw_value();
}

// Converts a tab handle to a session tab id.
int32_t ConvertTabHandleToSessionTabId(
    int32_t tab_handle,
    content::BrowserContext* browser_context) {
  tabs::TabInterface* tab = tabs::TabHandle(tab_handle).Get();
  if (!tab) {
    return api::tabs::TAB_ID_NONE;
  }
  return sessions::SessionTabHelper::IdForTab(tab->GetContents()).id();
}
}  // namespace

ExperimentalActorApiFunction::ExperimentalActorApiFunction() = default;

ExperimentalActorApiFunction::~ExperimentalActorApiFunction() = default;

bool ExperimentalActorApiFunction::PreRunValidation(std::string* error) {
#if !BUILDFLAG(ENABLE_GLIC)
  *error = "Actions not supported for this build configuration.";
  return false;
#else
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

  return true;
#endif
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
  task.set_tab_id(tab_handle);

  auto* actor_service = actor::ActorKeyedService::Get(browser_context());

  actor_service->StartTask(
      std::move(task),
      base::BindOnce(&ExperimentalActorStartTaskFunction::OnTaskStarted, this));

  return RespondLater();
}

void ExperimentalActorStartTaskFunction::OnTaskStarted(
    optimization_guide::proto::BrowserStartTaskResult task) {
  std::vector<uint8_t> data_buffer(task.ByteSizeLong());
  task.SerializeToArray(&data_buffer[0], task.ByteSizeLong());
  Respond(ArgumentList(api::experimental_actor::StartTask::Results::Create(
      std::move(data_buffer))));
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
  actor_service->ExecuteAction(
      std::move(action),
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

}  // namespace extensions
