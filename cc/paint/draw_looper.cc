// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "cc/paint/draw_looper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace cc {

bool DrawLooper::DrawLooper::Layer::operator==(const Layer& other) const {
  return offset == other.offset && blurSigma == other.blurSigma &&
         color == other.color && flags == other.flags;
}

DrawLooper::DrawLooper(std::vector<Layer> l) : layers_(std::move(l)) {}
DrawLooper::~DrawLooper() = default;

void DrawLooper::Layer::Apply(SkCanvas* canvas, SkPaint* paint) const {
  if (!(flags & kDontModifyPaintFlag)) {
    if (flags & kOverrideAlphaFlag)
      paint->setAlpha(0xFF);

    if (blurSigma > 0)
      paint->setMaskFilter(SkMaskFilter::MakeBlur(
          kNormal_SkBlurStyle, blurSigma, !(flags & kPostTransformFlag)));

    paint->setColorFilter(SkColorFilters::Blend(color, SkBlendMode::kSrcIn));
  }

  if (flags & kPostTransformFlag)
    canvas->setMatrix(
        canvas->getLocalToDevice().postTranslate(offset.fX, offset.fY));
  else
    canvas->translate(offset.fX, offset.fY);
}

bool DrawLooper::operator==(const DrawLooper& other) const {
  return layers_ == other.layers_;
}

// static
size_t DrawLooper::GetSerializedSize(const DrawLooper* looper) {
  size_t size = sizeof(bool);
  if (!looper)
    return size;

  size_t count = looper->layers_.size();
  size += sizeof(count);
  if (count == 0)
    return size;

  auto layer = looper->layers_.begin();

  size_t layer_size = sizeof(layer->offset.fX) + sizeof(layer->offset.fY) +
                      sizeof(layer->blurSigma) + sizeof(layer->color) +
                      sizeof(layer->flags);

  return size + count * layer_size;
}

DrawLooperBuilder::DrawLooperBuilder() = default;
DrawLooperBuilder::~DrawLooperBuilder() = default;

void DrawLooperBuilder::AddUnmodifiedContent(bool addOnTop) {
  AddShadow({0, 0}, 0, 0, DrawLooper::kDontModifyPaintFlag, addOnTop);
}

void DrawLooperBuilder::AddShadow(SkPoint offset,
                                  float blurSigma,
                                  SkColor color,
                                  uint32_t flags,
                                  bool addOnTop) {
  const DrawLooper::Layer layer = {offset, blurSigma, color,
                                   flags & DrawLooper::kAllFlagsMask};
  if (addOnTop)
    layers_.insert(layers_.begin(), layer);
  else
    layers_.push_back(layer);
}

sk_sp<DrawLooper> DrawLooperBuilder::Detach() {
  return sk_sp<DrawLooper>(new DrawLooper(std::move(layers_)));
}

}  // namespace cc
