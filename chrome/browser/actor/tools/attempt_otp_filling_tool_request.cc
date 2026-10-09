// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/actor_login_flow_verifier.h"
#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

namespace {

constexpr std::string_view kAttemptOtpFillingToolDescription =
    "Attempts to fill a one-time password (OTP) or verification code into "
    "input fields on the page.";
constexpr std::string_view kTriggerFieldDomNodeIdsParamDescription =
    "The DOM node IDs of the input fields where the OTP or verification code "
    "should be filled.";
constexpr std::string_view kForSigninParamDescription =
    "Whether the OTP is needed for a sign-in.";

}  // namespace

AttemptOtpFillingToolRequest::AttemptOtpFillingToolRequest(
    tabs::TabHandle tab_handle,
    std::vector<PageTarget> trigger_fields,
    bool for_signin,
    OtpType predicted_otp_type)
    : TabToolRequest(tab_handle),
      trigger_fields_(std::move(trigger_fields)),
      for_signin_(for_signin),
      predicted_otp_type_(predicted_otp_type) {}

AttemptOtpFillingToolRequest::AttemptOtpFillingToolRequest(
    const AttemptOtpFillingToolRequest&) = default;

AttemptOtpFillingToolRequest& AttemptOtpFillingToolRequest::operator=(
    const AttemptOtpFillingToolRequest&) = default;

AttemptOtpFillingToolRequest::~AttemptOtpFillingToolRequest() = default;

// static
std::optional<ToolDefinition>
AttemptOtpFillingToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kAttemptOtpFilling, kModelFacingName,
                               kAttemptOtpFillingToolDescription)
      .SetToolParameterSchema(
          ToolSchemaBuilder()
              .AddArrayProperty(kTriggerFieldDomNodeIdsParam,
                                kTriggerFieldDomNodeIdsParamDescription,
                                ToolSchemaBuilder::ArrayItemType::kInteger)
              .AddBooleanProperty(kForSigninParam, kForSigninParamDescription))
      .Build();
}

ToolRequest::CreateToolResult AttemptOtpFillingToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  auto* affiliation_service =
      AffiliationServiceFactory::GetForProfile(&tool_delegate.GetProfile());
  return {std::make_unique<AttemptOtpFillingTool>(
              task_id, tool_delegate, *actor_surface, trigger_fields_,
              for_signin_, predicted_otp_type_,
              std::make_unique<ActorLoginFlowVerifier>(
                  CHECK_DEREF(affiliation_service))),
          MakeOkResult()};
}

std::string_view AttemptOtpFillingToolRequest::Name() const {
  return kName;
}

void AttemptOtpFillingToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

}  // namespace actor
