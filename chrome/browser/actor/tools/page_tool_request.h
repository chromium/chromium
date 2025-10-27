// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

mojom::ToolTargetPtr ToMojo(const PageTarget& target);
// Tool requests targeting a specific, existing document should inherit from
// this subclass. Being page-scoped implies also being tab-scoped since a page
// exists inside a tab.
//
// Note: A page tool is scoped to a specific (local root) document, however,
// until tool invocation time it isn't valid to dereference the RenderFrameHost
// from the request. This is because the final frame that will be used isn't
// known until the request goes through TimeOfUseValidation and the tool is
// ready to invoke.
class PageToolRequest : public TabToolRequest {
 public:
  PageToolRequest(tabs::TabHandle tab_handle, const PageTarget& target);
  ~PageToolRequest() override;
  PageToolRequest(const PageToolRequest& other);

  // Converts this request into the ToolAction mojo message which can be
  // executed in the renderer.
  virtual mojom::ToolActionPtr ToMojoToolAction(
      content::RenderFrameHost& frame) const = 0;

  virtual std::unique_ptr<PageToolRequest> Clone() const = 0;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;

  // Returns what in the page the tool should act upon.
  const PageTarget& GetTarget() const;

 private:
  PageTarget target_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_REQUEST_H_
