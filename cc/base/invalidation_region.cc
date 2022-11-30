// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/invalidation_region.h"

#include "base/metrics/histogram.h"

namespace {

const int kMaxInvalidationRectCount = 256;

}  // namespace

namespace cc {

InvalidationRegion::InvalidationRegion() = default;

InvalidationRegion::~InvalidationRegion() = default;

void InvalidationRegion::Swap(Region* region) {
  FinalizePendingRects();
  region_.Swap(region);
}

void InvalidationRegion::Clear() {
  pending_rects_.clear();
  region_.Clear();
}

void InvalidationRegion::Union(const gfx::Rect& rect) {
  if (pending_rects_.size() >= kMaxInvalidationRectCount)
    pending_rects_[0].Union(rect);
  else
    pending_rects_.push_back(rect);
}

void InvalidationRegion::FinalizePendingRects() {
  if (pending_rects_.empty())
    return;

  if (region_.GetRegionComplexity() + pending_rects_.size() >
      kMaxInvalidationRectCount) {
    gfx::Rect pending_bounds = region_.bounds();
    for (auto& rect : pending_rects_)
      pending_bounds.Union(rect);
    region_ = pending_bounds;
  } else {
    for (auto& rect : pending_rects_)
      region_.Union(rect);
  }

  pending_rects_.clear();
}

}  // namespace cc
