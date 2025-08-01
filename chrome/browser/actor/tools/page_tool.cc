// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_tool.h"

#include <variant>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/geometry/point.h"

namespace actor {

using ::content::GlobalRenderFrameHostId;
using ::content::RenderFrameHost;
using ::content::RenderWidgetHost;
using ::content::WebContents;
using ::content::WebContentsObserver;
using ::optimization_guide::DocumentIdentifierUserData;
using optimization_guide::TargetNodeInfo;
using optimization_guide::proto::AnnotatedPageContent;
using ::tabs::TabHandle;
using ::tabs::TabInterface;

namespace {

// Finds the local root of a given RenderFrameHost. The local root is the
// highest ancestor in the frame tree that shares the same RenderWidgetHost.
RenderFrameHost* GetLocalRoot(RenderFrameHost* rfh) {
  RenderFrameHost* local_root = rfh;
  while (local_root && local_root->GetParent()) {
    if (local_root->GetRenderWidgetHost() !=
        local_root->GetParent()->GetRenderWidgetHost()) {
      break;
    }
    local_root = local_root->GetParent();
  }
  return local_root;
}

RenderFrameHost* GetRenderFrameForDocumentIdentifier(
    content::WebContents& web_contents,
    std::string_view target_document_token) {
  RenderFrameHost* render_frame = nullptr;
  web_contents.ForEachRenderFrameHostWithAction([&target_document_token,
                                                 &render_frame](
                                                    RenderFrameHost* rfh) {
    // Skip inactive frame and its children.
    if (!rfh->IsActive()) {
      return RenderFrameHost::FrameIterationAction::kSkipChildren;
    }
    auto* user_data = DocumentIdentifierUserData::GetForCurrentDocument(rfh);
    if (user_data && user_data->serialized_token() == target_document_token) {
      render_frame = rfh;
      return RenderFrameHost::FrameIterationAction::kStop;
    }
    return RenderFrameHost::FrameIterationAction::kContinue;
  });
  return render_frame;
}

RenderFrameHost* GetRootFrameForWidget(content::WebContents& web_contents,
                                       RenderWidgetHost* rwh) {
  RenderFrameHost* root_frame = nullptr;
  web_contents.ForEachRenderFrameHostWithAction([rwh, &root_frame](
                                                    RenderFrameHost* rfh) {
    if (!rfh->IsActive()) {
      return RenderFrameHost::FrameIterationAction::kSkipChildren;
    }
    // A frame is a local root if it has no parent or if its parent belongs
    // to a different widget. We are looking for the local root frame
    // associated with the target widget.
    if (rfh->GetRenderWidgetHost() == rwh &&
        (!rfh->GetParent() || rfh->GetParent()->GetRenderWidgetHost() != rwh)) {
      root_frame = rfh;
      return RenderFrameHost::FrameIterationAction::kStop;
    }
    return RenderFrameHost::FrameIterationAction::kContinue;
  });
  return root_frame;
}

RenderFrameHost* FindTargetLocalRootFrame(TabHandle tab_handle,
                                          PageTarget target) {
  TabInterface* tab = tab_handle.Get();
  if (!tab) {
    return nullptr;
  }

  WebContents& contents = *tab->GetContents();

  if (std::holds_alternative<gfx::Point>(target)) {
    RenderWidgetHost* target_rwh =
        contents.FindWidgetAtPoint(gfx::PointF(std::get<gfx::Point>(target)));
    if (!target_rwh) {
      return nullptr;
    }
    return GetRootFrameForWidget(contents, target_rwh);
  }

  CHECK(std::holds_alternative<DomNode>(target));

  RenderFrameHost* target_frame = GetRenderFrameForDocumentIdentifier(
      *tab->GetContents(), std::get<DomNode>(target).document_identifier);

  // After finding the target frame, walk up to its local root.
  return GetLocalRoot(target_frame);
}

// Return TargetNodeInfo from hit test against last observed APC. Returns
// std::nullopt if Target does not hit any node.
std::optional<TargetNodeInfo> FindLastObservedNodeForActionTarget(
    const AnnotatedPageContent* apc,
    const PageTarget& target) {
  if (!apc) {
    return std::nullopt;
  }
  // TODO(rodneyding): Refactor FindNode* API to include optional target frame
  // document identifier to reduce search space.
  if (std::holds_alternative<gfx::Point>(target)) {
    return optimization_guide::FindNodeAtPoint(*apc,
                                               std::get<gfx::Point>(target));
  }
  CHECK(std::holds_alternative<DomNode>(target));
  std::optional<TargetNodeInfo> result = optimization_guide::FindNodeWithID(
      *apc, std::get<DomNode>(target).document_identifier,
      std::get<DomNode>(target).node_id);
  // If such a node isn't found or the node is found under a different
  // document it's an error.
  if (!result || result->document_identifier.serialized_token() !=
                     std::get<DomNode>(target).document_identifier) {
    return std::nullopt;
  }
  return result;
}

// Perform validation based on APC hit test for coordinate based target to
// compare the candidate frame with the target frame identified in last
// observation.
bool ValidateTargetFrameCandidate(
    const PageTarget& target,
    RenderFrameHost* candidate_frame,
    WebContents& web_contents,
    const std::optional<TargetNodeInfo> target_node_info) {
  // Frame validation is performed only when targeting using coordinates.
  CHECK(std::holds_alternative<gfx::Point>(target));

  if (!target_node_info) {
    return false;
  }

  RenderFrameHost* apc_target_frame = GetRenderFrameForDocumentIdentifier(
      web_contents, target_node_info->document_identifier.serialized_token());

  // Only return the candidate if its RenderWidgetHost matches the target
  // and it's also a local root frame(i.e. has no parent or parent has
  // a different RenderWidgetHost)
  if (apc_target_frame && apc_target_frame->GetRenderWidgetHost() ==
                              candidate_frame->GetRenderWidgetHost()) {
    return true;
  }
  return false;
}

// Helper function to create ObservedToolTarget mojom struct from
// TargetNodeInfo struct.
mojom::ObservedToolTargetPtr ToMojoObservedToolTarget(
    const std::optional<optimization_guide::TargetNodeInfo>&
        observed_target_node_info) {
  if (!observed_target_node_info) {
    return nullptr;
  }
  mojom::ObservedToolTargetPtr observed_target =
      mojom::ObservedToolTarget::New();
  observed_target->node_attribute =
      blink::mojom::AIPageContentAttributes::New();
  const auto& content_attributes =
      observed_target_node_info->node->content_attributes();
  if (content_attributes.has_common_ancestor_dom_node_id()) {
    observed_target->node_attribute->dom_node_id =
        content_attributes.common_ancestor_dom_node_id();
  }
  if (content_attributes.has_geometry()) {
    observed_target->node_attribute->geometry =
        blink::mojom::AIPageContentGeometry::New();
    observed_target->node_attribute->geometry->outer_bounding_box =
        gfx::Rect(content_attributes.geometry().outer_bounding_box().x(),
                  content_attributes.geometry().outer_bounding_box().y(),
                  content_attributes.geometry().outer_bounding_box().width(),
                  content_attributes.geometry().outer_bounding_box().height());
    observed_target->node_attribute->geometry->visible_bounding_box = gfx::Rect(
        content_attributes.geometry().visible_bounding_box().x(),
        content_attributes.geometry().visible_bounding_box().y(),
        content_attributes.geometry().visible_bounding_box().width(),
        content_attributes.geometry().visible_bounding_box().height());
    observed_target->node_attribute->geometry->is_fixed_or_sticky_position =
        content_attributes.geometry().is_fixed_or_sticky_position();
  }
  return observed_target;
}

}  // namespace

// Observer to track if the a given RenderFrameHost is changed.
class RenderFrameChangeObserver : public WebContentsObserver {
 public:
  RenderFrameChangeObserver(RenderFrameHost& rfh, base::OnceClosure callback)
      : WebContentsObserver(WebContents::FromRenderFrameHost(&rfh)),
        rfh_id_(rfh.GetGlobalId()),
        callback_(std::move(callback)) {}

  // WebContentsObserver
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* /*new_host*/) override {
    if (!callback_) {
      return;
    }

    if (old_host && old_host->GetGlobalId() == rfh_id_) {
      std::move(callback_).Run();
    }
  }

 private:
  GlobalRenderFrameHostId rfh_id_;
  base::OnceClosure callback_;
};

PageTool::PageTool(TaskId task_id,
                   ToolDelegate& tool_delegate,
                   const PageToolRequest& request)
    : Tool(task_id, tool_delegate), request_(request.Clone()) {}

PageTool::~PageTool() = default;

void PageTool::Validate(ValidateCallback callback) {
  // No browser-side validation yet.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), MakeOkResult()));
}

mojom::ActionResultPtr PageTool::TimeOfUseValidation(
    const AnnotatedPageContent* last_observation) {
  TabInterface* tab = request_->GetTabHandle().Get();
  if (!tab) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway);
  }

  journal().Log(JournalURL(), task_id(), mojom::JournalTrack::kActor,
                "TimeOfUseValidation",
                "TabHandle:" + base::ToString(tab->GetHandle()));

  RenderFrameHost* frame =
      FindTargetLocalRootFrame(request_->GetTabHandle(), request_->GetTarget());
  if (!frame) {
    return MakeResult(mojom::ActionResultCode::kFrameWentAway);
  }
  // TODO(crbug.com/426021822): FindNodeAtPoint does not handle corner cases
  // like clip paths. Need more checks to ensure we don't drop actions
  // unnecessarily.
  observed_target_node_info_ = FindLastObservedNodeForActionTarget(
      last_observation, request_->GetTarget());

  if (!observed_target_node_info_) {
    journal().Log(JournalURL(), task_id(), mojom::JournalTrack::kActor,
                  "TimeOfUseValidation", "No observed target found in APC.");
  }

  // Perform validation for coordinate based target only.
  // TODO(bokan): We can't perform a TOCTOU check If there's no last
  // observation. Consider what to do in this case.
  if (std::holds_alternative<gfx::Point>(request_->GetTarget()) &&
      last_observation) {
    if (!ValidateTargetFrameCandidate(request_->GetTarget(), frame,
                                      *tab->GetContents(),
                                      observed_target_node_info_)) {
      return MakeResult(
          mojom::ActionResultCode::kFrameLocationChangedSinceObservation);
    }
  }

  has_completed_time_of_use_ = true;
  target_document_ = frame->GetWeakDocumentPtr();

  return MakeOkResult();
}

void PageTool::Invoke(InvokeCallback callback) {
  // Frame was validated in TimeOfUseValidation.
  CHECK(GetFrame());
  RenderFrameHost& frame = *GetFrame();

  journal().EnsureJournalBound(frame);

  invoke_callback_ = std::move(callback);

  auto invocation = actor::mojom::ToolInvocation::New();
  invocation->action = request_->ToMojoToolAction();
  invocation->target = ToMojo(request_->GetTarget());
  invocation->observed_target =
      ToMojoObservedToolTarget(observed_target_node_info_);
  invocation->task_id = task_id().value();

  // ToolRequest params are checked for validity at creation.
  CHECK(invocation->action);

  frame.GetRemoteAssociatedInterfaces()->GetInterface(&chrome_render_frame_);

  // Watch for the RenderFrameHost being swapped out by a navigation (e.g. after
  // clicking on a link). In that case, finish the invocation successfully as
  // the ToolController will wait on the new page to load if needed. We rely on
  // this running before the RenderFrameHost is destroyed since otherwise the
  // chrome_render_frame_ mojo pipe will call the disconnect error handler which
  // finishes the invocation with an error. Finally, this also handles cases
  // where the old frame is put into the BFCache since in that case we may not
  // get a reply from the renderer at all.
  // Note: If there's already an in progress navigation then
  // frame_change_observer may call FinishInvoke as a result of that navigation
  // rather than the tool use. In that case we'll return success as if the tool
  // completed successfully (expecting that's fine, as a new observation will be
  // taken).
  // `this` Unretained because the observer is owned by this class and thus
  // removed on destruction.
  frame_change_observer_ = std::make_unique<RenderFrameChangeObserver>(
      frame, base::BindOnce(&PageTool::FinishInvoke, base::Unretained(this),
                            MakeOkResult()));

  // `this` Unretained because this class owns the mojo pipe that invokes the
  // callbacks.
  // TODO(crbug.com/423932492): It's not clear why but it appears that sometimes
  // the frame goes away before the RenderFrameChangeObserver fires. It should
  // be ok to assume this happens as a result of a navigation and treat the tool
  // invocation as successful but might be worth better understanding how this
  // can happen.
  chrome_render_frame_.set_disconnect_handler(base::BindOnce(
      &PageTool::FinishInvoke, base::Unretained(this), MakeOkResult()));
  chrome_render_frame_->InvokeTool(
      std::move(invocation),
      base::BindOnce(&PageTool::FinishInvoke, base::Unretained(this)));
}

std::string PageTool::DebugString() const {
  // TODO(crbug.com/402210051): Add more details here about tool params.
  return absl::StrFormat("PageTool:%s", JournalEvent().c_str());
}

GURL PageTool::JournalURL() const {
  if (has_completed_time_of_use_) {
    if (RenderFrameHost* frame = GetFrame()) {
      return frame->GetLastCommittedURL();
    } else {
      return GURL();
    }
  }
  return request_->GetURLForJournal();
}

std::string PageTool::JournalEvent() const {
  return request_->JournalEvent();
}

std::unique_ptr<ObservationDelayController> PageTool::GetObservationDelayer()
    const {
  CHECK(has_completed_time_of_use_);

  RenderFrameHost* frame = GetFrame();

  // It's the caller's responsibility to ensure a frame is still live if calling
  // this method.
  CHECK(frame);

  return std::make_unique<ObservationDelayController>(*frame);
}

void PageTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                      InvokeCallback callback) const {
  task.AddTab(request_->GetTabHandle(), std::move(callback));
}

void PageTool::FinishInvoke(mojom::ActionResultPtr result) {
  if (!invoke_callback_) {
    return;
  }

  frame_change_observer_.reset();

  std::move(invoke_callback_).Run(std::move(result));

  // WARNING: `this` may now be destroyed.
}

void PageTool::PostFinishInvoke(mojom::ActionResultCode result_code) {
  CHECK(invoke_callback_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PageTool::FinishInvoke, weak_ptr_factory_.GetWeakPtr(),
                     MakeResult(result_code)));
}

content::RenderFrameHost* PageTool::GetFrame() const {
  CHECK(has_completed_time_of_use_);
  return target_document_.AsRenderFrameHostIfValid();
}

}  // namespace actor
