// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/representative_surface_set.h"

#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

TEST(RepresentativeSurfaceTest, SetSerialization) {
  RepresentativeSurfaceSet set = {
      RepresentativeSurface(blink::IdentifiableSurface::FromMetricHash(2)),
      RepresentativeSurface(blink::IdentifiableSurface::FromMetricHash(1)),
      RepresentativeSurface(blink::IdentifiableSurface::FromMetricHash(10))};

  // Order is lexicographical.
  EXPECT_EQ(std::string("1,10,2"),
            (EncodeIdentifiabilityFieldTrialParam<
                RepresentativeSurfaceSet, RepresentativeSurfaceToString>(set)));
}

TEST(RepresentativeSurfaceTest, ListSerialization) {
  RepresentativeSurfaceList list = {
      RepresentativeSurface(blink::IdentifiableSurface::FromMetricHash(2)),
      RepresentativeSurface(blink::IdentifiableSurface::FromMetricHash(1)),
      RepresentativeSurface(blink::IdentifiableSurface::FromMetricHash(10))};

  // Order is preserved.
  EXPECT_EQ(
      std::string("2,1,10"),
      (EncodeIdentifiabilityFieldTrialParam<
          RepresentativeSurfaceList, RepresentativeSurfaceToString>(list)));
}
