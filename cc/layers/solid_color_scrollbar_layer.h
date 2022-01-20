// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_H_
#define CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/layers/scrollbar_layer_base.h"

namespace cc {

// A solid color scrollbar that can be fully drawn on the impl thread. In
// practice, this is used for overlay scrollbars on Android.
class CC_EXPORT SolidColorScrollbarLayer : public ScrollbarLayerBase {
 public:
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  static scoped_refptr<SolidColorScrollbarLayer> CreateOrReuse(
      scoped_refptr<Scrollbar>,
      SolidColorScrollbarLayer* existing_layer);

  static scoped_refptr<SolidColorScrollbarLayer> Create(
      ScrollbarOrientation orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar);

  SolidColorScrollbarLayer(const SolidColorScrollbarLayer&) = delete;
  SolidColorScrollbarLayer& operator=(const SolidColorScrollbarLayer&) = delete;

  // Layer overrides.
  bool OpacityCanAnimateOnImplThread() const override;
  void SetOpacity(float opacity) override;
  void SetNeedsDisplayRect(const gfx::Rect& rect) override;
  bool HitTestable() const override;

  int thumb_thickness() const { return thumb_thickness_; }
  int track_start() const { return track_start_; }

  ScrollbarLayerType GetScrollbarLayerType() const override;

 private:
  SolidColorScrollbarLayer(ScrollbarOrientation orientation,
                           int thumb_thickness,
                           int track_start,
                           bool is_left_side_vertical_scrollbar);
  ~SolidColorScrollbarLayer() override;

  int thumb_thickness_;
  int track_start_;
};

}  // namespace cc

#endif  // CC_LAYERS_SOLID_COLOR_SCROLLBAR_LAYER_H_
