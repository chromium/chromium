// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/encountered_surface_tracker.h"

#include <stdint.h>
#include <utility>

#include "base/containers/lru_cache.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

const unsigned EncounteredSurfaceTracker::kMaxTrackedEntries;

EncounteredSurfaceTracker::EncounteredSurfaceTracker()
    : surfaces_(base::LRUCacheSet<std::pair<uint64_t, uint64_t>>(
          kMaxTrackedEntries)) {}

EncounteredSurfaceTracker::~EncounteredSurfaceTracker() = default;

void EncounteredSurfaceTracker::Reset() {
  surfaces_.Clear();
}

bool EncounteredSurfaceTracker::IsNewEncounter(uint64_t source_id,
                                               uint64_t surface) {
  if (blink::IdentifiableSurface::FromMetricHash(surface).GetType() ==
      blink::IdentifiableSurface::Type::kReservedInternal)
    return false;

  if (surfaces_.Get(std::make_pair(source_id, surface)) != surfaces_.end()) {
    return false;
  }
  surfaces_.Put(std::make_pair(source_id, surface));
  return true;
}
