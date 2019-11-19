// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/paint_worklet_image_provider.h"

#include <utility>
#include "base/bind_helpers.h"

namespace cc {

PaintWorkletImageProvider::PaintWorkletImageProvider(
    PaintWorkletRecordMap records)
    : records_(std::move(records)) {}

PaintWorkletImageProvider::~PaintWorkletImageProvider() = default;

PaintWorkletImageProvider::PaintWorkletImageProvider(
    PaintWorkletImageProvider&& other) = default;

PaintWorkletImageProvider& PaintWorkletImageProvider::operator=(
    PaintWorkletImageProvider&& other) = default;

ImageProvider::ScopedResult PaintWorkletImageProvider::GetPaintRecordResult(
    scoped_refptr<PaintWorkletInput> input) {
  // The |records_| contains all known PaintWorkletInputs, whether they are
  // painted or not, so |input| should always exist in it.
  auto it = records_.find(input);
  DCHECK(it != records_.end());
  return ImageProvider::ScopedResult(it->second.second);
}

}  // namespace cc
