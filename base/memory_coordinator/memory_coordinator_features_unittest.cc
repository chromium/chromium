// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_coordinator_features.h"

#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::internal {

namespace {

using Mode = MemoryConsumerMode;

MemoryConsumerModeOverrides ParseWithOverrides(
    const FieldTrialParams& overrides) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMemoryConsumerModeOverrides, overrides);
  return ParseMemoryConsumerModeOverrides();
}

}  // namespace

TEST(MemoryCoordinatorFeaturesTest, EmptyWhenFeatureDisabled) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kMemoryConsumerModeOverrides);
  EXPECT_TRUE(ParseMemoryConsumerModeOverrides().empty());
}

TEST(MemoryCoordinatorFeaturesTest, AllModes) {
  EXPECT_EQ(ParseWithOverrides(
                {{"A", "disabled"}, {"B", "stateless"}, {"C", "stateful"}}),
            MemoryConsumerModeOverrides({{"A", Mode::kDisabled},
                                         {"B", Mode::kStateless},
                                         {"C", Mode::kStateful}}));
}

TEST(MemoryCoordinatorFeaturesTest, UnknownModesAreIgnored) {
  EXPECT_EQ(
      ParseWithOverrides({{"A", "unknown"}, {"B", ""}, {"D", "stateless"}}),
      MemoryConsumerModeOverrides({{"D", Mode::kStateless}}));
}

class ShouldUseStatefulMemoryPressureTest
    : public testing::TestWithParam<bool> {
 public:
  ShouldUseStatefulMemoryPressureTest() {
    std::vector<test::FeatureRefAndParams> enabled_features = {
        {kMemoryConsumerModeOverrides,
         {{"A", "stateless"}, {"B", "stateful"}, {"C", "disabled"}}}};
    std::vector<test::FeatureRef> disabled_features;
    if (IsStatefulEnabled()) {
      enabled_features.push_back({kStatefulMemoryPressure, {}});
    } else {
      disabled_features.push_back(kStatefulMemoryPressure);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  bool IsStatefulEnabled() const { return GetParam(); }

 private:
  test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ShouldUseStatefulMemoryPressureTest,
                         testing::Bool());

TEST_P(ShouldUseStatefulMemoryPressureTest, Overrides) {
  EXPECT_FALSE(ShouldUseStatefulMemoryPressure("A"));
  EXPECT_TRUE(ShouldUseStatefulMemoryPressure("B"));
  // Disabled and unlisted consumers follow kStatefulMemoryPressure.
  EXPECT_EQ(ShouldUseStatefulMemoryPressure("C"), IsStatefulEnabled());
  EXPECT_EQ(ShouldUseStatefulMemoryPressure("D"), IsStatefulEnabled());
}

TEST_P(ShouldUseStatefulMemoryPressureTest, OverridesFeatureDisabled) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kMemoryConsumerModeOverrides);
  EXPECT_EQ(ShouldUseStatefulMemoryPressure("A"), IsStatefulEnabled());
  EXPECT_EQ(ShouldUseStatefulMemoryPressure("B"), IsStatefulEnabled());
}

}  // namespace base::internal
