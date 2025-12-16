// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_H_

#include <memory>

#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

class PageToolRequest;
class RenderFrameChangeObserver;

// A page tool is any tool implemented in the renderer by ToolExecutor. This
// class is shared by multiple tools and serves to implement the mojo shuttling
// of the request to the renderer.
class PageTool : public Tool {
 public:
  PageTool(TaskId task_id,
           ToolDelegate& tool_delegate,
           const PageToolRequest& params);
  ~PageTool() override;

  // actor::Tool
  void Validate(ToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(ToolCallback callback) override;
  void Cancel() override;
  std::string DebugString() const override;
  GURL JournalURL() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;
  tabs::TabHandle GetTargetTab() const override;

 private:
  // Callback for navigation.
  void OnRenderFrameHostChanged();

  // Callback when the renderer process is gone.
  void OnRenderFrameGone();

  void OnTimeout();

  void FinishInvoke(mojom::ActionResultPtr result);

  content::RenderFrameHost* GetFrame() const;

  ToolCallback invoke_callback_;
  std::unique_ptr<PageToolRequest> request_;

  std::unique_ptr<RenderFrameChangeObserver> frame_change_observer_;
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame_;

  // Whether TimeOfUseValidation has completed. GetFrame can only be queried
  // after this has happened.
  bool has_completed_time_of_use_ = false;

  // Set during TimeOfUseValidation.
  content::WeakDocumentPtr target_document_;

  // Set during TimeOfUseValidation. Contains the hit test result against
  // observed page content.
  mojom::ObservedToolTargetPtr observed_target_;

  base::OneShotTimer timeout_timer_;

  base::WeakPtrFactory<PageTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_H_
