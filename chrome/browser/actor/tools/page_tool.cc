// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_tool.h"

#include <variant>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/page_target_util.h"
#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "pdf/buildflags.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

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

  RenderFrameHost* apc_target_frame =
      optimization_guide::GetRenderFrameForDocumentIdentifier(
          web_contents,
          target_node_info->document_identifier.serialized_token());

  // Only return the candidate if its RenderWidgetHost matches the target
  // and it's also a local root frame(i.e. has no parent or parent has
  // a different RenderWidgetHost)
  if (apc_target_frame && apc_target_frame->GetRenderWidgetHost() ==
                              candidate_frame->GetRenderWidgetHost()) {
    return true;
  }

#if BUILDFLAG(ENABLE_PDF)
  // TODO(b/458776473): Remove once PdfOopif is shipped. The APC is sent
  // correctly for these pages.
  if (base::FeatureList::IsEnabled(kActorBypassTOUValidationForGuestView) &&
      !base::FeatureList::IsEnabled(chrome_pdf::features::kPdfOopif) &&
      apc_target_frame && !candidate_frame->GetParent() &&
      !candidate_frame->IsFencedFrameRoot() &&
      candidate_frame->GetParentOrOuterDocumentOrEmbedder() ==
          apc_target_frame) {
    return true;
  }
#endif

  return false;
}

// Helper function to create ObservedToolTarget mojom struct from
// TargetNodeInfo struct.
mojom::ObservedToolTargetPtr ToMojoObservedToolTarget(
    const std::optional<optimization_guide::TargetNodeInfo>&
        observed_target_node_info,
    RenderFrameHost& target_frame) {
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
    // Transform to frame's widget coordinate space.
    const gfx::Point outer_box_origin_point = gfx::ToRoundedPoint(
        target_frame.GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(
            content_attributes.geometry().outer_bounding_box().x(),
            content_attributes.geometry().outer_bounding_box().y())));
    observed_target->node_attribute->geometry->outer_bounding_box = gfx::Rect(
        outer_box_origin_point,
        {content_attributes.geometry().outer_bounding_box().width(),
         content_attributes.geometry().outer_bounding_box().height()});
    const gfx::Point visible_box_origin_point = gfx::ToRoundedPoint(
        target_frame.GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(
            content_attributes.geometry().visible_bounding_box().x(),
            content_attributes.geometry().visible_bounding_box().y())));
    observed_target->node_attribute->geometry->visible_bounding_box = gfx::Rect(
        visible_box_origin_point,
        {content_attributes.geometry().visible_bounding_box().width(),
         content_attributes.geometry().visible_bounding_box().height()});
  }
  return observed_target;
}

}  // namespace

// Observer to track if the a given RenderFrameHost is changed.
class RenderFrameChangeObserver : public WebContentsObserver {
 public:
  RenderFrameChangeObserver(RenderFrameHost& rfh,
                            base::OnceClosure on_frame_navigated_callback,
                            base::OnceClosure on_frame_process_gone_callback)
      : WebContentsObserver(WebContents::FromRenderFrameHost(&rfh)),
        rfh_id_(rfh.GetGlobalId()),
        on_frame_navigated_callback_(std::move(on_frame_navigated_callback)),
        on_frame_process_gone_callback_(
            std::move(on_frame_process_gone_callback)) {}

  // `WebContentsObserver`:
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* /*new_host*/) override {
    if (!on_frame_navigated_callback_) {
      return;
    }

    if (old_host && old_host->GetGlobalId() == rfh_id_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_frame_navigated_callback_));
    }
  }
  void RenderFrameDeleted(RenderFrameHost* rfh) override {
    // The scoped frame has exited. It is not safe to continue the task.
    // TODO(crbug.com/423932492): Ideally the task could continue and the model
    // should be able to refresh the page. Currently the model is not aware of
    // the crashed frame because the screenshot does not include the sad tab
    // WebUI.
    if (rfh->GetGlobalId() == rfh_id_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_frame_process_gone_callback_));
    }
  }

 private:
  const GlobalRenderFrameHostId rfh_id_;
  base::OnceClosure on_frame_navigated_callback_;
  base::OnceClosure on_frame_process_gone_callback_;
};

PageTool::PageTool(TaskId task_id,
                   ToolDelegate& tool_delegate,
                   const PageToolRequest& request)
    : Tool(task_id, tool_delegate), request_(request.Clone()) {}

PageTool::~PageTool() = default;

void PageTool::Validate(ToolCallback callback) {
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

  journal().Log(
      JournalURL(), task_id(), "TimeOfUseValidation",
      JournalDetailsBuilder().Add("tab_handle", tab->GetHandle()).Build());

  RenderFrameHost* frame =
      FindTargetLocalRootFrame(request_->GetTabHandle(), request_->GetTarget());
  if (!frame) {
    return MakeResult(mojom::ActionResultCode::kFrameWentAway);
  }

  if (std::holds_alternative<gfx::Point>(request_->GetTarget())) {
    const gfx::Point& point = std::get<gfx::Point>(request_->GetTarget());
    gfx::Size content_size = tab->GetContents()->GetSize();
    if (!gfx::Rect(content_size).Contains(point)) {
      return MakeResult(mojom::ActionResultCode::kCoordinatesOutOfBounds);
    }
  }

  std::optional<TargetNodeInfo> observed_target_node_info;
  if (std::holds_alternative<gfx::Point>(request_->GetTarget())) {
    gfx::Point hit_test_target;

    // Convert target coordinate from DIPs to screen pixels.
    display::Screen* screen = display::Screen::Get();
    float scale_factor = screen
                             ->GetPreferredScaleFactorForWindow(
                                 tab->GetContents()->GetTopLevelNativeWindow())
                             .value();
    hit_test_target = gfx::ScaleToRoundedPoint(
        std::get<gfx::Point>(request_->GetTarget()), scale_factor);

    // TODO(crbug.com/426021822): FindNodeAtPoint does not handle corner cases
    // like clip paths. Need more checks to ensure we don't drop actions
    // unnecessarily.
    observed_target_node_info = FindLastObservedNodeForActionTargetPoint(
        last_observation, hit_test_target);
  } else {
    CHECK(std::holds_alternative<DomNode>(request_->GetTarget()));
    observed_target_node_info = FindLastObservedNodeForActionTargetId(
        last_observation, std::get<DomNode>(request_->GetTarget()));
  }

  if (!observed_target_node_info) {
    journal().Log(JournalURL(), task_id(), "TimeOfUseValidation",
                  JournalDetailsBuilder()
                      .Add("details", "No observed target found in APC.")
                      .Build());
  }

  // Perform validation for coordinate based target only.
  // TODO(bokan): We can't perform a TOCTOU check If there's no last
  // observation. Consider what to do in this case.
  if (std::holds_alternative<gfx::Point>(request_->GetTarget()) &&
      last_observation) {
    if (!ValidateTargetFrameCandidate(request_->GetTarget(), frame,
                                      *tab->GetContents(),
                                      observed_target_node_info)) {
      return MakeResult(
          mojom::ActionResultCode::kFrameLocationChangedSinceObservation);
    }
  }

  observed_target_ =
      ToMojoObservedToolTarget(observed_target_node_info, *frame);
  has_completed_time_of_use_ = true;
  target_document_ = frame->GetWeakDocumentPtr();

  return MakeOkResult();
}

void PageTool::Invoke(ToolCallback callback) {
  // Frame was validated in TimeOfUseValidation.
  CHECK(GetFrame());
  RenderFrameHost& frame = *GetFrame();

  journal().EnsureJournalBound(frame);

  invoke_callback_ = std::move(callback);

  auto invocation = actor::mojom::ToolInvocation::New();
  invocation->action = request_->ToMojoToolAction(frame);

  // Transform coordinate target from viewport space to widget space for use
  // within renderer.
  if (std::holds_alternative<gfx::Point>(request_->GetTarget())) {
    PageTarget transformed_target =
        gfx::ToRoundedPoint(frame.GetView()->TransformRootPointToViewCoordSpace(
            gfx::PointF(std::get<gfx::Point>(request_->GetTarget()))));
    invocation->target = ToMojo(transformed_target);
  } else {
    invocation->target = ToMojo(request_->GetTarget());
  }

  invocation->observed_target = std::move(observed_target_);

  invocation->task_id = task_id();

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
  // The observer also listens to the process exit signal from the renderer
  // (i.e., crashed). The invoke is finished with an error in this case.
  // `this` Unretained because the observer is owned by this class and thus
  // removed on destruction.
  frame_change_observer_ = std::make_unique<RenderFrameChangeObserver>(
      frame,
      base::BindOnce(&PageTool::OnRenderFrameHostChanged,
                     base::Unretained(this)),
      base::BindOnce(&PageTool::OnRenderFrameGone, base::Unretained(this)));

  timeout_timer_.Start(
      FROM_HERE, features::kGlicActorPageToolTimeout.Get(),
      base::BindOnce(&PageTool::OnTimeout, weak_ptr_factory_.GetWeakPtr()));

  if (base::FeatureList::IsEnabled(kActorSendBrowserSignalForAction)) {
    request_->WillSendToRenderer(frame.GetRenderWidgetHost());
  }
  chrome_render_frame_->InvokeTool(
      std::move(invocation),
      base::BindOnce(&PageTool::FinishInvoke, weak_ptr_factory_.GetWeakPtr()));
}

void PageTool::Cancel() {
  if (chrome_render_frame_.is_bound()) {
    journal().Log(JournalURL(), task_id(), "PageTool::Cancel",
                  JournalDetailsBuilder()
                      .Add("tab_handle", request_->GetTabHandle())
                      .Build());

    chrome_render_frame_->CancelTool(task_id());
  }
  FinishInvoke(MakeResult(mojom::ActionResultCode::kInvokeCanceled));
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

std::unique_ptr<ObservationDelayController> PageTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  CHECK(has_completed_time_of_use_);

  RenderFrameHost* frame = GetFrame();

  // It's the caller's responsibility to ensure a frame is still live if calling
  // this method.
  CHECK(frame);

  return std::make_unique<ObservationDelayController>(
      *frame, task_id(), journal(), page_stability_config);
}

void PageTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                      ToolCallback callback) const {
  task.AddTab(request_->GetTabHandle(), std::move(callback));
}

tabs::TabHandle PageTool::GetTargetTab() const {
  return request_->GetTabHandle();
}

void PageTool::OnRenderFrameHostChanged() {
  // Return error if tab itself is closed or the WebContents hosted in the tab
  // is being destroyed.
  if (!request_->GetTabHandle().Get() ||
      request_->GetTabHandle().Get()->GetContents()->IsBeingDestroyed()) {
    FinishInvoke(MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // The RenderFrameHost has been swapped out. This is likely due to a
  // navigation. Finish the invocation successfully as the ToolController will
  // wait on the new page to load if needed.
  FinishInvoke(MakeOkResult());
}

void PageTool::OnRenderFrameGone() {
  auto* tab_interface = request_->GetTabHandle().Get();

  mojom::ActionResultCode result_code = mojom::ActionResultCode::kFrameWentAway;
  if (!tab_interface || tab_interface->GetContents()->IsBeingDestroyed()) {
    result_code = mojom::ActionResultCode::kTabWentAway;
  } else if (tab_interface->GetContents()->IsCrashed()) {
    result_code = mojom::ActionResultCode::kRendererCrashed;
  }
  FinishInvoke(MakeResult(result_code));
}

void PageTool::OnTimeout() {
  if (chrome_render_frame_.is_bound()) {
    journal().Log(JournalURL(), task_id(), "PageTool::OnTimeout",
                  JournalDetailsBuilder()
                      .Add("tab_handle", request_->GetTabHandle())
                      .Build());

    chrome_render_frame_->CancelTool(task_id());
  }
  FinishInvoke(MakeResult(mojom::ActionResultCode::kToolTimeout));
}

void PageTool::FinishInvoke(mojom::ActionResultPtr result) {
  if (!invoke_callback_) {
    return;
  }

  timeout_timer_.Stop();
  frame_change_observer_.reset();

  std::move(invoke_callback_).Run(std::move(result));

  // WARNING: `this` may now be destroyed.
}

content::RenderFrameHost* PageTool::GetFrame() const {
  CHECK(has_completed_time_of_use_);
  return target_document_.AsRenderFrameHostIfValid();
}

}  // namespace actor
