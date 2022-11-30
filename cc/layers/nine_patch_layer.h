// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_NINE_PATCH_LAYER_H_
#define CC_LAYERS_NINE_PATCH_LAYER_H_

#include <memory>

#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class CC_EXPORT NinePatchLayer : public UIResourceLayer {
 public:
  static scoped_refptr<NinePatchLayer> Create();

  NinePatchLayer(const NinePatchLayer&) = delete;
  NinePatchLayer& operator=(const NinePatchLayer&) = delete;

  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override;

  // |border| is the space around the center rectangular region in layer space
  // (known as aperture in image space).  |border.x()| and |border.y()| are the
  // size of the left and top boundary, respectively.
  // |border.width()-border.x()| and |border.height()-border.y()| are the size
  // of the right and bottom boundary, respectively.
  void SetBorder(const gfx::Rect& border);

  // aperture is in the pixel space of the bitmap resource and refers to
  // the center patch of the ninepatch (which is unused in this
  // implementation). We split off eight rects surrounding it and stick them
  // on the edges of the layer. The corners are unscaled, the top and bottom
  // rects are x-stretched to fit, and the left and right rects are
  // y-stretched to fit.
  void SetAperture(const gfx::Rect& aperture);
  void SetFillCenter(bool fill_center);
  void SetNearestNeighbor(bool nearest_neighbor);

  // |rect| is the space completely occluded by another layer in layer
  // space. This can be used for example to occlude the entire window's
  // content when drawing the shadow with a 9 patches layer.
  void SetLayerOcclusion(const gfx::Rect& occlusion);

 private:
  NinePatchLayer();
  ~NinePatchLayer() override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;

  ProtectedSequenceReadable<gfx::Rect> border_;
  ProtectedSequenceReadable<bool> fill_center_;
  ProtectedSequenceReadable<bool> nearest_neighbor_;

  // The transparent center region that shows the parent layer's contents in
  // image space.
  ProtectedSequenceReadable<gfx::Rect> image_aperture_;

  // The occluded region in layer space set by SetLayerOcclusion. It is
  // usually larger than |image_aperture_|.
  ProtectedSequenceReadable<gfx::Rect> layer_occlusion_;
};

}  // namespace cc

#endif  // CC_LAYERS_NINE_PATCH_LAYER_H_
