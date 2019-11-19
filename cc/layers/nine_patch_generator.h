// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_NINE_PATCH_GENERATOR_H_
#define CC_LAYERS_NINE_PATCH_GENERATOR_H_

#include <string>
#include <vector>

#include "cc/cc_export.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class DictionaryValue;
}

namespace viz {
class RenderPass;
class SharedQuadState;
}  // namespace viz

namespace cc {
class LayerImpl;

class CC_EXPORT NinePatchGenerator {
 public:
  class Patch {
   public:
    Patch(const gfx::RectF& image_rect,
          const gfx::Size& total_image_bounds,
          const gfx::RectF& output_rect);

    gfx::RectF image_rect;
    gfx::RectF normalized_image_rect;
    gfx::RectF output_rect;
  };

  NinePatchGenerator();

  // The bitmap stretches out the bounds of the layer.  The following picture
  // illustrates the parameters associated with the dimensions.
  //
  // Layer space layout
  //
  // --------------------------------
  // |         :    :               |
  // |         J    C               |
  // |         :    :               |
  // |      ------------------      |
  // |      |       :        |      |
  // |~~~I~~|  ------------  |      |
  // |      |  |          |  |      |
  // |      |  |          |  |      |
  // |~~~A~~|~~|          |~~|~B~~~~|
  // |      |  |          |  |      |
  // |      L  ------------  |      |
  // |      |       :        |      |
  // |      ---K--------------      |
  // |              D               |
  // |              :               |
  // |              :               |
  // --------------------------------
  //
  // Bitmap space layout
  //
  // ~~~~~~~~~~ W ~~~~~~~~~~
  // :     :                |
  // :     Y                |
  // :     :                |
  // :~~X~~------------     |
  // :     |          :     |
  // :     |          :     |
  // H     |          Q     |
  // :     |          :     |
  // :     ~~~~~P~~~~~      |
  // :                      |
  // :                      |
  // :                      |
  // ------------------------
  //
  // |image_bounds| = (W, H)
  // |image_aperture| = (X, Y, P, Q)
  // |border| = (A, C, A + B, C + D)
  // |occlusion_rectangle| = (I, J, K, L)
  // |fill_center| indicates whether to draw the center quad or not.
  bool SetLayout(const gfx::Size& image_bounds,
                 const gfx::Size& output_bounds,
                 const gfx::Rect& image_aperture,
                 const gfx::Rect& border,
                 const gfx::Rect& output_occlusion,
                 bool fill_center,
                 bool nearest_neighbor);

  std::vector<Patch> GeneratePatches() const;

  void AppendQuads(LayerImpl* layer_impl,
                   UIResourceId ui_resource_id,
                   viz::RenderPass* render_pass,
                   viz::SharedQuadState* shared_quad_state,
                   const std::vector<Patch>& patches);

  void AsJson(base::DictionaryValue* dictionary) const;
  void CheckGeometryLimitations();

 private:
  std::vector<Patch> ComputeQuadsWithOcclusion() const;
  std::vector<Patch> ComputeQuadsWithoutOcclusion() const;

  // The center patch in image space.
  gfx::Rect image_aperture_;

  // An inset border that the patches will be mapped to.
  gfx::Rect border_;

  gfx::Size image_bounds_;
  gfx::Size output_bounds_;

  bool fill_center_;
  bool nearest_neighbor_;

  gfx::Rect output_occlusion_;
};

}  // namespace cc

#endif  // CC_LAYERS_NINE_PATCH_GENERATOR_H_
