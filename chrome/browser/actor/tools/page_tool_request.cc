// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_tool_request.h"

#include "chrome/browser/actor/tools/page_tool.h"
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

// static
mojom::ToolTargetPtr PageToolRequest::ToMojoToolTarget(const Target& target) {
  if (std::holds_alternative<CoordinateTarget>(target)) {
    return actor::mojom::ToolTarget::NewCoordinate(
        std::get<CoordinateTarget>(target));
  }

  NodeTarget node_id = std::get<NodeTarget>(target);
  return actor::mojom::ToolTarget::NewDomNodeId(
      node_id.value_or(kRootElementDomNodeId));
}

PageToolRequest::PageToolRequest(TabHandle tab_handle,
                                 std::string_view document_identifier,
                                 const Target& target)
    : TabToolRequest(tab_handle),
      document_identifier_(document_identifier),
      target_(target) {}

PageToolRequest::~PageToolRequest() = default;

PageToolRequest::PageToolRequest(const PageToolRequest& other) = default;

ToolRequest::CreateToolResult PageToolRequest::CreateTool(
    AggregatedJournal& journal) const {
  if (!GetTabHandle().Get()) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<PageTool>(*this, journal), MakeOkResult()};
}

const std::optional<std::string>& PageToolRequest::DocumentIdentifier() const {
  return document_identifier_;
}

const PageToolRequest::Target& PageToolRequest::GetTarget() const {
  return target_;
}

}  // namespace actor
