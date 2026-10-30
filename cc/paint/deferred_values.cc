// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/deferred_values.h"

#include <utility>

#include "base/check.h"

namespace cc {

DeferredValueGenerator::DeferredValueGenerator() = default;
DeferredValueGenerator::~DeferredValueGenerator() = default;

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

void DeferredColorValueGenerator::Sample() {
  curve_->Sample(progress(), &value_);
}

DeferredValueMap GenerateDeferredValueMap(DeferredValues& values) {
  base::flat_map<DeferredPropertyKey, SkColor4f>::container_type colors;
  colors.reserve(values.size());
  for (auto& [key, generator] : values) {
    generator->RecomputeValue();
    if (const auto* color = DeferredColorValueGenerator::From(generator.get());
        color && color->has_value()) {
      colors.emplace_back(key, color->GetColor());
    }
  }
  // `values` is already sorted by key, so build the `flat_map` in one pass.
  return DeferredValueMap(base::flat_map<DeferredPropertyKey, SkColor4f>(
      base::sorted_unique, std::move(colors)));
}

}  // namespace cc
