// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_H_

#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_invocation.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrameHost;
}

namespace actor {

// A page tool is any tool implemented in the renderer by ToolExecutor. This
// class is shared by multiple tools and serves to implement the mojo shuttling
// of the request to the renderer.
class PageTool : public Tool {
 public:
  PageTool(content::RenderFrameHost& frame, const ToolInvocation& invocation);
  ~PageTool() override;

  // actor::Tool
  void Validate(ValidateCallback callback) override;
  void Invoke(InvokeCallback callback) override;
  std::string DebugString() const override;

 private:
  ToolInvocation invocation_;
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_H_
