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
#include "cc/paint/filter_operations.h"
#include "cc/slim/layer_tree.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"

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
  cc_layer = cc::Layer::Create();
  return base::AdoptRef(new Layer(std::move(cc_layer)));
}

Layer::Layer(scoped_refptr<cc::Layer> cc_layer)
    : cc_layer_(std::move(cc_layer)), id_(g_next_id.GetNext()) {}

Layer::~Layer() {
  RemoveAllChildren();
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
  WillAddChildSlim(child.get());
  const size_t index = std::min(position, children_.size());
  children_.insert(children_.begin() + index, std::move(child));
}

void Layer::WillAddChildSlim(Layer* child) {
  child->RemoveFromParentSlim();
  child->parent_ = this;
  child->SetLayerTree(layer_tree());
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
  old_child->parent_ = nullptr;
  old_child->SetLayerTree(nullptr);

  if (new_child) {
    WillAddChildSlim(new_child.get());
    *it = std::move(new_child);
  } else {
    children_.erase(it);
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
  parent_ = nullptr;
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
    child->parent_ = nullptr;
  }
  children_.clear();
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

void Layer::SetPosition(const gfx::PointF& position) {
  cc_layer()->SetPosition(position);
}

const gfx::PointF Layer::position() const {
  return cc_layer()->position();
}

void Layer::SetBounds(const gfx::Size& bounds) {
  cc_layer()->SetBounds(bounds);
}

const gfx::Size& Layer::bounds() const {
  return cc_layer()->bounds();
}

void Layer::SetTransform(const gfx::Transform& transform) {
  cc_layer()->SetTransform(transform);
}

const gfx::Transform& Layer::transform() const {
  return cc_layer()->transform();
}

void Layer::SetTransformOrigin(const gfx::Point3F& origin) {
  cc_layer()->SetTransformOrigin(origin);
}

gfx::Point3F Layer::transform_origin() const {
  return cc_layer()->transform_origin();
}

void Layer::SetIsDrawable(bool drawable) {
  cc_layer()->SetIsDrawable(drawable);
}

void Layer::SetBackgroundColor(SkColor4f color) {
  cc_layer()->SetBackgroundColor(color);
}

SkColor4f Layer::background_color() const {
  return cc_layer()->background_color();
}

void Layer::SetContentsOpaque(bool opaque) {
  cc_layer()->SetContentsOpaque(opaque);
}

bool Layer::contents_opaque() const {
  return cc_layer()->contents_opaque();
}

void Layer::SetOpacity(float opacity) {
  cc_layer()->SetOpacity(opacity);
}

float Layer::opacity() const {
  return cc_layer()->opacity();
}

bool Layer::draws_content() const {
  return cc_layer()->draws_content();
}

void Layer::SetDrawsContent(bool value) {
  cc_layer()->SetDrawsContent(value);
}

void Layer::SetHideLayerAndSubtree(bool hide) {
  cc_layer()->SetHideLayerAndSubtree(hide);
}

bool Layer::hide_layer_and_subtree() const {
  return cc_layer()->hide_layer_and_subtree();
}

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  cc_layer()->SetMasksToBounds(masks_to_bounds);
}

bool Layer::masks_to_bounds() const {
  return cc_layer()->masks_to_bounds();
}

void Layer::SetFilters(std::vector<Filter> filters) {
  cc_layer()->SetFilters(ToCcFilters(std::move(filters)));
}

}  // namespace cc::slim
