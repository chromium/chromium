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
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/ai/ai_data_keyed_service_factory.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/experimental_actor.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

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

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());
  if (!ai_data_service) {
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

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());

  ai_data_service->StartTask(
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

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());

  ai_data_service->StopTask(
      params->task_id,
      base::BindOnce(&ExperimentalActorStopTaskFunction::OnTaskStopped, this));

  return RespondLater();
}

void ExperimentalActorStopTaskFunction::OnTaskStopped(bool success) {
  if (!success) {
    Respond(Error("Task not found."));
    return;
  }
  Respond(ArgumentList(api::experimental_actor::StopTask::Results::Create()));
}

ExperimentalActorExecuteActionFunction::
    ExperimentalActorExecuteActionFunction() = default;

ExperimentalActorExecuteActionFunction::
    ~ExperimentalActorExecuteActionFunction() = default;

ExtensionFunction::ResponseAction
ExperimentalActorExecuteActionFunction::Run() {
  auto params = api::experimental_actor::ExecuteAction::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  optimization_guide::proto::BrowserAction action;
  if (!action.ParseFromArray(params->browser_action_proto.data(),
                             params->browser_action_proto.size())) {
    return RespondNow(
        Error("Parsing optimization_guide::proto::BrowserAction failed."));
  }

  auto* ai_data_service =
      AiDataKeyedServiceFactory::GetAiDataKeyedService(browser_context());

  ai_data_service->ExecuteAction(
      std::move(action),
      base::BindOnce(
          &ExperimentalActorExecuteActionFunction::OnResponseReceived, this));

  return RespondLater();
}

void ExperimentalActorExecuteActionFunction::OnResponseReceived(
    optimization_guide::proto::BrowserActionResult response) {
  std::vector<uint8_t> data_buffer(response.ByteSizeLong());
  if (!data_buffer.empty()) {
    response.SerializeToArray(&data_buffer[0], response.ByteSizeLong());
  }
  Respond(ArgumentList(api::experimental_actor::ExecuteAction::Results::Create(
      std::move(data_buffer))));
}

}  // namespace extensions
