// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/execute_js_tool_host.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal_render_frame_binder.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace actor {

namespace {

// How much longer than the script's timeout the browser waits, so that the
// renderer can report its own, more specific, timeout first.
constexpr base::TimeDelta kRendererTimeoutSlack = base::Seconds(1);

}  // namespace

ExecuteJsToolHost::ExecuteJsToolHost(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    ActorSurfaceHandle target_actor_surface_handle,
    const base::UnguessableToken& target_document_id,
    mojom::ToolActionPtr action)
    : Tool(task_id, tool_delegate),
      actor_surface_handle_(target_actor_surface_handle),
      target_document_id_(target_document_id),
      action_(std::move(action)) {}

ExecuteJsToolHost::~ExecuteJsToolHost() {
  TearDown();
}

void ExecuteJsToolHost::TearDown() {
  timeout_timer_.Stop();
  Observe(nullptr);
  tab_will_detach_subscription_ = {};
  target_document_render_frame_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExecuteJsToolHost::Validate(ToolCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     MakeOkResult(/*requires_page_stabilization=*/false)));
}

mojom::ActionResultPtr ExecuteJsToolHost::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (!actor_surface) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway);
  }

  content::RenderFrameHost* target_frame =
      optimization_guide::GetRenderFrameForDocumentIdentifier(
          *actor_surface->GetWebContents(), target_document_id_.ToString());
  if (!target_frame) {
    return MakeResult(mojom::ActionResultCode::kFrameWentAway,
                      /*requires_page_stabilization=*/false,
                      "The target document is no longer in the tab.");
  }
  if (!target_frame->IsInPrimaryMainFrame()) {
    return MakeResult(mojom::ActionResultCode::kExecuteJsTargetNotMainFrame,
                      /*requires_page_stabilization=*/false,
                      "ExecuteJs only runs in the main frame's document.");
  }

  target_frame_tree_node_id_ = target_frame->GetFrameTreeNodeId();
  target_document_ = target_frame->GetWeakDocumentPtr();
  return MakeOkResult(/*requires_page_stabilization=*/false);
}

std::unique_ptr<ObservationDelayController>
ExecuteJsToolHost::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return nullptr;
}

void ExecuteJsToolHost::Invoke(ToolCallback callback) {
  auto* frame = target_document_.AsRenderFrameHostIfValid();
  if (!frame) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       MakeResult(mojom::ActionResultCode::kFrameWentAway)));
    return;
  }

  AggregatedJournalRenderFrameBinder::EnsureBound(journal(), *frame);
  tool_done_callback_ = std::move(callback);

  const auto& execute_js_action = action_->get_execute_js();

  auto invocation = actor::mojom::ToolInvocation::New();
  invocation->action = action_->Clone();
  invocation->task_id = task_id();
  invocation->target =
      actor::mojom::ToolTarget::NewDomNodeId(kRootElementDomNodeId);

  frame->GetRemoteAssociatedInterfaces()->GetInterface(
      &target_document_render_frame_);

  lifecycle_ = Lifecycle::kInvokeSent;
  target_document_render_frame_->InvokeTool(
      std::move(invocation), base::BindOnce(&ExecuteJsToolHost::OnToolInvoked,
                                            weak_ptr_factory_.GetWeakPtr()));

  ActorSurface* actor_surface = actor_surface_handle_.Get();
  CHECK(actor_surface);
  Observe(actor_surface->GetWebContents());
  if (tabs::TabInterface* tab = actor_surface_handle_.GetTabHandle().Get()) {
    tab_will_detach_subscription_ = tab->RegisterWillDetach(
        base::BindRepeating(&ExecuteJsToolHost::OnTabWillBeRemoved,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // Mirror the renderer's handling of the timeout.
  base::TimeDelta timeout =
      execute_js_action->timeout_ms
          ? base::Milliseconds(execute_js_action->timeout_ms)
          : kExecuteJsDefaultTimeout;
  timeout = std::min(timeout, kExecuteJsMaxTimeout);
  timeout_timer_.Start(FROM_HERE, timeout + kRendererTimeoutSlack,
                       base::BindOnce(&ExecuteJsToolHost::OnToolTimeout,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void ExecuteJsToolHost::Cancel() {
  if (lifecycle_ == Lifecycle::kInvokeSent) {
    if (target_document_render_frame_) {
      target_document_render_frame_->CancelTool(task_id());
    }
    PostErrorResult(std::move(tool_done_callback_),
                    mojom::ActionResultCode::kInvokeCanceled);
  }
}

std::string ExecuteJsToolHost::DebugString() const {
  return absl::StrFormat(
      "ExecuteJsToolHost: script_code:%s",
      base::TruncateUTF8ToByteSize(action_->get_execute_js()->script_code,
                                   kExecuteJsMaxLoggedScriptBytes));
}

GURL ExecuteJsToolHost::JournalURL() const {
  auto* frame = target_document_.AsRenderFrameHostIfValid();
  return frame ? frame->GetLastCommittedURL() : GURL();
}

std::string ExecuteJsToolHost::JournalEvent() const {
  return DebugString();
}

void ExecuteJsToolHost::UpdateTaskBeforeInvoke(ActorTask& task,
                                               ToolCallback callback) const {
  task.AddActorSurface(actor_surface_handle_, /*stop_task_on_detach=*/true,
                       std::move(callback));
}

ActorSurfaceHandle ExecuteJsToolHost::GetTargetActorSurface() const {
  return actor_surface_handle_;
}

void ExecuteJsToolHost::OnToolInvoked(mojom::ActionResultPtr result) {
  if (lifecycle_ == Lifecycle::kDone) {
    return;
  }

  lifecycle_ = Lifecycle::kDone;
  auto callback = std::move(tool_done_callback_);
  TearDown();

  // The renderer already truncates long results, but don't rely on it.
  if (mojom::ExecuteJsResponse* response = result->execute_js_response.get();
      response && response->result.size() > kExecuteJsMaxResultBytes) {
    response->result = std::string(base::TruncateUTF8ToByteSize(
        response->result, kExecuteJsMaxResultBytes));
    response->truncated = true;
  }

  if (callback) {
    std::move(callback).Run(std::move(result));
  }
}

void ExecuteJsToolHost::PostErrorResult(ToolCallback tool_callback,
                                        mojom::ActionResultCode code,
                                        const std::string& message) {
  lifecycle_ = Lifecycle::kDone;
  TearDown();
  if (tool_callback) {
    std::move(tool_callback)
        .Run(MakeResult(code, /*requires_page_stabilization=*/false, message));
  }
}

void ExecuteJsToolHost::OnToolTimeout() {
  if (target_document_render_frame_) {
    target_document_render_frame_->CancelTool(task_id());
  }
  PostErrorResult(std::move(tool_done_callback_),
                  mojom::ActionResultCode::kExecuteJsTimeout,
                  "The renderer did not finish the script in time.");
}

void ExecuteJsToolHost::RenderFrameDeleted(content::RenderFrameHost* rfh) {
  if (lifecycle_ == Lifecycle::kInvokeSent &&
      rfh == target_document_.AsRenderFrameHostIfValid()) {
    PostErrorResult(std::move(tool_done_callback_),
                    mojom::ActionResultCode::kFrameWentAway,
                    "Target frame was deleted.");
  }
}

void ExecuteJsToolHost::OnTabWillBeRemoved(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (lifecycle_ == Lifecycle::kInvokeSent) {
    PostErrorResult(std::move(tool_done_callback_),
                    mojom::ActionResultCode::kTabWentAway,
                    "Tab was removed during script execution.");
  }
}

void ExecuteJsToolHost::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (lifecycle_ != Lifecycle::kInvokeSent) {
    return;
  }

  if (navigation_handle->GetFrameTreeNodeId() == target_frame_tree_node_id_ &&
      !navigation_handle->IsSameDocument()) {
    // The RenderFrame, and the tool in it, can outlive the document, so stop
    // the tool rather than let it wait for a promise until its timeout.
    if (target_document_render_frame_) {
      target_document_render_frame_->CancelTool(task_id());
    }
    PostErrorResult(std::move(tool_done_callback_),
                    mojom::ActionResultCode::kExecuteJsNavigated,
                    "The target frame navigated while the script was running.");
  }
}

}  // namespace actor
