// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_LAYER_H_
#define CC_SLIM_LAYER_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "cc/slim/filter.h"
#include "cc/slim/frame_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace cc {
class Layer;
}

namespace viz {
class CompositorRenderPass;
class SharedQuadState;
}  // namespace viz

namespace cc::slim {

class LayerTree;
class LayerTreeCcWrapper;
class LayerTreeImpl;

// Base class for composited layers. Special layer types are derived from
// this class. Each layer is an independent unit in the compositor, be that
// for transforming or for content. If a layer has content it can be
// transformed efficiently without requiring the content to be recreated.
// Layers form a tree, with each layer having 0 or more children, and a single
// parent (or none at the root). Layers within the tree, other than the root
// layer, are kept alive by that tree relationship, with refpointer ownership
// from parents to children.
class COMPONENT_EXPORT(CC_SLIM) Layer : public base::RefCounted<Layer> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
  static scoped_refptr<Layer> Create();

  Layer(const Layer&) = delete;
  Layer& operator=(const Layer&) = delete;

  // The list of children of this layer.
  const std::vector<scoped_refptr<Layer>>& children() const {
    return children_;
  }

  // Return the parent if any. Root layer has nullptr parent.
  Layer* parent() const { return parent_; }

  // A unique and stable id for the Layer. Ids are always positive.
  int id() const { return id_; }

  // Returns a pointer to the highest ancestor of this layer, or itself.
  Layer* RootLayer();

  // Internal method called when Layer is attached to a LayerTree.
  // This would happen when
  // a) the Layer is added to an existing Layer tree that is attached to a
  // LayerTreeHost.
  // b) the Layer is made the root layer of a LayerTreeHost.
  // c) the Layer is part of a Layer tree, and an ancestor is attached to a
  // LayerTreeHost via a) or b).
  // The |host| is the new LayerTreeHost which the Layer is now attached to.
  // Subclasses may override this if they have data or resources which are
  // specific to a LayerTreeHost that should be updated or reset. After this
  // returns the Layer will hold a pointer to the new LayerTreeHost.
  virtual void SetLayerTree(LayerTree* layer_tree);
  LayerTree* layer_tree() { return layer_tree_; }

  // Appends `child` to the list of children of this layer, and maintains
  // ownership of a reference to that `child`.
  void AddChild(scoped_refptr<Layer> child);
  // Inserts |child| into the list of children of this layer, before position
  // |index| (0 based) and maintains ownership of a reference to that |child|.
  void InsertChild(scoped_refptr<Layer> child, size_t position);
  // Removes an existing child |reference| from this layer's list of children,
  // and inserts |new_layer| it its place in the list. This layer maintains
  // ownership of a reference to the |new_layer|. The |new_layer| may be null,
  // in which case |reference| is simply removed from the list of children,
  // which ends this layers ownership of the child.
  void ReplaceChild(Layer* old_child, scoped_refptr<Layer> new_child);
  // Removes this layer from the list of children in its parent, removing the
  // parent's ownership of this layer.
  void RemoveFromParent();
  // Removes all children from this layer's list of children, removing ownership
  // of those children.
  void RemoveAllChildren();
  // Returns true if |ancestor| is this layer's parent or higher ancestor.
  bool HasAncestor(Layer* layer) const;

  // Set and get the position of this layer, relative to its parent. This is
  // specified in layer space, which ignores transforms for this layer or
  // ancestor layers. The root layer's position is not used as it always
  // appears at the origin of the viewport.
  void SetPosition(const gfx::PointF& position);
  // TODO(crbug.com/1408128): Return by reference after no longer wrapping cc.
  const gfx::PointF position() const;

  // Set and get the layers bounds. This is specified in layer space, which
  // ignores transforms for this layer or ancestor layers.
  void SetBounds(const gfx::Size& bounds);
  const gfx::Size& bounds() const;

  // Set or get the transform to be used when compositing this layer into its
  // target. The transform is inherited by this layers children.
  // Slim compositor implementation only supports transforms where
  // `Is2dTransform` is true and has CHECK for it. This includes scale,
  // translate, and shear in the x-y plane, as well as rotation about the z
  // axis.
  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const;

  // Set or get the origin to be used when applying the transform. The value is
  // a position in layer space, relative to the top left corner of this layer.
  // For instance, if set to the center of the layer, with a transform to rotate
  // 180deg around the X axis, it would flip the layer vertically around the
  // center of the layer, leaving it occupying the same space. Whereas set to
  // the top left of the layer, the rotation wouldoccur around the top of the
  // layer, moving it vertically while flipping it.
  void SetTransformOrigin(const gfx::Point3F& origin);
  gfx::Point3F transform_origin() const;

  // When true the layer may contribute to the compositor's output. When false,
  // it does not. This property does not apply to children of the layer, they
  // may contribute while this layer does not. The layer itself will determine
  // if it has content to contribute, but when false, this prevents it from
  // doing so.
  void SetIsDrawable(bool drawable);

  // Set and get the background color for the layer. This color is used to
  // calculate the safe opaque background color. Subclasses may also use the
  // color for other purposes.
  virtual void SetBackgroundColor(SkColor4f color);
  SkColor4f background_color() const;

  // Set or get an optimization hint that the contents of this layer are fully
  // opaque or not. If true, every pixel of content inside the layer's bounds
  // must be opaque or visual errors can occur. This applies only to this layer
  // and not to children, and does not imply the layer should be composited
  // opaquely, as effects may be applied such as opacity() or filters().
  void SetContentsOpaque(bool opaque);
  bool contents_opaque() const;

  // Set or get the opacity which should be applied to the contents of the layer
  // and its subtree (together as a single composited entity) when blending them
  // into their target. Note that this does not speak to the contents of this
  // layer, which may be opaque or not (see contents_opaque()). Note that the
  // opacity is cumulative since it applies to the layer's subtree.
  virtual void SetOpacity(float opacity);
  float opacity() const;

  // Is true if the layer will contribute content to the compositor's output.
  // Will be false if SetIsDrawable(false) is called. But will also be false if
  // the layer itself has no content to contribute, even though the layer was
  // given SetIsDrawable(true).
  bool draws_content() const;

  // Returns the number of layers in this layers subtree (excluding itself) for
  // which DrawsContent() is true.
  int NumDescendantsThatDrawContent() const;

  // Set or get if this layer and its subtree should be part of the compositor's
  // output to the screen. When set to true, the layer's subtree does not appear
  // to the user, but still remains part of the tree with all its normal drawing
  // properties.
  void SetHideLayerAndSubtree(bool hide);
  bool hide_layer_and_subtree() const;

  // Set or get that this layer clips its subtree to within its bounds. Content
  // of children will be intersected with the bounds of this layer when true.
  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const;

  // Set or get the list of filter effects to be applied to the contents of the
  // layer and its subtree (together as a single composited entity) when
  // drawing them into their target.
  // Note a layer with filter is more expensive than two layers layers with
  // opacity blending, so always prefer to use additional layers if possible.
  void SetFilters(std::vector<Filter> filters);

 protected:
  friend class LayerTreeCcWrapper;
  friend class LayerTreeImpl;

  explicit Layer(scoped_refptr<cc::Layer> cc_layer);
  virtual ~Layer();

  // Called by LayerTree.
  gfx::Transform ComputeTransformToParent() const;
  absl::optional<gfx::Transform> ComputeTransformFromParent() const;
  bool HasFilters() const;
  // This method counts this layer, This is different from
  // `NumDescendantsThatDrawContent` which counts descendent layers only.
  int GetNumDrawingLayersInSubtree() const;

  void UpdateDrawsContent();
  virtual bool HasDrawableContent() const;

  // `transform_to_target` is the transform from this layer's space to the
  // space of target render pass this is layer is drawn to.
  // `transform_to_root` is similar and transform to the root render pass.
  // They are the same if this layer draws to the root render pass.
  // `opacity` parameter cumulative opacity when drawing this layer.
  // `SetOpacity` applies to the entire subtree, `opacity` parameter contains
  // opacity from parents and may be different from `opacity()` method.
  virtual void AppendQuads(viz::CompositorRenderPass& render_pass,
                           FrameData& data,
                           const gfx::Transform& transform_to_root,
                           const gfx::Transform& transform_to_target,
                           const gfx::Rect* clip_in_target,
                           const gfx::Rect& visible_rect,
                           float opacity);
  virtual viz::SharedQuadState* CreateAndAppendSharedQuadState(
      viz::CompositorRenderPass& render_pass,
      const gfx::Transform& transform_to_target,
      const gfx::Rect* clip_in_target,
      const gfx::Rect& visible_rect,
      float opacity);

  void NotifyTreeChanged();
  void NotifyPropertyChanged();

  const scoped_refptr<cc::Layer> cc_layer_;

 private:
  friend class base::RefCounted<Layer>;

  cc::Layer* cc_layer() const { return cc_layer_.get(); }

  void WillAddChildSlim(Layer* child);
  void InsertChildSlim(scoped_refptr<Layer> child, size_t position);
  void RemoveFromParentSlim();
  void SetParentSlim(Layer* parent);
  void ChangeDrawableDescendantsBySlim(int num);

  const int id_;
  raw_ptr<Layer> parent_ = nullptr;
  std::vector<scoped_refptr<Layer>> children_;

  raw_ptr<LayerTree, DanglingUntriaged> layer_tree_ = nullptr;

  int num_descendants_that_draw_content_ = 0;

  gfx::PointF position_;
  gfx::Size bounds_;
  gfx::Transform transform_;
  gfx::Point3F transform_origin_;

  std::vector<Filter> filters_;

  SkColor4f background_color_ = SkColors::kTransparent;
  float opacity_ = 1.0f;
  bool is_drawable_ : 1;
  bool contents_opaque_ : 1;
  bool draws_content_ : 1;
  bool hide_layer_and_subtree_ : 1;
  bool masks_to_bounds_ : 1;
};

}  // namespace cc::slim

#endif  // CC_SLIM_LAYER_H_
