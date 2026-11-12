// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/deferred_values.h"

#include <utility>

#include "base/check.h"

namespace cc {

DeferredValueGenerator::DeferredValueGenerator() = default;
DeferredValueGenerator::~DeferredValueGenerator() = default;

bool DeferredValueGenerator::CanRunOnImplThread() const {
  return true;
}

bool DeferredValueGenerator::SetProgress(std::optional<float> progress) {
  if (progress_ == progress) {
    return false;
  }
  progress_ = progress;
  generation_++;
  return true;
}

void DeferredValueGenerator::RecomputeValue() {
  if (!NeedsRecompute()) {
    return;
  }
  if (progress_.has_value()) {
    Sample();
    has_value_ = true;
  } else {
    // Drop any previous sample so raster falls back to the painted value.
    // TODO(crbug.com/575916161): The painted value is only correct until the
    // animation first ticks, so it may be stale here. Decide what to use
    // instead.
    has_value_ = false;
  }
  last_generation_ = generation_;
}

DeferredColorValueGenerator::DeferredColorValueGenerator(
    scoped_refptr<const AnimationCurveProxy<SkColor4f>> curve)
    : curve_(std::move(curve)) {
  CHECK(curve_);
}

DeferredColorValueGenerator::~DeferredColorValueGenerator() = default;

DeferredValueGenerator::Type DeferredColorValueGenerator::GetType() const {
  return Type::kColor;
}

std::unique_ptr<DeferredValueGenerator>
DeferredColorValueGenerator::CreateWithSameSource() const {
  return std::make_unique<DeferredColorValueGenerator>(curve_);
}

bool DeferredColorValueGenerator::HasSameSource(
    const DeferredValueGenerator& other) const {
  // A curve never changes once cc has it, so equal pointers mean equal curves.
  const auto* color_other = From(&other);
  return color_other && curve_.get() == color_other->curve();
}

void DeferredColorValueGenerator::Sample() {
  curve_->Sample(progress(), &value_);
}

}  // namespace cc
