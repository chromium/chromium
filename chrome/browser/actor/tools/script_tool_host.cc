// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/script_tool_host.h"

#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/page.h"
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
  target_document_origin_ = frame->GetLastCommittedOrigin();

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
    case Lifecycle::kWaitingForNavigation:
    case Lifecycle::kPendingResultFromNewDocment:
      journal().Log(
          JournalURL(), task_id(), "ScriptToolHost::Cancel",
          JournalDetailsBuilder().Add("tab_handle", target_tab_).Build());

      if (target_document_render_frame_) {
        target_document_render_frame_->CancelTool(task_id());
      }

      // TODO(khushalsagar): Should we cancel the ongoing navigation here? See
      // crbug.com/478276089.
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
  if (result_on_new_document) {
    auto* contents = target_tab_.Get()->GetContents();
    CHECK(contents);

    lifecycle_ = Lifecycle::kWaitingForNavigation;
    pending_result_ = std::move(result);
    Observe(contents);
    return;
  }

  lifecycle_ = Lifecycle::kDone;
  std::move(tool_done_callback_).Run(std::move(result));
}

void ScriptToolHost::OnResultReceivedFromNewDocument(
    const std::string& result) {
  CHECK_EQ(lifecycle_, Lifecycle::kPendingResultFromNewDocment);
  CHECK(pending_result_);

  lifecycle_ = Lifecycle::kDone;
  pending_result_->script_tool_response->result = result;
  std::move(tool_done_callback_).Run(std::move(pending_result_));
}

void ScriptToolHost::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  switch (lifecycle_) {
    case Lifecycle::kInitial:
    case Lifecycle::kDone:
      NOTREACHED();
    case Lifecycle::kInvokeSent:
      // If the old_host is destroyed before we get a result from the renderer,
      // we have to process this as a failure since we can't provide the
      // invocation result.
      if (old_host && old_host == target_document_.AsRenderFrameHostIfValid()) {
        PostErrorResult(std::move(tool_done_callback_),
                        mojom::ActionResultCode::kFrameWentAway);
      }
      break;
    case Lifecycle::kPendingResultFromNewDocment:
      if (old_host && old_host == new_document_.AsRenderFrameHostIfValid()) {
        PostErrorResult(std::move(tool_done_callback_),
                        mojom::ActionResultCode::kFrameWentAway);
      }
      break;
    case Lifecycle::kWaitingForNavigation:
      // RFH swap is too early and doesn't provide the committed origin. We use
      // PrimaryPageChanged which is dispatched after the committed origin is
      // available.
      break;
  }
}

void ScriptToolHost::PrimaryPageChanged(content::Page& page) {
  if (lifecycle_ != Lifecycle::kWaitingForNavigation) {
    return;
  }

  auto& new_host = page.GetMainDocument();
  if (!new_host.GetLastCommittedOrigin().IsSameOriginWith(
          target_document_origin_)) {
    // If we end with a cross-origin navigation, assume execution
    // failure.
    PostErrorResult(std::move(tool_done_callback_),
                    mojom::ActionResultCode::kScriptToolCrossOriginNavigation);
    return;
  }
  // The new navigation has committed. Send a request to the renderer to
  // pull the result.
  lifecycle_ = Lifecycle::kPendingResultFromNewDocment;
  new_document_ = new_host.GetWeakDocumentPtr();
  new_host.GetRemoteAssociatedInterfaces()->GetInterface(
      &new_document_render_frame_);
  new_document_render_frame_->GetCrossDocumentScriptToolResult(
      base::BindOnce(&ScriptToolHost::OnResultReceivedFromNewDocument,
                     weak_ptr_factory_.GetWeakPtr()));

  // TODO(khushalsagar): We need to address the case where this navigation never
  // commits in which case PrimaryPageChanged won't be dispatched. See
  // crbug.com/478063859.
}

void ScriptToolHost::RenderFrameDeleted(content::RenderFrameHost* rfh) {
  bool terminate_with_error = false;
  switch (lifecycle_) {
    case Lifecycle::kInitial:
    case Lifecycle::kDone:
      NOTREACHED();
    case Lifecycle::kInvokeSent:
    case Lifecycle::kWaitingForNavigation:
      // Note: If a new navigation is committed, OnRenderFrameHostChanged will
      // be dispatched before RenderFrameDeleted. If we're receiving the
      // RenderFrameDeleted notification in this state, it's safe to assume
      // there was an error/crash in the old frame or the tab was closed.
      terminate_with_error =
          (rfh == target_document_.AsRenderFrameHostIfValid());
      break;
    case Lifecycle::kPendingResultFromNewDocment:
      terminate_with_error = (rfh == new_document_.AsRenderFrameHostIfValid());
      break;
  }

  if (terminate_with_error) {
    PostErrorResult(std::move(tool_done_callback_),
                    MaybeGetErrorCodeForTab(target_tab_.Get())
                        .value_or(mojom::ActionResultCode::kFrameWentAway));
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
  target_document_render_frame_.reset();
  new_document_render_frame_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace actor
