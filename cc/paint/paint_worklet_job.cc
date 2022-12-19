// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_worklet_job.h"

#include <utility>

namespace cc {

PaintWorkletJob::PaintWorkletJob(
    int layer_id,
    scoped_refptr<const PaintWorkletInput> input,
    AnimatedPropertyValues animated_property_values)
    : layer_id_(layer_id),
      input_(std::move(input)),
      animated_property_values_(std::move(animated_property_values)) {}

PaintWorkletJob::PaintWorkletJob(const PaintWorkletJob& other) = default;
PaintWorkletJob::PaintWorkletJob(PaintWorkletJob&& other) = default;
PaintWorkletJob::~PaintWorkletJob() = default;

void PaintWorkletJob::SetOutput(PaintRecord output) {
  DCHECK(output_.empty());
  output_ = std::move(output);
}

}  // namespace cc
