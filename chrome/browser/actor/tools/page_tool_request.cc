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

PageToolRequest::Target::Target(const NodeTarget& node_target)
    : impl_(node_target) {}

PageToolRequest::Target::Target(const CoordinateTarget& coordinate_target)
    : impl_(coordinate_target) {}

PageToolRequest::Target::Target(const Target& other) = default;

PageToolRequest::Target::~Target() = default;

// static
mojom::ToolTargetPtr PageToolRequest::ToMojoToolTarget(const Target& target) {
  // TODO(crbug.com/419037299): This needs to take in a target RenderFrameHost&
  // and convert from WebContents-relative coordinates into Widget-local
  // coordinates.
  if (target.is_coordinate()) {
    return actor::mojom::ToolTarget::NewCoordinate(target.coordinate());
  }

  CHECK(target.is_node());
  return actor::mojom::ToolTarget::NewDomNodeId(target.node().dom_node_id);
}

PageToolRequest::PageToolRequest(TabHandle tab_handle, const Target& target)
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

const PageToolRequest::Target& PageToolRequest::GetTarget() const {
  return target_;
}

}  // namespace actor
