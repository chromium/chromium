// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_tool.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/geometry/point.h"

namespace {

using ::content::GlobalRenderFrameHostId;
using ::content::RenderFrameHost;
using ::content::WebContents;
using ::content::WebContentsObserver;
using ::optimization_guide::proto::Action;
using ::optimization_guide::proto::ActionTarget;
using ::optimization_guide::proto::ClickAction_ClickCount;
using ::optimization_guide::proto::ClickAction_ClickType;
using ::optimization_guide::proto::ScrollAction_ScrollDirection;
using ::optimization_guide::proto::TypeAction_TypeMode;

void SetMojoTarget(const ActionTarget& target,
                   actor::mojom::ToolTargetPtr& out_mojo_target) {
  if (target.has_coordinate()) {
    out_mojo_target = actor::mojom::ToolTarget::NewCoordinate(
        gfx::Point(target.coordinate().x(), target.coordinate().y()));
  } else {
    // A ContentNodeId of 0 indicates the viewport. The mojo message indicates
    // viewport by omitting a target.
    if (target.content_node_id() > 0) {
      out_mojo_target =
          actor::mojom::ToolTarget::NewDomNodeId(target.content_node_id());
    }
  }
}

// Set mojom for click action based on proto. Returns false if the proto does
// not contain correct/sufficient information, true otherwise.
bool SetClickToolArgs(actor::mojom::ClickActionPtr& click,
                      const Action& action) {
  SetMojoTarget(action.click().target(), click->target);

  switch (action.click().click_type()) {
    case ClickAction_ClickType::ClickAction_ClickType_LEFT:
      click->type = actor::mojom::ClickAction::Type::kLeft;
      break;
    case ClickAction_ClickType::ClickAction_ClickType_RIGHT:
      click->type = actor::mojom::ClickAction::Type::kRight;
      break;
    case ClickAction_ClickType::ClickAction_ClickType_UNKNOWN_CLICK_TYPE:
    case ClickAction_ClickType::
        ClickAction_ClickType_ClickAction_ClickType_INT_MAX_SENTINEL_DO_NOT_USE_:
    case ClickAction_ClickType::
        ClickAction_ClickType_ClickAction_ClickType_INT_MIN_SENTINEL_DO_NOT_USE_:
      // TODO(issuetracker.google.com/412700289): Revert once this is set.
      click->type = actor::mojom::ClickAction::Type::kLeft;
      break;
      // return false;
  }

  switch (action.click().click_count()) {
    case ClickAction_ClickCount::ClickAction_ClickCount_SINGLE:
      click->count = actor::mojom::ClickAction::Count::kSingle;
      break;
    case ClickAction_ClickCount::ClickAction_ClickCount_DOUBLE:
      click->count = actor::mojom::ClickAction::Count::kDouble;
      break;
    case ClickAction_ClickCount::ClickAction_ClickCount_UNKNOWN_CLICK_COUNT:
    case ClickAction_ClickCount::
        ClickAction_ClickCount_ClickAction_ClickCount_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ClickAction_ClickCount::
        ClickAction_ClickCount_ClickAction_ClickCount_INT_MAX_SENTINEL_DO_NOT_USE_:
      // TODO(issuetracker.google.com/412700289): Revert once this is set.
      click->count = actor::mojom::ClickAction::Count::kSingle;
      break;
      // return false;
  }
  return true;
}

// Set mojom for mouse move action based on proto.
void SetMouseMoveToolArgs(actor::mojom::MouseMoveActionPtr& move,
                          const Action& action) {
  SetMojoTarget(action.move_mouse().target(), move->target);
}

// Set mojom for type action based on proto.
// Returns false if the proto does not contain correct/sufficient information,
// true otherwise.
bool SetTypeToolArgs(actor::mojom::TypeActionPtr& type_action,
                     const Action& action) {
  SetMojoTarget(action.type().target(), type_action->target);

  type_action->text = action.type().text();
  type_action->follow_by_enter = action.type().follow_by_enter();

  // Map proto enum to mojom enum
  switch (action.type().mode()) {
    case TypeAction_TypeMode::TypeAction_TypeMode_DELETE_EXISTING:
      type_action->mode = actor::mojom::TypeAction::Mode::kDeleteExisting;
      break;
    case TypeAction_TypeMode::TypeAction_TypeMode_PREPEND:
      type_action->mode = actor::mojom::TypeAction::Mode::kPrepend;
      break;
    case TypeAction_TypeMode::TypeAction_TypeMode_APPEND:
      type_action->mode = actor::mojom::TypeAction::Mode::kAppend;
      break;
    case TypeAction_TypeMode::TypeAction_TypeMode_UNKNOWN_TYPE_MODE:
    case TypeAction_TypeMode::
        TypeAction_TypeMode_TypeAction_TypeMode_INT_MIN_SENTINEL_DO_NOT_USE_:
    case TypeAction_TypeMode::
        TypeAction_TypeMode_TypeAction_TypeMode_INT_MAX_SENTINEL_DO_NOT_USE_:
      // TODO(issuetracker.google.com/412700289): Revert once this is set.
      type_action->mode = actor::mojom::TypeAction::Mode::kDeleteExisting;
      break;
      //      DLOG(ERROR) << "TypeAction proto type mode not supported"
      //                  << action.type().mode();
      //      return false;
  }

  return true;
}

bool SetScrollToolArgs(actor::mojom::ScrollActionPtr& scroll,
                       const Action& action) {
  if (action.scroll().has_target()) {
    SetMojoTarget(action.scroll().target(), scroll->target);
  }
  switch (action.scroll().direction()) {
    case ScrollAction_ScrollDirection::ScrollAction_ScrollDirection_LEFT:
      scroll->direction = actor::mojom::ScrollAction::ScrollDirection::kLeft;
      break;
    case ScrollAction_ScrollDirection::ScrollAction_ScrollDirection_RIGHT:
      scroll->direction = actor::mojom::ScrollAction::ScrollDirection::kRight;
      break;
    case ScrollAction_ScrollDirection::ScrollAction_ScrollDirection_UP:
      scroll->direction = actor::mojom::ScrollAction::ScrollDirection::kUp;
      break;
    case ScrollAction_ScrollDirection::ScrollAction_ScrollDirection_DOWN:
      scroll->direction = actor::mojom::ScrollAction::ScrollDirection::kDown;
      break;
    case ScrollAction_ScrollDirection::
        ScrollAction_ScrollDirection_UNKNOWN_SCROLL_DIRECTION:
    case ScrollAction_ScrollDirection::
        ScrollAction_ScrollDirection_ScrollAction_ScrollDirection_INT_MIN_SENTINEL_DO_NOT_USE_:
    case ScrollAction_ScrollDirection::
        ScrollAction_ScrollDirection_ScrollAction_ScrollDirection_INT_MAX_SENTINEL_DO_NOT_USE_:
      // TODO(issuetracker.google.com/412700289): Revert once this is set.
      scroll->direction = actor::mojom::ScrollAction::ScrollDirection::kDown;
      break;
      // return false;
  }
  scroll->distance = action.scroll().distance();
  return true;
}

void SetSelectToolArgs(actor::mojom::SelectActionPtr& select,
                       const Action& action) {
  SetMojoTarget(action.select().target(), select->target);
  select->value = action.select().value();
}

void SetDragAndReleaseToolArgs(
    actor::mojom::DragAndReleaseActionPtr& drag_and_release,
    Action action) {
  SetMojoTarget(action.drag_and_release().from_target(),
                drag_and_release->from_target);
  SetMojoTarget(action.drag_and_release().to_target(),
                drag_and_release->to_target);
}

}  // namespace

namespace actor {

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

PageTool::PageTool(AggregatedJournal& journal,
                   RenderFrameHost& frame,
                   const Action& action)
    : render_frame_host_(frame.GetWeakDocumentPtr()), action_(action) {
  journal.EnsureJournalBound(frame);
}

PageTool::~PageTool() = default;

void PageTool::Validate(ValidateCallback callback) {
  // No browser-side validation yet.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), MakeOkResult()));
}

void PageTool::Invoke(InvokeCallback callback) {
  invoke_callback_ = std::move(callback);
  RenderFrameHost* frame = render_frame_host_.AsRenderFrameHostIfValid();
  if (!frame) {
    PostFinishInvoke(mojom::ActionResultCode::kFrameWentAway);
    return;
  }

  auto request = actor::mojom::ToolInvocation::New();

  switch (action_.action_case()) {
    case Action::ActionCase::kClick: {
      auto click = mojom::ClickAction::New();
      if (!SetClickToolArgs(click, action_)) {
        PostFinishInvoke(mojom::ActionResultCode::kArgumentsInvalid);
        return;
      }
      request->action = mojom::ToolAction::NewClick(std::move(click));
      break;
    }
    case Action::ActionCase::kType: {
      auto type = mojom::TypeAction::New();
      if (!SetTypeToolArgs(type, action_)) {
        PostFinishInvoke(mojom::ActionResultCode::kArgumentsInvalid);
        return;
      }
      request->action = mojom::ToolAction::NewType(std::move(type));
      break;
    }
    case Action::ActionCase::kScroll: {
      auto scroll = mojom::ScrollAction::New();
      if (!SetScrollToolArgs(scroll, action_)) {
        PostFinishInvoke(mojom::ActionResultCode::kArgumentsInvalid);
        return;
      }
      request->action = mojom::ToolAction::NewScroll(std::move(scroll));
      break;
    }
    case Action::ActionCase::kMoveMouse: {
      auto mouse_move = mojom::MouseMoveAction::New();
      SetMouseMoveToolArgs(mouse_move, action_);
      request->action = mojom::ToolAction::NewMouseMove(std::move(mouse_move));
      break;
    }
    case Action::ActionCase::kDragAndRelease: {
      auto drag_and_release = mojom::DragAndReleaseAction::New();
      SetDragAndReleaseToolArgs(drag_and_release, action_);
      request->action =
          mojom::ToolAction::NewDragAndRelease(std::move(drag_and_release));
      break;
    }
    case Action::ActionCase::kSelect: {
      auto select = mojom::SelectAction::New();
      SetSelectToolArgs(select, action_);
      request->action = mojom::ToolAction::NewSelect(std::move(select));
      break;
    }
    case Action::ActionCase::kNavigate:
    case Action::ActionCase::kBack:
    case Action::ActionCase::kForward:
    case Action::ActionCase::kWait:
    case Action::kCreateTab:
    case Action::kCloseTab:
    case Action::kActivateTab:
    case Action::kCreateWindow:
    case Action::kCloseWindow:
    case Action::kActivateWindow:
    case Action::kYieldToUser:
    case Action::ActionCase::ACTION_NOT_SET:
      NOTREACHED();
  }

  frame->GetRemoteAssociatedInterfaces()->GetInterface(&chrome_render_frame_);

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
      *frame, base::BindOnce(&PageTool::FinishInvoke, base::Unretained(this),
                             MakeOkResult()));

  // `this` Unretained because this class owns the mojo pipe that invokes the
  // callbacks.
  chrome_render_frame_.set_disconnect_handler(
      base::BindOnce(&PageTool::FinishInvoke, base::Unretained(this),
                     MakeResult(mojom::ActionResultCode::kExecutorDestroyed)));
  chrome_render_frame_->InvokeTool(
      std::move(request),
      base::BindOnce(&PageTool::FinishInvoke, base::Unretained(this)));
}

std::string PageTool::DebugString() const {
  // TODO(crbug.com/402210051): Add more details here about tool params.
  return absl::StrFormat("PageTool:%s", JournalEvent().c_str());
}

std::string PageTool::JournalEvent() const {
  switch (action_.action_case()) {
    case Action::ActionCase::kClick: {
      return "Click";
    }
    case Action::ActionCase::kType: {
      return "Type";
    }
    case Action::ActionCase::kScroll: {
      return "Scroll";
    }
    case Action::ActionCase::kMoveMouse: {
      return "MoveMouse";
    }
    case Action::ActionCase::kDragAndRelease: {
      return "DragAndRelease";
    }
    case Action::ActionCase::kSelect: {
      return "Select";
    }
    case Action::ActionCase::kNavigate:
    case Action::ActionCase::kBack:
    case Action::ActionCase::kForward:
    case Action::ActionCase::kWait:
    case Action::kCreateTab:
    case Action::kCloseTab:
    case Action::kActivateTab:
    case Action::kCreateWindow:
    case Action::kCloseWindow:
    case Action::kActivateWindow:
    case Action::kYieldToUser:
    case Action::ActionCase::ACTION_NOT_SET:
      NOTREACHED();
  }
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

}  // namespace actor
