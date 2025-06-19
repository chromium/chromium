// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/browser_action_util.h"

#include "chrome/common/actor/actor_logging.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/point_f.h"

namespace actor {

using ::content::RenderFrameHost;
using ::content::RenderWidgetHost;
using ::content::WebContents;
using ::optimization_guide::DocumentIdentifierUserData;
using ::optimization_guide::proto::Action;
using ::optimization_guide::proto::ActionTarget;
using ::optimization_guide::proto::DocumentIdentifier;

namespace {

bool IsTargetingTab(const Action& action) {
  switch (action.action_case()) {
    case Action::ActionCase::kClick:
    case Action::ActionCase::kType:
    case Action::ActionCase::kScroll:
    case Action::ActionCase::kMoveMouse:
    case Action::ActionCase::kDragAndRelease:
    case Action::ActionCase::kSelect:
    // These actions target neither tabs nor frames.
    case Action::ActionCase::kCreateTab:
    case Action::ActionCase::kCloseTab:
    case Action::ActionCase::kActivateTab:
    case Action::ActionCase::kCreateWindow:
    case Action::ActionCase::kCloseWindow:
    case Action::ActionCase::kActivateWindow:
    case Action::ActionCase::kYieldToUser:
      return false;
    case Action::ActionCase::kNavigate:
    case Action::ActionCase::kBack:
    case Action::ActionCase::kForward:
    case Action::ActionCase::kWait:
      return true;
    case Action::ActionCase::ACTION_NOT_SET:
      NOTREACHED();
  }
}

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

}  // namespace

const ActionTarget* ExtractTarget(const Action& action) {
  switch (action.action_case()) {
    case Action::ActionCase::kClick:
      if (!action.click().has_target()) {
        return nullptr;
      }
      return &action.click().target();
    case Action::ActionCase::kType:
      if (!action.type().has_target()) {
        return nullptr;
      }
      return &action.type().target();
    case Action::ActionCase::kScroll:
      if (!action.scroll().has_target()) {
        return nullptr;
      }
      return &action.scroll().target();
    case Action::ActionCase::kMoveMouse:
      if (!action.move_mouse().has_target()) {
        return nullptr;
      }
      return &action.move_mouse().target();
    case Action::ActionCase::kDragAndRelease:
      if (!action.drag_and_release().has_from_target()) {
        return nullptr;
      }
      return &action.drag_and_release().from_target();
    case Action::ActionCase::kSelect:
      if (!action.select().has_target()) {
        return nullptr;
      }
      return &action.select().target();
    case Action::ActionCase::kNavigate:
    case Action::ActionCase::kBack:
    case Action::ActionCase::kForward:
    case Action::ActionCase::kWait:
    case Action::ActionCase::kCreateTab:
    case Action::ActionCase::kCloseTab:
    case Action::ActionCase::kActivateTab:
    case Action::ActionCase::kCreateWindow:
    case Action::ActionCase::kCloseWindow:
    case Action::ActionCase::kActivateWindow:
    case Action::ActionCase::kYieldToUser:
    case Action::ActionCase::ACTION_NOT_SET:
      return nullptr;
  }
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

RenderFrameHost* FindTargetLocalRootFrame(WebContents& web_contents,
                                          const Action& action) {
  // If the action targets the tab as a whole, the target is the primary main
  // frame.
  if (IsTargetingTab(action)) {
    return web_contents.GetPrimaryMainFrame();
  }

  const ActionTarget* target = ExtractTarget(action);
  if (!target) {
    // A scroll action may not have a target, which indicates scrolling the main
    // frame.
    if (action.action_case() == Action::kScroll) {
      return web_contents.GetPrimaryMainFrame();
    }
    ACTOR_LOG() << "Page-level BrowserAction did not specify an ActionTarget.";
    return nullptr;
  }

  if (target->has_content_node_id()) {
    const std::string& serialized_token =
        target->document_identifier().serialized_token();

    RenderFrameHost* target_frame =
        GetRenderFrameForDocumentIdentifier(web_contents, serialized_token);
    // After finding the target frame, walk up to its local root.
    return GetLocalRoot(target_frame);
  }

  if (target->has_coordinate()) {
    const gfx::PointF target_point =
        gfx::PointF(target->coordinate().x(), target->coordinate().y());
    RenderWidgetHost* target_rwh = web_contents.FindWidgetAtPoint(target_point);
    if (!target_rwh) {
      return nullptr;
    }
    return GetRootFrameForWidget(web_contents, target_rwh);
  }

  ACTOR_LOG() << "Page-level BrowserAction ActionTarget is invalid.";
  return nullptr;
}

}  // namespace actor
