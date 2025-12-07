// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_DRAG_AND_RELEASE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_DRAG_AND_RELEASE_TOOL_REQUEST_H_

#include <memory>
#include <string>

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

  DragAndReleaseToolRequest(tabs::TabHandle tab_handle,
                            const PageTarget& from_target,
                            const PageTarget& to_target);
  ~DragAndReleaseToolRequest() override;
  DragAndReleaseToolRequest(const DragAndReleaseToolRequest& other);

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
