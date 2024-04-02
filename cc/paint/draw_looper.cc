// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/draw_looper.h"

#include <utility>

#include "third_party/skia/include/core/SkBlurTypes.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace cc {

DrawLooper::DrawLooper(std::vector<Layer> l) : layers_(std::move(l)) {}
DrawLooper::~DrawLooper() = default;

void DrawLooper::Layer::Apply(SkCanvas* canvas, SkPaint* paint) const {
  if (!(flags & kDontModifyPaintFlag)) {
    if (flags & kOverrideAlphaFlag) {
      paint->setAlpha(0xFF);
    }

    if (blur_sigma > 0) {
      paint->setMaskFilter(SkMaskFilter::MakeBlur(
          kNormal_SkBlurStyle, blur_sigma, !(flags & kPostTransformFlag)));
    }

    paint->setColorFilter(SkColorFilters::Blend(color, SkColorSpace::MakeSRGB(),
                                                SkBlendMode::kSrcIn));
  }

  if (flags & kPostTransformFlag) {
    canvas->setMatrix(
        canvas->getLocalToDevice().postTranslate(offset.fX, offset.fY));
  } else {
    canvas->translate(offset.fX, offset.fY);
  }
}

bool DrawLooper::EqualsForTesting(const DrawLooper& other) const {
  return layers_ == other.layers_;
}

DrawLooperBuilder::DrawLooperBuilder() = default;
DrawLooperBuilder::~DrawLooperBuilder() = default;

void DrawLooperBuilder::AddUnmodifiedContent(bool add_on_top) {
  AddShadow({0, 0}, 0, SkColors::kBlack, DrawLooper::kDontModifyPaintFlag,
            add_on_top);
}

void DrawLooperBuilder::AddShadow(SkPoint offset,
                                  float blur_sigma,
                                  SkColor4f color,
                                  uint32_t flags,
                                  bool add_on_top) {
  const DrawLooper::Layer layer = {offset, blur_sigma, color,
                                   flags & DrawLooper::kAllFlagsMask};
  if (add_on_top) {
    layers_.insert(layers_.begin(), layer);
  } else {
    layers_.push_back(layer);
  }
}

sk_sp<DrawLooper> DrawLooperBuilder::Detach() {
  return sk_sp<DrawLooper>(new DrawLooper(std::move(layers_)));
}

}  // namespace cc
