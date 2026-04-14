// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(CriticalUserJourneyRegistryTest,
     MaxJourneyLengthDoesNotExceedHistogramMax) {
  // The maximum step duration histogram is defined up to step 5 in
  // tools/metrics/histograms/metadata/critical_user_journeys/histograms.xml
  // under the token "ToStep".
  constexpr size_t kMaxHistogramStep = 5;

  CriticalUserJourneyRegistry registry;
  registry.AddJourneys();

  for (const auto& journey : registry.journeys()) {
    EXPECT_LE(journey->steps().size(), kMaxHistogramStep)
        << "Journey " << journey->name()
        << " exceeds the maximum allowed length of " << kMaxHistogramStep;
  }
}

}  // namespace metrics
