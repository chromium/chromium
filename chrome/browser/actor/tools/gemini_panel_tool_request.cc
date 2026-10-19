// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/gemini_panel_tool_request.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/gemini_panel_tool.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/web_contents.h"

namespace actor {

namespace {

constexpr std::string_view kPromptGeminiPanelToolDescription =
    "Send a prmopt to the Gemini side panel in Chrome. Use this for image "
    "generation or when the user explicitly asks to use Gemini to fulfil a "
    "task.";
constexpr std::string_view kPromptParamDescription =
    "The request, question, or prompt to send to the Gemini side panel. Must "
    "not be empty.";

constexpr std::string_view kCloseGeminiPanelToolDescription =
    "Closes the Gemini side panel in Chrome if it is currently open.";

}  // namespace

PromptGeminiPanelToolRequest::PromptGeminiPanelToolRequest(
    glic::InvokeWithAutoSubmitPasskey passkey,
    ActorSurfaceHandle actor_surface_handle,
    std::string prompt)
    : auto_submit_passkey_(passkey),
      actor_surface_handle_(actor_surface_handle),
      prompt_(std::move(prompt)) {
  CHECK(!actor_surface_handle_.is_null());
}

PromptGeminiPanelToolRequest::~PromptGeminiPanelToolRequest() = default;

PromptGeminiPanelToolRequest::PromptGeminiPanelToolRequest(
    const PromptGeminiPanelToolRequest&) = default;

PromptGeminiPanelToolRequest& PromptGeminiPanelToolRequest::operator=(
    const PromptGeminiPanelToolRequest&) = default;

GURL PromptGeminiPanelToolRequest::GetURLForJournal() const {
  if (ActorSurface* actor_surface = actor_surface_handle_.Get()) {
    return actor_surface->GetWebContents()->GetLastCommittedURL();
  }
  return ToolRequest::GetURLForJournal();
}

ActorSurfaceHandle PromptGeminiPanelToolRequest::GetActorSurfaceHandle() const {
  return actor_surface_handle_;
}

bool PromptGeminiPanelToolRequest::RequiresUrlCheckInCurrentTab() const {
  return false;
}

// static
std::optional<ToolDefinition>
PromptGeminiPanelToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kPromptGeminiPanel, kModelFacingName,
                               kPromptGeminiPanelToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddStringProperty(
          kPromptParam, kPromptParamDescription))
      .Build();
}

ToolRequest::CreateToolResult PromptGeminiPanelToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway)};
  }

  return {
      std::make_unique<GeminiPanelTool>(auto_submit_passkey_, task_id,
                                        tool_delegate, *actor_surface, prompt_),
      MakeOkResult()};
}

void PromptGeminiPanelToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view PromptGeminiPanelToolRequest::Name() const {
  return kName;
}

CloseGeminiPanelToolRequest::CloseGeminiPanelToolRequest(
    ActorSurfaceHandle actor_surface_handle)
    : actor_surface_handle_(actor_surface_handle) {
  CHECK(!actor_surface_handle_.is_null());
}

CloseGeminiPanelToolRequest::~CloseGeminiPanelToolRequest() = default;

CloseGeminiPanelToolRequest::CloseGeminiPanelToolRequest(
    const CloseGeminiPanelToolRequest&) = default;

CloseGeminiPanelToolRequest& CloseGeminiPanelToolRequest::operator=(
    const CloseGeminiPanelToolRequest&) = default;

GURL CloseGeminiPanelToolRequest::GetURLForJournal() const {
  if (ActorSurface* actor_surface = actor_surface_handle_.Get()) {
    return actor_surface->GetWebContents()->GetLastCommittedURL();
  }
  return ToolRequest::GetURLForJournal();
}

ActorSurfaceHandle CloseGeminiPanelToolRequest::GetActorSurfaceHandle() const {
  return actor_surface_handle_;
}

// static
std::optional<ToolDefinition> CloseGeminiPanelToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kCloseGeminiPanel, kModelFacingName,
                               kCloseGeminiPanelToolDescription)
      .Build();
}

bool CloseGeminiPanelToolRequest::RequiresUrlCheckInCurrentTab() const {
  return false;
}

ToolRequest::CreateToolResult CloseGeminiPanelToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway)};
  }

  return {
      std::make_unique<GeminiPanelTool>(task_id, tool_delegate, *actor_surface),
      MakeOkResult()};
}

void CloseGeminiPanelToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view CloseGeminiPanelToolRequest::Name() const {
  return kName;
}

}  // namespace actor
