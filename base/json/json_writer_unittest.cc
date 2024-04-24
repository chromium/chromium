// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/json/json_writer.h"

#include <optional>

#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_util.h"
#endif

namespace base {

namespace {

std::string FixNewlines(const std::string& json) {
  // The pretty-printer uses a different newline style on Windows than on
  // other platforms.
#if BUILDFLAG(IS_WIN)
  std::string result;
  ReplaceChars(json, "\n", "\r\n", &result);
  return result;
#else
  return json;
#endif
}

}  // namespace

TEST(JsonWriterTest, BasicTypes) {
  // Test null.
  EXPECT_EQ(WriteJson(Value()), "null");

  // Test empty dict.
  EXPECT_EQ(WriteJson(Value(Value::Type::DICT)), "{}");

  // Test empty list.
  EXPECT_EQ(WriteJson(Value(Value::Type::LIST)), "[]");

  // Test integer values.
  EXPECT_EQ(WriteJson(Value(42)), "42");

  // Test boolean values.
  EXPECT_EQ(WriteJson(Value(true)), "true");

  // Test Real values should always have a decimal or an 'e'.
  EXPECT_EQ(WriteJson(Value(1.0)), "1.0");

  // Test Real values in the range (-1, 1) must have leading zeros
  EXPECT_EQ(WriteJson(Value(0.2)), "0.2");

  // Test Real values in the range (-1, 1) must have leading zeros
  EXPECT_EQ(WriteJson(Value(-0.8)), "-0.8");

  // Test String values.
  EXPECT_EQ(WriteJson(Value("foo")), "\"foo\"");
}

TEST(JsonWriterTest, NestedTypes) {
  // Writer unittests like empty list/dict nesting,
  // list list nesting, etc.
  auto dict =
      Value::Dict().Set("list", Value::List()
                                    .Append(Value::Dict().Set("inner int", 10))
                                    .Append(Value::Dict())
                                    .Append(Value::List())
                                    .Append(true));

  EXPECT_EQ(WriteJson(dict), "{\"list\":[{\"inner int\":10},{},[],true]}");

  // Test the pretty-printer.
  EXPECT_EQ(WriteJsonWithOptions(dict, JSONWriter::OPTIONS_PRETTY_PRINT),
            FixNewlines(R"({
   "list": [ {
      "inner int": 10
   }, {
   }, [  ], true ]
}
)"));
}

TEST(JsonWriterTest, KeysWithPeriods) {
  EXPECT_EQ(WriteJson(Value::Dict()  //
                          .Set("a.b", 3)
                          .Set("c", 2)
                          .Set("d.e.f", Value::Dict().Set("g.h.i.j", 1))),
            R"({"a.b":3,"c":2,"d.e.f":{"g.h.i.j":1}})");

  EXPECT_EQ(WriteJson(Value::Dict()  //
                          .SetByDottedPath("a.b", 2)
                          .Set("a.b", 1)),
            R"({"a":{"b":2},"a.b":1})");
}

TEST(JsonWriterTest, BinaryValues) {
  const auto kBinaryData = base::as_bytes(base::make_span("asdf", 4u));

  // Binary values should return errors unless suppressed via the
  // `OPTIONS_OMIT_BINARY_VALUES` flag.
  EXPECT_EQ(WriteJson(Value(kBinaryData)), std::nullopt);
  EXPECT_EQ(WriteJsonWithOptions(Value(kBinaryData),
                                 JsonOptions::OPTIONS_OMIT_BINARY_VALUES),
            "");

  auto binary_list = Value::List()
                         .Append(Value(kBinaryData))
                         .Append(5)
                         .Append(Value(kBinaryData))
                         .Append(2)
                         .Append(Value(kBinaryData));
  EXPECT_EQ(WriteJson(binary_list), std::nullopt);
  EXPECT_EQ(
      WriteJsonWithOptions(binary_list, JSONWriter::OPTIONS_OMIT_BINARY_VALUES),
      "[5,2]");

  auto binary_dict = Value::Dict()
                         .Set("a", Value(kBinaryData))
                         .Set("b", 5)
                         .Set("c", Value(kBinaryData))
                         .Set("d", 2)
                         .Set("e", Value(kBinaryData));
  EXPECT_EQ(WriteJson(binary_dict), std::nullopt);
  EXPECT_EQ(
      WriteJsonWithOptions(binary_dict, JSONWriter::OPTIONS_OMIT_BINARY_VALUES),
      R"({"b":5,"d":2})");
}

TEST(JsonWriterTest, DoublesAsInts) {
  // Test allowing a double with no fractional part to be written as an integer.
  Value double_value(1e10);
  EXPECT_EQ(
      WriteJsonWithOptions(double_value,
                           JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION),
      "10000000000");
}

TEST(JsonWriterTest, StackOverflow) {
  Value::List deep_list;
  const size_t max_depth = 100000;

  for (size_t i = 0; i < max_depth; ++i) {
    Value::List new_top_list;
    new_top_list.Append(std::move(deep_list));
    deep_list = std::move(new_top_list);
  }

  Value deep_list_value(std::move(deep_list));
  EXPECT_EQ(WriteJson(deep_list_value), std::nullopt);
  EXPECT_EQ(
      WriteJsonWithOptions(deep_list_value, JSONWriter::OPTIONS_PRETTY_PRINT),
      std::nullopt);

  // We cannot just let `deep_list` tear down since it
  // would cause a stack overflow. Therefore, we tear
  // down the deep list manually.
  deep_list = std::move(deep_list_value).TakeList();
  while (!deep_list.empty()) {
    DCHECK_EQ(deep_list.size(), 1u);
    Value::List inner_list = std::move(deep_list[0]).TakeList();
    deep_list = std::move(inner_list);
  }
}

TEST(JsonWriterTest, TestMaxDepthWithValidNodes) {
  // Create JSON to the max depth - 1.  Nodes at that depth are still valid
  // for writing which matches the JSONParser logic.
  std::string nested_json;
  for (int i = 0; i < 199; ++i) {
    std::string node = "[";
    for (int j = 0; j < 5; j++) {
      node.append(StringPrintf("%d,", j));
    }
    nested_json.insert(0, node);
    nested_json.append("]");
  }

  // Ensure we can read and write the JSON
  ASSERT_OK_AND_ASSIGN(Value value,
                       JSONReader::ReadAndReturnValueWithError(
                           nested_json, JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_TRUE(WriteJson(std::move(value)).has_value());
}

// Test that the JSONWriter::Write method still works.
TEST(JsonWriterTest, JSONWriterWriteSuccess) {
  std::string output_js;

  EXPECT_TRUE(
      JSONWriter::Write(base::Value::Dict().Set("key", "value"), &output_js));
  EXPECT_EQ(output_js, R"({"key":"value"})");
}

// Test that the JSONWriter::Write method still works.
TEST(JsonWriterTest, JSONWriterWriteFailure) {
  std::string output_js;

  EXPECT_FALSE(JSONWriter::Write(
      base::Value::Dict()  //
          .Set("key",
               base::Value::Dict().Set("nested-key", base::Value::Dict())),
      &output_js, /*max_depth=*/1));
}

// Test that the JSONWriter::WriteWithOptions method still works.
TEST(JsonWriterTest, JSONWriterWriteWithOptionsSuccess) {
  std::string output_js;
  EXPECT_TRUE(JSONWriter::WriteWithOptions(
      base::Value::Dict().Set("key", "value"), JSONWriter::OPTIONS_PRETTY_PRINT,
      &output_js));
  EXPECT_EQ(output_js, FixNewlines(R"({
   "key": "value"
}
)"));
}

// Test that the JSONWriter::WriteWithOptions method still works.
TEST(JsonWriterTest, JSONWriterWriteWithOptionsFailure) {
  std::string output_js;

  EXPECT_FALSE(JSONWriter::WriteWithOptions(
      base::Value::Dict().Set(
          "key", base::Value::Dict().Set("nested-key", base::Value::Dict())),
      JSONWriter::OPTIONS_PRETTY_PRINT, &output_js, /*max_depth=*/1));
}

}  // namespace base
