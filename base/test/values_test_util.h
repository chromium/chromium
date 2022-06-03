// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_VALUES_TEST_UTIL_H_
#define BASE_TEST_VALUES_TEST_UTIL_H_

#include <iosfwd>
#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace base {

// All the functions below expect that the value for the given key in
// the given dictionary equals the given expected value.

void ExpectDictBooleanValue(bool expected_value,
                            const Value& value,
                            const std::string& key);

void ExpectDictIntegerValue(int expected_value,
                            const Value& value,
                            const std::string& key);

void ExpectDictStringValue(const std::string& expected_value,
                           const Value& value,
                           const std::string& key);

void ExpectDictValue(const Value& expected_value,
                     const Value& value,
                     const std::string& key);

void ExpectStringValue(const std::string& expected_str, const Value& actual);

namespace test {

// A custom GMock matcher which matches if a base::Value is a dictionary which
// has a key |key| that is equal to |value|.
testing::Matcher<const base::Value&> DictionaryHasValue(
    const std::string& key,
    const base::Value& expected_value);

// A custom GMock matcher which matches if a base::Value is a dictionary which
// contains all key/value pairs from |template_value|.
testing::Matcher<const base::Value&> DictionaryHasValues(
    const base::Value& template_value);

// A custom GMock matcher.  For details, see
// https://github.com/google/googletest/blob/644319b9f06f6ca9bf69fe791be399061044bc3d/googlemock/docs/CookBook.md#writing-new-polymorphic-matchers
class IsJsonMatcher {
 public:
  explicit IsJsonMatcher(base::StringPiece json);
  explicit IsJsonMatcher(const base::Value& value);
  IsJsonMatcher(const IsJsonMatcher& other);
  ~IsJsonMatcher();

  bool MatchAndExplain(base::StringPiece json,
                       testing::MatchResultListener* listener) const;
  bool MatchAndExplain(const base::Value& value,
                       testing::MatchResultListener* listener) const;
  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(std::ostream* os) const;

 private:
  IsJsonMatcher& operator=(const IsJsonMatcher& other) = delete;

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

// Parses |json| as JSON, allowing trailing commas, and returns the resulting
// value.  If |json| fails to parse, causes an EXPECT failure and returns the
// Null Value.
Value ParseJson(StringPiece json);

// DEPRECATED.
// Parses |json| as JSON, allowing trailing commas, and returns the
// resulting value.  If the json fails to parse, causes an EXPECT
// failure and returns the Null Value (but never a NULL pointer).
std::unique_ptr<Value> ParseJsonDeprecated(StringPiece json);

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_VALUES_TEST_UTIL_H_
