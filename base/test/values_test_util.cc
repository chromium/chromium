// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"

#include <ostream>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/optional_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

void ExpectDictBooleanValue(bool expected_value,
                            const Value::Dict& dict,
                            StringPiece path) {
  EXPECT_EQ(dict.FindBoolByDottedPath(path),
            absl::make_optional(expected_value))
      << path;
}

void ExpectDictIntegerValue(int expected_value,
                            const Value::Dict& dict,
                            StringPiece path) {
  EXPECT_EQ(dict.FindIntByDottedPath(path), absl::make_optional(expected_value))
      << path;
}

void ExpectDictStringValue(StringPiece expected_value,
                           const Value::Dict& dict,
                           StringPiece path) {
  EXPECT_EQ(OptionalFromPtr(dict.FindStringByDottedPath(path)),
            absl::make_optional(expected_value))
      << path;
}

void ExpectDictValue(const Value::Dict& expected_value,
                     const Value::Dict& dict,
                     StringPiece path) {
  const Value* found_value = dict.FindByDottedPath(path);
  ASSERT_TRUE(found_value) << path;
  EXPECT_EQ(*found_value, expected_value) << path;
}

void ExpectDictValue(const Value& expected_value,
                     const Value::Dict& dict,
                     StringPiece path) {
  const Value* found_value = dict.FindByDottedPath(path);
  ASSERT_TRUE(found_value) << path;
  EXPECT_EQ(*found_value, expected_value) << path;
}

void ExpectStringValue(const std::string& expected_str, const Value& actual) {
  EXPECT_EQ(Value::Type::STRING, actual.type());
  EXPECT_EQ(expected_str, actual.GetString());
}

namespace test {

namespace {

std::string FormatAsJSON(const base::Value& value) {
  std::string json;
  JSONWriter::Write(value, &json);
  return json;
}

class DictionaryHasValueMatcher
    : public testing::MatcherInterface<const base::Value&> {
 public:
  DictionaryHasValueMatcher(const std::string& key,
                            const base::Value& expected_value)
      : key_(key), expected_value_(expected_value.Clone()) {}

  ~DictionaryHasValueMatcher() = default;

  bool MatchAndExplain(const base::Value& value,
                       testing::MatchResultListener* listener) const override {
    if (!value.is_dict()) {
      *listener << "The value '" << FormatAsJSON(value)
                << "' is not a dictionary";
      return false;
    }
    const base::Value* sub_value = value.GetDict().Find(key_);
    if (!sub_value) {
      *listener << "Dictionary '" << FormatAsJSON(value)
                << "' does not have key '" << key_ << "'";
      return false;
    }
    if (*sub_value != expected_value_) {
      *listener << "Dictionary value under key '" << key_ << "' is '"
                << FormatAsJSON(*sub_value) << "', expected '"
                << FormatAsJSON(expected_value_) << "'";
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "has key '" << key_ << "' with value '"
        << FormatAsJSON(expected_value_) << "'";
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "does not have key '" << key_ << "' with value '"
        << FormatAsJSON(expected_value_) << "'";
  }

 private:
  DictionaryHasValueMatcher& operator=(const DictionaryHasValueMatcher& other) =
      delete;

  const std::string key_;
  const base::Value expected_value_;
};

class DictionaryHasValuesMatcher
    : public testing::MatcherInterface<const base::Value&> {
 public:
  DictionaryHasValuesMatcher(const base::Value& template_value)
      : template_value_(template_value.Clone()) {
    CHECK(template_value.is_dict());
  }

  ~DictionaryHasValuesMatcher() = default;

  bool MatchAndExplain(const base::Value& value,
                       testing::MatchResultListener* listener) const override {
    if (!value.is_dict()) {
      *listener << "The value '" << FormatAsJSON(value)
                << "' is not a dictionary";
      return false;
    }

    bool ok = true;
    for (auto template_dict_item : template_value_.GetDict()) {
      const base::Value* sub_value =
          value.GetDict().Find(template_dict_item.first);
      if (!sub_value) {
        *listener << "\nDictionary does not have key '"
                  << template_dict_item.first << "'";
        ok = false;
        continue;
      }
      if (*sub_value != template_dict_item.second) {
        *listener << "\nDictionary value under key '"
                  << template_dict_item.first << "' is '"
                  << FormatAsJSON(*sub_value) << "', expected '"
                  << FormatAsJSON(template_dict_item.second) << "'";
        ok = false;
      }
    }
    return ok;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "contains all key-values from '" << FormatAsJSON(template_value_)
        << "'";
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "does not contain key-values from '" << FormatAsJSON(template_value_)
        << "'";
  }

 private:
  DictionaryHasValueMatcher& operator=(const DictionaryHasValueMatcher& other) =
      delete;

  const base::Value template_value_;
};

// Attempts to parse `json` as JSON. Returns resulting Value on success, has an
// EXPECT failure and returns nullopt on failure. If `expected_type` is
// provided, treats `json` parsing as a Value of a different type as a failure.
//
absl::optional<Value> ParseJsonHelper(
    StringPiece json,
    absl::optional<Value::Type> expected_type) {
  auto result = JSONReader::ReadAndReturnValueWithError(
      json, JSON_PARSE_CHROMIUM_EXTENSIONS | JSON_ALLOW_TRAILING_COMMAS);
  if (!result.has_value()) {
    ADD_FAILURE() << "Failed to parse \"" << json
                  << "\": " << result.error().message;
    return absl::nullopt;
  }
  if (expected_type && result->type() != *expected_type) {
    ADD_FAILURE() << "JSON is of wrong type: " << json;
    return absl::nullopt;
  }
  return std::move(*result);
}

}  // namespace

testing::Matcher<const base::Value&> DictionaryHasValue(
    const std::string& key,
    const base::Value& expected_value) {
  return testing::MakeMatcher(
      new DictionaryHasValueMatcher(key, expected_value));
}

testing::Matcher<const base::Value&> DictionaryHasValues(
    const base::Value& template_value) {
  return testing::MakeMatcher(new DictionaryHasValuesMatcher(template_value));
}

IsJsonMatcher::IsJsonMatcher(base::StringPiece json)
    : expected_value_(test::ParseJson(json)) {}

IsJsonMatcher::IsJsonMatcher(const base::Value& value)
    : expected_value_(value.Clone()) {}

IsJsonMatcher::IsJsonMatcher(const IsJsonMatcher& other)
    : expected_value_(other.expected_value_.Clone()) {}

IsJsonMatcher::~IsJsonMatcher() = default;

bool IsJsonMatcher::MatchAndExplain(
    base::StringPiece json,
    testing::MatchResultListener* listener) const {
  // This is almost the same logic as ParseJson, but the parser uses stricter
  // options for JSON data that is assumed to be generated by the code under
  // test rather than written by hand as part of a unit test.
  auto ret = JSONReader::ReadAndReturnValueWithError(json, JSON_PARSE_RFC);
  if (!ret.has_value()) {
    *listener << "Failed to parse \"" << json << "\": " << ret.error().message;
    return false;
  }
  return MatchAndExplain(*ret, listener);
}

bool IsJsonMatcher::MatchAndExplain(
    const base::Value& value,
    testing::MatchResultListener* /* listener */) const {
  return expected_value_ == value;
}

bool IsJsonMatcher::MatchAndExplain(
    const base::Value::Dict& dict,
    testing::MatchResultListener* /* listener */) const {
  return expected_value_.is_dict() && expected_value_.GetDict() == dict;
}

bool IsJsonMatcher::MatchAndExplain(
    const base::Value::List& list,
    testing::MatchResultListener* /* listener */) const {
  return expected_value_.is_list() && expected_value_.GetList() == list;
}

void IsJsonMatcher::DescribeTo(std::ostream* os) const {
  *os << "is the JSON value " << expected_value_;
}

void IsJsonMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "is not the JSON value " << expected_value_;
}

Value ParseJson(StringPiece json) {
  absl::optional<Value> result =
      ParseJsonHelper(json, /*expected_type=*/absl::nullopt);
  return result.has_value() ? std::move(*result) : Value();
}

Value::Dict ParseJsonDict(StringPiece json) {
  absl::optional<Value> result =
      ParseJsonHelper(json, /*expected_type=*/Value::Type::DICT);
  return result.has_value() ? std::move(*result).TakeDict() : Value::Dict();
}

Value::List ParseJsonList(StringPiece json) {
  absl::optional<Value> result =
      ParseJsonHelper(json, /*expected_type=*/Value::Type::LIST);
  return result.has_value() ? std::move(*result).TakeList() : Value::List();
}

}  // namespace test
}  // namespace base
