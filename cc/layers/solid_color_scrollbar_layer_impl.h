// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_IMPL_H_
#define CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_IMPL_H_

#include <memory>

#include "cc/cc_export.h"
#include "cc/layers/scrollbar_layer_impl_base.h"

namespace cc {

class CC_EXPORT SolidColorScrollbarLayerImpl : public ScrollbarLayerImplBase {
 public:
  static std::unique_ptr<SolidColorScrollbarLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      ScrollbarOrientation orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar);
  ~SolidColorScrollbarLayerImpl() override;

  // LayerImpl overrides.
  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void PushPropertiesTo(LayerImpl* layer) override;

  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  int ThumbThickness() const override;

  void set_color(SkColor4f color) { color_ = color; }

 protected:
  SolidColorScrollbarLayerImpl(LayerTreeImpl* tree_impl,
                               int id,
                               ScrollbarOrientation orientation,
                               int thumb_thickness,
                               int track_start,
                               bool is_left_side_vertical_scrollbar);

  // ScrollbarLayerImplBase implementation.
  int ThumbLength() const override;
  float TrackLength() const override;
  int TrackStart() const override;
  bool IsThumbResizable() const override;

 private:
  int thumb_thickness_;
  int track_start_;
  SkColor4f color_ = SkColors::kTransparent;
};

}  // namespace cc

#endif  // CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_IMPL_H_
