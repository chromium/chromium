// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/encountered_surface_tracker.h"

#include <set>
#include <vector>

#include "base/rand_util.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

const unsigned EncounteredSurfaceTracker::kMaxTrackedSurfaces;
const unsigned EncounteredSurfaceTracker::kMaxTrackedSources;

EncounteredSurfaceTracker::EncounteredSurfaceTracker() {
  Reset();
}

EncounteredSurfaceTracker::~EncounteredSurfaceTracker() = default;

void EncounteredSurfaceTracker::Reset() {
  surfaces_.clear();
}

bool EncounteredSurfaceTracker::IsNewEncounter(uint64_t source_id,
                                               uint64_t surface) {
  if (blink::IdentifiableSurface::FromMetricHash(surface).GetType() ==
      blink::IdentifiableSurface::Type::kReservedInternal)
    return false;

  auto it = surfaces_.find(surface);
  if (it == surfaces_.end()) {
    // We need to add an entry for this surface, possibly bumping an entry out.
    surfaces_.insert(std::make_pair(surface, std::set<uint64_t>{source_id}));
    if (surfaces_.size() > kMaxTrackedSurfaces) {
      // Remove a random one.
      surfaces_.erase(base::RandInt(0, kMaxTrackedSurfaces));
    }
    return true;
  }

  if (it->second.contains(source_id))
    return false;

  it->second.insert(source_id);
  if (it->second.size() > kMaxTrackedSources) {
    // Remove a random one.
    it->second.erase(base::RandInt(0, kMaxTrackedSources));
  }
  return true;
}
