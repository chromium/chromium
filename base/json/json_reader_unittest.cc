// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/json/json_reader.h"

#include <stddef.h>

#include <cmath>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/features.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rust_buildflags.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// MSan will do a better job detecting over-read errors if the input is not
// nul-terminated on the heap. This will copy |input| to a new buffer owned by
// |owner|, returning a std::string_view to |owner|.
std::string_view MakeNotNullTerminatedInput(const char* input,
                                            std::unique_ptr<char[]>* owner) {
  size_t str_len = strlen(input);
  owner->reset(new char[str_len]);
  memcpy(owner->get(), input, str_len);
  return std::string_view(owner->get(), str_len);
}

}  // namespace

namespace base {

class JSONReaderTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatureState(base::features::kUseRustJsonParser,
                                       using_rust_);
  }

 protected:
  bool using_rust_ = GetParam();
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(JSONReaderTest, Whitespace) {
  std::optional<Value> root = JSONReader::Read("   null   ");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_none());
}

TEST_P(JSONReaderTest, InvalidString) {
  // These are invalid because they do not represent a JSON value,
  // see https://tools.ietf.org/rfc/rfc8259.txt
  EXPECT_FALSE(JSONReader::Read(""));
  EXPECT_FALSE(JSONReader::Read("nu"));
}

TEST_P(JSONReaderTest, SimpleBool) {
#if BUILDFLAG(BUILD_RUST_JSON_READER)
  base::HistogramTester histograms;
#endif  // BUILDFLAG(BUILD_RUST_JSON_READER)
  std::optional<Value> root = JSONReader::Read("true  ");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_bool());
#if BUILDFLAG(BUILD_RUST_JSON_READER)
  histograms.ExpectTotalCount("Security.JSONParser.ParsingTime", 1);
#endif  // BUILDFLAG(BUILD_RUST_JSON_READER)
}

TEST_P(JSONReaderTest, EmbeddedComments) {
  std::optional<Value> root = JSONReader::Read("/* comment */null");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_none());
  root = JSONReader::Read("40 /* comment */");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_int());
  root = JSONReader::Read("true // comment");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_bool());
  // Comments in different contexts.
  root = JSONReader::Read("{   \"cheese\": 3\n\n   // Here's a comment\n}");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_dict());
  root = JSONReader::Read("{   \"cheese\": 3// Here's a comment\n}");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_dict());
  // Multiple comment markers.
  root = JSONReader::Read(
      "{   \"cheese\": 3// Here's a comment // and another\n}");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_dict());
  root = JSONReader::Read("/* comment */\"sample string\"");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_string());
  EXPECT_EQ("sample string", root->GetString());
  root = JSONReader::Read("[1, /* comment, 2 ] */ \n 3]");
  ASSERT_TRUE(root);
  Value::List* list = root->GetIfList();
  ASSERT_TRUE(list);
  ASSERT_EQ(2u, list->size());
  ASSERT_TRUE((*list)[0].is_int());
  EXPECT_EQ(1, (*list)[0].GetInt());
  ASSERT_TRUE((*list)[1].is_int());
  EXPECT_EQ(3, (*list)[1].GetInt());
  root = JSONReader::Read("[1, /*a*/2, 3]");
  ASSERT_TRUE(root);
  list = root->GetIfList();
  ASSERT_TRUE(list);
  EXPECT_EQ(3u, (*list).size());
  root = JSONReader::Read("/* comment **/42");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_int());
  EXPECT_EQ(42, root->GetInt());
  root = JSONReader::Read(
      "/* comment **/\n"
      "// */ 43\n"
      "44");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_int());
  EXPECT_EQ(44, root->GetInt());

  // At one point, this parsed successfully as the value three.
  EXPECT_FALSE(JSONReader::Read("/33"));
}

TEST_P(JSONReaderTest, Ints) {
  std::optional<Value> root = JSONReader::Read("43");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_int());
  EXPECT_EQ(43, root->GetInt());
}

TEST_P(JSONReaderTest, NonDecimalNumbers) {
  // According to RFC 8259, oct, hex, and leading zeros are invalid JSON.
  EXPECT_FALSE(JSONReader::Read("043"));
  EXPECT_FALSE(JSONReader::Read("0x43"));
  EXPECT_FALSE(JSONReader::Read("00"));
}

TEST_P(JSONReaderTest, NumberZero) {
  // Test 0 (which needs to be special cased because of the leading zero
  // clause).
  std::optional<Value> root = JSONReader::Read("0");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_int());
  EXPECT_EQ(0, root->GetInt());
}

TEST_P(JSONReaderTest, LargeIntPromotion) {
  // Numbers that overflow ints should succeed, being internally promoted to
  // storage as doubles
  std::optional<Value> root = JSONReader::Read("2147483648");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(2147483648.0, root->GetDouble());
  root = JSONReader::Read("-2147483649");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(-2147483649.0, root->GetDouble());
}

TEST_P(JSONReaderTest, LargerIntIsLossy) {
  // Parse LONG_MAX as a JSON number (not a JSON string). The result of the
  // parse is a base::Value, either a (32-bit) int or a (64-bit) double.
  // LONG_MAX would overflow an int and can only be approximated by a double.
  // In this case, parsing is lossy.
  const char* etc807 = "9223372036854775807";
  const char* etc808 = "9223372036854775808.000000";
  std::optional<Value> root = JSONReader::Read(etc807);
  ASSERT_TRUE(root);
  ASSERT_FALSE(root->is_int());
  ASSERT_TRUE(root->is_double());
  // We use StringPrintf instead of NumberToString, because the NumberToString
  // function does not let you specify the precision, and its default output,
  // "9.223372036854776e+18", isn't precise enough to see the lossiness.
  EXPECT_EQ(std::string(etc808), StringPrintf("%f", root->GetDouble()));
}

TEST_P(JSONReaderTest, Doubles) {
  std::optional<Value> root = JSONReader::Read("43.1");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(43.1, root->GetDouble());

  root = JSONReader::Read("4.3e-1");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(.43, root->GetDouble());

  root = JSONReader::Read("2.1e0");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(2.1, root->GetDouble());

  root = JSONReader::Read("2.1e+0001");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(21.0, root->GetDouble());

  root = JSONReader::Read("0.01");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(0.01, root->GetDouble());

  root = JSONReader::Read("1.00");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(1.0, root->GetDouble());

  // Some "parse to float64" implementations find this one tricky.
  // https://github.com/serde-rs/json/issues/707
  root = JSONReader::Read("122.416294033786585");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(122.416294033786585, root->GetDouble());

  // This is syntaxtically valid, but out of range of a double.
  auto value =
      JSONReader::ReadAndReturnValueWithError("1e1000", JSON_PARSE_RFC);
  ASSERT_FALSE(value.has_value());
}

TEST_P(JSONReaderTest, FractionalNumbers) {
  // Fractional parts must have a digit before and after the decimal point.
  EXPECT_FALSE(JSONReader::Read("1."));
  EXPECT_FALSE(JSONReader::Read(".1"));
  EXPECT_FALSE(JSONReader::Read("1.e10"));
}

TEST_P(JSONReaderTest, ExponentialNumbers) {
  // Exponent must have a digit following the 'e'.
  EXPECT_FALSE(JSONReader::Read("1e"));
  EXPECT_FALSE(JSONReader::Read("1E"));
  EXPECT_FALSE(JSONReader::Read("1e1."));
  EXPECT_FALSE(JSONReader::Read("1e1.0"));
}

TEST_P(JSONReaderTest, InvalidInfNAN) {
  // The largest finite double is roughly 1.8e308.
  EXPECT_FALSE(JSONReader::Read("1e1000"));
  EXPECT_FALSE(JSONReader::Read("-1e1000"));
  EXPECT_FALSE(JSONReader::Read("NaN"));
  EXPECT_FALSE(JSONReader::Read("nan"));
  EXPECT_FALSE(JSONReader::Read("inf"));
}

TEST_P(JSONReaderTest, InvalidNumbers) {
  EXPECT_TRUE(JSONReader::Read("4.3"));
  EXPECT_FALSE(JSONReader::Read("4."));
  EXPECT_FALSE(JSONReader::Read("4.3.1"));
  EXPECT_FALSE(JSONReader::Read("4e3.1"));
  EXPECT_FALSE(JSONReader::Read("4.a"));
  EXPECT_FALSE(JSONReader::Read("42a"));
}

TEST_P(JSONReaderTest, Zeroes) {
  std::optional<Value> root = JSONReader::Read("0");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_int());
  EXPECT_DOUBLE_EQ(0, root->GetInt());

  root = JSONReader::Read("0.0");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(0.0, root->GetDouble());
  EXPECT_FALSE(std::signbit(root->GetDouble()));

  root = JSONReader::Read("-0");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(0.0, root->GetDouble());
  EXPECT_TRUE(std::signbit(root->GetDouble()));

  root = JSONReader::Read("-0.0");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  EXPECT_DOUBLE_EQ(-0.0, root->GetDouble());
  EXPECT_TRUE(std::signbit(root->GetDouble()));
}

TEST_P(JSONReaderTest, SimpleString) {
  std::optional<Value> root = JSONReader::Read("\"hello world\"");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_string());
  EXPECT_EQ("hello world", root->GetString());
}

TEST_P(JSONReaderTest, EmptyString) {
  std::optional<Value> root = JSONReader::Read("\"\"");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_string());
  EXPECT_EQ("", root->GetString());
}

TEST_P(JSONReaderTest, BasicStringEscapes) {
  std::optional<Value> root =
      JSONReader::Read("\" \\\"\\\\\\/\\b\\f\\n\\r\\t\"");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_string());
  EXPECT_EQ(" \"\\/\b\f\n\r\t", root->GetString());
}

TEST_P(JSONReaderTest, UnicodeEscapes) {
  // Test hex and unicode escapes including the null character.
  std::optional<Value> root =
      JSONReader::Read("\"\\x41\\xFF\\x00\\u1234\\u0000\"");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_string());
  const std::string& str_val = root->GetString();
  EXPECT_EQ(std::wstring(L"A\x00FF\0\x1234\0", 5), UTF8ToWide(str_val));

  // The contents of a Unicode escape may only be four hex chars. Previously the
  // parser accepted things like "0x01" and "0X01".
  EXPECT_FALSE(JSONReader::Read("\"\\u0x12\""));

  // Surrogate pairs are allowed in JSON.
  EXPECT_TRUE(JSONReader::Read("\"\\uD834\\uDD1E\""));  // U+1D11E
}

TEST_P(JSONReaderTest, InvalidStrings) {
  EXPECT_FALSE(JSONReader::Read("\"no closing quote"));
  EXPECT_FALSE(JSONReader::Read("\"\\z invalid escape char\""));
  EXPECT_FALSE(JSONReader::Read("\"\\xAQ invalid hex code\""));
  EXPECT_FALSE(JSONReader::Read("not enough hex chars\\x1\""));
  EXPECT_FALSE(JSONReader::Read("\"not enough escape chars\\u123\""));
  EXPECT_FALSE(JSONReader::Read("\"extra backslash at end of input\\\""));
}

TEST_P(JSONReaderTest, BasicArray) {
  std::optional<Value> root = JSONReader::Read("[true, false, null]");
  ASSERT_TRUE(root);
  Value::List* list = root->GetIfList();
  ASSERT_TRUE(list);
  EXPECT_EQ(3U, list->size());

  // Test with trailing comma.  Should be parsed the same as above.
  std::optional<Value> root2 =
      JSONReader::Read("[true, false, null, ]", JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(root2);
  EXPECT_EQ(*list, *root2);
}

TEST_P(JSONReaderTest, EmptyArray) {
  std::optional<Value> value = JSONReader::Read("[]");
  ASSERT_TRUE(value);
  Value::List* list = value->GetIfList();
  ASSERT_TRUE(list);
  EXPECT_TRUE(list->empty());
}

TEST_P(JSONReaderTest, CompleteArray) {
  std::optional<Value> value = JSONReader::Read("[\"a\", 3, 4.56, null]");
  ASSERT_TRUE(value);
  Value::List* list = value->GetIfList();
  ASSERT_TRUE(list);
  EXPECT_EQ(4U, list->size());
}

TEST_P(JSONReaderTest, NestedArrays) {
  std::optional<Value> value = JSONReader::Read(
      "[[true], [], {\"smell\": \"nice\",\"taste\": \"yummy\" }, [false, [], "
      "[null]], null]");
  ASSERT_TRUE(value);
  Value::List* list = value->GetIfList();
  ASSERT_TRUE(list);
  EXPECT_EQ(5U, list->size());

  // Lots of trailing commas.
  std::optional<Value> root2 = JSONReader::Read(
      "[[true], [], {\"smell\": \"nice\",\"taste\": \"yummy\" }, [false, [], "
      "[null, ]  , ], null,]",
      JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(root2);
  EXPECT_EQ(*list, *root2);
}

TEST_P(JSONReaderTest, InvalidArrays) {
  // Missing close brace.
  EXPECT_FALSE(JSONReader::Read("[[true], [], [false, [], [null]], null"));

  // Too many commas.
  EXPECT_FALSE(JSONReader::Read("[true,, null]"));
  EXPECT_FALSE(JSONReader::Read("[true,, null]", JSON_ALLOW_TRAILING_COMMAS));

  // No commas.
  EXPECT_FALSE(JSONReader::Read("[true null]"));

  // Trailing comma.
  EXPECT_FALSE(JSONReader::Read("[true,]"));
}

TEST_P(JSONReaderTest, ArrayTrailingComma) {
  // Valid if we set |allow_trailing_comma| to true.
  std::optional<Value> value =
      JSONReader::Read("[true,]", JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(value);
  Value::List* list = value->GetIfList();
  ASSERT_TRUE(list);
  ASSERT_EQ(1U, list->size());
  const Value& value1 = (*list)[0];
  ASSERT_TRUE(value1.is_bool());
  EXPECT_TRUE(value1.GetBool());
}

TEST_P(JSONReaderTest, ArrayTrailingCommaNoEmptyElements) {
  // Don't allow empty elements, even if |allow_trailing_comma| is
  // true.
  EXPECT_FALSE(JSONReader::Read("[,]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(JSONReader::Read("[true,,]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(JSONReader::Read("[,true,]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(JSONReader::Read("[true,,false]", JSON_ALLOW_TRAILING_COMMAS));
}

TEST_P(JSONReaderTest, EmptyDictionary) {
  std::optional<Value> dict_val = JSONReader::Read("{}");
  ASSERT_TRUE(dict_val);
  ASSERT_TRUE(dict_val->is_dict());
}

TEST_P(JSONReaderTest, CompleteDictionary) {
  std::optional<Value> root1 = JSONReader::Read(
      "{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\", \"bool\": "
      "false, \"more\": {} }");
  ASSERT_TRUE(root1);
  const Value::Dict* root1_dict = root1->GetIfDict();
  ASSERT_TRUE(root1_dict);
  auto double_val = root1_dict->FindDouble("number");
  ASSERT_TRUE(double_val);
  EXPECT_DOUBLE_EQ(9.87654321, *double_val);
  const Value* null_val = root1_dict->Find("null");
  ASSERT_TRUE(null_val);
  EXPECT_TRUE(null_val->is_none());
  const std::string* str_val = root1_dict->FindString("S");
  ASSERT_TRUE(str_val);
  EXPECT_EQ("str", *str_val);
  auto bool_val = root1_dict->FindBool("bool");
  ASSERT_TRUE(bool_val);
  ASSERT_FALSE(*bool_val);

  std::optional<Value> root2 = JSONReader::Read(
      "{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\", \"bool\": "
      "false, \"more\": {},}",
      JSON_PARSE_CHROMIUM_EXTENSIONS | JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(root2);
  Value::Dict* root2_dict = root2->GetIfDict();
  ASSERT_TRUE(root2_dict);
  EXPECT_EQ(*root1_dict, *root2_dict);

  // Test newline equivalence.
  root2 = JSONReader::Read(
      "{\n"
      "  \"number\":9.87654321,\n"
      "  \"null\":null,\n"
      "  \"\\x53\":\"str\",\n"
      "  \"bool\": false,\n"
      "  \"more\": {},\n"
      "}\n",
      JSON_PARSE_CHROMIUM_EXTENSIONS | JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(root2);
  root2_dict = root2->GetIfDict();
  ASSERT_TRUE(root2);
  EXPECT_EQ(*root1_dict, *root2_dict);

  root2 = JSONReader::Read(
      "{\r\n"
      "  \"number\":9.87654321,\r\n"
      "  \"null\":null,\r\n"
      "  \"\\x53\":\"str\",\r\n"
      "  \"bool\": false,\r\n"
      "  \"more\": {},\r\n"
      "}\r\n",
      JSON_PARSE_CHROMIUM_EXTENSIONS | JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(root2);
  root2_dict = root2->GetIfDict();
  ASSERT_TRUE(root2_dict);
  EXPECT_EQ(*root1_dict, *root2_dict);
}

TEST_P(JSONReaderTest, NestedDictionaries) {
  std::optional<Value> root1 = JSONReader::Read(
      "{\"inner\":{\"array\":[true, 3, 4.56, null]},\"false\":false,\"d\":{}}");
  ASSERT_TRUE(root1);
  const base::Value::Dict* root1_dict = root1->GetIfDict();
  ASSERT_TRUE(root1_dict);
  const Value::Dict* inner_dict = root1_dict->FindDict("inner");
  ASSERT_TRUE(inner_dict);
  const Value::List* inner_array = inner_dict->FindList("array");
  ASSERT_TRUE(inner_array);
  EXPECT_EQ(4U, inner_array->size());
  auto bool_value = root1_dict->FindBool("false");
  ASSERT_TRUE(bool_value);
  EXPECT_FALSE(*bool_value);
  inner_dict = root1_dict->FindDict("d");
  EXPECT_TRUE(inner_dict);

  std::optional<Value> root2 = JSONReader::Read(
      "{\"inner\": {\"array\":[true, 3, 4.56, null] , "
      "},\"false\":false,\"d\":{},}",
      JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(root2);
  EXPECT_EQ(*root1_dict, *root2);
}

TEST_P(JSONReaderTest, DictionaryKeysWithPeriods) {
  std::optional<Value> root =
      JSONReader::Read("{\"a.b\":3,\"c\":2,\"d.e.f\":{\"g.h.i.j\":1}}");
  ASSERT_TRUE(root);
  Value::Dict* root_dict = root->GetIfDict();
  ASSERT_TRUE(root_dict);

  auto integer_value = root_dict->FindInt("a.b");
  ASSERT_TRUE(integer_value);
  EXPECT_EQ(3, *integer_value);
  integer_value = root_dict->FindInt("c");
  ASSERT_TRUE(integer_value);
  EXPECT_EQ(2, *integer_value);
  const Value::Dict* inner_dict = root_dict->FindDict("d.e.f");
  ASSERT_TRUE(inner_dict);
  EXPECT_EQ(1U, inner_dict->size());
  integer_value = inner_dict->FindInt("g.h.i.j");
  ASSERT_TRUE(integer_value);
  EXPECT_EQ(1, *integer_value);

  root = JSONReader::Read("{\"a\":{\"b\":2},\"a.b\":1}");
  ASSERT_TRUE(root);
  root_dict = root->GetIfDict();
  ASSERT_TRUE(root_dict);
  const Value* integer_path_value = root_dict->FindByDottedPath("a.b");
  ASSERT_TRUE(integer_path_value);
  EXPECT_EQ(2, integer_path_value->GetInt());
  integer_value = root_dict->FindInt("a.b");
  ASSERT_TRUE(integer_value);
  EXPECT_EQ(1, *integer_value);
}

TEST_P(JSONReaderTest, DuplicateKeys) {
  std::optional<Value> root = JSONReader::Read("{\"x\":1,\"x\":2,\"y\":3}");
  ASSERT_TRUE(root);
  const Value::Dict* root_dict = root->GetIfDict();
  ASSERT_TRUE(root_dict);

  auto integer_value = root_dict->FindInt("x");
  ASSERT_TRUE(integer_value);
  EXPECT_EQ(2, *integer_value);
}

TEST_P(JSONReaderTest, InvalidDictionaries) {
  // No closing brace.
  EXPECT_FALSE(JSONReader::Read("{\"a\": true"));

  // Keys must be quoted strings.
  EXPECT_FALSE(JSONReader::Read("{foo:true}"));
  EXPECT_FALSE(JSONReader::Read("{1234: false}"));
  EXPECT_FALSE(JSONReader::Read("{:false}"));
  EXPECT_FALSE(JSONReader::Read("{ , }"));

  // Trailing comma.
  EXPECT_FALSE(JSONReader::Read("{\"a\":true,}"));

  // Too many commas.
  EXPECT_FALSE(JSONReader::Read("{\"a\":true,,\"b\":false}"));
  EXPECT_FALSE(JSONReader::Read("{\"a\":true,,\"b\":false}",
                                JSON_ALLOW_TRAILING_COMMAS));

  // No separator.
  EXPECT_FALSE(JSONReader::Read("{\"a\" \"b\"}"));

  // Lone comma.
  EXPECT_FALSE(JSONReader::Read("{,}"));
  EXPECT_FALSE(JSONReader::Read("{,}", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(JSONReader::Read("{\"a\":true,,}", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(JSONReader::Read("{,\"a\":true}", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(JSONReader::Read("{\"a\":true,,\"b\":false}",
                                JSON_ALLOW_TRAILING_COMMAS));
}

TEST_P(JSONReaderTest, StackOverflow) {
  std::string evil(1000000, '[');
  evil.append(std::string(1000000, ']'));
  EXPECT_FALSE(JSONReader::Read(evil));

  // A few thousand adjacent lists is fine.
  std::string not_evil("[");
  not_evil.reserve(15010);
  for (int i = 0; i < 5000; ++i)
    not_evil.append("[],");
  not_evil.append("[]]");
  std::optional<Value> value = JSONReader::Read(not_evil);
  ASSERT_TRUE(value);
  Value::List* list = value->GetIfList();
  ASSERT_TRUE(list);
  EXPECT_EQ(5001U, list->size());
}

TEST_P(JSONReaderTest, UTF8Input) {
  std::optional<Value> root = JSONReader::Read("\"\xe7\xbd\x91\xe9\xa1\xb5\"");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_string());
  const std::string& str_val = root->GetString();
  EXPECT_EQ(L"\x7f51\x9875", UTF8ToWide(str_val));

  root = JSONReader::Read("{\"path\": \"/tmp/\xc3\xa0\xc3\xa8\xc3\xb2.png\"}");
  ASSERT_TRUE(root);
  const Value::Dict* root_dict = root->GetIfDict();
  ASSERT_TRUE(root_dict);
  const std::string* maybe_string = root_dict->FindString("path");
  ASSERT_TRUE(maybe_string);
  EXPECT_EQ("/tmp/\xC3\xA0\xC3\xA8\xC3\xB2.png", *maybe_string);

  // JSON can encode non-characters.
  const char* const noncharacters[] = {
      "\"\xEF\xB7\x90\"",      // U+FDD0
      "\"\xEF\xB7\x9F\"",      // U+FDDF
      "\"\xEF\xB7\xAF\"",      // U+FDEF
      "\"\xEF\xBF\xBE\"",      // U+FFFE
      "\"\xEF\xBF\xBF\"",      // U+FFFF
      "\"\xF0\x9F\xBF\xBE\"",  // U+01FFFE
      "\"\xF0\x9F\xBF\xBF\"",  // U+01FFFF
      "\"\xF0\xAF\xBF\xBE\"",  // U+02FFFE
      "\"\xF0\xAF\xBF\xBF\"",  // U+02FFFF
      "\"\xF0\xBF\xBF\xBE\"",  // U+03FFFE
      "\"\xF0\xBF\xBF\xBF\"",  // U+03FFFF
      "\"\xF1\x8F\xBF\xBE\"",  // U+04FFFE
      "\"\xF1\x8F\xBF\xBF\"",  // U+04FFFF
      "\"\xF1\x9F\xBF\xBE\"",  // U+05FFFE
      "\"\xF1\x9F\xBF\xBF\"",  // U+05FFFF
      "\"\xF1\xAF\xBF\xBE\"",  // U+06FFFE
      "\"\xF1\xAF\xBF\xBF\"",  // U+06FFFF
      "\"\xF1\xBF\xBF\xBE\"",  // U+07FFFE
      "\"\xF1\xBF\xBF\xBF\"",  // U+07FFFF
      "\"\xF2\x8F\xBF\xBE\"",  // U+08FFFE
      "\"\xF2\x8F\xBF\xBF\"",  // U+08FFFF
      "\"\xF2\x9F\xBF\xBE\"",  // U+09FFFE
      "\"\xF2\x9F\xBF\xBF\"",  // U+09FFFF
      "\"\xF2\xAF\xBF\xBE\"",  // U+0AFFFE
      "\"\xF2\xAF\xBF\xBF\"",  // U+0AFFFF
      "\"\xF2\xBF\xBF\xBE\"",  // U+0BFFFE
      "\"\xF2\xBF\xBF\xBF\"",  // U+0BFFFF
      "\"\xF3\x8F\xBF\xBE\"",  // U+0CFFFE
      "\"\xF3\x8F\xBF\xBF\"",  // U+0CFFFF
      "\"\xF3\x9F\xBF\xBE\"",  // U+0DFFFE
      "\"\xF3\x9F\xBF\xBF\"",  // U+0DFFFF
      "\"\xF3\xAF\xBF\xBE\"",  // U+0EFFFE
      "\"\xF3\xAF\xBF\xBF\"",  // U+0EFFFF
      "\"\xF3\xBF\xBF\xBE\"",  // U+0FFFFE
      "\"\xF3\xBF\xBF\xBF\"",  // U+0FFFFF
      "\"\xF4\x8F\xBF\xBE\"",  // U+10FFFE
      "\"\xF4\x8F\xBF\xBF\"",  // U+10FFFF
  };
  for (auto* noncharacter : noncharacters) {
    root = JSONReader::Read(noncharacter);
    ASSERT_TRUE(root);
    ASSERT_TRUE(root->is_string());
    EXPECT_EQ(std::string(noncharacter + 1, strlen(noncharacter) - 2),
              root->GetString());
  }
}

TEST_P(JSONReaderTest, InvalidUTF8Input) {
  EXPECT_FALSE(JSONReader::Read("\"345\xb0\xa1\xb0\xa2\""));
  EXPECT_FALSE(JSONReader::Read("\"123\xc0\x81\""));
  EXPECT_FALSE(JSONReader::Read("\"abc\xc0\xae\""));
}

TEST_P(JSONReaderTest, UTF16Escapes) {
  std::optional<Value> root = JSONReader::Read("\"\\u20ac3,14\"");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_string());
  EXPECT_EQ(
      "\xe2\x82\xac"
      "3,14",
      root->GetString());

  root = JSONReader::Read("\"\\ud83d\\udca9\\ud83d\\udc6c\"");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_string());
  EXPECT_EQ("\xf0\x9f\x92\xa9\xf0\x9f\x91\xac", root->GetString());
}

TEST_P(JSONReaderTest, InvalidUTF16Escapes) {
  const char* const cases[] = {
      "\"\\u123\"",          // Invalid scalar.
      "\"\\ud83d\"",         // Invalid scalar.
      "\"\\u$%@!\"",         // Invalid scalar.
      "\"\\uzz89\"",         // Invalid scalar.
      "\"\\ud83d\\udca\"",   // Invalid lower surrogate.
      "\"\\ud83d\\ud83d\"",  // Invalid lower surrogate.
      "\"\\ud83d\\uaaaZ\"",  // Invalid lower surrogate.
      "\"\\ud83foo\"",       // No lower surrogate.
      "\"\\ud83d\\foo\"",    // No lower surrogate.
      "\"\\ud83\\foo\"",     // Invalid upper surrogate.
      "\"\\ud83d\\u1\"",     // No lower surrogate.
      "\"\\ud83\\u1\"",      // Invalid upper surrogate.
  };
  std::optional<Value> root;
  for (auto* i : cases) {
    root = JSONReader::Read(i);
    EXPECT_FALSE(root) << i;
  }
}

TEST_P(JSONReaderTest, LiteralRoots) {
  std::optional<Value> root = JSONReader::Read("null");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_none());

  root = JSONReader::Read("true");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_bool());
  EXPECT_TRUE(root->GetBool());

  root = JSONReader::Read("10");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_int());
  EXPECT_EQ(10, root->GetInt());

  root = JSONReader::Read("\"root\"");
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_string());
  EXPECT_EQ("root", root->GetString());
}

TEST_P(JSONReaderTest, ReadFromFile) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(base::DIR_TEST_DATA, &path));
  path = path.AppendASCII("json");
  ASSERT_TRUE(base::PathExists(path));

  std::string input;
  ASSERT_TRUE(ReadFileToString(path.AppendASCII("bom_feff.json"), &input));

  EXPECT_THAT(
      JSONReader::ReadAndReturnValueWithError(input),
      base::test::ValueIs(::testing::Property(&base::Value::is_dict, true)));
}

// Tests that the root of a JSON object can be deleted safely while its
// children outlive it.
TEST_P(JSONReaderTest, StringOptimizations) {
  Value dict_literal_0;
  Value dict_literal_1;
  Value dict_string_0;
  Value dict_string_1;
  Value list_value_0;
  Value list_value_1;

  {
    std::optional<Value> root = JSONReader::Read(
        "{"
        "  \"test\": {"
        "    \"foo\": true,"
        "    \"bar\": 3.14,"
        "    \"baz\": \"bat\","
        "    \"moo\": \"cow\""
        "  },"
        "  \"list\": ["
        "    \"a\","
        "    \"b\""
        "  ]"
        "}",
        JSON_PARSE_RFC);
    ASSERT_TRUE(root);
    Value::Dict* root_dict = root->GetIfDict();
    ASSERT_TRUE(root_dict);

    Value::Dict* dict = root_dict->FindDict("test");
    ASSERT_TRUE(dict);
    Value::List* list = root_dict->FindList("list");
    ASSERT_TRUE(list);

    Value* to_move = dict->Find("foo");
    ASSERT_TRUE(to_move);
    dict_literal_0 = std::move(*to_move);
    to_move = dict->Find("bar");
    ASSERT_TRUE(to_move);
    dict_literal_1 = std::move(*to_move);
    to_move = dict->Find("baz");
    ASSERT_TRUE(to_move);
    dict_string_0 = std::move(*to_move);
    to_move = dict->Find("moo");
    ASSERT_TRUE(to_move);
    dict_string_1 = std::move(*to_move);
    ASSERT_TRUE(dict->Remove("foo"));
    ASSERT_TRUE(dict->Remove("bar"));
    ASSERT_TRUE(dict->Remove("baz"));
    ASSERT_TRUE(dict->Remove("moo"));

    ASSERT_EQ(2u, list->size());
    list_value_0 = std::move((*list)[0]);
    list_value_1 = std::move((*list)[1]);
    list->clear();
  }

  ASSERT_TRUE(dict_literal_0.is_bool());
  EXPECT_TRUE(dict_literal_0.GetBool());

  ASSERT_TRUE(dict_literal_1.is_double());
  EXPECT_EQ(3.14, dict_literal_1.GetDouble());

  ASSERT_TRUE(dict_string_0.is_string());
  EXPECT_EQ("bat", dict_string_0.GetString());

  ASSERT_TRUE(dict_string_1.is_string());
  EXPECT_EQ("cow", dict_string_1.GetString());

  ASSERT_TRUE(list_value_0.is_string());
  EXPECT_EQ("a", list_value_0.GetString());
  ASSERT_TRUE(list_value_1.is_string());
  EXPECT_EQ("b", list_value_1.GetString());
}

// A smattering of invalid JSON designed to test specific portions of the
// parser implementation against buffer overflow. Best run with DCHECKs so
// that the one in NextChar fires.
TEST_P(JSONReaderTest, InvalidSanity) {
  const char* const kInvalidJson[] = {
      "/* test *", "{\"foo\"", "{\"foo\":", "  [", "\"\\u123g\"", "{\n\"eh:\n}",
  };

  for (size_t i = 0; i < std::size(kInvalidJson); ++i) {
    LOG(INFO) << "Sanity test " << i << ": <" << kInvalidJson[i] << ">";
    auto root = JSONReader::ReadAndReturnValueWithError(kInvalidJson[i]);
    EXPECT_FALSE(root.has_value());
    EXPECT_NE("", root.error().message);
  }
}

TEST_P(JSONReaderTest, IllegalTrailingNull) {
  const char json[] = {'"', 'n', 'u', 'l', 'l', '"', '\0'};
  std::string json_string(json, sizeof(json));
  auto root = JSONReader::ReadAndReturnValueWithError(json_string);
  EXPECT_FALSE(root.has_value());
  EXPECT_NE("", root.error().message);
}

TEST_P(JSONReaderTest, ASCIIControlCodes) {
  // A literal NUL byte or a literal new line, in a JSON string, should be
  // rejected. RFC 8259 section 7 says "the characters that MUST be escaped
  // [include]... the control characters (U+0000 through U+001F)".
  //
  // Currently, we accept \r and \n in JSON strings because they are widely used
  // and somewhat useful (especially when nesting JSON messages), but reject all
  // other control characters.
  {
    const char json[] = "\"a\rn\nc\"";
    auto root = JSONReader::Read(json);
    ASSERT_TRUE(root);
    ASSERT_TRUE(root->is_string());
    EXPECT_EQ(5u, root->GetString().length());
  }

  {
    // Replace the \r with a disallowed \f, and require parsing to fail:
    const char json[] = "\"a\fn\nc\"";
    auto root = JSONReader::ReadAndReturnValueWithError(json);
    EXPECT_FALSE(root.has_value());
    EXPECT_NE("", root.error().message);
  }
}

TEST_P(JSONReaderTest, MaxNesting) {
  std::string json(R"({"outer": { "inner": {"foo": true}}})");
  EXPECT_FALSE(JSONReader::Read(json, JSON_PARSE_RFC, 3));
  EXPECT_TRUE(JSONReader::Read(json, JSON_PARSE_RFC, 4));
}

TEST_P(JSONReaderTest, Decode4ByteUtf8Char) {
  // kUtf8Data contains a 4 byte unicode character (a smiley!) that JSONReader
  // should be able to handle. The UTF-8 encoding of U+1F607 SMILING FACE WITH
  // HALO is "\xF0\x9F\x98\x87".
  const char kUtf8Data[] = "[\"😇\",[],[],[],{\"google:suggesttype\":[]}]";
  std::optional<Value> root = JSONReader::Read(kUtf8Data, JSON_PARSE_RFC);
  ASSERT_TRUE(root);
  Value::List* list = root->GetIfList();
  ASSERT_TRUE(list);
  ASSERT_EQ(5u, list->size());
  ASSERT_TRUE((*list)[0].is_string());
  EXPECT_EQ("\xF0\x9F\x98\x87", (*list)[0].GetString());
}

TEST_P(JSONReaderTest, DecodeUnicodeNonCharacter) {
  // Tests Unicode code points (encoded as escaped UTF-16) that are not valid
  // characters.
  EXPECT_TRUE(JSONReader::Read("[\"\\uFDD0\"]"));         // U+FDD0
  EXPECT_TRUE(JSONReader::Read("[\"\\uFDDF\"]"));         // U+FDDF
  EXPECT_TRUE(JSONReader::Read("[\"\\uFDEF\"]"));         // U+FDEF
  EXPECT_TRUE(JSONReader::Read("[\"\\uFFFE\"]"));         // U+FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uFFFF\"]"));         // U+FFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uD83F\\uDFFE\"]"));  // U+01FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uD83F\\uDFFF\"]"));  // U+01FFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uD87F\\uDFFE\"]"));  // U+02FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uD87F\\uDFFF\"]"));  // U+02FFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uD8BF\\uDFFE\"]"));  // U+03FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uD8BF\\uDFFF\"]"));  // U+03FFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uD8FF\\uDFFE\"]"));  // U+04FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uD8FF\\uDFFF\"]"));  // U+04FFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uD93F\\uDFFE\"]"));  // U+05FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uD93F\\uDFFF\"]"));  // U+05FFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uD97F\\uDFFE\"]"));  // U+06FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uD97F\\uDFFF\"]"));  // U+06FFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uD9BF\\uDFFE\"]"));  // U+07FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uD9BF\\uDFFF\"]"));  // U+07FFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uD9FF\\uDFFE\"]"));  // U+08FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uD9FF\\uDFFF\"]"));  // U+08FFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uDA3F\\uDFFE\"]"));  // U+09FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uDA3F\\uDFFF\"]"));  // U+09FFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uDA7F\\uDFFE\"]"));  // U+0AFFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uDA7F\\uDFFF\"]"));  // U+0AFFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uDABF\\uDFFE\"]"));  // U+0BFFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uDABF\\uDFFF\"]"));  // U+0BFFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uDAFF\\uDFFE\"]"));  // U+0CFFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uDAFF\\uDFFF\"]"));  // U+0CFFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uDB3F\\uDFFE\"]"));  // U+0DFFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uDB3F\\uDFFF\"]"));  // U+0DFFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uDB7F\\uDFFE\"]"));  // U+0EFFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uDB7F\\uDFFF\"]"));  // U+0EFFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uDBBF\\uDFFE\"]"));  // U+0FFFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uDBBF\\uDFFF\"]"));  // U+0FFFFF
  EXPECT_TRUE(JSONReader::Read("[\"\\uDBFF\\uDFFE\"]"));  // U+10FFFE
  EXPECT_TRUE(JSONReader::Read("[\"\\uDBFF\\uDFFF\"]"));  // U+10FFFF
}

TEST_P(JSONReaderTest, DecodeNegativeEscapeSequence) {
  EXPECT_FALSE(JSONReader::Read("[\"\\x-A\"]"));
  EXPECT_FALSE(JSONReader::Read("[\"\\u-00A\"]"));
}

// Verifies invalid code points are replaced.
TEST_P(JSONReaderTest, ReplaceInvalidCharacters) {
  // U+D800 is a lone high surrogate.
  const std::string invalid_high = "\"\xED\xA0\x80\"";
  std::optional<Value> value =
      JSONReader::Read(invalid_high, JSON_REPLACE_INVALID_CHARACTERS);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_string());
  // Expect three U+FFFD (one for each UTF-8 byte in the invalid code point).
  EXPECT_EQ("\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD", value->GetString());

  // U+DFFF is a lone low surrogate.
  const std::string invalid_low = "\"\xED\xBF\xBF\"";
  value = JSONReader::Read(invalid_low, JSON_REPLACE_INVALID_CHARACTERS);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_string());
  // Expect three U+FFFD (one for each UTF-8 byte in the invalid code point).
  EXPECT_EQ("\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD", value->GetString());
}

TEST_P(JSONReaderTest, ReplaceInvalidUTF16EscapeSequence) {
  // U+D800 is a lone high surrogate.
  const std::string invalid_high = "\"_\\uD800_\"";
  std::optional<Value> value =
      JSONReader::Read(invalid_high, JSON_REPLACE_INVALID_CHARACTERS);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ("_\xEF\xBF\xBD_", value->GetString());

  // U+DFFF is a lone low surrogate.
  const std::string invalid_low = "\"_\\uDFFF_\"";
  value = JSONReader::Read(invalid_low, JSON_REPLACE_INVALID_CHARACTERS);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ("_\xEF\xBF\xBD_", value->GetString());
}

TEST_P(JSONReaderTest, ParseNumberErrors) {
  const struct {
    const char* input;
    bool parse_success;
    double value;
  } kCases[] = {
      // clang-format off
      {"1", true, 1},
      {"2.", false, 0},
      {"42", true, 42},
      {"6e", false, 0},
      {"43e2", true, 4300},
      {"43e-", false, 0},
      {"9e-3", true, 0.009},
      {"2e+", false, 0},
      {"2e+2", true, 200},
      // clang-format on
  };

  for (unsigned int i = 0; i < std::size(kCases); ++i) {
    auto test_case = kCases[i];
    SCOPED_TRACE(StringPrintf("case %u: \"%s\"", i, test_case.input));

    std::unique_ptr<char[]> input_owner;
    std::string_view input =
        MakeNotNullTerminatedInput(test_case.input, &input_owner);

    std::optional<Value> result = JSONReader::Read(input);
    EXPECT_EQ(test_case.parse_success, result.has_value());

    if (!result)
      continue;

    ASSERT_TRUE(result->is_double() || result->is_int());
    EXPECT_EQ(test_case.value, result->GetDouble());
  }
}

TEST_P(JSONReaderTest, UnterminatedInputs) {
  const char* const kCases[] = {
      // clang-format off
      "/",
      "//",
      "/*",
      "\"xxxxxx",
      "\"",
      "{   ",
      "[\t",
      "tru",
      "fals",
      "nul",
      "\"\\x",
      "\"\\x2",
      "\"\\u123",
      "\"\\uD803\\u",
      "\"\\",
      "\"\\/",
      // clang-format on
  };

  for (unsigned int i = 0; i < std::size(kCases); ++i) {
    auto* test_case = kCases[i];
    SCOPED_TRACE(StringPrintf("case %u: \"%s\"", i, test_case));

    std::unique_ptr<char[]> input_owner;
    std::string_view input =
        MakeNotNullTerminatedInput(test_case, &input_owner);

    EXPECT_FALSE(JSONReader::Read(input));
  }
}

TEST_P(JSONReaderTest, LineColumnCounting) {
  const struct {
    const char* input;
    int error_line;
    int error_column;
  } kCases[] = {
      // For all but the "q_is_not_etc" case, the error (indicated by ^ in the
      // comments) is seeing a digit when expecting ',' or ']'.
      {
          // Line and column counts are 1-based, not 0-based.
          "q_is_not_the_start_of_any_valid_JSON_token",
          1,
          1,
      },
      {
          "[2,4,6 8",
          // -----^
          1,
          8,
      },
      {
          "[2,4,6\t8",
          // ------^
          1,
          8,
      },
      {
          "[2,4,6\n8",
          // ------^
          2,
          1,
      },
      {
          "[\n0,\n1,\n2,\n3,4,5,6 7,\n8,\n9\n]",
          // ---------------------^
          5,
          9,
      },
      {
          // Same as the previous example, but with "\r\n"s instead of "\n"s.
          "[\r\n0,\r\n1,\r\n2,\r\n3,4,5,6 7,\r\n8,\r\n9\r\n]",
          // -----------------------------^
          5,
          9,
      },
      // The JSON spec forbids unescaped ASCII control characters (including
      // line breaks) within a string, but our implementation is more lenient.
      {
          "[\"3\n1\" 4",
          // --------^
          2,
          4,
      },
      {
          "[\"3\r\n1\" 4",
          // ----------^
          2,
          4,
      },
  };

  for (unsigned int i = 0; i < std::size(kCases); ++i) {
    auto test_case = kCases[i];
    SCOPED_TRACE(StringPrintf("case %u: \"%s\"", i, test_case.input));

    auto root = JSONReader::ReadAndReturnValueWithError(
        test_case.input, JSON_PARSE_RFC | JSON_ALLOW_NEWLINES_IN_STRINGS);
    EXPECT_FALSE(root.has_value());
    EXPECT_EQ(test_case.error_line, root.error().line);
    EXPECT_EQ(test_case.error_column, root.error().column);
  }
}

TEST_P(JSONReaderTest, ChromiumExtensions) {
  // All of these cases should parse with JSON_PARSE_CHROMIUM_EXTENSIONS but
  // fail with JSON_PARSE_RFC.
  const struct {
    // The JSON input.
    const char* input;
    // What JSON_* option permits this extension.
    int option;
  } kCases[] = {
      {"{ /* comment */ \"foo\": 3 }", JSON_ALLOW_COMMENTS},
      {"{ // comment\n \"foo\": 3 }", JSON_ALLOW_COMMENTS},
      {"[\"\\xAB\"]", JSON_ALLOW_X_ESCAPES},
      {"[\"\n\"]", JSON_ALLOW_NEWLINES_IN_STRINGS},
      {"[\"\r\"]", JSON_ALLOW_NEWLINES_IN_STRINGS},
  };

  for (size_t i = 0; i < std::size(kCases); ++i) {
    SCOPED_TRACE(testing::Message() << "case " << i);
    const auto& test_case = kCases[i];

    auto result = JSONReader::ReadAndReturnValueWithError(test_case.input,
                                                          JSON_PARSE_RFC);
    EXPECT_FALSE(result.has_value());

    result = JSONReader::ReadAndReturnValueWithError(
        test_case.input, JSON_PARSE_RFC | test_case.option);
    EXPECT_TRUE(result.has_value());

    result = JSONReader::ReadAndReturnValueWithError(
        test_case.input, JSON_PARSE_CHROMIUM_EXTENSIONS);
    EXPECT_TRUE(result.has_value());

    result = JSONReader::ReadAndReturnValueWithError(
        test_case.input, JSON_PARSE_CHROMIUM_EXTENSIONS & ~test_case.option);
    EXPECT_FALSE(result.has_value());
  }
}

// For every control character, place it unescaped in a string and ensure that:
// a) It doesn't parse with JSON_PARSE_RFC
// b) It doesn't parse with JSON_PARSE_CHROMIUM_EXTENSIONS
// c) It does parse with JSON_ALLOW_CONTROL_CHARS
TEST_P(JSONReaderTest, UnescapedControls) {
  std::string input = "\"foo\"";
  // ECMA-404 (JSON standard) section 9: characters from 0x00 to 0x1f must be
  // escaped.
  for (char c = 0x00; c <= 0x1f; c++) {
    input[1] = c;

    auto result = JSONReader::Read(input, JSON_PARSE_RFC);
    EXPECT_FALSE(result.has_value());

    bool should_parse_with_extensions = (c == '\r' || c == '\n');
    result = JSONReader::Read(input, JSON_PARSE_CHROMIUM_EXTENSIONS);
    EXPECT_EQ(should_parse_with_extensions, result.has_value());

    result = JSONReader::Read(input, JSON_ALLOW_CONTROL_CHARS);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_string());
    EXPECT_EQ(result->GetString().length(), input.length() - 2);
    EXPECT_EQ(result->GetString()[0], c);
  }
}

TEST_P(JSONReaderTest, UsingRust) {
  ASSERT_EQ(JSONReader::UsingRust(), using_rust_);
}

INSTANTIATE_TEST_SUITE_P(All,
                         JSONReaderTest,
#if BUILDFLAG(BUILD_RUST_JSON_READER)
                         testing::Bool(),
#else   // BUILDFLAG(BUILD_RUST_JSON_READER)
                         testing::Values(false),
#endif  // BUILDFLAG(BUILD_RUST_JSON_READER)
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Rust" : "Cpp";
                         });

}  // namespace base
