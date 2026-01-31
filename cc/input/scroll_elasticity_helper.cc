// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_elasticity_helper.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_elasticity_utils.h"
#include "cc/trees/scroll_node.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

namespace {

void ApplyDrawnStretchAmount(LayerTreeImpl& target_tree,
                             const ScrollNode& scroll_node,
                             const gfx::Vector2dF& stretch_amount) {
  // Applies the drawn elastic overscroll to the `TransformTree`
  if (!target_tree.property_trees()
           ->transform_tree_mutable()
           .SetDrawnElasticOverscroll(scroll_node.element_id, stretch_amount)) {
    return;
  }

  PropertyTrees& property_trees = *target_tree.property_trees();
  // Invalidate transform node.
  TransformTree& transform_tree = property_trees.transform_tree_mutable();
  const bool is_root =
      scroll_node.scrolls_inner_viewport || scroll_node.scrolls_outer_viewport;
  if (TransformNode* transform_node =
          transform_tree.Node(scroll_node.transform_id)) {
#if BUILDFLAG(IS_ANDROID)
    // We set this flag to prevent re-rastering based on constantly updating the
    // transform. This is only applicable to Android, where we have a stretch
    // that affects the raster scale.
    const bool has_stretch = !stretch_amount.IsZero();
    // TODO (crbug.com/41102897): Look into potential edge case where clearing
    // this on a transform with an animation could result in this value being
    // cleared to false un-intentionally. This would only be an issue for
    // non-root scrollers.
    transform_node->has_potential_animation = has_stretch;
#endif
    transform_node->needs_local_transform_update = true;
    transform_node->SetTransformChanged(DamageReason::kCompositorScroll);
    transform_tree.set_needs_update(true);
  }
  // Also invalidate tree and host impl.
  target_tree.set_needs_update_draw_properties();
  if (LayerTreeHostImpl* host_impl = target_tree.host_impl()) {
    host_impl->SetNeedsRedraw(/*animation_only=*/false,
                              /*skip_if_inside_draw=*/false);
    if (target_tree.IsActiveTree() && is_root) {
      host_impl->SetFullViewportDamage();
    }
  }
}

}  // namespace

class ScrollElasticityHelperImpl : public ScrollElasticityHelper {
 public:
  explicit ScrollElasticityHelperImpl(LayerTreeHostImpl* host_impl);
  ~ScrollElasticityHelperImpl() override;

  gfx::Vector2dF ConstrainOverscrollDelta(
      ElementId element_id,
      const gfx::Vector2dF& delta) const override;
  bool IsUserOverscrollable(ElementId) const override;
  void ResetStretchAmounts() override;
  void ApplyStretchAmountsToPending() override;
  void ApplyStretchAmountsToActive() override;
  gfx::Vector2dF StretchAmount(ElementId) const override;
  gfx::Size ScrollBounds(ElementId) const override;
  void SetStretchAmount(ElementId,
                        const gfx::Vector2dF& stretch_amount) override;
  gfx::PointF ScrollOffset(ElementId) const override;
  gfx::PointF MaxScrollOffset(ElementId) const override;
  void ScrollBy(ElementId, const gfx::Vector2dF& delta) override;
  void RequestOneBeginFrame() override;
  void AnimationFinished(ElementId) override;
  bool IsRoot(ElementId) const;
  const ScrollNode* GetNode(ElementId) const;
  ScrollNode* GetNode(ElementId);

 private:
  void ApplyStretchAmountsTo(LayerTreeImpl& target_tree);
  raw_ptr<LayerTreeHostImpl> host_impl_;
};

ScrollElasticityHelperImpl::ScrollElasticityHelperImpl(
    LayerTreeHostImpl* layer_tree)
    : host_impl_(layer_tree) {}

ScrollElasticityHelperImpl::~ScrollElasticityHelperImpl() = default;

bool ScrollElasticityHelperImpl::IsRoot(ElementId element_id) const {
  const ScrollNode* inner = host_impl_->InnerViewportScrollNode();
  const ScrollNode* outer = host_impl_->OuterViewportScrollNode();
  return (inner && element_id == inner->element_id) ||
         (outer && element_id == outer->element_id);
}

ScrollNode* ScrollElasticityHelperImpl::GetNode(ElementId element_id) {
  return host_impl_->GetScrollTree().FindNodeFromElementId(element_id);
}

const ScrollNode* ScrollElasticityHelperImpl::GetNode(
    ElementId element_id) const {
  return host_impl_->GetScrollTree().FindNodeFromElementId(element_id);
}

gfx::Vector2dF ScrollElasticityHelperImpl::ConstrainOverscrollDelta(
    ElementId element_id,
    const gfx::Vector2dF& delta) const {
  const ScrollNode* scroll_node = IsRoot(element_id)
                                      ? host_impl_->OuterViewportScrollNode()
                                      : GetNode(element_id);
  if (!scroll_node) {
    return gfx::Vector2dF();
  }

  gfx::Vector2dF constrained_delta = delta;
  if (!scroll_node->user_scrollable_horizontal) {
    constrained_delta.set_x(0);
  }
  if (!scroll_node->user_scrollable_vertical) {
    constrained_delta.set_y(0);
  }
  return constrained_delta;
}

bool ScrollElasticityHelperImpl::IsUserOverscrollable(
    ElementId element_id) const {
  const LayerTreeSettings& settings = host_impl_->GetSettings();
  if (IsRoot(element_id)) {
    return settings.enable_elastic_overscroll_on_root;
  }
  return settings.enable_elastic_overscroll_for_subscroll;
}

gfx::Vector2dF ScrollElasticityHelperImpl::StretchAmount(
    ElementId element_id) const {
  const bool is_root = IsRoot(element_id);
  const ScrollNode* scroll_node =
      is_root ? host_impl_->InnerViewportScrollNode() : GetNode(element_id);
  if (!scroll_node) {
    return gfx::Vector2dF();
  }
  const gfx::Vector2dF stretch = host_impl_->active_tree()
                                     ->property_trees()
                                     ->scroll_tree()
                                     .GetElasticOverscroll(*scroll_node);
  // Ensure only composited non-root scrollers have elastic overscroll.
  CHECK(scroll_elasticity_utils::ShouldAllowOverscrollEffect(
            *scroll_node, host_impl_->GetScrollTree(), host_impl_) ||
        stretch.IsZero());
  return stretch;
}

gfx::Size ScrollElasticityHelperImpl::ScrollBounds(ElementId element_id) const {
  const ScrollNode* scroll_node = IsRoot(element_id)
                                      ? host_impl_->OuterViewportScrollNode()
                                      : GetNode(element_id);
  return scroll_node ? scroll_node->container_bounds : gfx::Size();
}

void ScrollElasticityHelperImpl::ResetStretchAmounts() {
  base::flat_map<ElementId, gfx::Vector2dF>& elastic_overscroll =
      host_impl_->active_tree()
          ->property_trees()
          ->scroll_tree_mutable()
          .elastic_overscroll();
  const std::vector to_remove(elastic_overscroll.begin(),
                              elastic_overscroll.end());
  for (auto& [element_id, _] : to_remove) {
    SetStretchAmount(element_id, gfx::Vector2dF());
  }
  elastic_overscroll.clear();
}

void ScrollElasticityHelperImpl::ApplyStretchAmountsToPending() {
  // We only need to sync drawn values to pending tree if one can exist.
  if (host_impl_->CommitsToActiveTree()) {
    return;
  }
  ApplyStretchAmountsTo(*host_impl_->pending_tree());
}

void ScrollElasticityHelperImpl::ApplyStretchAmountsToActive() {
  ApplyStretchAmountsTo(*host_impl_->active_tree());
}

void ScrollElasticityHelperImpl::ApplyStretchAmountsTo(
    LayerTreeImpl& target_tree) {
  const ScrollTree& active_scroll_tree =
      host_impl_->active_tree()->property_trees()->scroll_tree();
  const ScrollTree& target_scroll_tree =
      target_tree.property_trees()->scroll_tree();
  const auto& active_elastic_overscroll_map =
      active_scroll_tree.elastic_overscroll();

  // Remove drawn elastic overscrolls on the target `TransformTree` that no
  // longer exist on the active `ScrollTree`.
  std::vector<ElementId> to_remove;
  for (auto& [element_id, _] : target_tree.property_trees()
                                   ->transform_tree_mutable()
                                   .drawn_elastic_overscroll()) {
    if (!active_elastic_overscroll_map.contains(element_id)) {
      to_remove.push_back(element_id);
    }
  }
  for (const ElementId element_id : to_remove) {
    const ScrollNode* scroll_node =
        target_scroll_tree.FindNodeFromElementId(element_id);
    if (!scroll_node) {
      continue;
    }
    ApplyDrawnStretchAmount(target_tree, *scroll_node, gfx::Vector2dF());
  }

  // Loop over elastic overscroll values from the active `ScrollTree`.
  for (const auto [element_id, elastic_overscroll] :
       active_elastic_overscroll_map) {
    const ScrollNode* scroll_node =
        target_scroll_tree.FindNodeFromElementId(element_id);
    // A nullptr guard must be used for the `scroll_node` here - as there are
    // cases when a `ScrollNode` on the `target_scroll_tree` does not exist on
    // the `active_scroll_tree`. (e.g. When main commits to the pending tree,
    // removing a `ScrollNode` - with a pre-existing active tree.)
    if (!scroll_node) {
      continue;
    }
    if (!scroll_elasticity_utils::ShouldAllowOverscrollEffect(
            *scroll_node, target_scroll_tree, host_impl_)) {
      continue;
    }

    // When targeting the active tree - only update composited scrollers.
    if (target_tree.IsActiveTree() &&
        !target_scroll_tree.CanRealizeScrollsOnActiveTree(*scroll_node)) {
      continue;
    }
    // Apply the deferred update to the target tree.
    ApplyDrawnStretchAmount(target_tree, *scroll_node, elastic_overscroll);
  }
}

void ScrollElasticityHelperImpl::SetStretchAmount(
    ElementId element_id,
    const gfx::Vector2dF& stretch_amount) {
  const bool is_root = IsRoot(element_id);
  const ScrollNode* scroll_node =
      is_root ? host_impl_->InnerViewportScrollNode() : GetNode(element_id);
  if (!scroll_node) {
    return;
  }

  // Disable elastic overscroll for scrollers that are not threaded, as
  // currently the elastic overscroll effect is impl thread only.
  const gfx::Vector2dF effective_stretch_amount =
      scroll_elasticity_utils::ShouldAllowOverscrollEffect(
          *scroll_node, host_impl_->GetScrollTree(), host_impl_)
          ? stretch_amount
          : gfx::Vector2dF();

  LayerTreeImpl& active_tree = *host_impl_->active_tree();
  PropertyTrees& property_trees = *active_tree.property_trees();
  if (!property_trees.scroll_tree_mutable().SetElasticOverscroll(
          *scroll_node, effective_stretch_amount)) {
    return;
  }
  // Mark or defer update of `TransformTree`.
  if (property_trees.scroll_tree().CanRealizeScrollsOnActiveTree(
          *scroll_node)) {
    // Directly update active tree.
    ApplyDrawnStretchAmount(active_tree, *scroll_node,
                            effective_stretch_amount);
  } else if (property_trees.scroll_tree().CanRealizeScrollsOnPendingTree(
                 *scroll_node)) {
    // Defer update to pending tree.
    host_impl_->RequestImplSideInvalidationForRasterInducingScroll(element_id);
  }
}
gfx::PointF ScrollElasticityHelperImpl::ScrollOffset(
    ElementId element_id) const {
  if (IsRoot(element_id)) {
    return host_impl_->active_tree()->TotalScrollOffset();
  }
  return host_impl_->active_tree()->TotalScrollOffset(element_id);
}
gfx::PointF ScrollElasticityHelperImpl::MaxScrollOffset(
    ElementId element_id) const {
  if (IsRoot(element_id)) {
    return host_impl_->active_tree()->TotalMaxScrollOffset();
  }
  return host_impl_->active_tree()->TotalMaxScrollOffset(element_id);
}

void ScrollElasticityHelperImpl::ScrollBy(ElementId element_id,
                                          const gfx::Vector2dF& delta) {
  ScrollNode* scroll_node;
  if (IsRoot(element_id)) {
    scroll_node = host_impl_->OuterViewportScrollNode()
                      ? host_impl_->OuterViewportScrollNode()
                      : host_impl_->InnerViewportScrollNode();
  } else {
    scroll_node = host_impl_->GetScrollTree().FindNodeFromElementId(element_id);
  }
  if (scroll_node) {
    LayerTreeImpl* tree_impl = host_impl_->active_tree();
    tree_impl->property_trees()->scroll_tree_mutable().ScrollBy(
        *scroll_node, delta, tree_impl);
  }
}

void ScrollElasticityHelperImpl::RequestOneBeginFrame() {
  host_impl_->SetNeedsOneBeginImplFrame();
}

void ScrollElasticityHelperImpl::AnimationFinished(ElementId element_id) {
  host_impl_->ElasticOverscrollAnimationFinished(element_id);
}

// static
ScrollElasticityHelper* ScrollElasticityHelper::CreateForLayerTreeHostImpl(
    LayerTreeHostImpl* host_impl) {
  return new ScrollElasticityHelperImpl(host_impl);
}

}  // namespace cc
