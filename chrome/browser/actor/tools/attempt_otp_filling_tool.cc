// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"

#include "chrome/common/actor/action_result.h"

namespace actor {

// TODO(b/500265255): Take a Tab Handle and one or maybe many trigger fields.
AttemptOtpFillingTool::AttemptOtpFillingTool(TaskId task_id,
                                             ToolDelegate& tool_delegate)
    : Tool(task_id, tool_delegate) {}

AttemptOtpFillingTool::~AttemptOtpFillingTool() = default;

void AttemptOtpFillingTool::Validate(ToolCallback callback) {
  // We have no validation so far, but this could be a place to check (once
  // more) if the feature is enabled and that the user has not permanently opted
  // out.
  // Note: There's also the method TimeOfUseValidation for checks that happen
  // synchronously just before Invoke().

  std::move(callback).Run(MakeOkResult());
}

void AttemptOtpFillingTool::Invoke(ToolCallback callback) {
  // TODO(b/484334125): Check for user consent before filling!

  // TODO(b/500265255): Call the not yet existing actor service for OTP filling.
  std::move(callback).Run(MakeOkResult());
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
  // TODO(b/500265255): Wait for stabilization
  return nullptr;
}

tabs::TabHandle AttemptOtpFillingTool::GetTargetTab() const {
  // TODO(b/500265255): Return its tab, once it has any.
  return tabs::TabHandle::Null();
}

}  // namespace actor
