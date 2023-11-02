// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/encountered_surface_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {
uint64_t metric(int i) {
  return blink::IdentifiableSurface::FromTypeAndToken(
             blink::IdentifiableSurface::Type::kWebFeature, i)
      .ToUkmMetricHash();
}
}  // namespace

TEST(EncounteredSurfaceTrackerTest, Dedup) {
  EncounteredSurfaceTracker t;
  EXPECT_TRUE(t.IsNewEncounter(0, metric(1)));
  EXPECT_FALSE(t.IsNewEncounter(0, metric(1)));
  EXPECT_TRUE(t.IsNewEncounter(1, metric(1)));
}

TEST(EncounteredSurfaceTrackerTest, SizeLimit) {
  EncounteredSurfaceTracker t;
  for (uint64_t i = 0; i < EncounteredSurfaceTracker::kMaxTrackedSurfaces;
       i++) {
    EXPECT_TRUE(t.IsNewEncounter(0, metric(i))) << ": 0," << i;
  }
  // Now the tracker should be full, but we will still allow new sources.
  for (uint64_t i = 0; i < EncounteredSurfaceTracker::kMaxTrackedSurfaces;
       i++) {
    EXPECT_FALSE(t.IsNewEncounter(0, metric(i))) << ": 0," << i;
    EXPECT_TRUE(t.IsNewEncounter(1, metric(i))) << ": 1," << i;
  }

  // Add an extra one. This should bump one of the surfaces out.
  t.IsNewEncounter(0, EncounteredSurfaceTracker::kMaxTrackedSurfaces + 1);

  // We expect only kMaxTrackedSurfaces to return true for a new surface.
  unsigned num_true = 0;
  for (uint64_t i = 0; i < EncounteredSurfaceTracker::kMaxTrackedSurfaces + 1;
       i++) {
    if (t.IsNewEncounter(2, metric(i)))
      num_true++;
  }
  EXPECT_EQ(EncounteredSurfaceTracker::kMaxTrackedSurfaces, num_true);
}

TEST(EncounteredSurfaceTrackerTest, Reset) {
  EncounteredSurfaceTracker t;
  EXPECT_TRUE(t.IsNewEncounter(0, metric(0))) << ": 0,0";
  EXPECT_FALSE(t.IsNewEncounter(0, metric(0))) << ": 0,0";
  t.Reset();
  EXPECT_TRUE(t.IsNewEncounter(0, metric(0))) << ": 0,0";
}

TEST(EncounteredSurfaceTrackerTest, InvalidMetric) {
  EncounteredSurfaceTracker t;
  EXPECT_FALSE(t.IsNewEncounter(
      0, blink::IdentifiableSurface::FromTypeAndToken(
             blink::IdentifiableSurface::Type::kReservedInternal, 1)
             .ToUkmMetricHash()));
}
