// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_tool.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
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
using ::optimization_guide::proto::AnnotatedPageContent;
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
                                          PageToolRequest::Target target) {
  TabInterface* tab = tab_handle.Get();
  if (!tab) {
    return nullptr;
  }

  WebContents& contents = *tab->GetContents();

  if (target.is_coordinate()) {
    RenderWidgetHost* target_rwh =
        contents.FindWidgetAtPoint(gfx::PointF(target.coordinate()));
    if (!target_rwh) {
      return nullptr;
    }
    return GetRootFrameForWidget(contents, target_rwh);
  }

  CHECK(target.is_node());

  RenderFrameHost* target_frame = GetRenderFrameForDocumentIdentifier(
      *tab->GetContents(), target.node().document_identifier);

  // After finding the target frame, walk up to its local root.
  return GetLocalRoot(target_frame);
}

// Perform validation based on APC and document identifier for coordinate based
// target to compare the candidate frame with the target frame identified in
// last observation.
bool ValidateTargetFrameCandidate(
    const PageToolRequest::Target& target,
    RenderFrameHost* candidate_frame,
    WebContents& web_contents,
    const AnnotatedPageContent* last_observed_page_content) {
  // Frame validation is performed only when targeting using coordinates.
  CHECK(target.is_coordinate());

  if (!last_observed_page_content) {
    // TODO(bokan): We can't perform a TOCTOU check If there's no last
    // observation. Consider what to do in this case.
    return true;
  }

  // TODO(crbug.com/426021822): FindNodeAtPoint does not handle corner cases
  // like clip paths. Need more checks to ensure we don't drop actions
  // unnecessarily.
  std::optional<optimization_guide::TargetNodeInfo> target_node_info =
      optimization_guide::FindNodeAtPoint(*last_observed_page_content,
                                          target.coordinate());
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
                   AggregatedJournal& journal,
                   const PageToolRequest& request)
    : Tool(task_id, journal), request_(request.Clone()) {}

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

  RenderFrameHost* frame =
      FindTargetLocalRootFrame(request_->GetTabHandle(), request_->GetTarget());
  if (!frame) {
    return MakeResult(mojom::ActionResultCode::kFrameWentAway);
  }

  // Perform validation for coordinate based target only.
  if (request_->GetTarget().is_coordinate()) {
    if (!ValidateTargetFrameCandidate(request_->GetTarget(), frame,
                                      *tab->GetContents(), last_observation)) {
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

  auto request = actor::mojom::ToolInvocation::New();
  request->action = request_->ToMojoToolAction();

  // ToolRequest params are checked for validity at creation.
  CHECK(request->action);

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
      std::move(request),
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
