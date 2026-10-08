// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_target_util.h"

#include <variant>

#include "chrome/browser/actor/actor_surface.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"

namespace actor {

using ::content::RenderFrameHost;
using ::optimization_guide::DocumentIdentifierUserData;
using ::optimization_guide::TargetNodeInfo;
using ::optimization_guide::proto::AnnotatedPageContent;

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

RenderFrameHost* GetRootFrameForWidget(content::WebContents& web_contents,
                                       content::RenderWidgetHost* rwh) {
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

// Return TargetNodeInfo from hit test against last observed APC. Returns
// std::nullopt if Target does not hit any node.
std::optional<TargetNodeInfo> FindLastObservedNodeForActionTargetId(
    const AnnotatedPageContent* apc,
    const DomNode& target) {
  if (!apc) {
    return std::nullopt;
  }
  std::optional<TargetNodeInfo> result = optimization_guide::FindNodeWithID(
      *apc, target.document_identifier, target.node_id);
  // If such a node isn't found or the node is found under a different
  // document it's an error.
  if (!result || result->document_identifier.serialized_token() !=
                     target.document_identifier) {
    return std::nullopt;
  }
  return result;
}

std::optional<TargetNodeInfo> FindLastObservedNodeForActionTargetPoint(
    const AnnotatedPageContent* apc,
    const gfx::Point& target_blink_pixels) {
  if (!apc) {
    return std::nullopt;
  }

  // TODO(crbug.com/426021822): FindNodeAtPoint does not handle corner cases
  // like clip paths. Need more checks to ensure we don't drop actions
  // unnecessarily.
  // TODO(rodneyding): Refactor FindNode* API to include optional target frame
  // document identifier to reduce search space.
  return optimization_guide::FindNodeAtPoint(*apc, target_blink_pixels);
}

}  // namespace

// Return `TargetNodeInfo` from hit test against last observed APC. Returns
// std::nullopt if Target does not hit any node.
//
// PageTarget coordinates are view-relative DIPs. This function handles
// scaling to visual-viewport-relative device pixels (BlinkSpace) required by
// APC hit testing.
std::optional<TargetNodeInfo> FindLastObservedNodeForActionTarget(
    const AnnotatedPageContent* apc,
    const PageTarget& target,
    ActorSurfaceHandle actor_surface_handle) {
  ActorSurface* actor_surface = actor_surface_handle.Get();
  return std::visit(
      absl::Overload{
          [&](const DomNode& node) {
            return FindLastObservedNodeForActionTargetId(apc, node);
          },
          [&](const gfx::Point& point_dip) {
            float dsf = 1.0f;
            content::WebContents* contents =
                actor_surface ? actor_surface->GetWebContents() : nullptr;
            content::RenderWidgetHostView* view =
                contents ? contents->GetRenderWidgetHostView() : nullptr;
            if (view) {
              dsf = view->GetDeviceScaleFactor();
            }
            // APC hit testing requires visual-viewport-relative
            // device pixels (BlinkSpace). PageTarget points are
            // provided in DIPs.
            return FindLastObservedNodeForActionTargetPoint(
                apc, gfx::ScaleToRoundedPoint(point_dip, dsf));
          },
      },
      target);
}

RenderFrameHost* FindTargetLocalRootFrame(
    ActorSurfaceHandle actor_surface_handle,
    PageTarget target) {
  ActorSurface* actor_surface = actor_surface_handle.Get();
  content::WebContents* contents =
      actor_surface ? actor_surface->GetWebContents() : nullptr;
  if (!contents) {
    return nullptr;
  }

  if (std::holds_alternative<gfx::Point>(target)) {
    content::RenderWidgetHost* target_rwh =
        contents->FindWidgetAtPoint(gfx::PointF(std::get<gfx::Point>(target)));
    if (!target_rwh) {
      return nullptr;
    }
    return GetRootFrameForWidget(*contents, target_rwh);
  }

  CHECK(std::holds_alternative<DomNode>(target));

  content::RenderFrameHost* target_frame =
      optimization_guide::GetRenderFrameForDocumentIdentifier(
          *contents, std::get<DomNode>(target).document_identifier);

  // After finding the target frame, walk up to its local root.
  return GetLocalRoot(target_frame);
}

autofill::FieldGlobalId GetFieldIdFromPageTarget(
    const AnnotatedPageContent* last_observation,
    ActorSurfaceHandle actor_surface_handle,
    const PageTarget& target) {
  ActorSurface* actor_surface = actor_surface_handle.Get();
  if (!actor_surface) {
    return {};
  }
  if (std::optional<TargetNodeInfo> node_info =
          FindLastObservedNodeForActionTarget(last_observation, target,
                                              actor_surface_handle)) {
    if (content::WebContents* web_contents = actor_surface->GetWebContents()) {
      if (RenderFrameHost* rfh =
              optimization_guide::GetRenderFrameForDocumentIdentifier(
                  *web_contents,
                  node_info->document_identifier.serialized_token())) {
        // Validation check: ensure that the target frame's underlying widget
        // matches the widget returned by hit testing at the targeted
        // coordinate. This prevents a compromised renderer from redirecting
        // a coordinate-targeted action to its own frame via a spoofed
        // popup_window.
        if (std::holds_alternative<gfx::Point>(target)) {
          // FindWidgetAtPoint expects view-relative DIPs. PageTarget points are
          // already in DIPs.
          const gfx::Point& point = std::get<gfx::Point>(target);
          content::RenderWidgetHost* actual_rwh =
              web_contents->FindWidgetAtPoint(gfx::PointF(point));
          if (!actual_rwh || actual_rwh != rfh->GetRenderWidgetHost()) {
            return {};
          }
        }

        return autofill::FieldGlobalId(
            autofill::LocalFrameToken(rfh->GetFrameToken().value()),
            autofill::FieldRendererId(node_info->node->content_attributes()
                                          .common_ancestor_dom_node_id()));
      }
    }
  }
  return {};
}

}  // namespace actor
