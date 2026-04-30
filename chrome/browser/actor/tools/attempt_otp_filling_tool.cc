// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/actor/actor_task.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/actor/core/shared_types.h"

namespace actor {

AttemptOtpFillingTool::AttemptOtpFillingTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    tabs::TabHandle tab_handle,
    std::vector<PageTarget> trigger_fields,
    bool for_signin)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab_handle),
      trigger_fields_(std::move(trigger_fields)),
      for_signin_(for_signin) {}

AttemptOtpFillingTool::~AttemptOtpFillingTool() = default;

void AttemptOtpFillingTool::Validate(ToolCallback callback) {
  // This could be a place to check (once more) if the feature is enabled and
  // that the user has not permanently opted out.
  // Note: There's also the method TimeOfUseValidation for checks that happen
  // synchronously just before Invoke().

  // TODO(b/500265255): Move this validation to the converter once we add it in
  // chrome/browser/actor/actor_proto_conversion.cc .
  if (trigger_fields_.empty()) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                   /*requires_page_stabilization=*/false,
                   "At least one trigger field must be provided."));
    return;
  }
  std::move(callback).Run(MakeOkResult());
}

mojom::ActionResultPtr AttemptOtpFillingTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  if (!GetTargetTab().Get()) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway,
                      /*requires_page_stabilization=*/false,
                      "Target tab was destroyed before invocation.");
  }

  return MakeOkResult();
}

void AttemptOtpFillingTool::Invoke(ToolCallback callback) {
  // TODO(b/484334125): Check for user consent before filling!
  // The checks will be somewhat different, depending on the for_signin
  // parameter.
  // For now, we just log it ...
  journal().Log(JournalURL(), task_id(), "AttemptOtpFillingTool::Invoke",
                JournalDetailsBuilder()
                    .Add("trigger_fields_count", trigger_fields_.size())
                    .Add("for_signin", for_signin_)
                    .Build());

  // TODO(b/500265255): Call the not yet existing actor service for OTP filling.
  std::move(callback).Run(MakeOkResult());
}

void AttemptOtpFillingTool::UpdateTaskBeforeInvoke(
    ActorTask& task,
    ToolCallback callback) const {
  task.AddTab(GetTargetTab(), /*stop_task_on_detach=*/true,
              std::move(callback));
}

std::string AttemptOtpFillingTool::DebugString() const {
  // This ends up in chrome://actor-internals and will be used for debugging.
  return "AttemptOtpFillingTool";
}

std::string AttemptOtpFillingTool::JournalEvent() const {
  return "AttemptOtpFillingTool";
}

std::unique_ptr<ObservationDelayController>
AttemptOtpFillingTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  content::RenderFrameHost* rfh =
      GetTargetTab().Get()->GetContents()->GetPrimaryMainFrame();

  return std::make_unique<ObservationDelayController>(
      *rfh, task_id(), journal(), std::move(page_stability_config));
}

tabs::TabHandle AttemptOtpFillingTool::GetTargetTab() const {
  return tab_handle_;
}

}  // namespace actor
