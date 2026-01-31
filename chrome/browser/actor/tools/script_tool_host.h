// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SCRIPT_TOOL_HOST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SCRIPT_TOOL_HOST_H_

#include <memory>

#include "chrome/browser/actor/tools/tool.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace actor {

// This class is used to manage execution of WebMCP/Script tools in the
// renderer.
class ScriptToolHost : public Tool, content::WebContentsObserver {
 public:
  ScriptToolHost(TaskId task_id,
                 ToolDelegate& tool_delegate,
                 tabs::TabHandle target_tab,
                 const base::UnguessableToken& target_document_id,
                 mojom::ToolActionPtr action);
  ~ScriptToolHost() override;

  // actor::Tool
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
  tabs::TabHandle GetTargetTab() const override;

 private:
  enum class Lifecycle {
    kInitial,
    kInvokeSent,
    kWaitingForNavigation,
    kPendingResultFromNewDocment,
    kDone
  };

  // WebContentsObserver implementation.
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* /*new_host*/) override;
  void RenderFrameDeleted(content::RenderFrameHost* rfh) override;
  void PrimaryPageChanged(content::Page& page) override;

  void TearDown();
  void OnToolInvokedInOldDocument(mojom::ActionResultPtr result);
  void OnResultReceivedFromNewDocument(const std::string& result);
  void PostErrorResult(ToolCallback tool_callback,
                       mojom::ActionResultCode code);

  Lifecycle lifecycle_{Lifecycle::kInitial};
  tabs::TabHandle target_tab_;
  const base::UnguessableToken target_document_id_;
  const mojom::ToolActionPtr action_;
  ToolCallback tool_done_callback_;

  // A reference to the target Document for the tool execution.
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
      target_document_render_frame_;
  content::WeakDocumentPtr target_document_;
  url::Origin target_document_origin_;

  // The result provided by the old Document. This is set when we have received
  // the response from the old Document. It's populated with the result pulled
  // from the new Document before the tool invocation is finished.
  mojom::ActionResultPtr pending_result_;

  // A reference to the new Document if this tool resulted in a cross-document
  // navigation and the result will be provided on the new Document.
  // After we have received a response from the old Document, we wait for the
  // first cross-document navigation to commit in the browser. These fields are
  // set to that Document if it's same-origin with the old Document.
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
      new_document_render_frame_;
  content::WeakDocumentPtr new_document_;

  base::WeakPtrFactory<ScriptToolHost> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SCRIPT_TOOL_HOST_H_
