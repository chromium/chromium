// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/traced_value.h"

#include <cmath>
#include <cstddef>
#include <string_view>
#include <utility>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::trace_event {

TEST(TraceEventArgumentTest, InitializerListCreatedContainers) {
  std::string json;
  TracedValue::Build(
      {
          {"empty_array", TracedValue::Array({})},
          {"empty_dictionary", TracedValue::Dictionary({})},
          {"nested_array", TracedValue::Array({
                               TracedValue::Array({}),
                               TracedValue::Dictionary({}),
                               true,
                           })},
          {"nested_dictionary", TracedValue::Dictionary({
                                    {"d", TracedValue::Dictionary({})},
                                    {"a", TracedValue::Array({})},
                                    {"b", true},
                                })},
      })
      ->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{\"empty_array\":[],\"empty_dictionary\":{},"
      "\"nested_array\":[[],{},true],"
      "\"nested_dictionary\":{\"d\":{},\"a\":[],\"b\":true}}",
      json);
}

TEST(TraceEventArgumentTest, InitializerListCreatedFlatDictionary) {
  std::string json;
  TracedValue::Build({{"bool_var", true},
                      {"double_var", 3.14},
                      {"int_var", 2020},
                      {"literal_var", "literal"}})
      ->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{\"bool_var\":true,\"double_var\":3.14,\"int_var\":2020,\""
      "literal_var\":\"literal\"}",
      json);
}

TEST(TraceEventArgumentTest, ArrayAndDictionaryScope) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  {
    auto dictionary = value->BeginDictionaryScoped("dictionary_name");
    value->SetInteger("my_int", 1);
  }
  {
    auto array = value->BeginArrayScoped("array_name");
    value->AppendInteger(2);
  }
  {
    auto surround_dictionary =
        value->BeginDictionaryScoped("outside_dictionary");
    value->SetBoolean("my_bool", true);
    {
      auto inside_array = value->BeginArrayScoped("inside_array");
      value->AppendBoolean(false);
    }
    {
      auto inside_array = value->BeginDictionaryScoped("inside_dictionary");
      value->SetBoolean("inner_bool", false);
    }
  }
  {
    auto surround_array = value->BeginArrayScoped("outside_array");
    value->AppendBoolean(false);
    {
      auto inside_dictionary = value->AppendDictionaryScoped();
      value->SetBoolean("my_bool", true);
    }
    {
      auto inside_array = value->AppendArrayScoped();
      value->AppendBoolean(false);
    }
  }
  {
    auto dictionary = value->BeginDictionaryScopedWithCopiedName(
        std::string("wonderful_") + std::string("world"));
  }
  {
    auto array = value->BeginArrayScopedWithCopiedName(
        std::string("wonderful_") + std::string("array"));
  }
  std::string json;
  value->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{"
      "\"dictionary_name\":{\"my_int\":1},"
      "\"array_name\":[2],"
      "\"outside_dictionary\":{\"my_bool\":true,\"inside_array\":[false],"
      "\"inside_dictionary\":{\"inner_bool\":false}},"
      "\"outside_array\":[false,{\"my_bool\":true},[false]],"
      "\"wonderful_world\":{},"
      "\"wonderful_array\":[]"
      "}",
      json);
}

std::string SayHello() {
  // Create a string by concatenating two strings, so that there is no literal
  // corresponding to the result.
  return std::string("hello ") + std::string("world");
}

TEST(TraceEventArgumentTest, StringAndPointerConstructors) {
  std::string json;
  const char* const_char_ptr_var = "const char* value";
  TracedValue::Build(
      {
          {"literal_var", "literal"},
          {"std_string_var", std::string("std::string value")},
          {"string_from_function", SayHello()},
          {"string_from_lambda", [] { return std::string("hello"); }()},
          {"base_string_piece_var", std::string_view("std::string_view value")},
          {"const_char_ptr_var", const_char_ptr_var},
          {"void_nullptr", static_cast<void*>(nullptr)},
          {"int_nullptr", static_cast<int*>(nullptr)},
          {"void_1234ptr", reinterpret_cast<void*>(0x1234)},
      })
      ->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{\"literal_var\":\"literal\","
      "\"std_string_var\":\"std::string value\","
      "\"string_from_function\":\"hello world\","
      "\"string_from_lambda\":\"hello\","
      "\"base_string_piece_var\":\"std::string_view value\","
      "\"const_char_ptr_var\":\"const char* value\","
      "\"void_nullptr\":\"0x0\","
      "\"int_nullptr\":\"0x0\","
      "\"void_1234ptr\":\"0x1234\"}",
      json);
}

TEST(TraceEventArgumentTest, FlatDictionary) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  value->SetBoolean("bool", true);
  value->SetDouble("double", 0.0);
  value->SetInteger("int", 2014);
  value->SetString("string", "string");
  value->SetPointer("ptr", reinterpret_cast<void*>(0x1234));
  std::string json = "PREFIX";
  value->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "PREFIX{\"bool\":true,\"double\":0.0,\"int\":2014,\"string\":\"string\","
      "\"ptr\":\"0x1234\"}",
      json);
}

TEST(TraceEventArgumentTest, NoDotPathExpansion) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  value->SetBoolean("bo.ol", true);
  value->SetDouble("doub.le", 0.0);
  value->SetInteger("in.t", 2014);
  value->SetString("str.ing", "str.ing");
  std::string json;
  value->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{\"bo.ol\":true,\"doub.le\":0.0,\"in.t\":2014,\"str.ing\":\"str.ing\"}",
      json);
}

TEST(TraceEventArgumentTest, Hierarchy) {
  std::unique_ptr<TracedValue> value(new TracedValue());
  value->BeginArray("a1");
  value->AppendInteger(1);
  value->AppendBoolean(true);
  value->BeginDictionary();
  value->SetInteger("i2", 3);
  value->EndDictionary();
  value->EndArray();
  value->SetBoolean("b0", true);
  value->SetDouble("d0", 0.0);
  value->BeginDictionary("dict1");
  value->BeginDictionary("dict2");
  value->SetBoolean("b2", false);
  value->EndDictionary();
  value->SetInteger("i1", 2014);
  value->SetString("s1", "foo");
  value->EndDictionary();
  value->SetInteger("i0", 2014);
  value->SetString("s0", "foo");
  std::string json;
  value->AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{\"a1\":[1,true,{\"i2\":3}],\"b0\":true,\"d0\":0.0,\"dict1\":{\"dict2\":"
      "{\"b2\":false},\"i1\":2014,\"s1\":\"foo\"},\"i0\":2014,\"s0\":"
      "\"foo\"}",
      json);
}

TEST(TraceEventArgumentTest, LongStrings) {
  std::string kLongString = "supercalifragilisticexpialidocious";
  std::string kLongString2 = "0123456789012345678901234567890123456789";
  std::array<char, 4096> kLongString3;
  for (size_t i = 0; i < kLongString3.size(); ++i) {
    kLongString3[i] = 'a' + (i % 25);
  }
  kLongString3.back() = '\0';

  std::unique_ptr<TracedValue> value(new TracedValue());
  value->SetString("a", "short");
  value->SetString("b", kLongString);
  value->BeginArray("c");
  value->AppendString(kLongString2);
  value->AppendString("");
  value->BeginDictionary();
  value->SetString("a", kLongString3.data());
  value->EndDictionary();
  value->EndArray();

  std::string json;
  value->AppendAsTraceFormat(&json);
  EXPECT_EQ("{\"a\":\"short\",\"b\":\"" + kLongString + "\",\"c\":[\"" +
                kLongString2 + "\",\"\",{\"a\":\"" + kLongString3.data() +
                "\"}]}",
            json);
}

TEST(TraceEventArgumentTest, PassTracedValue) {
  auto dict_value = std::make_unique<TracedValue>();
  dict_value->SetInteger("a", 1);

  auto nested_dict_value = std::make_unique<TracedValue>();
  nested_dict_value->SetInteger("b", 2);
  nested_dict_value->BeginArray("c");
  nested_dict_value->AppendString("foo");
  nested_dict_value->EndArray();

  dict_value->SetValue("e", nested_dict_value.get());

  // Check the merged result.
  std::string json;
  dict_value->AppendAsTraceFormat(&json);
  EXPECT_EQ("{\"a\":1,\"e\":{\"b\":2,\"c\":[\"foo\"]}}", json);

  // Check that the passed nestd dict was left unouthced.
  json = "";
  nested_dict_value->AppendAsTraceFormat(&json);
  EXPECT_EQ("{\"b\":2,\"c\":[\"foo\"]}", json);

  // And that it is still usable.
  nested_dict_value->SetInteger("f", 3);
  nested_dict_value->BeginDictionary("g");
  nested_dict_value->EndDictionary();
  json = "";
  nested_dict_value->AppendAsTraceFormat(&json);
  EXPECT_EQ("{\"b\":2,\"c\":[\"foo\"],\"f\":3,\"g\":{}}", json);
}

TEST(TraceEventArgumentTest, NanAndInfinityJSON) {
  TracedValueJSON value;
  value.SetDouble("nan", std::nan(""));
  value.SetDouble("infinity", INFINITY);
  value.SetDouble("negInfinity", -INFINITY);
  std::string json;
  value.AppendAsTraceFormat(&json);
  EXPECT_EQ(
      "{\"nan\":\"NaN\",\"infinity\":\"Infinity\","
      "\"negInfinity\":\"-Infinity\"}",
      json);

  std::string formatted_json = value.ToFormattedJSON();
  // Remove CR and LF to make the result platform-independent.
  ReplaceChars(formatted_json, "\n\r", "", &formatted_json);
  EXPECT_EQ(
      "{"
      "   \"infinity\": \"Infinity\","
      "   \"nan\": \"NaN\","
      "   \"negInfinity\": \"-Infinity\""
      "}",
      formatted_json);
}

}  // namespace base::trace_event
