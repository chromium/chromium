// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool_request.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

ChangePasswordToolRequest::ChangePasswordToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    std::optional<PageTarget> current_password_field,
    std::optional<PageTarget> new_password_field,
    std::optional<PageTarget> confirm_new_password_field,
    std::optional<PageTarget> confirm_button)
    : TabToolRequest(actor_surface_handle),
      current_password_field_(std::move(current_password_field)),
      new_password_field_(std::move(new_password_field)),
      confirm_new_password_field_(std::move(confirm_new_password_field)),
      confirm_button_(std::move(confirm_button)) {}

ChangePasswordToolRequest::ChangePasswordToolRequest(
    const ChangePasswordToolRequest&) = default;

ChangePasswordToolRequest& ChangePasswordToolRequest::operator=(
    const ChangePasswordToolRequest&) = default;

ChangePasswordToolRequest::~ChangePasswordToolRequest() = default;

ToolRequest::CreateToolResult ChangePasswordToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }
  return {
      std::make_unique<ChangePasswordTool>(
          task_id, tool_delegate, *actor_surface, current_password_field_,
          new_password_field_, confirm_new_password_field_, confirm_button_),
      MakeOkResult()};
}

void ChangePasswordToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ChangePasswordToolRequest::Name() const {
  return kName;
}

}  // namespace actor
