// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/layer.h"

#include <algorithm>
#include <atomic>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/check.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "cc/paint/filter_operation.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/layer_tree_impl.h"
#include "components/viz/common/quads/shared_quad_state.h"

namespace cc::slim {

namespace {

base::AtomicSequenceNumber g_next_id;

cc::FilterOperations ToCcFilters(std::vector<cc::slim::Filter> filters) {
  cc::FilterOperations cc_filters;
  for (const auto& slim_filter : filters) {
    switch (slim_filter.type()) {
      case cc::slim::Filter::kBrightness:
        cc_filters.Append(
            cc::FilterOperation::CreateBrightnessFilter(slim_filter.amount()));
        break;
      case cc::slim::Filter::kSaturation:
        cc_filters.Append(
            cc::FilterOperation::CreateSaturateFilter(slim_filter.amount()));
        break;
    }
  }
  return cc_filters;
}

bool DescendantLayerHasOffsetTag(const Layer* layer) {
  for (auto& child : layer->children()) {
    if (child->offset_tag() || DescendantLayerHasOffsetTag(child.get())) {
      return true;
    }
  }
  return false;
}

// Verifies that there are no overlapping `OffsetTag`s in the layer tree for all
// descendants and ancestors of `layer`. This only validates the part of the
// layer tree that contains `layer`.
bool VerifyOffsetTagTree(const Layer* layer) {
  bool subtree_has_offset_tag = DescendantLayerHasOffsetTag(layer);
  for (const Layer* target = layer; target; target = target->parent()) {
    if (target->offset_tag() && subtree_has_offset_tag) {
      // The offset tag from this layer applies to the subtree so if this
      // happens there are overlapping tags.
      return false;
    }
    subtree_has_offset_tag = subtree_has_offset_tag || layer->offset_tag();
  }
  return true;
}

}  // namespace

// static
scoped_refptr<Layer> Layer::Create() {
  return base::AdoptRef(new Layer());
}
Layer::Layer() : id_(g_next_id.GetNext() + 1) {}

Layer::~Layer() {
  RemoveAllChildren();
  DCHECK_EQ(num_descendants_that_draw_content_, 0);
}

void Layer::SetLayerTree(LayerTree* layer_tree) {
  if (layer_tree_ == layer_tree) {
    return;
  }

  layer_tree_ = layer_tree;
  for (auto& child : children_) {
    child->SetLayerTree(layer_tree);
  }
}

Layer* Layer::RootLayer() {
  Layer* layer = this;
  while (layer->parent_) {
    layer = layer->parent_;
  }
  return layer;
}

void Layer::AddChild(scoped_refptr<Layer> child) {
  InsertChildSlim(std::move(child), children_.size());
}

void Layer::InsertChild(scoped_refptr<Layer> child, size_t position) {
  InsertChildSlim(std::move(child), position);
}

void Layer::InsertChildSlim(scoped_refptr<Layer> child, size_t position) {
  if (position < children_.size() && children_.at(position) == child) {
    return;
  }
  WillAddChildSlim(child.get());
  const size_t index = std::min(position, children_.size());
  children_.insert(children_.begin() + index, std::move(child));
}

void Layer::WillAddChildSlim(Layer* child) {
  child->RemoveFromParentSlim();
  child->SetParentSlim(this);
  child->SetLayerTree(layer_tree());
  child->NotifySubtreeChanged();
}

void Layer::ReplaceChild(Layer* old_child, scoped_refptr<Layer> new_child) {
  if (old_child->parent_ != this || old_child == new_child.get()) {
    return;
  }

  auto it = base::ranges::find_if(
      children_, [&](auto& ptr) { return ptr.get() == old_child; });
  CHECK(it != children_.end(), base::NotFatalUntil::M130);
  old_child->SetParentSlim(nullptr);
  old_child->SetLayerTree(nullptr);

  if (new_child) {
    WillAddChildSlim(new_child.get());
    *it = std::move(new_child);
  } else {
    children_.erase(it);
    NotifyPropertyChanged();
  }
}

void Layer::RemoveFromParent() {
  RemoveFromParentSlim();
}

void Layer::RemoveFromParentSlim() {
  if (!parent_) {
    return;
  }

  SetLayerTree(nullptr);
  std::erase_if(parent_->children_,
                [&](auto& ptr) { return ptr.get() == this; });
  parent_->NotifyPropertyChanged();
  SetParentSlim(nullptr);
}

void Layer::RemoveAllChildren() {
  if (children_.empty()) {
    return;
  }

  for (auto& child : children_) {
    child->SetLayerTree(nullptr);
    child->SetParentSlim(nullptr);
  }
  children_.clear();
  NotifySubtreeChanged();
}

bool Layer::HasAncestor(Layer* layer) const {
  for (Layer* ancestor = parent_; ancestor; ancestor = ancestor->parent_) {
    if (ancestor == layer) {
      return true;
    }
  }
  return false;
}

void Layer::SetParentSlim(Layer* parent) {
  if (parent_ == parent) {
    return;
  }
  int drawing_layers_in_subtree = GetNumDrawingLayersInSubtree();
  if (parent_) {
    parent_->ChangeDrawableDescendantsBySlim(0 - drawing_layers_in_subtree);
  }
  parent_ = parent;
  if (parent_) {
    parent_->ChangeDrawableDescendantsBySlim(drawing_layers_in_subtree);
  }
  DCHECK(VerifyOffsetTagTree(this));
}

void Layer::ChangeDrawableDescendantsBySlim(int num) {
  DCHECK_GE(num_descendants_that_draw_content_ + num, 0);
  if (!num) {
    return;
  }
  num_descendants_that_draw_content_ += num;
  if (parent_) {
    parent_->ChangeDrawableDescendantsBySlim(num);
  }
}

void Layer::SetPosition(const gfx::PointF& position) {
  if (position_ == position) {
    return;
  }
  position_ = position;
  NotifySubtreeChanged();
}

void Layer::SetBounds(const gfx::Size& bounds) {
  if (bounds_ == bounds) {
    return;
  }
  bounds_ = bounds;
  if (masks_to_bounds_) {
    NotifySubtreeChanged();
  } else {
    NotifyPropertyChanged();
  }
}

void Layer::SetTransform(const gfx::Transform& transform) {
  CHECK(transform.Is2dTransform());
  if (transform_ == transform) {
    return;
  }
  transform_ = transform;
  NotifySubtreeChanged();
}

void Layer::SetTransformOrigin(const gfx::PointF& origin) {
  if (transform_origin_ == origin) {
    return;
  }
  transform_origin_ = origin;
  NotifySubtreeChanged();
}

void Layer::SetIsDrawable(bool drawable) {
  if (is_drawable_ == drawable) {
    return;
  }
  is_drawable_ = drawable;
  UpdateDrawsContent();
}

void Layer::SetBackgroundColor(SkColor4f color) {
  if (background_color_ == color) {
    return;
  }
  background_color_ = color;
  NotifyPropertyChanged();
}

void Layer::SetContentsOpaque(bool opaque) {
  if (contents_opaque_ == opaque) {
    return;
  }
  contents_opaque_ = opaque;
  NotifySubtreeChanged();
}

void Layer::SetOpacity(float opacity) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  if (opacity_ == opacity) {
    return;
  }
  opacity_ = opacity;
  NotifySubtreeChanged();
}

void Layer::SetOffsetTag(const viz::OffsetTag& offset_tag) {
  if (offset_tag_ == offset_tag) {
    return;
  }
  offset_tag_ = offset_tag;
  DCHECK(VerifyOffsetTagTree(this));
  NotifySubtreeChanged();
}

int Layer::NumDescendantsThatDrawContent() const {
  return num_descendants_that_draw_content_;
}

void Layer::UpdateDrawsContent() {
  bool value = HasDrawableContent();
  if (draws_content_ == value) {
    return;
  }
  draws_content_ = value;
  if (parent_) {
    parent_->ChangeDrawableDescendantsBySlim(value ? 1 : -1);
  }
  NotifyPropertyChanged();
}

void Layer::SetHideLayerAndSubtree(bool hide) {
  if (hide_layer_and_subtree_ == hide) {
    return;
  }
  hide_layer_and_subtree_ = hide;
  NotifySubtreeChanged();
}

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  if (masks_to_bounds_ == masks_to_bounds) {
    return;
  }
  masks_to_bounds_ = masks_to_bounds;
  NotifySubtreeChanged();
}

void Layer::SetRoundedCorner(const gfx::RoundedCornersF& corner_radii) {
  if (rounded_corners_ == corner_radii) {
    return;
  }
  rounded_corners_ = corner_radii;
  NotifySubtreeChanged();
}

void Layer::SetGradientMask(const gfx::LinearGradient& gradient_mask) {
  if (gradient_mask_ == gradient_mask) {
    return;
  }
  gradient_mask_ = gradient_mask;
  NotifySubtreeChanged();
}

bool Layer::HasNonTrivialMaskFilterInfo() const {
  return !rounded_corners_.IsEmpty() || !gradient_mask_.IsEmpty();
}

void Layer::SetFilters(std::vector<Filter> filters) {
  if (filters_ == filters) {
    return;
  }
  filters_ = std::move(filters);
  NotifySubtreeChanged();
}

bool Layer::HasDrawableContent() const {
  return is_drawable_;
}

gfx::Transform Layer::ComputeTransformToParent() const {
  // Layer transform is:
  // position x transform_origin x transform x -transform_origin
  gfx::Transform transform =
      gfx::Transform::MakeTranslation(position_.x(), position_.y());
  transform.Translate(transform_origin_.x(), transform_origin_.y());
  transform.PreConcat(transform_);
  transform.Translate(-transform_origin_.x(), -transform_origin_.y());
  return transform;
}

std::optional<gfx::Transform> Layer::ComputeTransformFromParent() const {
  // TODO(crbug.com/40888305): Consider caching this result since GetInverse
  // may be expensive.
  gfx::Transform inverse_transform;
  if (!transform_.GetInverse(&inverse_transform)) {
    return std::nullopt;
  }
  // TransformFromParent is:
  // transform_origin x inverse_transform x -transform_origin x -position
  gfx::Transform from_parent;
  from_parent.Translate(transform_origin_.x(), transform_origin_.y());
  from_parent.PreConcat(inverse_transform);
  from_parent.Translate(-transform_origin_.x(), -transform_origin_.y());
  from_parent.Translate(-position_.x(), -position_.y());
  return from_parent;
}

bool Layer::HasFilters() const {
  return !filters_.empty();
}

cc::FilterOperations Layer::GetFilters() const {
  return ToCcFilters(filters_);
}

int Layer::GetNumDrawingLayersInSubtree() const {
  return num_descendants_that_draw_content_ + (draws_content_ ? 1 : 0);
}

bool Layer::GetAndResetPropertyChanged() {
  bool changed = property_changed_;
  property_changed_ = false;
  return changed;
}

bool Layer::GetAndResetSubtreePropertyChanged() {
  bool changed = subtree_property_changed_;
  subtree_property_changed_ = false;
  return changed;
}

void Layer::AppendQuads(viz::CompositorRenderPass& render_pass,
                        FrameData& data,
                        const gfx::Transform& transform_to_root,
                        const gfx::Transform& transform_to_target,
                        const gfx::Rect* clip_in_target,
                        const gfx::Rect& visible_rect,
                        float opacity) {}

viz::SharedQuadState* Layer::CreateAndAppendSharedQuadState(
    viz::CompositorRenderPass& render_pass,
    FrameData& data,
    const gfx::Transform& transform_to_target,
    const gfx::Rect* clip_in_target,
    const gfx::Rect& visible_rect,
    float opacity) {
  viz::SharedQuadState* quad_state =
      render_pass.CreateAndAppendSharedQuadState();
  const gfx::Rect layer_rect{bounds()};
  DCHECK(layer_rect.Contains(visible_rect));
  std::optional<gfx::Rect> clip_opt;
  if (clip_in_target) {
    clip_opt = *clip_in_target;
  }
  quad_state->SetAll(transform_to_target, layer_rect, visible_rect,
                     data.mask_filter_info_in_target, clip_opt,
                     contents_opaque(), opacity, SkBlendMode::kSrcOver,
                     /*sorting_context=*/0,
                     /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  quad_state->is_fast_rounded_corner = true;
  quad_state->offset_tag = data.offset_tag;
  return quad_state;
}

void Layer::NotifySubtreeChanged() {
  subtree_property_changed_ = true;
  if (layer_tree_) {
    static_cast<LayerTreeImpl*>(layer_tree_)->NotifyTreeChanged();
  }
}

void Layer::NotifyPropertyChanged() {
  property_changed_ = true;
  if (layer_tree_) {
    static_cast<LayerTreeImpl*>(layer_tree_)->NotifyTreeChanged();
  }
}

}  // namespace cc::slim
