// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer_impl_base.h"

#include <algorithm>

#include "base/numerics/ranges.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

ScrollbarLayerImplBase::ScrollbarLayerImplBase(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation,
    bool is_left_side_vertical_scrollbar,
    bool is_overlay)
    : LayerImpl(tree_impl, id),
      is_overlay_scrollbar_(is_overlay),
      thumb_thickness_scale_factor_(1.f),
      current_pos_(0.f),
      clip_layer_length_(0.f),
      scroll_layer_length_(0.f),
      orientation_(orientation),
      is_left_side_vertical_scrollbar_(is_left_side_vertical_scrollbar),
      vertical_adjust_(0.f) {
  set_is_scrollbar(true);
}

ScrollbarLayerImplBase::~ScrollbarLayerImplBase() {
  layer_tree_impl()->UnregisterScrollbar(this);
}

void ScrollbarLayerImplBase::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  DCHECK(layer->ToScrollbarLayer());
  layer->ToScrollbarLayer()->set_is_overlay_scrollbar(is_overlay_scrollbar_);
  layer->ToScrollbarLayer()->SetScrollElementId(scroll_element_id());
}

ScrollbarLayerImplBase* ScrollbarLayerImplBase::ToScrollbarLayer() {
  return this;
}

void ScrollbarLayerImplBase::SetScrollElementId(ElementId scroll_element_id) {
  if (scroll_element_id_ == scroll_element_id)
    return;

  layer_tree_impl()->UnregisterScrollbar(this);
  scroll_element_id_ = scroll_element_id;
  layer_tree_impl()->RegisterScrollbar(this);
}

bool ScrollbarLayerImplBase::SetCurrentPos(float current_pos) {
  if (current_pos_ == current_pos)
    return false;
  current_pos_ = current_pos;
  NoteLayerPropertyChanged();
  return true;
}

float ScrollbarLayerImplBase::current_pos() const {
  DCHECK(!layer_tree_impl()->ScrollbarGeometriesNeedUpdate());
  return current_pos_;
}

float ScrollbarLayerImplBase::clip_layer_length() const {
  DCHECK(!layer_tree_impl()->ScrollbarGeometriesNeedUpdate());
  return clip_layer_length_;
}

float ScrollbarLayerImplBase::scroll_layer_length() const {
  DCHECK(!layer_tree_impl()->ScrollbarGeometriesNeedUpdate());
  return scroll_layer_length_;
}

float ScrollbarLayerImplBase::vertical_adjust() const {
  DCHECK(!layer_tree_impl()->ScrollbarGeometriesNeedUpdate());
  return vertical_adjust_;
}

bool ScrollbarLayerImplBase::CanScrollOrientation() const {
  PropertyTrees* property_trees = layer_tree_impl()->property_trees();
  const auto* scroll_node =
      property_trees->scroll_tree.FindNodeFromElementId(scroll_element_id_);
  DCHECK(scroll_node);
  // TODO(bokan): Looks like we sometimes get here without a ScrollNode. It
  // should be safe to just return false here (we don't use scroll_element_id_
  // anywhere else) so we can merge the fix. Once merged, will investigate the
  // underlying cause. https://crbug.com/924068.
  if (!scroll_node)
    return false;

  if (orientation() == ScrollbarOrientation::HORIZONTAL) {
    if (!scroll_node->user_scrollable_horizontal)
      return false;
  } else {
    if (!scroll_node->user_scrollable_vertical)
      return false;
  }

  // Ensure the clip_layer_length and scroll_layer_length values are up-to-date.
  // TODO(pdr): Instead of using the clip and scroll layer lengths which require
  // an update, refactor to use the scroll tree (ScrollTree::MaxScrollOffset
  // as in LayerTreeHostImpl::TryScroll).
  layer_tree_impl()->UpdateScrollbarGeometries();

  // Ensure clip_layer_length is smaller than scroll_layer_length, not including
  // small deltas due to floating point error.
  return !MathUtil::IsFloatNearlyTheSame(clip_layer_length(),
                                         scroll_layer_length()) &&
         clip_layer_length() < scroll_layer_length();
}

void ScrollbarLayerImplBase::SetVerticalAdjust(float vertical_adjust) {
  if (vertical_adjust_ == vertical_adjust)
    return;
  vertical_adjust_ = vertical_adjust;
  NoteLayerPropertyChanged();
}

void ScrollbarLayerImplBase::SetClipLayerLength(float clip_layer_length) {
  if (clip_layer_length_ == clip_layer_length)
    return;
  clip_layer_length_ = clip_layer_length;
  NoteLayerPropertyChanged();
}

void ScrollbarLayerImplBase::SetScrollLayerLength(float scroll_layer_length) {
  if (scroll_layer_length_ == scroll_layer_length)
    return;
  scroll_layer_length_ = scroll_layer_length;
  NoteLayerPropertyChanged();
  return;
}

void ScrollbarLayerImplBase::SetThumbThicknessScaleFactor(float factor) {
  if (thumb_thickness_scale_factor_ == factor)
    return;
  thumb_thickness_scale_factor_ = factor;
  NoteLayerPropertyChanged();
}

gfx::Rect ScrollbarLayerImplBase::ComputeThumbQuadRectWithThumbThicknessScale(
    float thumb_thickness_scale_factor) const {
  // Thumb extent is the length of the thumb in the scrolling direction, thumb
  // thickness is in the perpendicular direction. Here's an example of a
  // horizontal scrollbar - inputs are above the scrollbar, computed values
  // below:
  //
  //    |<------------------- track_length_ ------------------->|
  //
  // |--| <-- start_offset
  //
  // +--+----------------------------+------------------+-------+--+
  // |<||                            |##################|       ||>|
  // +--+----------------------------+------------------+-------+--+
  //
  //                                 |<- thumb_length ->|
  //
  // |<------- thumb_offset -------->|
  //
  // For painted, scrollbars, the length is fixed. For solid color scrollbars we
  // have to compute it. The ratio of the thumb's length to the track's length
  // is the same as that of the visible viewport to the total viewport, unless
  // that would make the thumb's length less than its thickness.
  //
  // vertical_adjust_ is used when the layer geometry from the main thread is
  // not in sync with what the user sees. For instance on Android scrolling the
  // top bar controls out of view reveals more of the page content. We want the
  // root layer scrollbars to reflect what the user sees even if we haven't
  // received new layer geometry from the main thread.  If the user has scrolled
  // down by 50px and the initial viewport size was 950px the geometry would
  // look something like this:
  //
  // vertical_adjust_ = 50, scroll position 0, visible ratios 99%
  // Layer geometry:             Desired thumb positions:
  // +--------------------+-+   +----------------------+   <-- 0px
  // |                    |v|   |                     #|
  // |                    |e|   |                     #|
  // |                    |r|   |                     #|
  // |                    |t|   |                     #|
  // |                    |i|   |                     #|
  // |                    |c|   |                     #|
  // |                    |a|   |                     #|
  // |                    |l|   |                     #|
  // |                    | |   |                     #|
  // |                    |l|   |                     #|
  // |                    |a|   |                     #|
  // |                    |y|   |                     #|
  // |                    |e|   |                     #|
  // |                    |r|   |                     #|
  // +--------------------+-+   |                     #|
  // | horizontal  layer  | |   |                     #|
  // +--------------------+-+   |                     #|  <-- 950px
  // |                      |   |                     #|
  // |                      |   |##################### |
  // +----------------------+   +----------------------+  <-- 1000px
  //
  // The layer geometry is set up for a 950px tall viewport, but the user can
  // actually see down to 1000px. Thus we have to move the quad for the
  // horizontal scrollbar down by the vertical_adjust_ factor and lay the
  // vertical thumb out on a track lengthed by the vertical_adjust_ factor. This
  // means the quads may extend outside the layer's bounds.

  // With the length known, we can compute the thumb's position.
  float track_length = TrackLength();
  int thumb_length = ThumbLength();
  int thumb_thickness = ThumbThickness();
  float maximum = scroll_layer_length() - clip_layer_length();

  // With the length known, we can compute the thumb's position.
  float clamped_current_pos = base::ClampToRange(current_pos(), 0.0f, maximum);

  int thumb_offset = TrackStart();
  if (maximum > 0) {
    float ratio = clamped_current_pos / maximum;
    float max_offset = track_length - thumb_length;
    thumb_offset += static_cast<int>(ratio * max_offset);
  }

  float thumb_thickness_adjustment =
      thumb_thickness * (1.f - thumb_thickness_scale_factor);

  gfx::RectF thumb_rect;
  if (orientation_ == HORIZONTAL) {
    thumb_rect = gfx::RectF(thumb_offset,
                            vertical_adjust_ + thumb_thickness_adjustment,
                            thumb_length,
                            thumb_thickness - thumb_thickness_adjustment);
  } else {
    thumb_rect = gfx::RectF(
        is_left_side_vertical_scrollbar_
            ? bounds().width() - thumb_thickness
            : thumb_thickness_adjustment,
        thumb_offset,
        thumb_thickness - thumb_thickness_adjustment,
        thumb_length);
  }

  return gfx::ToEnclosingRect(thumb_rect);
}

gfx::Rect ScrollbarLayerImplBase::ComputeExpandedThumbQuadRect() const {
  DCHECK(is_overlay_scrollbar());
  return ComputeThumbQuadRectWithThumbThicknessScale(1.f);
}

gfx::Rect ScrollbarLayerImplBase::ComputeThumbQuadRect() const {
  return ComputeThumbQuadRectWithThumbThicknessScale(
      thumb_thickness_scale_factor_);
}

void ScrollbarLayerImplBase::SetOverlayScrollbarLayerOpacityAnimated(
    float opacity) {
  DCHECK(is_overlay_scrollbar());
  if (!layer_tree_impl())
    return;

  PropertyTrees* property_trees = layer_tree_impl()->property_trees();

  EffectNode* node = property_trees->effect_tree.Node(effect_tree_index());
  if (node->opacity == opacity)
    return;

  node->opacity = opacity;
  node->effect_changed = true;
  property_trees->changed = true;
  property_trees->effect_tree.set_needs_update(true);
  layer_tree_impl()->set_needs_update_draw_properties();
}

LayerTreeSettings::ScrollbarAnimator
ScrollbarLayerImplBase::GetScrollbarAnimator() const {
  return layer_tree_impl()->settings().scrollbar_animator;
}

bool ScrollbarLayerImplBase::HasFindInPageTickmarks() const {
  return false;
}

bool ScrollbarLayerImplBase::SupportsDragSnapBack() const {
  return false;
}

gfx::Rect ScrollbarLayerImplBase::BackButtonRect() const {
  return gfx::Rect(0, 0);
}

gfx::Rect ScrollbarLayerImplBase::ForwardButtonRect() const {
  return gfx::Rect(0, 0);
}

gfx::Rect ScrollbarLayerImplBase::BackTrackRect() const {
  return gfx::Rect(0, 0);
}

gfx::Rect ScrollbarLayerImplBase::ForwardTrackRect() const {
  return gfx::Rect(0, 0);
}

// This manages identifying which part of a composited scrollbar got hit based
// on the position_in_widget.
ScrollbarPart ScrollbarLayerImplBase::IdentifyScrollbarPart(
    const gfx::PointF position_in_widget) const {
  const gfx::Point pointer_location(position_in_widget.x(),
                                    position_in_widget.y());
  if (BackButtonRect().Contains(pointer_location))
    return ScrollbarPart::BACK_BUTTON;

  if (ForwardButtonRect().Contains(pointer_location))
    return ScrollbarPart::FORWARD_BUTTON;

  if (ComputeThumbQuadRect().Contains(pointer_location))
    return ScrollbarPart::THUMB;

  if (BackTrackRect().Contains(pointer_location))
    return ScrollbarPart::BACK_TRACK;

  if (ForwardTrackRect().Contains(pointer_location))
    return ScrollbarPart::FORWARD_TRACK;

  // TODO(arakeri): Once crbug.com/952314 is fixed, add a DCHECK to verify that
  // the point that is passed in is within the TrackRect. Also, please note that
  // hit testing other scrollbar parts is not yet implemented.
  return ScrollbarPart::NO_PART;
}

}  // namespace cc
