// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_IMAGE_LAYER_H_
#define CC_LAYERS_PICTURE_IMAGE_LAYER_H_

#include <stddef.h>

#include "cc/cc_export.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class CC_EXPORT PictureImageLayer : public PictureLayer, ContentLayerClient {
 public:
  static scoped_refptr<PictureImageLayer> Create();

  PictureImageLayer(const PictureImageLayer&) = delete;
  PictureImageLayer& operator=(const PictureImageLayer&) = delete;

  void SetImage(PaintImage image,
                const SkMatrix& matrix,
                bool uses_width_as_height);

  // Layer implementation.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  gfx::Rect PaintableRegion() override;

  // ContentLayerClient implementation.
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      ContentLayerClient::PaintingControlSetting painting_control) override;
  bool FillsBoundsCompletely() const override;
  size_t GetApproximateUnsharedMemoryUsage() const override;

 protected:
  bool HasDrawableContent() const override;

 private:
  PictureImageLayer();
  ~PictureImageLayer() override;

  PaintImage image_;
  SkMatrix matrix_;
  bool uses_width_as_height_;
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_IMAGE_LAYER_H_
