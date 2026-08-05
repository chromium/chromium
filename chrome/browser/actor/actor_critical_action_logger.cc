// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_critical_action_logger.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/actor/core/task_id.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace actor {

namespace {

std::string GetActionMetadata(const ToolRequest& action,
                              critical_actions::ActionType action_type) {
  if (action_type != critical_actions::ActionType::kFormFill) {
    return "";
  }

  CHECK_EQ(action.Name(), AttemptFormFillingToolRequest::kName);
  const auto& form_req =
      *static_cast<const AttemptFormFillingToolRequest*>(&action);
  base::ListValue data_list;
  for (const auto& req : form_req.requests()) {
    data_list.Append(autofill::ActorFormFillingRequestedDataToStringView(
        req.requested_data));
  }
  base::DictValue dict;
  dict.Set("requested_data", std::move(data_list));

  std::string json_metadata;
  if (base::JSONWriter::Write(dict, &json_metadata)) {
    return json_metadata;
  }
  return "";
}

critical_actions::ActionType EvaluateLoginRequest(
    const mojom::ActionResult& result) {
  if (!result.attempt_login_status.has_value()) {
    return critical_actions::ActionType::kUnknown;
  }

  switch (*result.attempt_login_status) {
    case mojom::AttemptLoginStatus::kFederated:
      return critical_actions::ActionType::kFederatedLogin;
    case mojom::AttemptLoginStatus::kPasswordManager:
      return critical_actions::ActionType::kGooglePasswordManager;
  }
  return critical_actions::ActionType::kUnknown;
}

critical_actions::ActionType EvaluateToolRequest(
    const ToolRequest& action,
    const mojom::ActionResult& result) {
  const std::string_view name = action.Name();

  if (name == AttemptLoginToolRequest::kName) {
    return EvaluateLoginRequest(result);
  }
  if (name == AttemptOtpFillingToolRequest::kName) {
    return critical_actions::ActionType::kCredentialsOtp;
  }
  if (name == AttemptFormFillingToolRequest::kName) {
    return critical_actions::ActionType::kFormFill;
  }

  return critical_actions::ActionType::kUnknown;
}

}  // namespace

void ActorCriticalActionLogger::MaybeLogAction(
    ActorTask& task,
    Profile* profile,
    const ToolRequest& action,
    const mojom::ActionResult& result,
    ukm::SourceId navigation_id) {
  critical_actions::ActionType action_type =
      EvaluateToolRequest(action, result);
  if (action_type == critical_actions::ActionType::kUnknown) {
    return;
  }

  LogAgentSelfReportedAction(profile, task.source_info().id.value_or(""),
                             action_type, action.GetURLForJournal(),
                             navigation_id, task.id(),
                             GetActionMetadata(action, action_type));
}

void ActorCriticalActionLogger::LogAgentSelfReportedAction(
    Profile* profile,
    std::string conversation_id,
    critical_actions::ActionType action_type,
    const GURL& url,
    ukm::SourceId navigation_id,
    TaskId actor_task_id,
    std::string metadata) {
  if (!profile) {
    return;
  }

  critical_actions::CriticalActionService* service =
      critical_actions::CriticalActionFactory::GetForProfile(profile);
  if (!service) {
    return;
  }

  LogEntry(*service, action_type, std::move(conversation_id), actor_task_id,
           url, std::move(metadata), navigation_id);
}

void ActorCriticalActionLogger::LogEntry(
    critical_actions::CriticalActionService& service,
    critical_actions::ActionType action_type,
    std::string conversation_id,
    TaskId actor_task_id,
    const GURL& url,
    std::string metadata,
    ukm::SourceId navigation_id) {
  critical_actions::CriticalActionEntry entry;
  entry.critical_action_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.timestamp = base::Time::Now();
  entry.action_source = critical_actions::ActionSource::kActor;
  entry.action_type = action_type;
  entry.conversation_id = std::move(conversation_id);
  entry.actor_task_id = actor_task_id.is_null()
                            ? ""
                            : base::NumberToString(actor_task_id.value());
  entry.url = url;
  entry.metadata = std::move(metadata);

  service.AddCriticalActionWithNavigationId(entry, navigation_id);
}

}  // namespace actor
