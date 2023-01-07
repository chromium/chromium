// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_FEATURE_PARAMETER_INTERFACE_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_FEATURE_PARAMETER_INTERFACE_H_

#include <cstddef>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Desired state of a feature in a test instantiation.
struct FeatureState {
  const base::Feature* feature;
  bool is_enabled;
};

// FeatureStateArray is the actual parameter passed to tests. It is just an
// array of features and their desired state.
template <size_t N>
using FeatureStateArray = std::array<FeatureState, N>;

// Alias for an element that holds all permutations of FeatureStateArray
// For any N, the amount of permutations is N^2
template <size_t N>
using FeatureStateArrayPermutations =
    std::array<FeatureStateArray<N>, 1 << N /* = 2^N */>;

template <size_t N>
using BaseFeatureArray = std::array<const base::Feature*, N>;

}  // anonymous namespace

//
// Use this interface in tests when you want to provide features as parameters.
// Tests will run with the features enabled and disabled, in all possible
// permutations. Note that for N features, there are 2^N permutations.
//
// QUICK REFERENCE:
//
// Features: A and B ( N = 2 )
//
// --- 1. Create all possible permutations of the state of the features.
// const auto kAllFeaturePermutations =
//       FeatureAsParameterInterface<2>::Generator({&features::A,
//                                                  &features::B});
//
// Yields:  {{{A, true},  {B, true}},
//           {{A, true},  {B, false}},
//           {{A, false}, {B, true}},
//           {{A, false}, {B, false}}}
//
//
// --- 2. Inherit from FeatureAsParameterInterface<N>
// class MyTestClass : public FeatureAsParameterInterface<2> {
//   ...
// }
//
// --- 3. Write a parameterized test case.
// IN_PROC_BROWSER_TEST_P(MyTestClass, TestCase) {
//
//   3.1. Optionally tailor the test case when the feature is present or not.
//   if (IsFeatureEnabledInThisTestCase(features::A)) {
//     Do something when the feature A is enabled.
//   } else {
//     Do something when the feature A is disabled.
//   }
//
// }
//
// --- 4. Instantiate the tests with all possible permutations.
// INSTANTIATE_TEST_SUITE_P(MyTestSuiteName,
//                          MyTestClass,
//                          testing::ValuesIn(kAllFeaturePermutations),
//                          FeatureAsParameterInterface<2>::ParamInfoToString);
//
template <size_t N>
class FeatureAsParameterInterface
    : public ::testing::WithParamInterface<FeatureStateArray<N>> {
 public:
  // Constructor with optional features that are not being parameterized.
  FeatureAsParameterInterface(
      std::vector<base::test::FeatureRef> always_enabled_features = {},
      std::vector<base::test::FeatureRef> always_disabled_features = {});

  // Provides a description of the test case for GTest.
  // Pattern: _With_FeatureA_Enabled_With_FeatureB_Disabled_...
  static std::string ParamInfoToString(
      ::testing::TestParamInfo<FeatureStateArray<N>> param_info);
  static std::string FeatureStateArrayToString(
      const FeatureStateArray<N> feature_state_array);

  // Generates all possible test cases from an array of features. Similar to
  // other GTest Generators. Only used during compile time.
  static constexpr FeatureStateArrayPermutations<N> Generator(
      BaseFeatureArray<N> all_features);

  // Whether the given feature is enabled for the current test case.
  // Intentionally only works for features that are being parameterized.
  bool IsFeatureEnabledInThisTestCase(const base::Feature& feature);

 private:
  using InterfaceType = ::testing::WithParamInterface<FeatureStateArray<N>>;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// ----------------------------------------------------------------------------
// IMPLEMENTATION
// ----------------------------------------------------------------------------

template <size_t N>
FeatureAsParameterInterface<N>::FeatureAsParameterInterface(
    std::vector<base::test::FeatureRef> always_enabled_features,
    std::vector<base::test::FeatureRef> always_disabled_features) {
  // Initialize the vectors with the features that are not being parameterized
  // and should be always enabled/disabled.
  std::vector<base::test::FeatureRef> enabled_features(always_enabled_features),
      disabled_features(always_disabled_features);

  const FeatureStateArray<N> feature_state_array = InterfaceType::GetParam();
  for (const FeatureState& feature_state : feature_state_array) {
    if (feature_state.is_enabled) {
      enabled_features.push_back(*feature_state.feature);
    } else {
      disabled_features.push_back(*feature_state.feature);
    }
  }
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

template <size_t N>
std::string FeatureAsParameterInterface<N>::ParamInfoToString(
    ::testing::TestParamInfo<FeatureStateArray<N>> param_info) {
  return FeatureStateArrayToString(param_info.param);
}

template <size_t N>
std::string FeatureAsParameterInterface<N>::FeatureStateArrayToString(
    const FeatureStateArray<N> feature_state_array) {
  std::string test_description;
  for (const FeatureState& feature_state : feature_state_array) {
    const char kDescriptionPattern[] = "_With_%s_%s";
    test_description +=
        base::StringPrintf(kDescriptionPattern, feature_state.feature->name,
                           (feature_state.is_enabled ? "Enabled" : "Disabled"));
  }
  return test_description;
}

template <size_t N>
bool FeatureAsParameterInterface<N>::IsFeatureEnabledInThisTestCase(
    const base::Feature& feature) {
  const FeatureStateArray<N> feature_state_array = InterfaceType::GetParam();
  for (const FeatureState& feature_state : feature_state_array) {
    if (strcmp(feature_state.feature->name, feature.name) != 0)
      continue;

    return feature_state.is_enabled;
  }

  NOTREACHED() << "The requested feature isn't being parameterized.";
  return false;
}

template <size_t N>
constexpr FeatureStateArrayPermutations<N>
FeatureAsParameterInterface<N>::Generator(BaseFeatureArray<N> all_features) {
  // The result is an array of N^2 FeatureStateArrays
  FeatureStateArrayPermutations<N> result{};

  for (size_t permutation = 0; permutation < result.size(); ++permutation) {
    FeatureStateArray<N> current_test_case{};

    // Populate the current test case using the bits of the current permutation
    // to enable/disable features.
    for (size_t feature_index = 0; feature_index < N; ++feature_index) {
      const bool is_enabled = (permutation & (1 << feature_index));
      current_test_case[feature_index] = {
          FeatureState{all_features[feature_index], is_enabled}};
    }
    result[permutation] = current_test_case;
  }
  return result;
}

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_FEATURE_PARAMETER_INTERFACE_H_
