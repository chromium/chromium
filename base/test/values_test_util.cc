// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"

#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

namespace {

std::string FormatAsJSON(ValueView value) {
  std::string json;
  JSONWriter::Write(value, &json);
  return json;
}

// Attempts to parse `json` as JSON. Returns resulting Value on success, has an
// EXPECT failure and returns nullopt on failure. If `expected_type` is
// provided, treats `json` parsing as a Value of a different type as a failure.
//
std::optional<Value> ParseJsonHelper(std::string_view json,
                                     std::optional<Value::Type> expected_type,
                                     int options) {
  auto result = JSONReader::ReadAndReturnValueWithError(json, options);
  if (!result.has_value()) {
    ADD_FAILURE() << "Failed to parse \"" << json
                  << "\": " << result.error().message;
    return std::nullopt;
  }
  if (expected_type && result->type() != *expected_type) {
    ADD_FAILURE() << "JSON is of wrong type: " << json;
    return std::nullopt;
  }
  return std::move(*result);
}

bool CheckValue(const Value::Dict& dict,
                std::string_view template_key,
                const Value& template_value,
                testing::MatchResultListener* listener) {
  const Value* sub_value = dict.Find(template_key);
  if (!sub_value) {
    *listener << "\nDictionary does not have key '" << template_key << "'";
    return false;
  }
  if (*sub_value != template_value) {
    *listener << "\nDictionary value under key '" << template_key << "' is '"
              << FormatAsJSON(*sub_value) << "', expected '"
              << FormatAsJSON(template_value) << "'";
    return false;
  }
  return true;
}

}  // namespace

namespace internal {

DictionaryHasValueMatcher::DictionaryHasValueMatcher(
    std::string key,
    const Value& expected_value)
    : key_(std::move(key)), expected_value_(expected_value.Clone()) {}

DictionaryHasValueMatcher::DictionaryHasValueMatcher(std::string key,
                                                     Value&& expected_value)
    : key_(std::move(key)), expected_value_(std::move(expected_value)) {}

DictionaryHasValueMatcher::DictionaryHasValueMatcher(
    const DictionaryHasValueMatcher& other)
    : key_(other.key_), expected_value_(other.expected_value_.Clone()) {}

DictionaryHasValueMatcher& DictionaryHasValueMatcher::operator=(
    const DictionaryHasValueMatcher& other) {
  expected_value_ = other.expected_value_.Clone();
  return *this;
}

DictionaryHasValueMatcher::~DictionaryHasValueMatcher() = default;

bool DictionaryHasValueMatcher::MatchAndExplain(
    const Value& value,
    testing::MatchResultListener* listener) const {
  if (!value.is_dict()) {
    *listener << "Value is not a dictionary: " << FormatAsJSON(value);
    return false;
  }
  return MatchAndExplain(value.GetDict(), listener);
}

bool DictionaryHasValueMatcher::MatchAndExplain(
    const Value::Dict& dict,
    testing::MatchResultListener* listener) const {
  return CheckValue(dict, key_, expected_value_, listener);
}

void DictionaryHasValueMatcher::DescribeTo(std::ostream* os) const {
  *os << "has key '" << key_ << "' with value '"
      << FormatAsJSON(expected_value_) << "'";
}

void DictionaryHasValueMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "does not have key '" << key_ << "' with value '"
      << FormatAsJSON(expected_value_) << "'";
}

DictionaryHasValuesMatcher::DictionaryHasValuesMatcher(
    const Value::Dict& template_value)
    : template_value_(template_value.Clone()) {}

DictionaryHasValuesMatcher::DictionaryHasValuesMatcher(
    Value::Dict&& template_value)
    : template_value_(std::move(template_value)) {}

DictionaryHasValuesMatcher::DictionaryHasValuesMatcher(
    const DictionaryHasValuesMatcher& other)
    : template_value_(other.template_value_.Clone()) {}

DictionaryHasValuesMatcher& DictionaryHasValuesMatcher::operator=(
    const DictionaryHasValuesMatcher& other) {
  template_value_ = other.template_value_.Clone();
  return *this;
}

DictionaryHasValuesMatcher::~DictionaryHasValuesMatcher() = default;

bool DictionaryHasValuesMatcher::MatchAndExplain(
    const Value& value,
    testing::MatchResultListener* listener) const {
  if (!value.is_dict()) {
    *listener << "Value is not a dictionary: " << FormatAsJSON(value);
    return false;
  }
  return MatchAndExplain(value.GetDict(), listener);
}

bool DictionaryHasValuesMatcher::MatchAndExplain(
    const Value::Dict& dict,
    testing::MatchResultListener* listener) const {
  bool ok = true;
  for (auto [template_key, template_value] : template_value_) {
    ok &= CheckValue(dict, template_key, template_value, listener);
  }
  return ok;
}

void DictionaryHasValuesMatcher::DescribeTo(std::ostream* os) const {
  *os << "contains all key-values from '" << FormatAsJSON(template_value_)
      << "'";
}

void DictionaryHasValuesMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "does not contain key-values from '" << FormatAsJSON(template_value_)
      << "'";
}

IsSupersetOfValueMatcher::IsSupersetOfValueMatcher(const Value& template_value)
    : template_value_(template_value.Clone()) {}

IsSupersetOfValueMatcher::IsSupersetOfValueMatcher(
    const Value::Dict& template_value)
    : template_value_(template_value.Clone()) {}

IsSupersetOfValueMatcher::IsSupersetOfValueMatcher(
    const Value::List& template_value)
    : template_value_(template_value.Clone()) {}

IsSupersetOfValueMatcher::IsSupersetOfValueMatcher(Value&& template_value)
    : template_value_(std::move(template_value)) {}

IsSupersetOfValueMatcher::IsSupersetOfValueMatcher(Value::Dict&& template_value)
    : template_value_(std::move(template_value)) {}

IsSupersetOfValueMatcher::IsSupersetOfValueMatcher(Value::List&& template_value)
    : template_value_(std::move(template_value)) {}

IsSupersetOfValueMatcher::IsSupersetOfValueMatcher(
    const IsSupersetOfValueMatcher& other)
    : template_value_(other.template_value_.Clone()) {}

IsSupersetOfValueMatcher& IsSupersetOfValueMatcher::operator=(
    const IsSupersetOfValueMatcher& other) {
  template_value_ = other.template_value_.Clone();
  return *this;
}

IsSupersetOfValueMatcher::~IsSupersetOfValueMatcher() = default;

bool IsSupersetOfValueMatcher::MatchAndExplain(
    const Value& value,
    testing::MatchResultListener* listener) const {
  if (value.type() != template_value_.type()) {
    return testing::ExplainMatchResult(
        testing::Eq(Value::GetTypeName(template_value_.type())),
        Value::GetTypeName(value.type()), listener);
  }
  switch (value.type()) {
    case Value::Type::NONE:
    case Value::Type::BOOLEAN:
    case Value::Type::INTEGER:
    case Value::Type::STRING:
    case Value::Type::BINARY:
      return testing::ExplainMatchResult(
          testing::Eq(std::cref(template_value_)), value, listener);
    case Value::Type::DOUBLE:
      return testing::ExplainMatchResult(
          testing::DoubleEq(template_value_.GetDouble()), value.GetDouble(),
          listener);
    case Value::Type::DICT:
      return MatchAndExplain(value.GetDict(), listener);
    case Value::Type::LIST:
      return MatchAndExplain(value.GetList(), listener);
  }
}

bool IsSupersetOfValueMatcher::MatchAndExplain(
    const Value::Dict& dict,
    testing::MatchResultListener* listener) const {
  if (template_value_.type() != Value::Type::DICT) {
    return testing::ExplainMatchResult(
        testing::Eq(Value::GetTypeName(template_value_.type())),
        Value::GetTypeName(Value::Type::DICT), listener);
  }

  std::vector<testing::Matcher<const Value::Dict&>> matchers;
  for (auto [field_name, field_value] : template_value_.GetDict()) {
    matchers.push_back(testing::ResultOf(
        StrCat({"field '", field_name, "'"}),
        [field_name](const Value::Dict& dict) { return dict.Find(field_name); },
        testing::Pointee(IsSupersetOfValue(field_value))));
  }
  return testing::ExplainMatchResult(testing::AllOfArray(matchers), dict,
                                     listener);
}

bool IsSupersetOfValueMatcher::MatchAndExplain(
    const Value::List& list,
    testing::MatchResultListener* listener) const {
  if (template_value_.type() != Value::Type::LIST) {
    return testing::ExplainMatchResult(
        testing::Eq(Value::GetTypeName(template_value_.type())),
        Value::GetTypeName(Value::Type::LIST), listener);
  }

  std::vector<testing::Matcher<const Value&>> matchers;
  for (const auto& e : template_value_.GetList()) {
    matchers.push_back(IsSupersetOfValue(e));
  }
  return testing::ExplainMatchResult(testing::IsSupersetOf(matchers), list,
                                     listener);
}

void IsSupersetOfValueMatcher::DescribeTo(std::ostream* os) const {
  switch (template_value_.type()) {
    case Value::Type::NONE:
    case Value::Type::BOOLEAN:
    case Value::Type::INTEGER:
    case Value::Type::DOUBLE:
    case Value::Type::STRING:
    case Value::Type::BINARY:
      *os << "equals '" << FormatAsJSON(template_value_) << "'";
      return;
    case Value::Type::DICT:
    case Value::Type::LIST:
      *os << "is a superset of '" << FormatAsJSON(template_value_) << "'";
      return;
  }
  NOTREACHED();
}

void IsSupersetOfValueMatcher::DescribeNegationTo(std::ostream* os) const {
  switch (template_value_.type()) {
    case Value::Type::NONE:
    case Value::Type::BOOLEAN:
    case Value::Type::INTEGER:
    case Value::Type::DOUBLE:
    case Value::Type::STRING:
    case Value::Type::BINARY:
      *os << "does not equal '" << FormatAsJSON(template_value_) << "'";
      return;
    case Value::Type::DICT:
    case Value::Type::LIST:
      *os << "is not a superset of '" << FormatAsJSON(template_value_) << "'";
      return;
  }
  NOTREACHED();
}

IsJsonMatcher::IsJsonMatcher(std::string_view json)
    : expected_value_(test::ParseJson(json)) {}

IsJsonMatcher::IsJsonMatcher(const Value& value)
    : expected_value_(value.Clone()) {}

IsJsonMatcher::IsJsonMatcher(const Value::Dict& value)
    : expected_value_(Value(value.Clone())) {}

IsJsonMatcher::IsJsonMatcher(const Value::List& value)
    : expected_value_(Value(value.Clone())) {}

IsJsonMatcher::IsJsonMatcher(Value&& value)
    : expected_value_(std::move(value)) {}

IsJsonMatcher::IsJsonMatcher(Value::Dict&& value)
    : expected_value_(Value(std::move(value))) {}

IsJsonMatcher::IsJsonMatcher(Value::List&& value)
    : expected_value_(Value(std::move(value))) {}

IsJsonMatcher::IsJsonMatcher(const IsJsonMatcher& other)
    : expected_value_(other.expected_value_.Clone()) {}

IsJsonMatcher& IsJsonMatcher::operator=(const IsJsonMatcher& other) {
  expected_value_ = other.expected_value_.Clone();
  return *this;
}

IsJsonMatcher::~IsJsonMatcher() = default;

bool IsJsonMatcher::MatchAndExplain(
    std::string_view json,
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
    const Value& value,
    testing::MatchResultListener* /* listener */) const {
  return expected_value_ == value;
}

bool IsJsonMatcher::MatchAndExplain(
    const Value::Dict& dict,
    testing::MatchResultListener* /* listener */) const {
  return expected_value_.is_dict() && expected_value_.GetDict() == dict;
}

bool IsJsonMatcher::MatchAndExplain(
    const Value::List& list,
    testing::MatchResultListener* /* listener */) const {
  return expected_value_.is_list() && expected_value_.GetList() == list;
}

void IsJsonMatcher::DescribeTo(std::ostream* os) const {
  *os << "is the JSON value " << expected_value_;
}

void IsJsonMatcher::DescribeNegationTo(std::ostream* os) const {
  *os << "is not the JSON value " << expected_value_;
}

}  // namespace internal

Value ParseJson(std::string_view json, int options) {
  std::optional<Value> result =
      ParseJsonHelper(json, /*expected_type=*/std::nullopt, options);
  return result.has_value() ? std::move(*result) : Value();
}

Value::Dict ParseJsonDict(std::string_view json, int options) {
  std::optional<Value> result =
      ParseJsonHelper(json, /*expected_type=*/Value::Type::DICT, options);
  return result.has_value() ? std::move(*result).TakeDict() : Value::Dict();
}

Value::List ParseJsonList(std::string_view json, int options) {
  std::optional<Value> result =
      ParseJsonHelper(json, /*expected_type=*/Value::Type::LIST, options);
  return result.has_value() ? std::move(*result).TakeList() : Value::List();
}

Value::Dict ParseJsonDictFromFile(const FilePath& json_file_path) {
  std::string json;
  if (!ReadFileToString(json_file_path, &json)) {
    ADD_FAILURE() << "Failed to load json file for parsing. path="
                  << json_file_path;
    return {};
  }
  return ParseJsonDict(json);
}

expected<void, WriteJsonError> WriteJsonFile(const FilePath& json_file_path,
                                             ValueView root) {
  std::string json;
  if (!JSONWriter::Write(root, &json)) {
    return unexpected(WriteJsonError::kGenerateJsonFailure);
  }
  if (!WriteFile(json_file_path, json)) {
    return unexpected(WriteJsonError::kWriteFileFailure);
  }
  return {};
}

}  // namespace base::test
