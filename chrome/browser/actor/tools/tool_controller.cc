// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_controller.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/tools/tool_invocation.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tab_collections/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using content::RenderFrameHost;
using mojo::AssociatedRemote;
using tabs::TabInterface;

namespace actor {

namespace {
// Callback for the reply to the InvokeTool call on a page-level request. This
// is mainly a helper to hold the mojo interface until the reply is received.
void PageInvokeToolReply(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    ToolInvocation::ResultCallback result_callback,
    bool result_success) {
  std::move(result_callback).Run(result_success);
}
}  // namespace

ToolController::ToolController() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
}

ToolController::~ToolController() = default;

void ToolController::Invoke(const ToolInvocation& invocation,
                            ToolInvocation::ResultCallback result_callback) {
  TabInterface* target_tab = invocation.FindTargetTab();
  if (!target_tab) {
    // The tab for this action was closed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback), false));
    return;
  }

  if (invocation.IsTargetingPage()) {
    RenderFrameHost* frame = invocation.FindTargetFrame();

    AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
    frame->GetRemoteAssociatedInterfaces()->GetInterface(&chrome_render_frame);

    auto request = actor::mojom::ToolInvocation::New();
    // TODO(crbug.com/398849001): Fill this struct out
    request->dom_node_id = invocation.GetTargetDOMNodeId();

    // Bind the mojo remote into the callback to keep it alive in order to
    // receive the response.
    chrome::mojom::ChromeRenderFrame::Proxy_* chrome_render_frame_proxy =
        chrome_render_frame.get();
    auto reply_callback =
        base::BindOnce(&PageInvokeToolReply, std::move(chrome_render_frame),
                       std::move(result_callback));

    chrome_render_frame_proxy->InvokeTool(std::move(request),
                                          std::move(reply_callback));
  } else {
    CHECK(invocation.IsTargetingTab());
    // TODO(crbug.com/402731599): Implement tab-level actions.
  }
}

}  // namespace actor
