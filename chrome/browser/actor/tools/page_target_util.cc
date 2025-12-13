// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_target_util.h"

#include <variant>

#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/gfx/geometry/point.h"
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

}  // namespace

RenderFrameHost* FindTargetLocalRootFrame(tabs::TabHandle tab_handle,
                                          PageTarget target) {
  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab) {
    return nullptr;
  }

  content::WebContents& contents = *tab->GetContents();

  if (std::holds_alternative<gfx::Point>(target)) {
    content::RenderWidgetHost* target_rwh =
        contents.FindWidgetAtPoint(gfx::PointF(std::get<gfx::Point>(target)));
    if (!target_rwh) {
      return nullptr;
    }
    return GetRootFrameForWidget(contents, target_rwh);
  }

  CHECK(std::holds_alternative<DomNode>(target));

  content::RenderFrameHost* target_frame =
      optimization_guide::GetRenderFrameForDocumentIdentifier(
          *tab->GetContents(), std::get<DomNode>(target).document_identifier);

  // After finding the target frame, walk up to its local root.
  return GetLocalRoot(target_frame);
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
    const gfx::Point& target_pixels) {
  if (!apc) {
    return std::nullopt;
  }

  // TODO(rodneyding): Refactor FindNode* API to include optional target frame
  // document identifier to reduce search space.
  return optimization_guide::FindNodeAtPoint(*apc, target_pixels);
}

std::optional<optimization_guide::TargetNodeInfo>
FindLastObservedNodeForActionTarget(
    const optimization_guide::proto::AnnotatedPageContent* apc,
    const PageTarget& target) {
  return std::visit(
      absl::Overload{
          [&](const DomNode& node) {
            return FindLastObservedNodeForActionTargetId(apc, node);
          },
          [&](const gfx::Point& point) {
            return FindLastObservedNodeForActionTargetPoint(apc, point);
          },
      },
      target);
}

}  // namespace actor
