// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/raster_inducing_values.h"

#include <utility>

#include "base/no_destructor.h"

namespace cc {

// static
const RasterInducingValues& RasterInducingValues::Empty() {
  static const base::NoDestructor<RasterInducingValues> kEmpty;
  return *kEmpty;
}

RasterInducingValues::RasterInducingValues() = default;
RasterInducingValues::RasterInducingValues(
    ScrollOffsetMap scroll_offsets,
    DeferredValueMap compositor_animated_values)
    : scroll_offsets(std::move(scroll_offsets)),
      compositor_animated_values(std::move(compositor_animated_values)) {}

RasterInducingValues::RasterInducingValues(const RasterInducingValues& other) {
  *this = other;
}

RasterInducingValues& RasterInducingValues::operator=(
    const RasterInducingValues& other) {
  // Unpack both so adding a member fails to compile until copied here.
  auto& [this_scroll_offsets, this_compositor_animated_values] = *this;
  const auto& [other_scroll_offsets, other_compositor_animated_values] = other;
  this_scroll_offsets = other_scroll_offsets;
  this_compositor_animated_values.CopyFrom(other_compositor_animated_values);
  return *this;
}

RasterInducingValues::RasterInducingValues(RasterInducingValues&&) noexcept =
    default;
RasterInducingValues& RasterInducingValues::operator=(
    RasterInducingValues&&) noexcept = default;
RasterInducingValues::~RasterInducingValues() = default;

}  // namespace cc
