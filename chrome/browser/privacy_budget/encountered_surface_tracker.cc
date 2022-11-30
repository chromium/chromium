// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/encountered_surface_tracker.h"

#include <vector>

#include "base/rand_util.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

uint64_t hash(uint64_t x) {
  // This is should be a reasonable and fast reversible hash.
  // Based a bit on https://naml.us/post/inverse-of-a-hash-function/
  x = x * 88388617;  // calculate as (x<<23)+(x<<3)+x
  x = x ^ (x >> 31);
  x = x * 257;  // calculate as (x<<8)+x
  x = x ^ (x >> 13);
  x = x * 769;  // calculate as (x<<9)+(x<<8)+x
  x = x ^ (x >> 23);
  x = x * 127;  // calculate as (x<<7)-x
  return x;
}

}  // namespace

const unsigned EncounteredSurfaceTracker::kMaxTrackedSurfaces;

EncounteredSurfaceTracker::EncounteredSurfaceTracker() {
  Reset();
}

EncounteredSurfaceTracker::~EncounteredSurfaceTracker() = default;

void EncounteredSurfaceTracker::Reset() {
  seed_ = base::RandUint64();
  surfaces_.clear();
}

bool EncounteredSurfaceTracker::IsNewEncounter(uint64_t source_id,
                                               uint64_t surface) {
  if (blink::IdentifiableSurface::FromMetricHash(surface).GetType() ==
      blink::IdentifiableSurface::Type::kReservedInternal)
    return false;

  HashKey key = hash(surface ^ seed_);
  auto it = surfaces_.find(key);
  if (it == surfaces_.end()) {
    if (surfaces_.size() >= kMaxTrackedSurfaces &&
        key <= surfaces_.begin()->first) {
      return false;
    }
    // We need to add an entry for this surface, possibly bumping an entry out.
    surfaces_.insert(std::make_pair(key, base::flat_set<uint64_t>{source_id}));
    if (surfaces_.size() > kMaxTrackedSurfaces) {
      // Remove the smallest
      surfaces_.erase(surfaces_.begin());
    }
    return true;
  }

  if (it->second.contains(source_id))
    return false;
  it->second.insert(source_id);
  return true;
}
