// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/encountered_surface_tracker.h"

#include "base/containers/flat_set.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {
uint64_t metric(uint64_t i) {
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

TEST(EncounteredSurfaceTrackerTest, NeverDropsNewSurface) {
  EncounteredSurfaceTracker t;
  std::set<uint64_t> rand_numbers;
  for (uint64_t i = 0; i < 10000; ++i) {
    uint64_t new_rand_number;
    bool inserted;
    do {
      new_rand_number = base::RandUint64();
      inserted = rand_numbers.insert(new_rand_number).second;
    } while (!inserted);
    EXPECT_TRUE(t.IsNewEncounter(0, metric(new_rand_number)));
  }
}

TEST(EncounteredSurfaceTrackerTest, NeverDropsNewSource) {
  EncounteredSurfaceTracker t;
  std::set<uint64_t> rand_numbers;
  for (uint64_t i = 0; i < 10000; ++i) {
    uint64_t new_rand_number;
    bool inserted;
    do {
      new_rand_number = base::RandUint64();
      inserted = rand_numbers.insert(new_rand_number).second;
    } while (!inserted);
    EXPECT_TRUE(t.IsNewEncounter(new_rand_number, metric(0)));
  }
}

TEST(EncounteredSurfaceTrackerTest, SizeLimitForSources) {
  EncounteredSurfaceTracker t;
  for (uint64_t i = 0; i < EncounteredSurfaceTracker::kMaxTrackedEntries; i++) {
    EXPECT_TRUE(t.IsNewEncounter(i, metric(0)));
  }
  for (uint64_t i = 0; i < EncounteredSurfaceTracker::kMaxTrackedEntries; i++) {
    EXPECT_FALSE(t.IsNewEncounter(i, metric(0)));
  }

  // Adding a new one should bump the first one out.
  EXPECT_TRUE(t.IsNewEncounter(EncounteredSurfaceTracker::kMaxTrackedEntries,
                               metric(0)));
  EXPECT_TRUE(t.IsNewEncounter(0, metric(0)));
}

TEST(EncounteredSurfaceTrackerTest, SizeLimitForSurfaces) {
  EncounteredSurfaceTracker t;
  for (uint64_t i = 0; i < EncounteredSurfaceTracker::kMaxTrackedEntries; i++) {
    EXPECT_TRUE(t.IsNewEncounter(0, metric(i)));
  }
  for (uint64_t i = 0; i < EncounteredSurfaceTracker::kMaxTrackedEntries; i++) {
    EXPECT_FALSE(t.IsNewEncounter(0, metric(i)));
  }

  // Adding a new one should bump the first one out.
  EXPECT_TRUE(t.IsNewEncounter(
      0, metric(EncounteredSurfaceTracker::kMaxTrackedEntries)));
  EXPECT_TRUE(t.IsNewEncounter(0, metric(0)));
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
