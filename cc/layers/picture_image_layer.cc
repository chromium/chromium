// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_image_layer.h"

#include <stddef.h>

#include "cc/base/math_util.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

scoped_refptr<PictureImageLayer> PictureImageLayer::Create() {
  return base::WrapRefCounted(new PictureImageLayer());
}

PictureImageLayer::PictureImageLayer() : PictureLayer(this) {}

PictureImageLayer::~PictureImageLayer() {
  ClearClient();
}

std::unique_ptr<LayerImpl> PictureImageLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  auto layer_impl = PictureLayerImpl::Create(tree_impl, id());
  layer_impl->set_is_directly_composited_image(true);
  return std::move(layer_impl);
}

bool PictureImageLayer::HasDrawableContent() const {
  return image_ && PictureLayer::HasDrawableContent();
}

void PictureImageLayer::SetImage(PaintImage image,
                                 const SkMatrix& matrix,
                                 bool uses_width_as_height) {
  // SetImage() currently gets called whenever there is any
  // style change that affects the layer even if that change doesn't
  // affect the actual contents of the image (e.g. a CSS animation).
  // With this check in place we avoid unecessary texture uploads.
  if (image_ == image && matrix_ == matrix &&
      uses_width_as_height_ == uses_width_as_height) {
    return;
  }

  image_ = std::move(image);
  matrix_ = matrix;
  uses_width_as_height_ = uses_width_as_height;
  UpdateDrawsContent(HasDrawableContent());
  SetNeedsDisplay();
}

gfx::Rect PictureImageLayer::PaintableRegion() {
  return gfx::Rect(bounds());
}

scoped_refptr<DisplayItemList> PictureImageLayer::PaintContentsToDisplayList(
    ContentLayerClient::PaintingControlSetting painting_control) {
  DCHECK(image_);
  DCHECK_GT(image_.width(), 0);
  DCHECK_GT(image_.height(), 0);
  DCHECK(layer_tree_host());

  int width = uses_width_as_height_ ? image_.height() : image_.width();
  int height = uses_width_as_height_ ? image_.width() : image_.height();

  float content_to_layer_scale_x = static_cast<float>(bounds().width()) / width;
  float content_to_layer_scale_y =
      static_cast<float>(bounds().height()) / height;

  bool has_scale = !MathUtil::IsWithinEpsilon(content_to_layer_scale_x, 1.f) ||
                   !MathUtil::IsWithinEpsilon(content_to_layer_scale_y, 1.f);
  bool needs_save = has_scale || !matrix_.isIdentity();

  auto display_list = base::MakeRefCounted<DisplayItemList>();

  display_list->StartPaint();

  if (needs_save)
    display_list->push<SaveOp>();

  if (has_scale) {
    display_list->push<ScaleOp>(content_to_layer_scale_x,
                                content_to_layer_scale_y);
  }

  if (!matrix_.isIdentity())
    display_list->push<ConcatOp>(matrix_);

  // Because Android WebView resourceless software draw mode rasters directly
  // to the root canvas, this draw must use the SkBlendMode::kSrcOver so that
  // transparent images blend correctly.
  display_list->push<DrawImageOp>(image_, 0.f, 0.f, nullptr);

  if (needs_save)
    display_list->push<RestoreOp>();

  display_list->EndPaintOfUnpaired(PaintableRegion());
  display_list->Finalize();
  return display_list;
}

bool PictureImageLayer::FillsBoundsCompletely() const {
  return false;
}

size_t PictureImageLayer::GetApproximateUnsharedMemoryUsage() const {
  return 0;
}

}  // namespace cc
