// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TEST_USER_VARIATION_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TEST_USER_VARIATION_H_

#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

// Defines user authentication and profile state variations for interactive UI
// tests.
enum class UserVariation {
  kSignedIn,
  kSignedOut,
  kIncognito,
};

inline std::string UserVariationToString(
    const testing::TestParamInfo<UserVariation>& info) {
  switch (info.param) {
    case UserVariation::kSignedIn:
      return "SignedIn";
    case UserVariation::kSignedOut:
      return "SignedOut";
    case UserVariation::kIncognito:
      return "Incognito";
  }
}

// Name generator for parameterized tests with a tuple whose first element is
// UserVariation.
template <typename TupleType>
std::string UserVariationTupleToString(
    const testing::TestParamInfo<TupleType>& info) {
  std::string result =
      UserVariationToString(testing::TestParamInfo<UserVariation>(
          std::get<0>(info.param), info.index));
  if constexpr (std::tuple_size_v<TupleType> > 1) {
    if constexpr (std::is_same_v<std::tuple_element_t<1, TupleType>, bool>) {
      result += std::get<1>(info.param) ? "_True" : "_False";
    }
  }
  if constexpr (std::tuple_size_v<TupleType> > 2) {
    if constexpr (std::is_same_v<std::tuple_element_t<2, TupleType>, bool>) {
      result += std::get<2>(info.param) ? "_True" : "_False";
    }
  }
  return result;
}

}  // namespace contextual_tasks

// Macros for skipping specific user variations during test execution.
// In googletest, GTEST_SKIP() contains a 'return;' statement. To properly abort
// the test execution upon skipping, the skip macro must expand directly in the
// test method's scope.
// Every macro requires an explicit, non-empty reason explaining why the
// variation cannot be supported.
#define SKIP_IF(current, target, reason)                             \
  do {                                                               \
    const std::string_view skip_reason_view = (reason);              \
    CHECK(!skip_reason_view.empty())                                 \
        << "A non-empty skip reason must be provided.";              \
    if ((current) == (target)) {                                     \
      GTEST_SKIP() << "Skipped for variation: " << skip_reason_view; \
    }                                                                \
  } while (false)

#define SKIP_IF_INCOGNITO(variation, reason) \
  SKIP_IF((variation), ::contextual_tasks::UserVariation::kIncognito, (reason))

#define SKIP_IF_SIGNED_OUT(variation, reason) \
  SKIP_IF((variation), ::contextual_tasks::UserVariation::kSignedOut, (reason))

#define SKIP_IF_SIGNED_IN(variation, reason) \
  SKIP_IF((variation), ::contextual_tasks::UserVariation::kSignedIn, (reason))

#define SKIP_IF_NOT_SIGNED_IN(variation, reason)                       \
  do {                                                                 \
    const std::string_view skip_reason_view = (reason);                \
    CHECK(!skip_reason_view.empty())                                   \
        << "A non-empty skip reason must be provided.";                \
    if ((variation) != ::contextual_tasks::UserVariation::kSignedIn) { \
      GTEST_SKIP() << "Only supported for SignedIn users: "            \
                   << skip_reason_view;                                \
    }                                                                  \
  } while (false)

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TEST_USER_VARIATION_H_
