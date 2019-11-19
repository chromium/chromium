// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_
#define CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_

#include <memory>
#include <string>

#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class CC_EXPORT HeadsUpDisplayLayer : public Layer {
 public:
  static scoped_refptr<HeadsUpDisplayLayer> Create();

  HeadsUpDisplayLayer(const HeadsUpDisplayLayer&) = delete;
  HeadsUpDisplayLayer& operator=(const HeadsUpDisplayLayer&) = delete;

  void UpdateLocationAndSize(const gfx::Size& device_viewport,
                             float device_scale_factor);

  const std::vector<gfx::Rect>& LayoutShiftRects() const;
  void SetLayoutShiftRects(const std::vector<gfx::Rect>& rects);

  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  // Layer overrides.
  void PushPropertiesTo(LayerImpl* layer) override;

 protected:
  HeadsUpDisplayLayer();
  bool HasDrawableContent() const override;

 private:
  ~HeadsUpDisplayLayer() override;

  sk_sp<SkTypeface> typeface_;
  std::vector<gfx::Rect> layout_shift_rects_;
};

}  // namespace cc

#endif  // CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_
