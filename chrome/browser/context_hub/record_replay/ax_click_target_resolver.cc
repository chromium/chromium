// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/ax_click_target_resolver.h"

#include <cstddef>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <string>

#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "url/origin.h"

namespace context_hub {
namespace {

bool IsModalOrDialog(const ui::AXNode& node) {
  return ui::IsDialog(node.GetRole()) ||
         node.GetBoolAttribute(ax::mojom::BoolAttribute::kModal);
}

// Returns true only if `rfh` is active and every ancestor frame up to
// `main_rfh` is in the same frame tree (without crossing `<fencedframe>`,
// GuestView, or other outer-document boundaries) and is same-origin with
// `main_rfh`.
bool IsSameOriginDirectDescendant(const content::RenderFrameHost& main_rfh,
                                  const content::RenderFrameHost* rfh) {
  if (!rfh || !rfh->IsActive()) {
    return false;
  }
  const url::Origin& main_origin = main_rfh.GetLastCommittedOrigin();
  for (const content::RenderFrameHost* current = rfh; current != &main_rfh;
       current = current->GetParent()) {
    if (!current ||
        !main_origin.IsSameOriginWith(current->GetLastCommittedOrigin())) {
      return false;
    }
  }
  return true;
}

// Recursively traverses `tree` from `current_node` in reverse DOM order (and
// eligible in-process same-origin child frame trees) within bounded depth and
// node budgets, updating `best_node` and `best_area` with the smallest visible
// unignored node containing `hit_point` while respecting modal/dialog and
// subframe scoping.
void FindBestHitNode(const ui::AXTree& tree,
                     const ui::AXNode* current_node,
                     const gfx::PointF& hit_point,
                     const content::RenderFrameHost& main_rfh,
                     const content::RenderWidgetHost* target_rwh,
                     size_t depth,
                     size_t& visited_nodes,
                     std::set<ui::AXTreeID>& visited_trees,
                     float& best_area,
                     const ui::AXNode*& best_node) {
  constexpr size_t kMaxTraversalDepth = 256;
  constexpr size_t kMaxVisitedNodes = 10000;
  if (!current_node || depth > kMaxTraversalDepth ||
      visited_nodes >= kMaxVisitedNodes) {
    return;
  }
  ++visited_nodes;
  if (current_node->GetRole() == ax::mojom::Role::kColumn) {
    return;
  }

  // Do not return early on `IsInvisible()`: Blink keeps inert `<html>` (when
  // `<dialog>.showModal()` is open) and `visibility: hidden` structural
  // ancestors in the tree as `ignored invisible` while descendants may still be
  // visible. Skip bounds, modal/child-tree scoping, and candidate selection for
  // `current_node` when invisible, while still recursing into children.
  const bool is_invisible = current_node->data().IsInvisible();
  const bool is_ignored = current_node->IsIgnored();
  const bool skip_node = is_ignored || is_invisible;
  bool contains_point = false;
  float area = 0.0f;
  if (!is_invisible && (!is_ignored || current_node->data().HasChildTreeID())) {
    bool offscreen = false;
    const gfx::RectF bounds = tree.GetTreeBounds(current_node, &offscreen);
    contains_point = !offscreen && bounds.Contains(hit_point);
    area = bounds.size().GetArea();
  }

  // If `current_node` is a visible unignored modal or dialog containing
  // `hit_point` and no visually higher later-in-DOM overlay (`best_area ==
  // 0.0f` or `best_area < area`) has already matched, scope candidate
  // selection to this dialog subtree so smaller controls occluded behind the
  // dialog cannot steal the hit.
  if (!skip_node && contains_point && IsModalOrDialog(*current_node) &&
      best_area > 0.0f && best_area >= area) {
    float dialog_best_area = std::numeric_limits<float>::max();
    const ui::AXNode* dialog_best_node = nullptr;
    for (const ui::AXNode* child :
         std::views::reverse(current_node->GetAllChildren())) {
      if (visited_nodes >= kMaxVisitedNodes) {
        break;
      }
      FindBestHitNode(tree, child, hit_point, main_rfh, target_rwh, depth + 1,
                      visited_nodes, visited_trees, dialog_best_area,
                      dialog_best_node);
    }
    best_node = dialog_best_node ? dialog_best_node : current_node;
    best_area = 0.0f;
    return;
  }

  if (contains_point) {
    if (const std::optional<ui::AXTreeID> child_tree_id =
            current_node->data().GetChildTreeID()) {
      content::RenderFrameHost* child_rfh =
          content::RenderFrameHost::FromAXTreeID(child_tree_id.value());
      // Out-of-process subframes receive routed input directly on their own
      // `RenderWidgetHost`, so encountering one from an ancestor frame means
      // the ancestor occluded it. For in-process subframes
      // (`same_widget_child`) when no smaller later-in-DOM overlay has already
      // matched, scope hits to the subframe so occluded parent controls behind
      // it cannot steal clicks, and lock disallowed cross-origin subframes to
      // `current_node`.
      const bool same_widget_child =
          child_rfh && child_rfh->GetRenderWidgetHost() == target_rwh;
      if (same_widget_child && best_area > 0.0f && best_area >= area) {
        if (!IsSameOriginDirectDescendant(main_rfh, child_rfh)) {
          best_node = current_node;
          best_area = 0.0f;
          return;
        }
        if (ui::AXTreeManager* child_manager =
                ui::AXTreeManager::ForChildTree(*current_node)) {
          if (const ui::AXTree* child_tree = child_manager->ax_tree();
              child_tree && child_tree->root() &&
              visited_trees.insert(child_tree_id.value()).second) {
            const gfx::RectF unclipped_bounds = tree.GetTreeBounds(
                current_node, /*offscreen=*/nullptr, /*clip_bounds=*/false);
            float child_best_area = std::numeric_limits<float>::max();
            const ui::AXNode* child_best_node = nullptr;
            FindBestHitNode(*child_tree, child_tree->root(),
                            hit_point - unclipped_bounds.OffsetFromOrigin(),
                            main_rfh, target_rwh, depth + 1, visited_nodes,
                            visited_trees, child_best_area, child_best_node);
            best_node = child_best_node ? child_best_node : current_node;
            best_area = 0.0f;
            return;
          }
        }
        best_node = current_node;
        best_area = 0.0f;
        return;
      }
    }
  }

  // Always traverse descendants even when `!contains_point`, since overflowing
  // dropdowns, `position: absolute`/`fixed` popovers, and nested `0x0`
  // (`display: contents`) wrappers may render outside their parent's box.
  for (const ui::AXNode* child :
       std::views::reverse(current_node->GetAllChildren())) {
    if (visited_nodes >= kMaxVisitedNodes) {
      break;
    }
    FindBestHitNode(tree, child, hit_point, main_rfh, target_rwh, depth + 1,
                    visited_nodes, visited_trees, best_area, best_node);
  }

  if (contains_point && !skip_node) {
    if (area < best_area) {
      best_area = area;
      best_node = current_node;
    }
  }
}

// Walks up from `best_node` to find the nearest recordable interactive control
// or text field, returning `nullptr` if a password field, disabled control,
// scrollbar, embedding/child-tree host, or root-like boundary is encountered
// before finding an eligible target.
const ui::AXNode* FindClickableTargetNode(const ui::AXNode& best_node,
                                          float scale) {
  const gfx::RectF root_bounds =
      best_node.tree()->GetTreeBounds(best_node.tree()->root());
  const float root_area = root_bounds.size().GetArea();

  for (const ui::AXNode* node = &best_node; node;
       node = node->GetUnignoredParent()) {
    if (node->data().IsPasswordField()) {
      return nullptr;
    }

    const ax::mojom::Role role = node->GetRole();
    if (ui::IsRootLike(role) || ui::IsEmbeddingElement(role) ||
        node->data().HasChildTreeID() || role == ax::mojom::Role::kScrollBar ||
        node->data().GetRestriction() == ax::mojom::Restriction::kDisabled) {
      return nullptr;
    }

    // Skip structural landmarks that might have `onclick` handlers for event
    // delegation.
    if (ui::IsLandmark(role) || role == ax::mojom::Role::kArticle) {
      continue;
    }

    if (node->data().IsClickable() || ui::IsControl(role) ||
        node->data().IsTextField()) {
      // Skip giant layout containers (like `<body>` or `#root`) that are only
      // clickable due to React event delegation on the root.
      constexpr float kMaxClickableContainerDip = 600.0f;
      constexpr float kMaxClickableContainerViewportFraction = 0.5f;
      if (!node->data().IsTextField() &&
          (role == ax::mojom::Role::kGenericContainer ||
           role == ax::mojom::Role::kGroup)) {
        const gfx::RectF node_bounds = node->tree()->GetTreeBounds(
            node, /*offscreen=*/nullptr, /*clip_bounds=*/false);
        const float node_area = node_bounds.size().GetArea();
        if ((node_bounds.width() > kMaxClickableContainerDip * scale &&
             node_bounds.height() > kMaxClickableContainerDip * scale) ||
            (root_area > 0.0f &&
             node_area >= kMaxClickableContainerViewportFraction * root_area)) {
          continue;
        }
      }

      return node;
    }
  }

  return nullptr;
}

}  // namespace

std::optional<std::string> ResolveClickedElementLabel(
    content::RenderFrameHost& main_rfh,
    content::RenderFrameHost* target_rfh,
    const gfx::PointF& point_in_widget_dips) {
  if (!IsSameOriginDirectDescendant(main_rfh, target_rfh)) {
    return std::nullopt;
  }

  float scale = 1.0f;
  if (content::RenderWidgetHostView* target_view = target_rfh->GetView()) {
    if (const float dsf = target_view->GetDeviceScaleFactor(); dsf > 0.0f) {
      scale = dsf;
    }
  } else if (content::RenderWidgetHostView* main_view = main_rfh.GetView()) {
    if (const float dsf = main_view->GetDeviceScaleFactor(); dsf > 0.0f) {
      scale = dsf;
    }
  }
  const gfx::PointF hit_point = gfx::ScalePoint(point_in_widget_dips, scale);

  const ui::AXTreeID target_tree_id = target_rfh->GetAXTreeID();
  ui::AXTreeManager* manager = ui::AXTreeManager::FromID(target_tree_id);
  if (!manager || !manager->ax_tree() || !manager->ax_tree()->root()) {
    return std::nullopt;
  }

  size_t visited_nodes = 0;
  std::set<ui::AXTreeID> visited_trees = {target_tree_id};
  const ui::AXNode* best_node = nullptr;
  float best_area = std::numeric_limits<float>::max();
  FindBestHitNode(*manager->ax_tree(), manager->ax_tree()->root(), hit_point,
                  main_rfh, target_rfh->GetRenderWidgetHost(), /*depth=*/0,
                  visited_nodes, visited_trees, best_area, best_node);

  if (!best_node) {
    return std::nullopt;
  }

  const ui::AXNode* target_node = FindClickableTargetNode(*best_node, scale);
  if (!target_node) {
    return std::nullopt;
  }

  std::string name = SanitizeRecordedText(target_node->GetNameUTF8());
  if (name.empty() && !target_node->data().IsTextField() &&
      ui::IsText(best_node->GetRole())) {
    name = SanitizeRecordedText(best_node->GetNameUTF8());
  }
  if (name.empty()) {
    name = ui::ToString(target_node->GetRole());
  }
  return name;
}

}  // namespace context_hub
