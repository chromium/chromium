// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_DRAG_AND_RELEASE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_DRAG_AND_RELEASE_TOOL_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Simulates a mouse press, move, release sequence. As this is a PageTool, the
// sequence can only span a local subtree (i.e. cannot drag and drop between
// OOPIFs or RenderWidgetHosts).
class DragAndReleaseToolRequest : public PageToolRequest {
 public:
  static constexpr char kName[] = "DragAndRelease";
  static constexpr std::string_view kModelFacingName = "drag_and_release";
  static constexpr std::string_view kFromDomNodeIdParam = "from_dom_node_id";
  static constexpr std::string_view kToDomNodeIdParam = "to_dom_node_id";

  DragAndReleaseToolRequest(ActorSurfaceHandle actor_surface_handle,
                            const PageTarget& from_target,
                            const PageTarget& to_target);
  ~DragAndReleaseToolRequest() override;
  DragAndReleaseToolRequest(const DragAndReleaseToolRequest& other);

  // Returns the `ToolId::kDragAndRelease` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  std::string_view Name() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction(
      content::RenderFrameHost& frame) const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

 private:
  PageTarget from_target_;
  PageTarget to_target_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_DRAG_AND_RELEASE_TOOL_REQUEST_H_
