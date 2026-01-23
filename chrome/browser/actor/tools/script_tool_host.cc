// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/script_tool_host.h"

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace actor {

ScriptToolHost::ScriptToolHost(TaskId task_id,
                               ToolDelegate& tool_delegate,
                               tabs::TabHandle target_tab,
                               const base::UnguessableToken& target_document_id,
                               mojom::ToolActionPtr action)
    : Tool(task_id, tool_delegate),
      target_tab_(target_tab),
      target_document_id_(target_document_id),
      action_(std::move(action)) {}

ScriptToolHost::~ScriptToolHost() = default;

void ScriptToolHost::Validate(ToolCallback callback) {
  // No browser-side validation yet.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), MakeOkResult()));
}

mojom::ActionResultPtr ScriptToolHost::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  tabs::TabInterface* tab = target_tab_.Get();
  if (!tab || !tab->GetContents()) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway);
  }

  // Check that the target Document is associated with the target tab. Only
  // main frames are supported.
  // TODO(khushalsagar): Add support for subframes.
  auto primary_document_id = optimization_guide::DocumentIdentifierUserData::
                                 GetOrCreateForCurrentDocument(
                                     tab->GetContents()->GetPrimaryMainFrame())
                                     ->token();
  if (primary_document_id != target_document_id_) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway);
  }

  target_document_ =
      tab->GetContents()->GetPrimaryMainFrame()->GetWeakDocumentPtr();
  return MakeOkResult();
}

std::unique_ptr<ObservationDelayController>
ScriptToolHost::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return nullptr;
}

void ScriptToolHost::Invoke(ToolCallback callback) {
  auto* frame = target_document_.AsRenderFrameHostIfValid();
  CHECK(frame);

  journal().EnsureJournalBound(*frame);

  tool_done_callback_ = std::move(callback);

  auto invocation = actor::mojom::ToolInvocation::New();
  invocation->action = action_->Clone();
  invocation->task_id = task_id();
  invocation->target =
      actor::mojom::ToolTarget::NewDomNodeId(kRootElementDomNodeId);

  frame->GetRemoteAssociatedInterfaces()->GetInterface(
      &target_document_render_frame_);

  lifecycle_ = Lifecycle::kInvokeSent;
  target_document_render_frame_->InvokeTool(
      std::move(invocation),
      base::BindOnce(&ScriptToolHost::OnToolInvokedInOldDocument,
                     weak_ptr_factory_.GetWeakPtr()));
  Observe(target_tab_.Get()->GetContents());

  // TODO(khushalsagar): We likely need a timeout here if script hangs
  // indefinitely but will need to reconcile this with the user interaction
  // flow.
}

void ScriptToolHost::Cancel() {
  switch (lifecycle_) {
    case Lifecycle::kInitial:
    case Lifecycle::kDone:
      break;
    case Lifecycle::kInvokeSent:
      CHECK(target_document_render_frame_);
      journal().Log(
          JournalURL(), task_id(), "ScriptToolHost::Cancel",
          JournalDetailsBuilder().Add("tab_handle", target_tab_).Build());

      target_document_render_frame_->CancelTool(task_id());
      PostErrorResult(std::move(tool_done_callback_),
                      mojom::ActionResultCode::kInvokeCanceled);
      break;
  }
}

std::string ScriptToolHost::DebugString() const {
  const auto& script_tool = action_->get_script_tool();
  return absl::StrFormat("ScriptToolHost: name:%s\n input:%s",
                         script_tool->name, script_tool->input_arguments);
}

GURL ScriptToolHost::JournalURL() const {
  // TODO(khushalsagar): Use the new document's URL for the navigation case?
  auto* frame = target_document_.AsRenderFrameHostIfValid();
  return frame ? frame->GetLastCommittedURL() : GURL();
}

std::string ScriptToolHost::JournalEvent() const {
  return DebugString();
}

void ScriptToolHost::UpdateTaskBeforeInvoke(ActorTask& task,
                                            ToolCallback callback) const {
  task.AddTab(target_tab_, std::move(callback));
}

tabs::TabHandle ScriptToolHost::GetTargetTab() const {
  return target_tab_;
}

void ScriptToolHost::OnToolInvokedInOldDocument(mojom::ActionResultPtr result) {
  TearDown();

  const bool result_on_new_document = result && result->script_tool_response &&
                                      !result->script_tool_response->result;
  if (!result_on_new_document) {
    lifecycle_ = Lifecycle::kDone;
    std::move(tool_done_callback_).Run(std::move(result));
    return;
  }

  // TODO(khushalsagar): Add support for cross-document result tracking.
  lifecycle_ = Lifecycle::kDone;
  std::move(tool_done_callback_).Run(std::move(result));
}

void ScriptToolHost::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* /*new_host*/) {
  switch (lifecycle_) {
    case Lifecycle::kInitial:
    case Lifecycle::kDone:
      NOTREACHED();
    case Lifecycle::kInvokeSent:
      // If the old_host is destroyed before we get a result from the renderer,
      // we have to process this as a failure since we can't provide the
      // invocation result.
      if (old_host == target_document_.AsRenderFrameHostIfValid()) {
        PostErrorResult(std::move(tool_done_callback_),
                        mojom::ActionResultCode::kFrameWentAway);
      }
      break;
  }
}

void ScriptToolHost::RenderFrameDeleted(content::RenderFrameHost* rfh) {
  switch (lifecycle_) {
    case Lifecycle::kInitial:
    case Lifecycle::kDone:
      NOTREACHED();
    case Lifecycle::kInvokeSent:
      if (rfh == target_document_.AsRenderFrameHostIfValid()) {
        PostErrorResult(std::move(tool_done_callback_),
                        MaybeGetErrorCodeForTab(target_tab_.Get())
                            .value_or(mojom::ActionResultCode::kFrameWentAway));
      }
      break;
  }
}

void ScriptToolHost::PostErrorResult(ToolCallback tool_callback,
                                     mojom::ActionResultCode code) {
  lifecycle_ = Lifecycle::kDone;
  TearDown();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(tool_callback), MakeResult(code)));
}

void ScriptToolHost::TearDown() {
  Observe(nullptr);
}

}  // namespace actor
