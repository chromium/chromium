// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/gemini_panel_tool_request.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/web_contents.h"

namespace actor {

PromptGeminiPanelToolRequest::PromptGeminiPanelToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    std::string prompt)
    : actor_surface_handle_(actor_surface_handle), prompt_(std::move(prompt)) {
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

tabs::TabHandle PromptGeminiPanelToolRequest::GetTabHandle() const {
  return actor_surface_handle_.GetTabHandle();
}

ActorSurfaceHandle PromptGeminiPanelToolRequest::GetActorSurfaceHandle() const {
  return actor_surface_handle_;
}

bool PromptGeminiPanelToolRequest::RequiresUrlCheckInCurrentTab() const {
  return false;
}

ToolRequest::CreateToolResult PromptGeminiPanelToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {nullptr, MakeResult(mojom::ActionResultCode::kNotImplemented)};
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

tabs::TabHandle CloseGeminiPanelToolRequest::GetTabHandle() const {
  return actor_surface_handle_.GetTabHandle();
}

ActorSurfaceHandle CloseGeminiPanelToolRequest::GetActorSurfaceHandle() const {
  return actor_surface_handle_;
}

bool CloseGeminiPanelToolRequest::RequiresUrlCheckInCurrentTab() const {
  return false;
}

ToolRequest::CreateToolResult CloseGeminiPanelToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {nullptr, MakeResult(mojom::ActionResultCode::kNotImplemented)};
}

void CloseGeminiPanelToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view CloseGeminiPanelToolRequest::Name() const {
  return kName;
}

}  // namespace actor
