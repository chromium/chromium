// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/change_password_tool_request.h"

#include <memory>
#include <string_view>

#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

ChangePasswordToolRequest::ChangePasswordToolRequest(tabs::TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}

ChangePasswordToolRequest::ChangePasswordToolRequest(
    const ChangePasswordToolRequest&) = default;

ChangePasswordToolRequest& ChangePasswordToolRequest::operator=(
    const ChangePasswordToolRequest&) = default;

ChangePasswordToolRequest::~ChangePasswordToolRequest() = default;

ToolRequest::CreateToolResult ChangePasswordToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  tabs::TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }
  // TODO(b/561935036): Instantiate ChangePasswordTool once implemented.
  return {/*tool=*/nullptr,
          MakeResult(mojom::ActionResultCode::kNotImplemented,
                     /*requires_page_stabilization=*/false,
                     "ChangePasswordTool is not implemented yet.")};
}

void ChangePasswordToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ChangePasswordToolRequest::Name() const {
  return kName;
}

}  // namespace actor
