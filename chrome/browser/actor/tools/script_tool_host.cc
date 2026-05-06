// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/script_tool_host.h"

#include <set>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace actor {

namespace {

// The maximum amount of time a tool can take to execute, before it is assumed
// to have failed.
base::TimeDelta GetToolExecutionTimeout() {
  return kActorScriptToolExecutionTimeout.Get();
}

// The maximum amount of time to wait for a script tool to extract and return
// its result from a newly committed cross-document navigation. This timeout
// starts upon navigation commit, which happens when the first headers are
// received. This timeout covers downloading and parsing the main document HTML
// and firing DOMContentLoaded.
base::TimeDelta GetCrossDocumentResultTimeout() {
  return kActorScriptToolCrossDocumentTimeout.Get();
}

}  // namespace

void ScriptToolHost::OnToolTimeout() {
  PostErrorResult(std::move(tool_done_callback_),
                  mojom::ActionResultCode::kScriptToolNoResponse,
                  "Script tool execution timed out");
}

void ScriptToolHost::OnCrossDocumentResultTimeout() {
  PostErrorResult(std::move(tool_done_callback_),
                  mojom::ActionResultCode::kScriptToolNoResponse,
                  "Script tool cross-document result timed out");
}

ScriptToolHost::ScriptToolHost(TaskId task_id,
                               ToolDelegate& tool_delegate,
                               tabs::TabHandle target_tab,
                               const base::UnguessableToken& target_document_id,
                               mojom::ToolActionPtr action)
    : Tool(task_id, tool_delegate),
      target_tab_(target_tab),
      target_document_id_(target_document_id),
      action_(std::move(action)) {}

ScriptToolHost::~ScriptToolHost() {
  TearDown();
}

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

  target_frame_tree_node_id_ =
      tab->GetContents()->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  target_document_ =
      tab->GetContents()->GetPrimaryMainFrame()->GetWeakDocumentPtr();
  return MakeOkResult();
}

std::unique_ptr<ObservationDelayController>
ScriptToolHost::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  if (!base::FeatureList::IsEnabled(kActorScriptToolDelayObservation)) {
    return nullptr;
  }

  content::RenderFrameHost* frame = new_document_.AsRenderFrameHostIfValid();
  if (!frame) {
    frame = target_document_.AsRenderFrameHostIfValid();
  }

  if (!frame) {
    return nullptr;
  }

  return std::make_unique<ObservationDelayController>(
      *frame, task_id(), journal(), page_stability_config);
}

void ScriptToolHost::Invoke(ToolCallback callback) {
  InitializePendingResult();
  auto* frame = target_document_.AsRenderFrameHostIfValid();
  CHECK(frame);

  journal().EnsureJournalBound(*frame);

  tool_done_callback_ = std::move(callback);

  const auto& script_tool = action_->get_script_tool();
  RecordScriptToolInputSizeBytes(script_tool->input_arguments.size());

  auto invocation = actor::mojom::ToolInvocation::New();
  invocation->action = action_->Clone();
  invocation->task_id = task_id();
  execution_id_ = base::UnguessableToken::Create();
  invocation->execution_id = execution_id_;
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
  if (target_tab_.Get()) {
    tab_will_detach_subscription_ = target_tab_.Get()->RegisterWillDetach(
        base::BindRepeating(&ScriptToolHost::OnTabWillBeRemoved,
                            weak_ptr_factory_.GetWeakPtr()));
  }
  timeout_timer_.Start(FROM_HERE, GetToolExecutionTimeout(),
                       base::BindOnce(&ScriptToolHost::OnToolTimeout,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void ScriptToolHost::NotifyPaused() {
  // If a declarative form tool needs user action to confirm submission, then
  // don't timeout the tool invocation.
  // TODO(crbug.com/508075344): Restart the timer after the user submits.
  if (timeout_timer_.IsRunning()) {
    timeout_timer_.Stop();
  }
  if (cross_document_timeout_timer_.IsRunning()) {
    cross_document_timeout_timer_.Stop();
  }
}

void ScriptToolHost::Cancel() {
  switch (lifecycle_) {
    case Lifecycle::kInitial:
    case Lifecycle::kDone:
      break;
    case Lifecycle::kInvokeSent:
    case Lifecycle::kWaitingForNavigation:
    case Lifecycle::kPendingResultFromNewDocument:
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
  task.AddTab(target_tab_, /*stop_task_on_detach=*/true, std::move(callback));
}

tabs::TabHandle ScriptToolHost::GetTargetTab() const {
  return target_tab_;
}

void ScriptToolHost::OnToolInvokedInOldDocument(mojom::ActionResultPtr result) {
  if (lifecycle_ == Lifecycle::kInitial) {
    mojo::ReportBadMessage(
        "ScriptToolHost: callback invoked in kInitial state");
    return;
  }

  // It's possible for this callback to arrive right as a navigation starts,
  // transitioning the lifecycle before this is processed.
  if (lifecycle_ == Lifecycle::kPendingResultFromNewDocument ||
      lifecycle_ == Lifecycle::kDone) {
    return;
  }

  CHECK(web_contents());

  if (result && result->code == mojom::ActionResultCode::kOk) {
    result->requires_page_stabilization =
        base::FeatureList::IsEnabled(kActorScriptToolDelayObservation);
  }

  const bool has_tool_response = result && result->script_tool_response;
  if (has_tool_response && result->script_tool_response->tool) {
    pending_result_->script_tool_response->tool =
        result->script_tool_response->tool.Clone();
  }
  if (has_tool_response && !result->script_tool_response->result) {
    lifecycle_ = Lifecycle::kWaitingForNavigation;
    return;
  }

  lifecycle_ = Lifecycle::kDone;
  TearDown();
  RecordMetrics(*result);
  std::move(tool_done_callback_).Run(std::move(result));
}

void ScriptToolHost::OnResultReceivedFromNewDocument(
    const std::string& result) {
  CHECK_EQ(lifecycle_, Lifecycle::kPendingResultFromNewDocument);

  lifecycle_ = Lifecycle::kDone;
  SetScriptToolOutput(result);
  TearDown();
  RecordMetrics(*pending_result_);
  std::move(tool_done_callback_).Run(std::move(pending_result_));
}

void ScriptToolHost::RecordMetrics(const mojom::ActionResult& result) {
  RecordScriptToolActionResultCode(result.code);
  if (result.code == mojom::ActionResultCode::kOk) {
    RecordScriptToolOutputSizeBytes(
        result.script_tool_response->result->size());
  }
}

void ScriptToolHost::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // We early return in these cases:
  // - The tool is inactive (execution hasn't started or is already complete).
  // - The navigation is a same-document change (e.g., a URL fragment update).
  // - The navigation is occurring in a different frame than the target frame.
  if (lifecycle_ == Lifecycle::kInitial || lifecycle_ == Lifecycle::kDone ||
      navigation_handle->IsSameDocument() ||
      navigation_handle->GetFrameTreeNodeId() != target_frame_tree_node_id_) {
    return;
  }

  if (navigation_handle->GetScriptToolInvocationId() == execution_id_) {
    active_navigation_id_ = navigation_handle->GetNavigationId();
    if (lifecycle_ == Lifecycle::kInvokeSent) {
      lifecycle_ = Lifecycle::kWaitingForNavigation;
    }
  } else {
    if (lifecycle_ == Lifecycle::kInvokeSent ||
        lifecycle_ == Lifecycle::kWaitingForNavigation ||
        lifecycle_ == Lifecycle::kPendingResultFromNewDocument) {
      PostErrorResult(std::move(tool_done_callback_),
                      mojom::ActionResultCode::kScriptToolCancelled,
                      "Tool navigation replaced by unrelated navigation");
    }
  }
}

void ScriptToolHost::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (lifecycle_ != Lifecycle::kWaitingForNavigation) {
    return;
  }

  // Irrelevant navigations are already handled in DidStartNavigation().
  if (navigation_handle->GetNavigationId() != active_navigation_id_) {
    return;
  }

  // If we have invoked a script tool on the target document but failed to
  // receive a result, we are expecting to receive it after a successful
  // navigation to a new document.  Return an error from the tool if that
  // navigation failed to commit.
  if (!navigation_handle->HasCommitted()) {
    PostErrorResult(std::move(tool_done_callback_),
                    mojom::ActionResultCode::kScriptToolNavigationDidNotCommit,
                    base::StrCat({"Navigation failed: ",
                                  navigation_handle->GetURL().spec()}));
    return;
  }

  if (navigation_handle->IsErrorPage()) {
    PostErrorResult(
        std::move(tool_done_callback_),
        mojom::ActionResultCode::kScriptToolNavigationCommittedErrorPage,
        base::StrCat({"Navigation committed error page: ",
                      navigation_handle->GetURL().spec()}));
    return;
  }

  CHECK_EQ(navigation_handle->GetFrameTreeNodeId(), target_frame_tree_node_id_);

  auto* new_host = navigation_handle->GetRenderFrameHost();
  if (!new_host->GetLastCommittedOrigin().IsSameOriginWith(
          target_document_origin_)) {
    PostErrorResult(std::move(tool_done_callback_),
                    mojom::ActionResultCode::kScriptToolCrossOriginNavigation,
                    base::StrCat({"Cross-origin navigation to: ",
                                  new_host->GetLastCommittedURL().spec()}));
    return;
  }

  lifecycle_ = Lifecycle::kPendingResultFromNewDocument;

  new_document_ = new_host->GetWeakDocumentPtr();
  new_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &new_document_render_frame_);
  new_document_render_frame_->GetCrossDocumentScriptToolResult(
      execution_id_,
      base::BindOnce(&ScriptToolHost::OnResultReceivedFromNewDocument,
                     weak_ptr_factory_.GetWeakPtr()));

  cross_document_timeout_timer_.Start(
      FROM_HERE, GetCrossDocumentResultTimeout(),
      base::BindOnce(&ScriptToolHost::OnCrossDocumentResultTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScriptToolHost::DidFailLoad(content::RenderFrameHost* render_frame_host,
                                 const GURL& validated_url,
                                 int error_code) {
  // We care about loading when we are expecting a script tool result after
  // navigation to a new document.
  if (lifecycle_ != Lifecycle::kPendingResultFromNewDocument) {
    return;
  }

  if (render_frame_host == new_document_.AsRenderFrameHostIfValid()) {
    PostErrorResult(
        std::move(tool_done_callback_),
        mojom::ActionResultCode::kScriptToolNavigationFailedLoad,
        base::StrCat({"Navigation failed load: ", validated_url.spec()}));
  }
}

void ScriptToolHost::OnTabWillBeRemoved(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  PostErrorResult(std::move(tool_done_callback_),
                  mojom::ActionResultCode::kTabWentAway, "Tab was closed");
}

void ScriptToolHost::RenderFrameDeleted(content::RenderFrameHost* rfh) {
  bool terminate_with_error = false;
  switch (lifecycle_) {
    case Lifecycle::kInitial:
    case Lifecycle::kDone:
      NOTREACHED();
    case Lifecycle::kInvokeSent:
      terminate_with_error =
          (rfh == target_document_.AsRenderFrameHostIfValid());
      break;
    case Lifecycle::kWaitingForNavigation:
    case Lifecycle::kPendingResultFromNewDocument:
      if (rfh == new_document_.AsRenderFrameHostIfValid()) {
        terminate_with_error = true;
      }
      break;
  }

  if (terminate_with_error) {
    PostErrorResult(std::move(tool_done_callback_),
                    MaybeGetErrorCodeForTab(target_tab_.Get())
                        .value_or(mojom::ActionResultCode::kFrameWentAway));
  }
}

void ScriptToolHost::PostErrorResult(ToolCallback tool_callback,
                                     mojom::ActionResultCode code,
                                     const std::string& message) {
  lifecycle_ = Lifecycle::kDone;
  TearDown();
  auto result =
      MakeResult(code, /*requires_page_stabilization=*/false, message);
  RecordMetrics(*result);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(tool_callback), std::move(result)));
}

void ScriptToolHost::TearDown() {
  timeout_timer_.Stop();
  cross_document_timeout_timer_.Stop();
  Observe(nullptr);
  tab_will_detach_subscription_ = {};
  target_document_render_frame_.reset();
  new_document_render_frame_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ScriptToolHost::InitializePendingResult() {
  CHECK(!pending_result_);
  pending_result_ = mojom::ActionResult::New();
  pending_result_->code = mojom::ActionResultCode::kOk;
  pending_result_->requires_page_stabilization =
      base::FeatureList::IsEnabled(kActorScriptToolDelayObservation);
  pending_result_->script_tool_response = mojom::ScriptToolResponse::New();
  pending_result_->script_tool_response->input_arguments =
      action_->get_script_tool()->input_arguments;
  pending_result_->script_tool_response->tool = blink::mojom::ScriptTool::New();
  pending_result_->script_tool_response->tool->name =
      action_->get_script_tool()->name;
}

void ScriptToolHost::SetScriptToolOutput(const std::string& output) {
  CHECK(pending_result_);
  CHECK(pending_result_->script_tool_response);
  CHECK(!pending_result_->script_tool_response->result.has_value());
  // TODO(crbug.com/501491692): We should figure out how to pass script tool
  // inputs/outputs as movable containers to avoid unnecessary in-process
  // copies.
  pending_result_->script_tool_response->result = output;
}

}  // namespace actor
