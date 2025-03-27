// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_tool.h"

#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using content::RenderFrameHost;

namespace actor {

PageTool::PageTool(RenderFrameHost& frame, const ToolInvocation& invocation)
    : invocation_(invocation) {
  frame.GetRemoteAssociatedInterfaces()->GetInterface(&chrome_render_frame_);
}

PageTool::~PageTool() = default;

void PageTool::Validate(ValidateCallback callback) {
  // No browser-side validation yet.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void PageTool::Invoke(InvokeCallback callback) {
  auto request = actor::mojom::ToolInvocation::New();
  request->dom_node_id = invocation_.GetTargetDOMNodeId();

  chrome_render_frame_->InvokeTool(std::move(request), std::move(callback));
}

}  // namespace actor
