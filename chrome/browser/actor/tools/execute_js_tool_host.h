// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_EXECUTE_JS_TOOL_HOST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_EXECUTE_JS_TOOL_HOST_H_

#include <memory>
#include <string>

#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace actor {

// Manages execution of dynamic JavaScript code strings in the renderer.
class ExecuteJsToolHost : public Tool, content::WebContentsObserver {
 public:
  ExecuteJsToolHost(TaskId task_id,
                    ToolDelegate& tool_delegate,
                    ActorSurfaceHandle target_actor_surface_handle,
                    const base::UnguessableToken& target_document_id,
                    mojom::ToolActionPtr action);
  ~ExecuteJsToolHost() override;

  // actor::Tool implementation.
  void Validate(ToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void Invoke(ToolCallback callback) override;
  void Cancel() override;
  std::string DebugString() const override;
  GURL JournalURL() const override;
  std::string JournalEvent() const override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

 private:
  enum class Lifecycle { kInitial, kInvokeSent, kDone };

  // WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* rfh) override;
  void OnTabWillBeRemoved(tabs::TabInterface* tab,
                          tabs::TabInterface::DetachReason reason);
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  void OnToolInvoked(mojom::ActionResultPtr result);
  void PostErrorResult(ToolCallback tool_callback,
                       mojom::ActionResultCode code,
                       const std::string& message = "");
  void OnToolTimeout();
  void TearDown();

  Lifecycle lifecycle_{Lifecycle::kInitial};
  ActorSurfaceHandle actor_surface_handle_;
  const base::UnguessableToken target_document_id_;
  const mojom::ToolActionPtr action_;
  ToolCallback tool_done_callback_;

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
      target_document_render_frame_;
  content::WeakDocumentPtr target_document_;
  content::FrameTreeNodeId target_frame_tree_node_id_;

  base::OneShotTimer timeout_timer_;
  base::CallbackListSubscription tab_will_detach_subscription_;
  base::WeakPtrFactory<ExecuteJsToolHost> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_EXECUTE_JS_TOOL_HOST_H_
