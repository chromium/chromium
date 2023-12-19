// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_ENCOUNTERED_SURFACE_TRACKER_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_ENCOUNTERED_SURFACE_TRACKER_H_

#include <stdint.h>

#include "base/containers/lru_cache.h"

class EncounteredSurfaceTracker {
 public:
  // Maximum number of (source, surface) pairs that this class can track.
  // Prevents unbounded memory growth.
  static constexpr unsigned kMaxTrackedEntries = 10000;

  EncounteredSurfaceTracker();
  ~EncounteredSurfaceTracker();

  bool IsNewEncounter(uint64_t source_id, uint64_t surface);

  void Reset();

 private:
  // We use std::map and std::set since these containers are small and we need
  // to insert and erase frequently.
  base::LRUCacheSet<std::pair<uint64_t, uint64_t>> surfaces_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_ENCOUNTERED_SURFACE_TRACKER_H_
