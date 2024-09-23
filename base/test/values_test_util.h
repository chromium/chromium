// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_VALUES_TEST_UTIL_H_
#define BASE_TEST_VALUES_TEST_UTIL_H_

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace base {

// All the functions below expect that the value for the given path in
// the given dictionary equals the given expected value.

void ExpectDictBooleanValue(bool expected_value,
                            const Value::Dict& dict,
                            std::string_view path);

void ExpectDictIntegerValue(int expected_value,
                            const Value::Dict& dict,
                            std::string_view path);

void ExpectDictStringValue(std::string_view expected_value,
                           const Value::Dict& dict,
                           std::string_view path);

void ExpectDictValue(const Value::Dict& expected_value,
                     const Value::Dict& dict,
                     std::string_view path);

void ExpectDictValue(const Value& expected_value,
                     const Value::Dict& dict,
                     std::string_view path);

void ExpectStringValue(const std::string& expected_str, const Value& actual);

namespace test {

// A custom GMock matcher which matches if a base::Value::Dict has a key |key|
// that is equal to |value|.
testing::Matcher<const base::Value::Dict&> DictionaryHasValue(
    const std::string& key,
    const base::Value& expected_value);

// A custom GMock matcher which matches if a base::Value::Dict contains all
// key/value pairs from |template_value|.
testing::Matcher<const base::Value::Dict&> DictionaryHasValues(
    const base::Value::Dict& template_value);

// A custom GMock matcher.  For details, see
// https://github.com/google/googletest/blob/644319b9f06f6ca9bf69fe791be399061044bc3d/googlemock/docs/CookBook.md#writing-new-polymorphic-matchers
class IsJsonMatcher {
 public:
  explicit IsJsonMatcher(std::string_view json);
  explicit IsJsonMatcher(const base::Value& value);
  explicit IsJsonMatcher(const base::Value::Dict& value);
  explicit IsJsonMatcher(const base::Value::List& value);

  IsJsonMatcher(const IsJsonMatcher& other);
  IsJsonMatcher& operator=(const IsJsonMatcher& other);

  ~IsJsonMatcher();

  bool MatchAndExplain(std::string_view json,
                       testing::MatchResultListener* listener) const;
  bool MatchAndExplain(const base::Value& value,
                       testing::MatchResultListener* listener) const;
  bool MatchAndExplain(const base::Value::Dict& dict,
                       testing::MatchResultListener* listener) const;
  bool MatchAndExplain(const base::Value::List& list,
                       testing::MatchResultListener* listener) const;
  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(std::ostream* os) const;

 private:
  base::Value expected_value_;
};

// Creates a GMock matcher for testing equivalence of JSON values represented as
// either JSON strings or base::Value objects.  Parsing of the expected value
// uses ParseJson(), which allows trailing commas for convenience.  Parsing of
// the actual value follows the JSON spec strictly.
//
// Although it possible to use this matcher when the actual and expected values
// are both base::Value objects, there is no advantage in that case to using
// this matcher in place of GMock's normal equality semantics.
template <typename T>
inline testing::PolymorphicMatcher<IsJsonMatcher> IsJson(const T& value) {
  return testing::MakePolymorphicMatcher(IsJsonMatcher(value));
}

// Parses `json` as JSON, allowing trailing commas, and returns the resulting
// value.  If `json` fails to parse, causes an EXPECT failure and returns the
// Null Value.
Value ParseJson(std::string_view json);

// Just like ParseJson(), except returns Dicts/Lists. If `json` fails to parse
// or is not of the expected type, causes an EXPECT failure and returns an empty
// container.
Value::Dict ParseJsonDict(std::string_view json);
Value::List ParseJsonList(std::string_view json);

// Similar to `ParseJsonDict`, however it loads its contents from a file.
// Returns the parsed `Value::Dict` when successful. Otherwise, it causes an
// EXPECT failure, and returns an empty dict.
Value::Dict ParseJsonDictFromFile(const FilePath& json_file_path);

// An enumaration with the possible types of errors when calling
// `WriteJsonFile`.
enum class WriteJsonError {
  // Failed to generate a json string with the value provided.
  kGenerateJsonFailure,

  // Failed to write the json string into a file.
  kWriteFileFailure,
};

// Serialises `root` as a json string to a file. Returns a empty expected when
// successful. Otherwise returns an error.
expected<void, WriteJsonError> WriteJsonFile(const FilePath& json_file_path,
                                             ValueView root);

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_VALUES_TEST_UTIL_H_
