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

class ScrollElasticityHelperImpl : public ScrollElasticityHelper {
 public:
  explicit ScrollElasticityHelperImpl(LayerTreeHostImpl* host_impl);
  ~ScrollElasticityHelperImpl() override;

  bool IsUserScrollableHorizontal() const override;
  bool IsUserScrollableVertical() const override;
  gfx::Vector2dF StretchAmount() const override;
  gfx::Size ScrollBounds() const override;
  void SetStretchAmount(const gfx::Vector2dF& stretch_amount) override;
  gfx::PointF ScrollOffset() const override;
  gfx::PointF MaxScrollOffset() const override;
  void ScrollBy(const gfx::Vector2dF& delta) override;
  void RequestOneBeginFrame() override;

 private:
  raw_ptr<LayerTreeHostImpl> host_impl_;
};

ScrollElasticityHelperImpl::ScrollElasticityHelperImpl(
    LayerTreeHostImpl* layer_tree)
    : host_impl_(layer_tree) {}

ScrollElasticityHelperImpl::~ScrollElasticityHelperImpl() = default;

bool ScrollElasticityHelperImpl::IsUserScrollableHorizontal() const {
  const auto* scroll_node = host_impl_->OuterViewportScrollNode();
  if (!scroll_node)
    return false;
  return scroll_node->user_scrollable_horizontal;
}

bool ScrollElasticityHelperImpl::IsUserScrollableVertical() const {
  const auto* scroll_node = host_impl_->OuterViewportScrollNode();
  if (!scroll_node)
    return false;
  return scroll_node->user_scrollable_vertical;
}

gfx::Vector2dF ScrollElasticityHelperImpl::StretchAmount() const {
  return host_impl_->active_tree()->elastic_overscroll()->Current(true);
}

gfx::Size ScrollElasticityHelperImpl::ScrollBounds() const {
  return host_impl_->OuterViewportScrollNode()
             ? host_impl_->OuterViewportScrollNode()->container_bounds
             : gfx::Size();
}

void ScrollElasticityHelperImpl::SetStretchAmount(
    const gfx::Vector2dF& stretch_amount) {
  if (stretch_amount == StretchAmount())
    return;

  host_impl_->active_tree()->elastic_overscroll()->SetCurrent(stretch_amount);
  host_impl_->active_tree()->set_needs_update_draw_properties();
  host_impl_->SetNeedsCommit();
  host_impl_->SetNeedsRedraw();
  host_impl_->SetFullViewportDamage();
}

gfx::PointF ScrollElasticityHelperImpl::ScrollOffset() const {
  return host_impl_->active_tree()->TotalScrollOffset();
}

gfx::PointF ScrollElasticityHelperImpl::MaxScrollOffset() const {
  return host_impl_->active_tree()->TotalMaxScrollOffset();
}

void ScrollElasticityHelperImpl::ScrollBy(const gfx::Vector2dF& delta) {
  ScrollNode* root_scroll_node = host_impl_->OuterViewportScrollNode()
                                     ? host_impl_->OuterViewportScrollNode()
                                     : host_impl_->InnerViewportScrollNode();
  if (root_scroll_node) {
    LayerTreeImpl* tree_impl = host_impl_->active_tree();
    tree_impl->property_trees()->scroll_tree_mutable().ScrollBy(
        *root_scroll_node, delta, tree_impl);
  }
}

void ScrollElasticityHelperImpl::RequestOneBeginFrame() {
  host_impl_->SetNeedsOneBeginImplFrame();
}

// static
ScrollElasticityHelper* ScrollElasticityHelper::CreateForLayerTreeHostImpl(
    LayerTreeHostImpl* host_impl) {
  return new ScrollElasticityHelperImpl(host_impl);
}

}  // namespace cc
