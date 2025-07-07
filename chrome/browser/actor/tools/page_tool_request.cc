// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_tool_request.h"

#include "chrome/browser/actor/tools/page_tool.h"
#include "chrome/browser/actor/variant_visitor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace actor {

using content::RenderFrameHost;
using content::WebContents;
using optimization_guide::DocumentIdentifierUserData;
using tabs::TabHandle;

namespace {
constexpr Visitor ToMojoFn{
    [](const gfx::Point& pt) -> mojom::ToolTargetPtr {
      return actor::mojom::ToolTarget::NewCoordinate(pt);
    },
    [](const DomNode& node) -> mojom::ToolTargetPtr {
      return actor::mojom::ToolTarget::NewDomNodeId(node.node_id);
    },
};
}  // namespace

mojom::ToolTargetPtr ToMojo(const PageTarget& target) {
  return std::visit(ToMojoFn, target);
}

PageToolRequest::PageToolRequest(TabHandle tab_handle, const PageTarget& target)
    : TabToolRequest(tab_handle), target_(target) {}

PageToolRequest::~PageToolRequest() = default;

PageToolRequest::PageToolRequest(const PageToolRequest& other) = default;

ToolRequest::CreateToolResult PageToolRequest::CreateTool(
    TaskId task_id,
    AggregatedJournal& journal) const {
  if (!GetTabHandle().Get()) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<PageTool>(task_id, journal, *this), MakeOkResult()};
}

const PageTarget& PageToolRequest::GetTarget() const {
  return target_;
}

}  // namespace actor
