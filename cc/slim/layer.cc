// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/layer.h"

#include <algorithm>
#include <atomic>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/check.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/ranges/algorithm.h"
#include "cc/layers/layer.h"
#include "cc/paint/filter_operation.h"
#include "cc/slim/features.h"
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

}  // namespace

// static
scoped_refptr<Layer> Layer::Create() {
  scoped_refptr<cc::Layer> cc_layer;
  if (!features::IsSlimCompositorEnabled()) {
    cc_layer = cc::Layer::Create();
  }
  return base::AdoptRef(new Layer(std::move(cc_layer)));
}

Layer::Layer(scoped_refptr<cc::Layer> cc_layer)
    : cc_layer_(std::move(cc_layer)),
      id_(g_next_id.GetNext() + 1),
      is_drawable_(false),
      contents_opaque_(false),
      draws_content_(false),
      hide_layer_and_subtree_(false),
      masks_to_bounds_(false),
      property_changed_(false),
      subtree_property_changed_(false) {}

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
  if (cc_layer()) {
    cc_layer()->AddChild(child->cc_layer());
  }
  InsertChildSlim(std::move(child), children_.size());
}

void Layer::InsertChild(scoped_refptr<Layer> child, size_t position) {
  if (cc_layer()) {
    cc_layer()->InsertChild(child->cc_layer(), position);
  }
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
  if (cc_layer()) {
    cc_layer()->ReplaceChild(old_child->cc_layer(),
                             new_child ? new_child->cc_layer() : nullptr);
  }
  if (old_child->parent_ != this || old_child == new_child.get()) {
    return;
  }

  auto it = base::ranges::find_if(
      children_, [&](auto& ptr) { return ptr.get() == old_child; });
  DCHECK(it != children_.end());
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
  if (cc_layer()) {
    cc_layer()->RemoveFromParent();
  }
  RemoveFromParentSlim();
}

void Layer::RemoveFromParentSlim() {
  if (!parent_) {
    return;
  }

  SetLayerTree(nullptr);
  base::EraseIf(parent_->children_,
                [&](auto& ptr) { return ptr.get() == this; });
  parent_->NotifyPropertyChanged();
  SetParentSlim(nullptr);
}

void Layer::RemoveAllChildren() {
  if (cc_layer()) {
    cc_layer()->RemoveAllChildren();
  }

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
      DCHECK(!cc_layer() || cc_layer()->HasAncestor(layer->cc_layer()));
      return true;
    }
  }
  DCHECK(!cc_layer() || !cc_layer()->HasAncestor(layer->cc_layer()));
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
  if (cc_layer()) {
    cc_layer()->SetPosition(position);
    return;
  }
  if (position_ == position) {
    return;
  }
  position_ = position;
  NotifySubtreeChanged();
}

const gfx::PointF Layer::position() const {
  return cc_layer() ? cc_layer()->position() : position_;
}

void Layer::SetBounds(const gfx::Size& bounds) {
  if (cc_layer()) {
    cc_layer()->SetBounds(bounds);
    return;
  }
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

const gfx::Size& Layer::bounds() const {
  return cc_layer() ? cc_layer()->bounds() : bounds_;
}

void Layer::SetTransform(const gfx::Transform& transform) {
  if (cc_layer()) {
    cc_layer()->SetTransform(transform);
    return;
  }
  CHECK(transform.Is2dTransform());
  if (transform_ == transform) {
    return;
  }
  transform_ = transform;
  NotifySubtreeChanged();
}

const gfx::Transform& Layer::transform() const {
  return cc_layer() ? cc_layer()->transform() : transform_;
}

void Layer::SetTransformOrigin(const gfx::Point3F& origin) {
  if (cc_layer()) {
    cc_layer()->SetTransformOrigin(origin);
    return;
  }
  if (transform_origin_ == origin) {
    return;
  }
  transform_origin_ = origin;
  NotifySubtreeChanged();
}

gfx::Point3F Layer::transform_origin() const {
  return cc_layer() ? cc_layer()->transform_origin() : transform_origin_;
}

void Layer::SetIsDrawable(bool drawable) {
  if (cc_layer()) {
    cc_layer()->SetIsDrawable(drawable);
    return;
  }
  if (is_drawable_ == drawable) {
    return;
  }
  is_drawable_ = drawable;
  UpdateDrawsContent();
}

void Layer::SetBackgroundColor(SkColor4f color) {
  if (cc_layer()) {
    cc_layer()->SetBackgroundColor(color);
    return;
  }
  if (background_color_ == color) {
    return;
  }
  background_color_ = color;
  NotifyPropertyChanged();
}

SkColor4f Layer::background_color() const {
  return cc_layer() ? cc_layer()->background_color() : background_color_;
}

void Layer::SetContentsOpaque(bool opaque) {
  if (cc_layer()) {
    cc_layer()->SetContentsOpaque(opaque);
    return;
  }
  if (contents_opaque_ == opaque) {
    return;
  }
  contents_opaque_ = opaque;
  NotifySubtreeChanged();
}

bool Layer::contents_opaque() const {
  return cc_layer() ? cc_layer()->contents_opaque() : contents_opaque_;
}

void Layer::SetOpacity(float opacity) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  if (cc_layer()) {
    cc_layer()->SetOpacity(opacity);
    return;
  }
  if (opacity_ == opacity) {
    return;
  }
  opacity_ = opacity;
  NotifySubtreeChanged();
}

float Layer::opacity() const {
  return cc_layer() ? cc_layer()->opacity() : opacity_;
}

bool Layer::draws_content() const {
  return cc_layer() ? cc_layer()->draws_content() : draws_content_;
}

int Layer::NumDescendantsThatDrawContent() const {
  if (cc_layer()) {
    return cc_layer()->NumDescendantsThatDrawContent();
  }
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
  if (cc_layer()) {
    cc_layer()->SetHideLayerAndSubtree(hide);
    return;
  }
  if (hide_layer_and_subtree_ == hide) {
    return;
  }
  hide_layer_and_subtree_ = hide;
  NotifySubtreeChanged();
}

bool Layer::hide_layer_and_subtree() const {
  return cc_layer() ? cc_layer()->hide_layer_and_subtree()
                    : hide_layer_and_subtree_;
}

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  if (cc_layer()) {
    cc_layer()->SetMasksToBounds(masks_to_bounds);
    return;
  }
  if (masks_to_bounds_ == masks_to_bounds) {
    return;
  }
  masks_to_bounds_ = masks_to_bounds;
  NotifySubtreeChanged();
}

void Layer::SetRoundedCorner(const gfx::RoundedCornersF& corner_radii) {
  if (cc_layer()) {
    cc_layer()->SetRoundedCorner(corner_radii);
    return;
  }
  if (rounded_corners_ == corner_radii) {
    return;
  }
  rounded_corners_ = corner_radii;
  NotifySubtreeChanged();
}

const gfx::RoundedCornersF& Layer::corner_radii() const {
  return cc_layer() ? cc_layer()->corner_radii() : rounded_corners_;
}

bool Layer::HasRoundedCorner() const {
  return cc_layer() ? cc_layer()->HasRoundedCorner()
                    : !rounded_corners_.IsEmpty();
}

bool Layer::masks_to_bounds() const {
  return cc_layer() ? cc_layer()->masks_to_bounds() : masks_to_bounds_;
}

void Layer::SetFilters(std::vector<Filter> filters) {
  if (cc_layer()) {
    cc_layer()->SetFilters(ToCcFilters(std::move(filters)));
    return;
  }
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
  transform.Translate3d(transform_origin_.x(), transform_origin_.y(),
                        transform_origin_.z());
  transform.PreConcat(transform_);
  transform.Translate3d(-transform_origin_.x(), -transform_origin_.y(),
                        -transform_origin_.z());
  return transform;
}

absl::optional<gfx::Transform> Layer::ComputeTransformFromParent() const {
  // TODO(crbug.com/1408128): Consider caching this result since GetInverse
  // may be expensive.
  gfx::Transform inverse_transform;
  if (!transform_.GetInverse(&inverse_transform)) {
    return absl::nullopt;
  }
  // TransformFromParent is:
  // transform_origin x inverse_transform x -transform_origin x -position
  gfx::Transform from_parent;
  from_parent.Translate3d(transform_origin_.x(), transform_origin_.y(),
                          transform_origin_.z());
  from_parent.PreConcat(inverse_transform);
  from_parent.Translate3d(-transform_origin_.x(), -transform_origin_.y(),
                          -transform_origin_.z());
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
  absl::optional<gfx::Rect> clip_opt;
  if (clip_in_target) {
    clip_opt = *clip_in_target;
  }
  quad_state->SetAll(transform_to_target, layer_rect, visible_rect,
                     data.mask_filter_info_in_target, clip_opt,
                     contents_opaque(), opacity, SkBlendMode::kSrcOver, 0);
  quad_state->is_fast_rounded_corner = true;
  return quad_state;
}

void Layer::NotifySubtreeChanged() {
  if (cc_layer()) {
    return;
  }
  subtree_property_changed_ = true;
  if (layer_tree_) {
    static_cast<LayerTreeImpl*>(layer_tree_)->NotifyTreeChanged();
  }
}

void Layer::NotifyPropertyChanged() {
  if (cc_layer()) {
    return;
  }
  property_changed_ = true;
  if (layer_tree_) {
    static_cast<LayerTreeImpl*>(layer_tree_)->NotifyTreeChanged();
  }
}

}  // namespace cc::slim
