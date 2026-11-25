// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/paint_worklet_image_provider.h"

#include <utility>
#include "base/functional/callback_helpers.h"

namespace cc {

PaintWorkletImageProvider::PaintWorkletImageProvider(
    PaintWorkletRecordMap records)
    : records_(std::move(records)) {}

PaintWorkletImageProvider::~PaintWorkletImageProvider() = default;

PaintWorkletImageProvider::PaintWorkletImageProvider(
    PaintWorkletImageProvider&& other) = default;

PaintWorkletImageProvider& PaintWorkletImageProvider::operator=(
    PaintWorkletImageProvider&& other) = default;

std::optional<PaintRecord> PaintWorkletImageProvider::GetPaintRecordResult(
    scoped_refptr<PaintWorkletInput> input) {
  auto it = records_.find(input);
  // In the DiscardableImageMap::GatherDiscardableImages(), a DrawImageRect can
  // early exit the for loop if its paint rect is empty. In that case, the
  // |records_| will not contain that PaintWorkletInput, and we should return
  // std::nullopt.
  if (it == records_.end())
    return std::nullopt;
  return it->second.second;
}

}  // namespace cc
