// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>

#include "base/values.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/common/extensions/api/omnibox.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace extensions {

namespace omnibox = api::omnibox;
namespace SendSuggestions = omnibox::SendSuggestions;
namespace SetDefaultSuggestion = omnibox::SetDefaultSuggestion;

namespace {

const int kNone = ACMatchClassification::NONE;
const int kUrl = ACMatchClassification::URL;
const int kMatch = ACMatchClassification::MATCH;
const int kDim = ACMatchClassification::DIM;

void CompareClassification(const ACMatchClassifications& expected,
                           const ACMatchClassifications& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  for (size_t i = 0; i < expected.size() && i < actual.size(); ++i) {
    EXPECT_EQ(expected[i].offset, actual[i].offset) << "Index:" << i;
    EXPECT_EQ(expected[i].style, actual[i].style) << "Index:" << i;
  }
}

}  // namespace

// Test output key: n = character with no styling, d = dim, m = match, u = url
// u = 1, m = 2, d = 4. u+d = 5, etc.

//   0123456789
//    mmmm
// +       ddd
// = nmmmmndddn
TEST(ExtensionOmniboxTest, DescriptionStylesSimple) {
  base::Value::List list =
      base::Value::List().Append(42).Append(base::Value::List().Append(
          base::Value::Dict()
              .Set("content", "content")
              .Set("description", "description")
              .Set("descriptionStyles", base::Value::List()
                                            .Append(base::Value::Dict()
                                                        .Set("type", "match")
                                                        .Set("offset", 1)
                                                        .Set("length", 4))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "dim")
                                                        .Set("offset", 6)
                                                        .Set("length", 3)))));

  ACMatchClassifications styles_expected;
  styles_expected.push_back(ACMatchClassification(0, kNone));
  styles_expected.push_back(ACMatchClassification(1, kMatch));
  styles_expected.push_back(ACMatchClassification(5, kNone));
  styles_expected.push_back(ACMatchClassification(6, kDim));
  styles_expected.push_back(ACMatchClassification(9, kNone));

  std::optional<SendSuggestions::Params> params =
      SendSuggestions::Params::Create(list);
  EXPECT_TRUE(params);
  ASSERT_FALSE(params->suggest_results.empty());
  CompareClassification(styles_expected, StyleTypesToACMatchClassifications(
                                             params->suggest_results[0]));

  // Same input, but swap the order. Ensure it still works.
  base::Value::List swap_list =
      base::Value::List().Append(42).Append(base::Value::List().Append(
          base::Value::Dict()
              .Set("content", "content")
              .Set("description", "description")
              .Set("descriptionStyles", base::Value::List()
                                            .Append(base::Value::Dict()
                                                        .Set("type", "dim")
                                                        .Set("offset", 6)
                                                        .Set("length", 3))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "match")
                                                        .Set("offset", 1)
                                                        .Set("length", 4)))));

  std::optional<SendSuggestions::Params> swapped_params =
      SendSuggestions::Params::Create(swap_list);
  EXPECT_TRUE(swapped_params);
  ASSERT_FALSE(swapped_params->suggest_results.empty());
  CompareClassification(
      styles_expected,
      StyleTypesToACMatchClassifications(swapped_params->suggest_results[0]));
}

//   0123456789
//   uuuuu
// +          dd
// +          mm
// + mmmm
// +  dd
// = 3773unnnn66
TEST(ExtensionOmniboxTest, DescriptionStylesCombine) {
  base::Value::List list =
      base::Value::List().Append(42).Append(base::Value::List().Append(
          base::Value::Dict()
              .Set("content", "content")
              .Set("description", "description")
              .Set("descriptionStyles", base::Value::List()
                                            .Append(base::Value::Dict()
                                                        .Set("type", "url")
                                                        .Set("offset", 0)
                                                        .Set("length", 5))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "dim")
                                                        .Set("offset", 9)
                                                        .Set("length", 2))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "match")
                                                        .Set("offset", 9)
                                                        .Set("length", 2))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "match")
                                                        .Set("offset", 0)
                                                        .Set("length", 4))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "dim")
                                                        .Set("offset", 1)
                                                        .Set("length", 2)))));

  ACMatchClassifications styles_expected;
  styles_expected.push_back(ACMatchClassification(0, kUrl | kMatch));
  styles_expected.push_back(ACMatchClassification(1, kUrl | kMatch | kDim));
  styles_expected.push_back(ACMatchClassification(3, kUrl | kMatch));
  styles_expected.push_back(ACMatchClassification(4, kUrl));
  styles_expected.push_back(ACMatchClassification(5, kNone));
  styles_expected.push_back(ACMatchClassification(9, kMatch | kDim));

  std::optional<SendSuggestions::Params> params =
      SendSuggestions::Params::Create(list);
  EXPECT_TRUE(params);
  ASSERT_FALSE(params->suggest_results.empty());
  CompareClassification(styles_expected, StyleTypesToACMatchClassifications(
                                             params->suggest_results[0]));

  // Try moving the "dim/match" style pair at offset 9. Output should be the
  // same.
  base::Value::List moved_list =
      base::Value::List().Append(42).Append(base::Value::List().Append(
          base::Value::Dict()
              .Set("content", "content")
              .Set("description", "description")
              .Set("descriptionStyles", base::Value::List()
                                            .Append(base::Value::Dict()
                                                        .Set("type", "url")
                                                        .Set("offset", 0)
                                                        .Set("length", 5))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "match")
                                                        .Set("offset", 0)
                                                        .Set("length", 4))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "dim")
                                                        .Set("offset", 9)
                                                        .Set("length", 2))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "match")
                                                        .Set("offset", 9)
                                                        .Set("length", 2))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "dim")
                                                        .Set("offset", 1)
                                                        .Set("length", 2)))));

  std::optional<SendSuggestions::Params> moved_params =
      SendSuggestions::Params::Create(moved_list);
  EXPECT_TRUE(moved_params);
  ASSERT_FALSE(moved_params->suggest_results.empty());
  CompareClassification(styles_expected, StyleTypesToACMatchClassifications(
                                             moved_params->suggest_results[0]));
}

//   0123456789
//   uuuuu
// + mmmmm
// + mmm
// +   ddd
// + ddd
// = 77777nnnnn
TEST(ExtensionOmniboxTest, DescriptionStylesCombine2) {
  base::Value::List list =
      base::Value::List().Append(42).Append(base::Value::List().Append(
          base::Value::Dict()
              .Set("content", "content")
              .Set("description", "description")
              .Set("descriptionStyles", base::Value::List()
                                            .Append(base::Value::Dict()
                                                        .Set("type", "url")
                                                        .Set("offset", 0)
                                                        .Set("length", 5))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "match")
                                                        .Set("offset", 0)
                                                        .Set("length", 5))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "match")
                                                        .Set("offset", 0)
                                                        .Set("length", 3))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "dim")
                                                        .Set("offset", 2)
                                                        .Set("length", 3))
                                            .Append(base::Value::Dict()
                                                        .Set("type", "dim")
                                                        .Set("offset", 0)
                                                        .Set("length", 3)))));

  ACMatchClassifications styles_expected;
  styles_expected.push_back(ACMatchClassification(0, kUrl | kMatch | kDim));
  styles_expected.push_back(ACMatchClassification(5, kNone));

  std::optional<SendSuggestions::Params> params =
      SendSuggestions::Params::Create(list);
  EXPECT_TRUE(params);
  ASSERT_FALSE(params->suggest_results.empty());
  CompareClassification(styles_expected, StyleTypesToACMatchClassifications(
                                             params->suggest_results[0]));
}

//   0123456789
//   uuuuu
// + mmmmm
// + mmm
// +   ddd
// + ddd
// = 77777nnnnn
TEST(ExtensionOmniboxTest, DefaultSuggestResult) {
  // Default suggestions should not have a content parameter.
  base::Value::List list = base::Value::List().Append(
      base::Value::Dict()
          .Set("description", "description")
          .Set("descriptionStyles", base::Value::List()
                                        .Append(base::Value::Dict()
                                                    .Set("type", "url")
                                                    .Set("offset", 0)
                                                    .Set("length", 5))
                                        .Append(base::Value::Dict()
                                                    .Set("type", "match")
                                                    .Set("offset", 0)
                                                    .Set("length", 5))
                                        .Append(base::Value::Dict()
                                                    .Set("type", "match")
                                                    .Set("offset", 0)
                                                    .Set("length", 3))
                                        .Append(base::Value::Dict()
                                                    .Set("type", "dim")
                                                    .Set("offset", 2)
                                                    .Set("length", 3))
                                        .Append(base::Value::Dict()
                                                    .Set("type", "dim")
                                                    .Set("offset", 0)
                                                    .Set("length", 3))));

  std::optional<SetDefaultSuggestion::Params> params =
      SetDefaultSuggestion::Params::Create(list);
  EXPECT_TRUE(params);
}

}  // namespace extensions
