// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_parser.h"

#include <stddef.h>

#include <memory>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

class JSONParserTest : public testing::Test {
 public:
  JSONParser* NewTestParser(const std::string& input,
                            int options = JSON_PARSE_RFC) {
    JSONParser* parser = new JSONParser(options);
    parser->input_ = input;
    parser->index_ = 0;
    return parser;
  }

  // MSan will do a better job detecting over-read errors if the input is
  // not nul-terminated on the heap. This will copy |input| to a new buffer
  // owned by |owner|, returning a StringPiece to |owner|.
  StringPiece MakeNotNullTerminatedInput(const char* input,
                                         std::unique_ptr<char[]>* owner) {
    size_t str_len = strlen(input);
    owner->reset(new char[str_len]);
    memcpy(owner->get(), input, str_len);
    return StringPiece(owner->get(), str_len);
  }

  void TestLastThree(JSONParser* parser) {
    EXPECT_EQ(',', *parser->PeekChar());
    parser->ConsumeChar();
    EXPECT_EQ('|', *parser->PeekChar());
    parser->ConsumeChar();
    EXPECT_EQ('\0', *parser->pos());
    EXPECT_EQ(static_cast<size_t>(parser->index_), parser->input_.length());
  }
};

TEST_F(JSONParserTest, NextChar) {
  std::string input("Hello world");
  std::unique_ptr<JSONParser> parser(NewTestParser(input));

  EXPECT_EQ('H', *parser->pos());
  for (size_t i = 1; i < input.length(); ++i) {
    parser->ConsumeChar();
    EXPECT_EQ(input[i], *parser->PeekChar());
  }
  parser->ConsumeChar();
  EXPECT_EQ('\0', *parser->pos());
  EXPECT_EQ(static_cast<size_t>(parser->index_), parser->input_.length());
}

TEST_F(JSONParserTest, ConsumeString) {
  std::string input("\"test\",|");
  std::unique_ptr<JSONParser> parser(NewTestParser(input));
  Optional<Value> value(parser->ConsumeString());
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  std::string str;
  EXPECT_TRUE(value->GetAsString(&str));
  EXPECT_EQ("test", str);
}

TEST_F(JSONParserTest, ConsumeList) {
  std::string input("[true, false],|");
  std::unique_ptr<JSONParser> parser(NewTestParser(input));
  Optional<Value> value(parser->ConsumeList());
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_list());
  EXPECT_EQ(2u, value->GetList().size());
}

TEST_F(JSONParserTest, ConsumeDictionary) {
  std::string input("{\"abc\":\"def\"},|");
  std::unique_ptr<JSONParser> parser(NewTestParser(input));
  Optional<Value> value(parser->ConsumeDictionary());
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_dict());
  const std::string* str = value->FindStringKey("abc");
  ASSERT_TRUE(str);
  EXPECT_EQ("def", *str);
}

TEST_F(JSONParserTest, ConsumeLiterals) {
  // Literal |true|.
  std::string input("true,|");
  std::unique_ptr<JSONParser> parser(NewTestParser(input));
  Optional<Value> value(parser->ConsumeLiteral());
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  bool bool_value = false;
  EXPECT_TRUE(value->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);

  // Literal |false|.
  input = "false,|";
  parser.reset(NewTestParser(input));
  value = parser->ConsumeLiteral();
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);

  // Literal |null|.
  input = "null,|";
  parser.reset(NewTestParser(input));
  value = parser->ConsumeLiteral();
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  EXPECT_TRUE(value->is_none());
}

TEST_F(JSONParserTest, ConsumeNumbers) {
  // Integer.
  std::string input("1234,|");
  std::unique_ptr<JSONParser> parser(NewTestParser(input));
  Optional<Value> value(parser->ConsumeNumber());
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  int number_i;
  EXPECT_TRUE(value->GetAsInteger(&number_i));
  EXPECT_EQ(1234, number_i);

  // Negative integer.
  input = "-1234,|";
  parser.reset(NewTestParser(input));
  value = parser->ConsumeNumber();
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsInteger(&number_i));
  EXPECT_EQ(-1234, number_i);

  // Double.
  input = "12.34,|";
  parser.reset(NewTestParser(input));
  value = parser->ConsumeNumber();
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  double number_d;
  EXPECT_TRUE(value->GetAsDouble(&number_d));
  EXPECT_EQ(12.34, number_d);

  // Scientific.
  input = "42e3,|";
  parser.reset(NewTestParser(input));
  value = parser->ConsumeNumber();
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsDouble(&number_d));
  EXPECT_EQ(42000, number_d);

  // Negative scientific.
  input = "314159e-5,|";
  parser.reset(NewTestParser(input));
  value = parser->ConsumeNumber();
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsDouble(&number_d));
  EXPECT_EQ(3.14159, number_d);

  // Positive scientific.
  input = "0.42e+3,|";
  parser.reset(NewTestParser(input));
  value = parser->ConsumeNumber();
  EXPECT_EQ(',', *parser->pos());

  TestLastThree(parser.get());

  ASSERT_TRUE(value);
  EXPECT_TRUE(value->GetAsDouble(&number_d));
  EXPECT_EQ(420, number_d);
}

TEST_F(JSONParserTest, ErrorMessages) {
  JSONReader::ValueWithError root =
      JSONReader::ReadAndReturnValueWithError("[42]", JSON_PARSE_RFC);
  EXPECT_TRUE(root.error_message.empty());
  EXPECT_EQ(0, root.error_code);

  // Test line and column counting
  const char big_json[] = "[\n0,\n1,\n2,\n3,4,5,6 7,\n8,\n9\n]";
  // error here ----------------------------------^
  root = JSONReader::ReadAndReturnValueWithError(big_json, JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(JSONParser::FormatErrorMessage(5, 10, JSONReader::kSyntaxError),
            root.error_message);
  EXPECT_EQ(JSONReader::JSON_SYNTAX_ERROR, root.error_code);

  // Test line and column counting with "\r\n" line ending
  const char big_json_crlf[] =
      "[\r\n0,\r\n1,\r\n2,\r\n3,4,5,6 7,\r\n8,\r\n9\r\n]";
  // error here ----------------------^
  root = JSONReader::ReadAndReturnValueWithError(big_json_crlf, JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(JSONParser::FormatErrorMessage(5, 10, JSONReader::kSyntaxError),
            root.error_message);
  EXPECT_EQ(JSONReader::JSON_SYNTAX_ERROR, root.error_code);

  // Test each of the error conditions
  root = JSONReader::ReadAndReturnValueWithError("{},{}", JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(JSONParser::FormatErrorMessage(
                1, 3, JSONReader::kUnexpectedDataAfterRoot),
            root.error_message);
  EXPECT_EQ(JSONReader::JSON_UNEXPECTED_DATA_AFTER_ROOT, root.error_code);

  std::string nested_json;
  for (int i = 0; i < 201; ++i) {
    nested_json.insert(nested_json.begin(), '[');
    nested_json.append(1, ']');
  }
  root = JSONReader::ReadAndReturnValueWithError(nested_json, JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(JSONParser::FormatErrorMessage(1, 200, JSONReader::kTooMuchNesting),
            root.error_message);
  EXPECT_EQ(JSONReader::JSON_TOO_MUCH_NESTING, root.error_code);

  root = JSONReader::ReadAndReturnValueWithError("[1,]", JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(JSONParser::FormatErrorMessage(1, 4, JSONReader::kTrailingComma),
            root.error_message);
  EXPECT_EQ(JSONReader::JSON_TRAILING_COMMA, root.error_code);

  root =
      JSONReader::ReadAndReturnValueWithError("{foo:\"bar\"}", JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(
      JSONParser::FormatErrorMessage(1, 2, JSONReader::kUnquotedDictionaryKey),
      root.error_message);
  EXPECT_EQ(JSONReader::JSON_UNQUOTED_DICTIONARY_KEY, root.error_code);

  root = JSONReader::ReadAndReturnValueWithError("{\"foo\":\"bar\",}",
                                                 JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(JSONParser::FormatErrorMessage(1, 14, JSONReader::kTrailingComma),
            root.error_message);

  root = JSONReader::ReadAndReturnValueWithError("[nu]", JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(JSONParser::FormatErrorMessage(1, 2, JSONReader::kSyntaxError),
            root.error_message);
  EXPECT_EQ(JSONReader::JSON_SYNTAX_ERROR, root.error_code);

  root =
      JSONReader::ReadAndReturnValueWithError("[\"xxx\\xq\"]", JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(JSONParser::FormatErrorMessage(1, 7, JSONReader::kInvalidEscape),
            root.error_message);
  EXPECT_EQ(JSONReader::JSON_INVALID_ESCAPE, root.error_code);

  root =
      JSONReader::ReadAndReturnValueWithError("[\"xxx\\uq\"]", JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(JSONParser::FormatErrorMessage(1, 7, JSONReader::kInvalidEscape),
            root.error_message);
  EXPECT_EQ(JSONReader::JSON_INVALID_ESCAPE, root.error_code);

  root =
      JSONReader::ReadAndReturnValueWithError("[\"xxx\\q\"]", JSON_PARSE_RFC);
  EXPECT_FALSE(root.value);
  EXPECT_EQ(JSONParser::FormatErrorMessage(1, 7, JSONReader::kInvalidEscape),
            root.error_message);
  EXPECT_EQ(JSONReader::JSON_INVALID_ESCAPE, root.error_code);

  root = JSONReader::ReadAndReturnValueWithError(("[\"\\ufffe\"]"),
                                                 JSON_PARSE_RFC);
  EXPECT_EQ(JSONParser::FormatErrorMessage(1, 8, JSONReader::kInvalidEscape),
            root.error_message);
  EXPECT_EQ(JSONReader::JSON_INVALID_ESCAPE, root.error_code);
}

TEST_F(JSONParserTest, Decode4ByteUtf8Char) {
  // This test strings contains a 4 byte unicode character (a smiley!) that the
  // reader should be able to handle (the character is \xf0\x9f\x98\x87).
  const char kUtf8Data[] =
      "[\"😇\",[],[],[],{\"google:suggesttype\":[]}]";
  JSONReader::ValueWithError root =
      JSONReader::ReadAndReturnValueWithError(kUtf8Data, JSON_PARSE_RFC);
  EXPECT_TRUE(root.value) << root.error_message;
}

TEST_F(JSONParserTest, DecodeUnicodeNonCharacter) {
  // Tests Unicode code points (encoded as escaped UTF-16) that are not valid
  // characters.
  EXPECT_FALSE(JSONReader::Read("[\"\\ufdd0\"]"));
  EXPECT_FALSE(JSONReader::Read("[\"\\ufffe\"]"));
  EXPECT_FALSE(JSONReader::Read("[\"\\ud83f\\udffe\"]"));

  EXPECT_TRUE(
      JSONReader::Read("[\"\\ufdd0\"]", JSON_REPLACE_INVALID_CHARACTERS));
  EXPECT_TRUE(
      JSONReader::Read("[\"\\ufffe\"]", JSON_REPLACE_INVALID_CHARACTERS));
}

TEST_F(JSONParserTest, DecodeNegativeEscapeSequence) {
  EXPECT_FALSE(JSONReader::Read("[\"\\x-A\"]"));
  EXPECT_FALSE(JSONReader::Read("[\"\\u-00A\"]"));
}

// Verifies invalid utf-8 characters are replaced.
TEST_F(JSONParserTest, ReplaceInvalidCharacters) {
  const std::string bogus_char = "󿿿";
  const std::string quoted_bogus_char = "\"" + bogus_char + "\"";
  std::unique_ptr<JSONParser> parser(
      NewTestParser(quoted_bogus_char, JSON_REPLACE_INVALID_CHARACTERS));
  Optional<Value> value(parser->ConsumeString());
  ASSERT_TRUE(value);
  std::string str;
  EXPECT_TRUE(value->GetAsString(&str));
  EXPECT_EQ(kUnicodeReplacementString, str);
}

TEST_F(JSONParserTest, ReplaceInvalidUTF16EscapeSequence) {
  const std::string invalid = "\"\\ufffe\"";
  std::unique_ptr<JSONParser> parser(
      NewTestParser(invalid, JSON_REPLACE_INVALID_CHARACTERS));
  Optional<Value> value(parser->ConsumeString());
  ASSERT_TRUE(value);
  std::string str;
  EXPECT_TRUE(value->GetAsString(&str));
  EXPECT_EQ(kUnicodeReplacementString, str);
}

TEST_F(JSONParserTest, ParseNumberErrors) {
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

  for (unsigned int i = 0; i < base::size(kCases); ++i) {
    auto test_case = kCases[i];
    SCOPED_TRACE(StringPrintf("case %u: \"%s\"", i, test_case.input));

    std::unique_ptr<char[]> input_owner;
    StringPiece input =
        MakeNotNullTerminatedInput(test_case.input, &input_owner);

    Optional<Value> result = JSONReader::Read(input);
    EXPECT_EQ(test_case.parse_success, result.has_value());

    if (!result)
      continue;

    ASSERT_TRUE(result->is_double() || result->is_int());
    EXPECT_EQ(test_case.value, result->GetDouble());
  }
}

TEST_F(JSONParserTest, UnterminatedInputs) {
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

  for (unsigned int i = 0; i < base::size(kCases); ++i) {
    auto* test_case = kCases[i];
    SCOPED_TRACE(StringPrintf("case %u: \"%s\"", i, test_case));

    std::unique_ptr<char[]> input_owner;
    StringPiece input = MakeNotNullTerminatedInput(test_case, &input_owner);

    EXPECT_FALSE(JSONReader::Read(input));
  }
}

}  // namespace internal
}  // namespace base
