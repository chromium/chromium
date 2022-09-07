// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_conversion_helper.h"

#include <stddef.h>

#include <utility>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

TEST(TraceEventArgumentTest, SetTracedValueArgParameterPack) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  TracedValue* raw_value = value.get();
  SetTracedValueArg(raw_value, "not_traced");
  SetTracedValueArg(raw_value, "single_value", 42);
  SetTracedValueArg(raw_value, "pack", 42, false, 0.0, "hello");
  std::string json;
  value->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{"
      "\"single_value\":42,"
      "\"pack\":42,"
      "\"pack\":false,"
      "\"pack\":0.0,"
      "\"pack\":\"hello\""
      "}",
      json);
}

TEST(TraceEventArgumentTest, SetTracedValueArgCompatibleTypes) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  TracedValue* raw_value = value.get();
  SetTracedValueArg(raw_value, "float_literal", 0.0f);
  float my_float = 0.0f;
  SetTracedValueArg(raw_value, "my_float", my_float);
  char my_char = 13;
  SetTracedValueArg(raw_value, "my_char", my_char);
  std::string json;
  value->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{"
      "\"float_literal\":0.0,"
      "\"my_float\":0.0,"
      "\"my_char\":13"
      "}",
      json);
}

class UseFallback {};

TEST(TraceEventArgumentTest, SetTracedValueArgBasicTypes) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  TracedValue* raw_value = value.get();
  SetTracedValueArg(raw_value, "my_int", 1);
  SetTracedValueArg(raw_value, "my_double", 0.1);
  SetTracedValueArg(raw_value, "my_bool", false);
  SetTracedValueArg(raw_value, "my_literal", "hello");
  SetTracedValueArg(raw_value, "my_string",
                    std::string("wonderful_") + std::string("world"));
  SetTracedValueArg(raw_value, "my_void_ptr", static_cast<void*>(nullptr));
  SetTracedValueArg(raw_value, "use_fallback", UseFallback());
  UseFallback usefallback_with_variable;
  SetTracedValueArg(raw_value, "use_fallback", usefallback_with_variable);
  std::string json;
  value->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{\"my_int\":1,"
      "\"my_double\":0.1,"
      "\"my_bool\":false,"
      "\"my_literal\":\"hello\","
      "\"my_string\":\"wonderful_world\","
      "\"my_void_ptr\":\"0x0\","
      "\"use_fallback\":\"\\u003Cvalue>\","
      "\"use_fallback\":\"\\u003Cvalue>\""
      "}",
      json);
}

TEST(TraceEventConversionHelperTest, OstreamValueToString) {
  std::string zero = internal::OstreamValueToString(0);
  EXPECT_EQ("0", zero);
}

TEST(TraceEventConversionHelperTest, UseFallback) {
  std::string answer = ValueToString(UseFallback(), "fallback");
  EXPECT_EQ("fallback", answer);
}

// std::ostream::operator<<
TEST(TraceEventConversionHelperTest, StdOstream) {
  const char* literal = "hello literal";
  EXPECT_EQ(literal, ValueToString(literal));
  std::string str = "hello std::string";
  EXPECT_EQ(str, ValueToString(str));
  EXPECT_EQ("1", ValueToString(true));
}

// base::NumberToString
TEST(TraceEventConversionHelperTest, Number) {
  EXPECT_EQ("3.14159", ValueToString(3.14159));
  EXPECT_EQ("0", ValueToString(0.f));
  EXPECT_EQ("42", ValueToString(42));
}

class UseToString {
 public:
  std::string ToString() const { return "UseToString::ToString"; }
};

TEST(TraceEventConversionHelperTest, UseToString) {
  std::string answer = ValueToString(UseToString());
  EXPECT_EQ("UseToString::ToString", answer);
}

class UseFallbackNonConstTostring {
 public:
  std::string ToString() { return "don't return me, not const"; }
};

TEST(TraceEventConversionHelperTest, UseFallbackNonConstToString) {
  std::string answer = ValueToString(UseFallbackNonConstTostring(), "fallback");
  EXPECT_EQ("fallback", answer);
}

class ConfusingToStringAPI {
 public:
  ConfusingToStringAPI ToString() const { return ConfusingToStringAPI(); }
};

TEST(TraceEventConversionHelperTest, ConfusingToStringAPI) {
  std::string answer = ValueToString(ConfusingToStringAPI(), "fallback");
  EXPECT_EQ("fallback", answer);
}

// std::ostream::operator<<
TEST(TraceEventConversionHelperTest, UseOstreamOperator) {
  // Test that the output is the same as when calling OstreamValueToString.
  // Different platforms may represent the pointer differently, thus we don't
  // compare with a value.
  EXPECT_EQ(internal::OstreamValueToString((void*)0x123),
            ValueToString((void*)0x123));
}

class UseOperatorLessLess {};

std::ostream& operator<<(std::ostream& os, const UseOperatorLessLess&) {
  os << "UseOperatorLessLess";
  return os;
}

TEST(TraceEventConversionHelperTest, UseOperatorLessLess) {
  std::string answer = ValueToString(UseOperatorLessLess());
  EXPECT_EQ("UseOperatorLessLess", answer);
}

class HasBoth {
 public:
  std::string ToString() const { return "HasBoth::ToString"; }
};

std::ostream& operator<<(std::ostream& os, const HasBoth&) {
  os << "HasBoth::OperatorLessLess";
  return os;
}

TEST(TraceEventConversionHelperTest, HasBoth) {
  std::string answer = ValueToString(HasBoth());
  EXPECT_EQ("HasBoth::ToString", answer);
}

class HasData {
 public:
  const char* data() const { return "HasData"; }
};

TEST(TraceEventConversionHelperTest, HasData) {
  std::string answer = ValueToString(HasData());
  EXPECT_EQ("HasData", answer);
}

class HasNonConstData {
 public:
  const char* data() { return "HasNonConstData"; }
};

TEST(TraceEventConversionHelperTest, HasNonConstData) {
  std::string answer = ValueToString(HasNonConstData(), "fallback");
  EXPECT_EQ("fallback", answer);
}

class HasDataOfWrongType {
 public:
  void data() {}
};

TEST(TraceEventConversionHelperTest, HasDataOfWrongType) {
  std::string answer = ValueToString(HasDataOfWrongType(), "fallback");
  EXPECT_EQ("fallback", answer);
}

}  // namespace trace_event
}  // namespace base
