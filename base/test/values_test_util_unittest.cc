// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"

#include "base/types/optional_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

using testing::Not;

TEST(ValuesTestUtilTest, DictionaryHasValue) {
  // Identical field value is ok.
  EXPECT_THAT(
      ParseJson(R"json({"foo": {"bar": "baz"}})json"),
      DictionaryHasValue("foo", ParseJson(R"json({"bar": "baz"})json")));
  EXPECT_THAT(
      ParseJson(R"json({"foo": ["bar", "baz"]})json"),
      DictionaryHasValue("foo", ParseJson(R"json(["bar", "baz"])json")));

  // Other top-level keys are ignored.
  EXPECT_THAT(
      ParseJson(R"json({"foo": {"bar": "baz"}, "unused": 123})json"),
      DictionaryHasValue("foo", ParseJson(R"json({"bar": "baz"})json")));

  // Extra nested dict fields are not ok.
  EXPECT_THAT(
      ParseJson(R"json({"foo": {"bar": "baz", "unused": 123}})json"),
      Not(DictionaryHasValue("foo", ParseJson(R"json({"bar": "baz"})json"))));
  // Extra nested list elements are not ok.
  EXPECT_THAT(
      ParseJson(R"json({"foo": ["bar", "baz", "unused", 123]})json"),
      Not(DictionaryHasValue("foo", ParseJson(R"json(["bar", "baz"])json"))));

  // Dict argument also works.
  EXPECT_THAT(
      ParseJsonDict(R"json({"foo": {"bar": "baz"}})json"),
      DictionaryHasValue("foo", ParseJson(R"json({"bar": "baz"})json")));

  // Wrong types.
  EXPECT_THAT(
      ParseJson(R"json("foo")json"),
      Not(DictionaryHasValue("foo", ParseJson(R"json({"bar": "baz"})json"))));
  EXPECT_THAT(
      ParseJson(R"json(["foo"])json"),
      Not(DictionaryHasValue("foo", ParseJson(R"json({"bar": "baz"})json"))));
}

TEST(ValuesTestUtilTest, DictionaryHasValues) {
  // Identical value is ok.
  const DictValue template_dict =
      ParseJsonDict(R"json({"foo": {"bar": "baz"}})json");
  EXPECT_THAT(template_dict, DictionaryHasValues(template_dict));

  // Value argument also works.
  EXPECT_THAT(Value(template_dict.Clone()), DictionaryHasValues(template_dict));

  // Non-dict values are not ok.
  EXPECT_THAT(Value(ListValue()), Not(DictionaryHasValues(template_dict)));

  // Extra top-level dict fields are ok.
  EXPECT_THAT(ParseJson(R"json({"foo": {"bar": "baz"}, "unused": 2})json"),
              DictionaryHasValues(template_dict));
  // Extra nested dict fields are not ok.
  EXPECT_THAT(ParseJson(R"json({"foo": {"bar": "baz", "blah": true}})json"),
              Not(DictionaryHasValues(template_dict)));

  // Wrong type.
  EXPECT_THAT(ParseJson("3"), Not(DictionaryHasValues(DictValue())));
}

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

  EXPECT_THAT(
      ParseJson(R"json([{"bar": "baz", "list": [1, 2, 3, 100]}, 3, 42])json"),
      IsSupersetOfValue(R"json([{"bar": "baz"}, 42])json"));
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

TEST(ValuesTestUtilTest, IsJson) {
  // Positive tests: Value literals and serialized JSON strings.
  EXPECT_THAT(Value(3), IsJson(Value(3)));
  EXPECT_THAT(Value(3), IsJson("3"));

  EXPECT_THAT(Value("foo"), IsJson(Value("foo")));
  EXPECT_THAT(Value("foo"), IsJson("\"foo\""));

  EXPECT_THAT(DictValue().Set("foo", "bar"),
              IsJson(DictValue().Set("foo", "bar")));
  EXPECT_THAT(DictValue().Set("foo", "bar"),
              IsJson(R"json({"foo": "bar"})json"));

  EXPECT_THAT(ListValue().Append("foo").Append("bar"),
              IsJson(ListValue().Append("foo").Append("bar")));
  EXPECT_THAT(ListValue().Append("foo").Append("bar"),
              IsJson(R"json(["foo", "bar"])json"));

  // Negative tests: value mismatches.
  EXPECT_THAT(Value(4), Not(IsJson("3")));
  EXPECT_THAT(Value("bar"), Not(IsJson("\"foo\"")));
  EXPECT_THAT(DictValue().Set("baz", "quux"),
              Not(IsJson(R"json({"foo": "bar"})json")));
  EXPECT_THAT(ListValue().Append("foo").Append("quux"),
              Not(IsJson(R"json(["foo", "bar"])json")));

  // Negative tests: type mismatches.
  EXPECT_THAT(DictValue(), Not(IsJson("3")));
  EXPECT_THAT(DictValue(), Not(IsJson("\"foo\"")));
  EXPECT_THAT(ListValue(), Not(IsJson(R"json({"foo": "bar"})json")));
  EXPECT_THAT(DictValue(), Not(IsJson(R"json(["foo", "bar"])json")));
}

TEST(ValuesTestUtilTest, ParseJson) {
  EXPECT_EQ(ParseJson("3"), base::Value(3));
  EXPECT_NE(ParseJson("4"), base::Value(3));

  EXPECT_EQ(ParseJson("\"foo\""), base::Value("foo"));
  EXPECT_NE(ParseJson("\"bar\""), base::Value("foo"));

  EXPECT_EQ(ParseJson(R"json({"foo": "bar"})json"),
            base::DictValue().Set("foo", "bar"));
  EXPECT_NE(ParseJson(R"json({"bar": "baz"})json"),
            base::DictValue().Set("foo", "bar"));

  EXPECT_EQ(ParseJson(R"json(["foo", "bar"])json"),
            base::ListValue().Append("foo").Append("bar"));
  EXPECT_NE(ParseJson(R"json(["bar", "baz"])json"),
            base::ListValue().Append("foo").Append("bar"));

  EXPECT_NONFATAL_FAILURE(ParseJson("not json"),
                          R"(Failed to parse "not json")");
}

TEST(ValuesTestUtilTest, ParseJsonDict) {
  EXPECT_EQ(ParseJsonDict(R"json({"foo": "bar"})json"),
            base::DictValue().Set("foo", "bar"));
  EXPECT_NE(ParseJsonDict(R"json({"bar": "baz"})json"),
            base::DictValue().Set("foo", "bar"));

  EXPECT_NONFATAL_FAILURE(ParseJsonDict(R"json(["foo", "bar"])json"),
                          R"(JSON is of wrong type: ["foo", "bar"])");
  EXPECT_NONFATAL_FAILURE(ParseJsonDict("not json"),
                          R"(Failed to parse "not json")");
}

TEST(ValuesTestUtilTest, ParseJsonList) {
  EXPECT_EQ(ParseJsonList(R"json(["foo", "bar"])json"),
            base::ListValue().Append("foo").Append("bar"));
  EXPECT_NE(ParseJsonList(R"json(["bar", "baz"])json"),
            base::ListValue().Append("foo").Append("bar"));

  EXPECT_NONFATAL_FAILURE(ParseJsonList(R"json({"foo": "bar"})json"),
                          R"(JSON is of wrong type: {"foo": "bar"})");
  EXPECT_NONFATAL_FAILURE(ParseJsonDict("not json"),
                          R"(Failed to parse "not json")");
}

}  // namespace base::test
