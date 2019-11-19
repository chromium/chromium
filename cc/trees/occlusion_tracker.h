// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_OCCLUSION_TRACKER_H_
#define CC_TREES_OCCLUSION_TRACKER_H_

#include <vector>

#include "cc/base/simple_enclosed_region.h"
#include "cc/cc_export.h"
#include "cc/layers/effect_tree_layer_list_iterator.h"
#include "cc/trees/occlusion.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class LayerImpl;
class Region;
class RenderSurfaceImpl;

// This class is used to track occlusion of layers while traversing them in a
// front-to-back order. As each layer is visited, one of the methods in this
// class is called to notify it about the current target surface. Then,
// occlusion in the content space of the current layer may be queried, via
// Occlusion from GetCurrentOcclusionForLayer(). If the current layer owns a
// RenderSurfaceImpl, then occlusion on that RenderSurfaceImpl may also be
// queried via surfaceOccluded() and surfaceUnoccludedContentRect(). Finally,
// once finished with the layer, occlusion behind the layer should be marked by
// calling MarkOccludedBehindLayer().
class CC_EXPORT OcclusionTracker {
 public:
  explicit OcclusionTracker(const gfx::Rect& screen_space_clip_rect);
  OcclusionTracker(const OcclusionTracker&) = delete;
  ~OcclusionTracker();

  OcclusionTracker& operator=(const OcclusionTracker&) = delete;

  // Return an occlusion that retains the current state of the tracker
  // and can be used outside of a layer walk to check occlusion.
  Occlusion GetCurrentOcclusionForLayer(
      const gfx::Transform& draw_transform) const;
  Occlusion GetCurrentOcclusionForContributingSurface(
      const gfx::Transform& draw_transform) const;

  const RenderSurfaceImpl* OcclusionSurfaceForContributingSurface() const;
  // Called at the beginning of each step in EffectTreeLayerListIterator's
  // front-to-back traversal.
  void EnterLayer(const EffectTreeLayerListIterator::Position& iterator);
  // Called at the end of each step in EffectTreeLayerListIterator's
  // front-to-back traversal.
  void LeaveLayer(const EffectTreeLayerListIterator::Position& iterator);

  // Gives the region of the screen that is not occluded by something opaque.
  Region ComputeVisibleRegionInScreen(const LayerTreeImpl* layer_tree) const;

  void set_minimum_tracking_size(const gfx::Size& size) {
    minimum_tracking_size_ = size;
  }

 protected:
  struct StackObject {
    StackObject() : target(0) {}
    explicit StackObject(const RenderSurfaceImpl* target) : target(target) {}
    const RenderSurfaceImpl* target;
    SimpleEnclosedRegion occlusion_from_outside_target;
    SimpleEnclosedRegion occlusion_from_inside_target;
  };

  // The stack holds occluded regions for subtrees in the
  // RenderSurfaceImpl-Layer tree, so that when we leave a subtree we may apply
  // a mask to it, but not to the parts outside the subtree.
  // - The first time we see a new subtree under a target, we add that target to
  // the top of the stack. This can happen as a layer representing itself, or as
  // a target surface.
  // - When we visit a target surface, we apply its mask to its subtree, which
  // is at the top of the stack.
  // - When we visit a layer representing itself, we add its occlusion to the
  // current subtree, which is at the top of the stack.
  // - When we visit a layer representing a contributing surface, the current
  // target will never be the top of the stack since we just came from the
  // contributing surface.
  // We merge the occlusion at the top of the stack with the new current
  // subtree. This new target is pushed onto the stack if not already there.
  std::vector<StackObject> stack_;

 private:
  // Called when visiting a layer. If the target was not already current, then
  // this indicates we have entered a new surface subtree.
  void EnterRenderTarget(const RenderSurfaceImpl* new_target_surface);

  // Called when visiting a target surface. This indicates we have visited all
  // the layers within the surface, and we may perform any surface-wide
  // operations.
  void FinishedRenderTarget(const RenderSurfaceImpl* finished_target_surface);

  // Called when visiting a contributing surface. This indicates that we are
  // leaving our current surface, and entering the new one. We then perform any
  // operations required for merging results from the child subtree into its
  // parent.
  void LeaveToRenderTarget(const RenderSurfaceImpl* new_target_surface);

  // Add the layer's occlusion to the tracked state.
  void MarkOccludedBehindLayer(const LayerImpl* layer);

  gfx::Rect screen_space_clip_rect_;
  gfx::Size minimum_tracking_size_;
};

}  // namespace cc

#endif  // CC_TREES_OCCLUSION_TRACKER_H_
