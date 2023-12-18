// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_ENCOUNTERED_SURFACE_TRACKER_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_ENCOUNTERED_SURFACE_TRACKER_H_

#include <stdint.h>

#include <map>
#include <set>

class EncounteredSurfaceTracker {
 public:
  // Maximum number of surfaces that this class can track. Prevents unbounded
  // memory growth.
  static constexpr unsigned kMaxTrackedSurfaces = 1000;

  // Maximum number of sources that every surface can track. Prevents unbounded
  // memory growth.
  static constexpr unsigned kMaxTrackedSources = 1000;

  EncounteredSurfaceTracker();
  ~EncounteredSurfaceTracker();

  bool IsNewEncounter(uint64_t source_id, uint64_t surface);

  void Reset();

 private:
  // We use std::map and std::set since these containers are small and we need
  // to insert and erase frequently.
  std::map<uint64_t, std::set<uint64_t>> surfaces_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_ENCOUNTERED_SURFACE_TRACKER_H_
