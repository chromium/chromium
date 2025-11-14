// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_elasticity_helper.h"

#include "base/memory/raw_ptr.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"

namespace cc {

namespace {

void MarkNodeNeedsUpdate(
    LayerTreeHostImpl& host_impl,
    const ScrollNode& scroll_node,
    [[maybe_unused]] const gfx::Vector2dF& stretch_amount) {
  LayerTreeImpl& active_tree = *host_impl.active_tree();
  PropertyTrees& property_trees = *active_tree.property_trees();
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
  active_tree.set_needs_update_draw_properties();
  host_impl.SetNeedsRedraw(/*animation_only=*/false,
                           /*skip_if_inside_draw=*/false);
  if (is_root) {
    host_impl.SetFullViewportDamage();
  }
}

}  // namespace

class ScrollElasticityHelperImpl : public ScrollElasticityHelper {
 public:
  explicit ScrollElasticityHelperImpl(LayerTreeHostImpl* host_impl);
  ~ScrollElasticityHelperImpl() override;

  bool IsUserScrollableHorizontal(ElementId) const override;
  bool IsUserScrollableVertical(ElementId) const override;
  void ResetStretchAmounts() override;
  void ForceApplyStretchAmounts() override;
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

bool ScrollElasticityHelperImpl::IsUserScrollableHorizontal(
    ElementId element_id) const {
  const ScrollNode* scroll_node = IsRoot(element_id)
                                      ? host_impl_->OuterViewportScrollNode()
                                      : GetNode(element_id);
  if (!scroll_node) {
    return false;
  }
  return scroll_node->user_scrollable_horizontal;
}

bool ScrollElasticityHelperImpl::IsUserScrollableVertical(
    ElementId element_id) const {
  const ScrollNode* scroll_node = IsRoot(element_id)
                                      ? host_impl_->OuterViewportScrollNode()
                                      : GetNode(element_id);
  if (!scroll_node) {
    return false;
  }
  return scroll_node->user_scrollable_vertical;
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
  CHECK(is_root || scroll_node->is_composited || stretch.IsZero());
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

void ScrollElasticityHelperImpl::ForceApplyStretchAmounts() {
  LayerTreeImpl& active_tree = *host_impl_->active_tree();
  PropertyTrees& property_trees = *active_tree.property_trees();
  const ScrollTree& scroll_tree = property_trees.scroll_tree();

  for (const auto& [element_id, stretch_amount] :
       scroll_tree.elastic_overscroll()) {
    if (stretch_amount.IsZero()) {
      continue;
    }

    if (const ScrollNode* scroll_node =
            IsRoot(element_id) ? host_impl_->InnerViewportScrollNode()
                               : GetNode(element_id)) {
      // Marks dirty flags on transform node and tree.
      MarkNodeNeedsUpdate(*host_impl_, *scroll_node, stretch_amount);
    }
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

  // Disable elastic overscroll for scrollers that are not composited, as a
  // scroller must be composited to be able to have an elastic overscroll effect
  // applied to it on the impl thread.
  const gfx::Vector2dF effective_stretch_amount =
      is_root || scroll_node->is_composited ? stretch_amount : gfx::Vector2dF();

  LayerTreeImpl& active_tree = *host_impl_->active_tree();
  PropertyTrees& property_trees = *active_tree.property_trees();
  if (property_trees.scroll_tree_mutable().SetElasticOverscroll(
          *scroll_node, effective_stretch_amount)) {
    // Marks dirty flags on transform node and tree.
    MarkNodeNeedsUpdate(*host_impl_, *scroll_node, effective_stretch_amount);
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
