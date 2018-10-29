// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"

#include <stddef.h>

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(JSONReaderTest, Whitespace) {
  std::unique_ptr<Value> root = JSONReader().ReadToValue("   null   ");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_none());
}

TEST(JSONReaderTest, InvalidString) {
  EXPECT_FALSE(JSONReader().ReadToValue("nu"));
}

TEST(JSONReaderTest, SimpleBool) {
  std::unique_ptr<Value> root = JSONReader().ReadToValue("true  ");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_bool());
}

TEST(JSONReaderTest, EmbeddedComments) {
  std::unique_ptr<Value> root = JSONReader().ReadToValue("/* comment */null");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_none());
  root = JSONReader().ReadToValue("40 /* comment */");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_int());
  root = JSONReader().ReadToValue("true // comment");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_bool());
  root = JSONReader().ReadToValue("/* comment */\"sample string\"");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_string());
  std::string value;
  EXPECT_TRUE(root->GetAsString(&value));
  EXPECT_EQ("sample string", value);
  std::unique_ptr<ListValue> list =
      ListValue::From(JSONReader().ReadToValue("[1, /* comment, 2 ] */ \n 3]"));
  ASSERT_TRUE(list);
  EXPECT_EQ(2u, list->GetSize());
  int int_val = 0;
  EXPECT_TRUE(list->GetInteger(0, &int_val));
  EXPECT_EQ(1, int_val);
  EXPECT_TRUE(list->GetInteger(1, &int_val));
  EXPECT_EQ(3, int_val);
  list = ListValue::From(JSONReader().ReadToValue("[1, /*a*/2, 3]"));
  ASSERT_TRUE(list);
  EXPECT_EQ(3u, list->GetSize());
  root = JSONReader().ReadToValue("/* comment **/42");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_int());
  EXPECT_TRUE(root->GetAsInteger(&int_val));
  EXPECT_EQ(42, int_val);
  root = JSONReader().ReadToValue(
      "/* comment **/\n"
      "// */ 43\n"
      "44");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_int());
  EXPECT_TRUE(root->GetAsInteger(&int_val));
  EXPECT_EQ(44, int_val);
}

TEST(JSONReaderTest, Ints) {
  std::unique_ptr<Value> root = JSONReader().ReadToValue("43");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_int());
  int int_val = 0;
  EXPECT_TRUE(root->GetAsInteger(&int_val));
  EXPECT_EQ(43, int_val);
}

TEST(JSONReaderTest, NonDecimalNumbers) {
  // According to RFC4627, oct, hex, and leading zeros are invalid JSON.
  EXPECT_FALSE(JSONReader().ReadToValue("043"));
  EXPECT_FALSE(JSONReader().ReadToValue("0x43"));
  EXPECT_FALSE(JSONReader().ReadToValue("00"));
}

TEST(JSONReaderTest, NumberZero) {
  // Test 0 (which needs to be special cased because of the leading zero
  // clause).
  std::unique_ptr<Value> root = JSONReader().ReadToValue("0");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_int());
  int int_val = 1;
  EXPECT_TRUE(root->GetAsInteger(&int_val));
  EXPECT_EQ(0, int_val);
}

TEST(JSONReaderTest, LargeIntPromotion) {
  // Numbers that overflow ints should succeed, being internally promoted to
  // storage as doubles
  std::unique_ptr<Value> root = JSONReader().ReadToValue("2147483648");
  ASSERT_TRUE(root);
  double double_val;
  EXPECT_TRUE(root->is_double());
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(2147483648.0, double_val);
  root = JSONReader().ReadToValue("-2147483649");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(-2147483649.0, double_val);
}

TEST(JSONReaderTest, Doubles) {
  std::unique_ptr<Value> root = JSONReader().ReadToValue("43.1");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  double double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(43.1, double_val);

  root = JSONReader().ReadToValue("4.3e-1");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(.43, double_val);

  root = JSONReader().ReadToValue("2.1e0");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(2.1, double_val);

  root = JSONReader().ReadToValue("2.1e+0001");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(21.0, double_val);

  root = JSONReader().ReadToValue("0.01");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(0.01, double_val);

  root = JSONReader().ReadToValue("1.00");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_double());
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(1.0, double_val);
}

TEST(JSONReaderTest, FractionalNumbers) {
  // Fractional parts must have a digit before and after the decimal point.
  EXPECT_FALSE(JSONReader().ReadToValue("1."));
  EXPECT_FALSE(JSONReader().ReadToValue(".1"));
  EXPECT_FALSE(JSONReader().ReadToValue("1.e10"));
}

TEST(JSONReaderTest, ExponentialNumbers) {
  // Exponent must have a digit following the 'e'.
  EXPECT_FALSE(JSONReader().ReadToValue("1e"));
  EXPECT_FALSE(JSONReader().ReadToValue("1E"));
  EXPECT_FALSE(JSONReader().ReadToValue("1e1."));
  EXPECT_FALSE(JSONReader().ReadToValue("1e1.0"));
}

TEST(JSONReaderTest, InvalidNAN) {
  EXPECT_FALSE(JSONReader().ReadToValue("1e1000"));
  EXPECT_FALSE(JSONReader().ReadToValue("-1e1000"));
  EXPECT_FALSE(JSONReader().ReadToValue("NaN"));
  EXPECT_FALSE(JSONReader().ReadToValue("nan"));
  EXPECT_FALSE(JSONReader().ReadToValue("inf"));
}

TEST(JSONReaderTest, InvalidNumbers) {
  EXPECT_FALSE(JSONReader().ReadToValue("4.3.1"));
  EXPECT_FALSE(JSONReader().ReadToValue("4e3.1"));
  EXPECT_FALSE(JSONReader().ReadToValue("4.a"));
}

TEST(JSONReader, SimpleString) {
  std::unique_ptr<Value> root = JSONReader().ReadToValue("\"hello world\"");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_string());
  std::string str_val;
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ("hello world", str_val);
}

TEST(JSONReaderTest, EmptyString) {
  std::unique_ptr<Value> root = JSONReader().ReadToValue("\"\"");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_string());
  std::string str_val;
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ("", str_val);
}

TEST(JSONReaderTest, BasicStringEscapes) {
  std::unique_ptr<Value> root =
      JSONReader().ReadToValue("\" \\\"\\\\\\/\\b\\f\\n\\r\\t\\v\"");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_string());
  std::string str_val;
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ(" \"\\/\b\f\n\r\t\v", str_val);
}

TEST(JSONReaderTest, UnicodeEscapes) {
  // Test hex and unicode escapes including the null character.
  std::unique_ptr<Value> root =
      JSONReader().ReadToValue("\"\\x41\\x00\\u1234\\u0000\"");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_string());
  std::string str_val;
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ(std::wstring(L"A\0\x1234\0", 4), UTF8ToWide(str_val));
}

TEST(JSONReaderTest, InvalidStrings) {
  EXPECT_FALSE(JSONReader().ReadToValue("\"no closing quote"));
  EXPECT_FALSE(JSONReader().ReadToValue("\"\\z invalid escape char\""));
  EXPECT_FALSE(JSONReader().ReadToValue("\"\\xAQ invalid hex code\""));
  EXPECT_FALSE(JSONReader().ReadToValue("not enough hex chars\\x1\""));
  EXPECT_FALSE(JSONReader().ReadToValue("\"not enough escape chars\\u123\""));
  EXPECT_FALSE(
      JSONReader().ReadToValue("\"extra backslash at end of input\\\""));
}

TEST(JSONReaderTest, BasicArray) {
  std::unique_ptr<ListValue> list =
      ListValue::From(JSONReader::Read("[true, false, null]"));
  ASSERT_TRUE(list);
  EXPECT_EQ(3U, list->GetSize());

  // Test with trailing comma.  Should be parsed the same as above.
  std::unique_ptr<Value> root2 =
      JSONReader::Read("[true, false, null, ]", JSON_ALLOW_TRAILING_COMMAS);
  EXPECT_TRUE(list->Equals(root2.get()));
}

TEST(JSONReaderTest, EmptyArray) {
  std::unique_ptr<ListValue> list = ListValue::From(JSONReader::Read("[]"));
  ASSERT_TRUE(list);
  EXPECT_EQ(0U, list->GetSize());
}

TEST(JSONReaderTest, NestedArrays) {
  std::unique_ptr<ListValue> list = ListValue::From(
      JSONReader::Read("[[true], [], [false, [], [null]], null]"));
  ASSERT_TRUE(list);
  EXPECT_EQ(4U, list->GetSize());

  // Lots of trailing commas.
  std::unique_ptr<Value> root2 =
      JSONReader::Read("[[true], [], [false, [], [null, ]  , ], null,]",
                       JSON_ALLOW_TRAILING_COMMAS);
  EXPECT_TRUE(list->Equals(root2.get()));
}

TEST(JSONReaderTest, InvalidArrays) {
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

TEST(JSONReaderTest, ArrayTrailingComma) {
  // Valid if we set |allow_trailing_comma| to true.
  std::unique_ptr<ListValue> list =
      ListValue::From(JSONReader::Read("[true,]", JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(list);
  EXPECT_EQ(1U, list->GetSize());
  Value* tmp_value = nullptr;
  ASSERT_TRUE(list->Get(0, &tmp_value));
  EXPECT_TRUE(tmp_value->is_bool());
  bool bool_value = false;
  EXPECT_TRUE(tmp_value->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);
}

TEST(JSONReaderTest, ArrayTrailingCommaNoEmptyElements) {
  // Don't allow empty elements, even if |allow_trailing_comma| is
  // true.
  EXPECT_FALSE(JSONReader::Read("[,]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(JSONReader::Read("[true,,]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(JSONReader::Read("[,true,]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(JSONReader::Read("[true,,false]", JSON_ALLOW_TRAILING_COMMAS));
}

TEST(JSONReaderTest, EmptyDictionary) {
  std::unique_ptr<DictionaryValue> dict_val =
      DictionaryValue::From(JSONReader::Read("{}"));
  ASSERT_TRUE(dict_val);
}

TEST(JSONReaderTest, CompleteDictionary) {
  auto dict_val = DictionaryValue::From(JSONReader::Read(
      "{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\" }"));
  ASSERT_TRUE(dict_val);
  double double_val = 0.0;
  EXPECT_TRUE(dict_val->GetDouble("number", &double_val));
  EXPECT_DOUBLE_EQ(9.87654321, double_val);
  Value* null_val = nullptr;
  ASSERT_TRUE(dict_val->Get("null", &null_val));
  EXPECT_TRUE(null_val->is_none());
  std::string str_val;
  EXPECT_TRUE(dict_val->GetString("S", &str_val));
  EXPECT_EQ("str", str_val);

  std::unique_ptr<Value> root2 = JSONReader::Read(
      "{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\", }",
      JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(root2);
  EXPECT_TRUE(dict_val->Equals(root2.get()));

  // Test newline equivalence.
  root2 = JSONReader::Read(
      "{\n"
      "  \"number\":9.87654321,\n"
      "  \"null\":null,\n"
      "  \"\\x53\":\"str\",\n"
      "}\n",
      JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(root2);
  EXPECT_TRUE(dict_val->Equals(root2.get()));

  root2 = JSONReader::Read(
      "{\r\n"
      "  \"number\":9.87654321,\r\n"
      "  \"null\":null,\r\n"
      "  \"\\x53\":\"str\",\r\n"
      "}\r\n",
      JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(root2);
  EXPECT_TRUE(dict_val->Equals(root2.get()));
}

TEST(JSONReaderTest, NestedDictionaries) {
  std::unique_ptr<DictionaryValue> dict_val =
      DictionaryValue::From(JSONReader::Read(
          "{\"inner\":{\"array\":[true]},\"false\":false,\"d\":{}}"));
  ASSERT_TRUE(dict_val);
  DictionaryValue* inner_dict = nullptr;
  ASSERT_TRUE(dict_val->GetDictionary("inner", &inner_dict));
  ListValue* inner_array = nullptr;
  ASSERT_TRUE(inner_dict->GetList("array", &inner_array));
  EXPECT_EQ(1U, inner_array->GetSize());
  bool bool_value = true;
  EXPECT_TRUE(dict_val->GetBoolean("false", &bool_value));
  EXPECT_FALSE(bool_value);
  inner_dict = nullptr;
  EXPECT_TRUE(dict_val->GetDictionary("d", &inner_dict));

  std::unique_ptr<Value> root2 = JSONReader::Read(
      "{\"inner\": {\"array\":[true] , },\"false\":false,\"d\":{},}",
      JSON_ALLOW_TRAILING_COMMAS);
  EXPECT_TRUE(dict_val->Equals(root2.get()));
}

TEST(JSONReaderTest, DictionaryKeysWithPeriods) {
  std::unique_ptr<DictionaryValue> dict_val = DictionaryValue::From(
      JSONReader::Read("{\"a.b\":3,\"c\":2,\"d.e.f\":{\"g.h.i.j\":1}}"));
  ASSERT_TRUE(dict_val);
  int integer_value = 0;
  EXPECT_TRUE(dict_val->GetIntegerWithoutPathExpansion("a.b", &integer_value));
  EXPECT_EQ(3, integer_value);
  EXPECT_TRUE(dict_val->GetIntegerWithoutPathExpansion("c", &integer_value));
  EXPECT_EQ(2, integer_value);
  DictionaryValue* inner_dict = nullptr;
  ASSERT_TRUE(
      dict_val->GetDictionaryWithoutPathExpansion("d.e.f", &inner_dict));
  EXPECT_EQ(1U, inner_dict->size());
  EXPECT_TRUE(
      inner_dict->GetIntegerWithoutPathExpansion("g.h.i.j", &integer_value));
  EXPECT_EQ(1, integer_value);

  dict_val =
      DictionaryValue::From(JSONReader::Read("{\"a\":{\"b\":2},\"a.b\":1}"));
  ASSERT_TRUE(dict_val);
  EXPECT_TRUE(dict_val->GetInteger("a.b", &integer_value));
  EXPECT_EQ(2, integer_value);
  EXPECT_TRUE(dict_val->GetIntegerWithoutPathExpansion("a.b", &integer_value));
  EXPECT_EQ(1, integer_value);
}

TEST(JSONReaderTest, InvalidDictionaries) {
  // No closing brace.
  EXPECT_FALSE(JSONReader::Read("{\"a\": true"));

  // Keys must be quoted strings.
  EXPECT_FALSE(JSONReader::Read("{foo:true}"));
  EXPECT_FALSE(JSONReader::Read("{1234: false}"));
  EXPECT_FALSE(JSONReader::Read("{:false}"));

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

TEST(JSONReaderTest, StackOverflow) {
  std::string evil(1000000, '[');
  evil.append(std::string(1000000, ']'));
  EXPECT_FALSE(JSONReader::Read(evil));

  // A few thousand adjacent lists is fine.
  std::string not_evil("[");
  not_evil.reserve(15010);
  for (int i = 0; i < 5000; ++i)
    not_evil.append("[],");
  not_evil.append("[]]");
  std::unique_ptr<ListValue> list = ListValue::From(JSONReader::Read(not_evil));
  ASSERT_TRUE(list);
  EXPECT_EQ(5001U, list->GetSize());
}

TEST(JSONReaderTest, UTF8Input) {
  std::unique_ptr<Value> root =
      JSONReader().ReadToValue("\"\xe7\xbd\x91\xe9\xa1\xb5\"");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_string());
  std::string str_val;
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ(L"\x7f51\x9875", UTF8ToWide(str_val));

  std::unique_ptr<DictionaryValue> dict_val =
      DictionaryValue::From(JSONReader().ReadToValue(
          "{\"path\": \"/tmp/\xc3\xa0\xc3\xa8\xc3\xb2.png\"}"));
  ASSERT_TRUE(dict_val);
  EXPECT_TRUE(dict_val->GetString("path", &str_val));
  EXPECT_EQ("/tmp/\xC3\xA0\xC3\xA8\xC3\xB2.png", str_val);
}

TEST(JSONReaderTest, InvalidUTF8Input) {
  EXPECT_FALSE(JSONReader().ReadToValue("\"345\xb0\xa1\xb0\xa2\""));
  EXPECT_FALSE(JSONReader().ReadToValue("\"123\xc0\x81\""));
  EXPECT_FALSE(JSONReader().ReadToValue("\"abc\xc0\xae\""));
}

TEST(JSONReaderTest, UTF16Escapes) {
  std::unique_ptr<Value> root = JSONReader().ReadToValue("\"\\u20ac3,14\"");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_string());
  std::string str_val;
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ(
      "\xe2\x82\xac"
      "3,14",
      str_val);

  root = JSONReader().ReadToValue("\"\\ud83d\\udca9\\ud83d\\udc6c\"");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_string());
  str_val.clear();
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ("\xf0\x9f\x92\xa9\xf0\x9f\x91\xac", str_val);
}

TEST(JSONReaderTest, InvalidUTF16Escapes) {
  const char* const cases[] = {
      "\"\\u123\"",          // Invalid scalar.
      "\"\\ud83d\"",         // Invalid scalar.
      "\"\\u$%@!\"",         // Invalid scalar.
      "\"\\uzz89\"",         // Invalid scalar.
      "\"\\ud83d\\udca\"",   // Invalid lower surrogate.
      "\"\\ud83d\\ud83d\"",  // Invalid lower surrogate.
      "\"\\ud83d\\uaaaZ\""   // Invalid lower surrogate.
      "\"\\ud83foo\"",       // No lower surrogate.
      "\"\\ud83d\\foo\""     // No lower surrogate.
      "\"\\ud83\\foo\""      // Invalid upper surrogate.
      "\"\\ud83d\\u1\""      // No lower surrogate.
      "\"\\ud83\\u1\""       // Invalid upper surrogate.
  };
  std::unique_ptr<Value> root;
  for (auto* i : cases) {
    root = JSONReader().ReadToValue(i);
    EXPECT_FALSE(root) << i;
  }
}

TEST(JSONReaderTest, LiteralRoots) {
  std::unique_ptr<Value> root = JSONReader::Read("null");
  ASSERT_TRUE(root);
  EXPECT_TRUE(root->is_none());

  root = JSONReader::Read("true");
  ASSERT_TRUE(root);
  bool bool_value;
  EXPECT_TRUE(root->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);

  root = JSONReader::Read("10");
  ASSERT_TRUE(root);
  int integer_value;
  EXPECT_TRUE(root->GetAsInteger(&integer_value));
  EXPECT_EQ(10, integer_value);

  root = JSONReader::Read("\"root\"");
  ASSERT_TRUE(root);
  std::string str_val;
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ("root", str_val);
}

TEST(JSONReaderTest, ReadFromFile) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(base::DIR_TEST_DATA, &path));
  path = path.AppendASCII("json");
  ASSERT_TRUE(base::PathExists(path));

  std::string input;
  ASSERT_TRUE(ReadFileToString(path.AppendASCII("bom_feff.json"), &input));

  JSONReader reader;
  std::unique_ptr<Value> root(reader.ReadToValue(input));
  ASSERT_TRUE(root) << reader.GetErrorMessage();
  EXPECT_TRUE(root->is_dict());
}

// Tests that the root of a JSON object can be deleted safely while its
// children outlive it.
TEST(JSONReaderTest, StringOptimizations) {
  std::unique_ptr<Value> dict_literal_0;
  std::unique_ptr<Value> dict_literal_1;
  std::unique_ptr<Value> dict_string_0;
  std::unique_ptr<Value> dict_string_1;
  std::unique_ptr<Value> list_value_0;
  std::unique_ptr<Value> list_value_1;

  {
    std::unique_ptr<Value> root = JSONReader::Read(
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

    DictionaryValue* root_dict = nullptr;
    ASSERT_TRUE(root->GetAsDictionary(&root_dict));

    DictionaryValue* dict = nullptr;
    ListValue* list = nullptr;

    ASSERT_TRUE(root_dict->GetDictionary("test", &dict));
    ASSERT_TRUE(root_dict->GetList("list", &list));

    ASSERT_TRUE(dict->Remove("foo", &dict_literal_0));
    ASSERT_TRUE(dict->Remove("bar", &dict_literal_1));
    ASSERT_TRUE(dict->Remove("baz", &dict_string_0));
    ASSERT_TRUE(dict->Remove("moo", &dict_string_1));

    ASSERT_EQ(2u, list->GetSize());
    ASSERT_TRUE(list->Remove(0, &list_value_0));
    ASSERT_TRUE(list->Remove(0, &list_value_1));
  }

  bool b = false;
  double d = 0;
  std::string s;

  EXPECT_TRUE(dict_literal_0->GetAsBoolean(&b));
  EXPECT_TRUE(b);

  EXPECT_TRUE(dict_literal_1->GetAsDouble(&d));
  EXPECT_EQ(3.14, d);

  EXPECT_TRUE(dict_string_0->GetAsString(&s));
  EXPECT_EQ("bat", s);

  EXPECT_TRUE(dict_string_1->GetAsString(&s));
  EXPECT_EQ("cow", s);

  EXPECT_TRUE(list_value_0->GetAsString(&s));
  EXPECT_EQ("a", s);
  EXPECT_TRUE(list_value_1->GetAsString(&s));
  EXPECT_EQ("b", s);
}

// A smattering of invalid JSON designed to test specific portions of the
// parser implementation against buffer overflow. Best run with DCHECKs so
// that the one in NextChar fires.
TEST(JSONReaderTest, InvalidSanity) {
  const char* const kInvalidJson[] = {
      "/* test *", "{\"foo\"", "{\"foo\":", "  [", "\"\\u123g\"", "{\n\"eh:\n}",
  };

  for (size_t i = 0; i < arraysize(kInvalidJson); ++i) {
    JSONReader reader;
    LOG(INFO) << "Sanity test " << i << ": <" << kInvalidJson[i] << ">";
    EXPECT_FALSE(reader.ReadToValue(kInvalidJson[i]));
    EXPECT_NE(JSONReader::JSON_NO_ERROR, reader.error_code());
    EXPECT_NE("", reader.GetErrorMessage());
  }
}

TEST(JSONReaderTest, IllegalTrailingNull) {
  const char json[] = { '"', 'n', 'u', 'l', 'l', '"', '\0' };
  std::string json_string(json, sizeof(json));
  JSONReader reader;
  EXPECT_FALSE(reader.ReadToValue(json_string));
  EXPECT_EQ(JSONReader::JSON_UNEXPECTED_DATA_AFTER_ROOT, reader.error_code());
}

TEST(JSONReaderTest, MaxNesting) {
  std::string json(R"({"outer": { "inner": {"foo": true}}})");
  std::unique_ptr<Value> root;
  root = JSONReader::Read(json, JSON_PARSE_RFC, 3);
  ASSERT_FALSE(root);
  root = JSONReader::Read(json, JSON_PARSE_RFC, 4);
  ASSERT_TRUE(root);
}

}  // namespace base
