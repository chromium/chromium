// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_worklet_job.h"

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

void PaintWorkletJob::SetOutput(sk_sp<PaintRecord> output) {
  DCHECK(!output_);
  output_ = std::move(output);
}

}  // namespace cc
