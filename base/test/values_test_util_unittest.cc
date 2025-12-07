// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"

#include "base/types/optional_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

using testing::Not;

TEST(ValuesTestUtilTest, IsSupersetOfValue_Supersets) {
  // Identical value is ok.
  const Value template_dict = ParseJson(R"json({"foo": [{"bar": "baz"}]})json");
  const Value template_list =
      ParseJson(R"json([{"bar": "baz", "list": [1, 2, 3]}, 3, 42])json");
  EXPECT_THAT(template_dict, IsSupersetOfValue(template_dict));
  EXPECT_THAT(template_list, IsSupersetOfValue(template_list));

  // Extra top-level dict fields are ok.
  EXPECT_THAT(
      ParseJson(
          R"json({"foo": [{"bar": "baz", "blah": true}], "unused": 2})json"),
      IsSupersetOfValue(template_dict));
  // Extra nested dict fields are ok.
  EXPECT_THAT(
      ParseJson(
          R"json({"foo": [{"bar": "baz", "blah": true, "unused": 2}]})json"),
      IsSupersetOfValue(template_dict));

  // Extra top-level list elements are ok.
  EXPECT_THAT(
      ParseJson(R"json([{"bar": "baz", "list": [1, 2, 3]}, 3, 42, 100])json"),
      IsSupersetOfValue(template_list));
  // Extra nested list elements are ok.
  EXPECT_THAT(
      ParseJson(R"json([{"bar": "baz", "list": [1, 2, 3, 100]}, 3, 42])json"),
      IsSupersetOfValue(template_list));
}

TEST(ValuesTestUtilTest, IsSupersetOfValue_Subsets) {
  const Value template_dict =
      ParseJson(R"json({"foo": [{"bar": "baz"}, 3], "zip": "zap"})json");

  const Value template_list =
      ParseJson(R"json([{"bar": "baz", "list": [1, 2, 3]}, 3, 42])json");

  // Missing top-level list element.
  EXPECT_THAT(ParseJson(R"json([{"bar": "baz", "list": [1, 2, 3]}, 3])json"),
              Not(IsSupersetOfValue(template_list)));
  // Missing nested list element.
  EXPECT_THAT(ParseJson(R"json([{"bar": "baz", "list": [1, 3]}, 3, 42])json"),
              Not(IsSupersetOfValue(template_list)));

  // Missing top-level field.
  EXPECT_THAT(ParseJson(R"json({"foo": [{"bar": "baz"}, 3]})json"),
              Not(IsSupersetOfValue(template_dict)));

  // Missing field in nested dict.
  EXPECT_THAT(ParseJson(R"json({"bar": "baz", "nested": {}})json"),
              Not(IsSupersetOfValue(ParseJson(
                  R"json({"nested": {"missing": 3}, "bar": "baz"})json"))));

  // Missing field in nested dict inside a list.
  EXPECT_THAT(
      ParseJson(R"json({"bar": "baz", "nested": [{"unused": true}]})json"),
      Not(IsSupersetOfValue(
          ParseJson(R"json({"nested": [{"missing": 3}], "bar": "baz"})json"))));
}

TEST(ValuesTestUtilTest, IsSupersetOfValue_TypeMismatch) {
  // Wrong top-level type.
  EXPECT_THAT(ParseJson("3"), Not(IsSupersetOfValue(ParseJson(
                                  R"json({"foo": [{"bar": "baz"}]})json"))));

  // Wrong nested type.
  EXPECT_THAT(ParseJson(R"json({"foo": false})json"),
              Not(IsSupersetOfValue(ParseJson(R"json({"foo": "bar"})json"))));

  EXPECT_THAT(ParseJson("3"), Not(IsSupersetOfValue(ParseJsonList("[]"))));
  EXPECT_THAT(ParseJsonDict("{}"), Not(IsSupersetOfValue(ParseJsonList("[]"))));
  EXPECT_THAT(ParseJsonList("[2]"),
              Not(IsSupersetOfValue(ParseJsonList("[1, 2, true]"))));
}

}  // namespace base::test
