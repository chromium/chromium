// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/json/json_parser.h"

#include <cmath>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"

namespace base {
namespace internal {

namespace {

// Values 1000 and above are used by JSONFileValueSerializer::JsonFileError.
static_assert(JSONParser::JSON_PARSE_ERROR_COUNT < 1000,
              "JSONParser error out of bounds");

std::string ErrorCodeToString(JSONParser::JsonParseError error_code) {
  switch (error_code) {
    case JSONParser::JSON_NO_ERROR:
      return std::string();
    case JSONParser::JSON_SYNTAX_ERROR:
      return JSONParser::kSyntaxError;
    case JSONParser::JSON_INVALID_ESCAPE:
      return JSONParser::kInvalidEscape;
    case JSONParser::JSON_UNEXPECTED_TOKEN:
      return JSONParser::kUnexpectedToken;
    case JSONParser::JSON_TRAILING_COMMA:
      return JSONParser::kTrailingComma;
    case JSONParser::JSON_TOO_MUCH_NESTING:
      return JSONParser::kTooMuchNesting;
    case JSONParser::JSON_UNEXPECTED_DATA_AFTER_ROOT:
      return JSONParser::kUnexpectedDataAfterRoot;
    case JSONParser::JSON_UNSUPPORTED_ENCODING:
      return JSONParser::kUnsupportedEncoding;
    case JSONParser::JSON_UNQUOTED_DICTIONARY_KEY:
      return JSONParser::kUnquotedDictionaryKey;
    case JSONParser::JSON_UNREPRESENTABLE_NUMBER:
      return JSONParser::kUnrepresentableNumber;
    case JSONParser::JSON_PARSE_ERROR_COUNT:
      NOTREACHED();
  }
  NOTREACHED();
}

const int32_t kExtendedASCIIStart = 0x80;
constexpr base_icu::UChar32 kUnicodeReplacementPoint = 0xFFFD;

// UnprefixedHexStringToInt acts like |HexStringToInt|, but enforces that the
// input consists purely of hex digits. I.e. no "0x" nor "OX" prefix is
// permitted.
bool UnprefixedHexStringToInt(std::string_view input, int* output) {
  for (size_t i = 0; i < input.size(); i++) {
    if (!IsHexDigit(input[i])) {
      return false;
    }
  }
  return HexStringToInt(input, output);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ChromiumJsonExtension {
  kCComment,
  kCppComment,
  kXEscape,
  kVerticalTabEscape,
  kControlCharacter,
  kNewlineInString,
  kMaxValue = kNewlineInString,
};

const char kExtensionHistogramName[] =
    "Security.JSONParser.ChromiumExtensionUsage";

}  // namespace

// This is U+FFFD.
const char kUnicodeReplacementString[] = "\xEF\xBF\xBD";

const char JSONParser::kSyntaxError[] = "Syntax error.";
const char JSONParser::kInvalidEscape[] = "Invalid escape sequence.";
const char JSONParser::kUnexpectedToken[] = "Unexpected token.";
const char JSONParser::kTrailingComma[] = "Trailing comma not allowed.";
const char JSONParser::kTooMuchNesting[] = "Too much nesting.";
const char JSONParser::kUnexpectedDataAfterRoot[] =
    "Unexpected data after root element.";
const char JSONParser::kUnsupportedEncoding[] =
    "Unsupported encoding. JSON must be UTF-8.";
const char JSONParser::kUnquotedDictionaryKey[] =
    "Dictionary keys must be quoted.";
const char JSONParser::kUnrepresentableNumber[] =
    "Number cannot be represented.";

JSONParser::JSONParser(int options, size_t max_depth)
    : options_(options),
      max_depth_(max_depth),
      index_(0),
      stack_depth_(0),
      line_number_(0),
      index_last_line_(0),
      error_code_(JSON_NO_ERROR),
      error_line_(0),
      error_column_(0) {
  CHECK_LE(max_depth, kAbsoluteMaxDepth);
}

JSONParser::~JSONParser() = default;

std::optional<Value> JSONParser::Parse(std::string_view input) {
  input_ = input;
  index_ = 0;
  // Line and column counting is 1-based, but |index_| is 0-based. For example,
  // if input is "Aaa\nB" then 'A' and 'B' are both in column 1 (at lines 1 and
  // 2) and have indexes of 0 and 4. We track the line number explicitly (the
  // |line_number_| field) and the column number implicitly (the difference
  // between |index_| and |index_last_line_|). In calculating that difference,
  // |index_last_line_| is the index of the '\r' or '\n', not the index of the
  // first byte after the '\n'. For the 'B' in "Aaa\nB", its |index_| and
  // |index_last_line_| would be 4 and 3: 'B' is in column (4 - 3) = 1. We
  // initialize |index_last_line_| to -1, not 0, since -1 is the (out of range)
  // index of the imaginary '\n' immediately before the start of the string:
  // 'A' is in column (0 - -1) = 1.
  line_number_ = 1;
  index_last_line_ = static_cast<size_t>(-1);

  error_code_ = JSON_NO_ERROR;
  error_line_ = 0;
  error_column_ = 0;

  // When the input JSON string starts with a UTF-8 Byte-Order-Mark,
  // advance the start position to avoid the ParseNextToken function mis-
  // treating a Unicode BOM as an invalid character and returning NULL.
  ConsumeIfMatch("\xEF\xBB\xBF");

  // Parse the first and any nested tokens.
  std::optional<Value> root(ParseNextToken());
  if (!root)
    return std::nullopt;

  // Make sure the input stream is at an end.
  if (GetNextToken() != T_END_OF_INPUT) {
    ReportError(JSON_UNEXPECTED_DATA_AFTER_ROOT, 0);
    return std::nullopt;
  }

  return root;
}

JSONParser::JsonParseError JSONParser::error_code() const {
  return error_code_;
}

std::string JSONParser::GetErrorMessage() const {
  return FormatErrorMessage(error_line_, error_column_,
                            ErrorCodeToString(error_code_));
}

int JSONParser::error_line() const {
  return error_line_;
}

int JSONParser::error_column() const {
  return error_column_;
}

// JSONParser private //////////////////////////////////////////////////////////

std::optional<std::string_view> JSONParser::PeekChars(size_t count) {
  if (count > input_.length() - index_) {
    return std::nullopt;
  }
  // Using string_view::substr() was historically significantly slower
  // (according to base_perftests) than constructing a substring manually.
  //
  // TODO(crbug.com/40284755): Is this still the case? Ideally the bounds check
  // performed by substr would be deleted by the optimizer for being redundant
  // with the runtime check above. However, to do so, the compiler would need
  // to know `index_ <= input_.length()` is a class invariant. If we
  // restructured the code so that we only stored the remaining data, that
  // would avoid this, but it would prevent rewinding (the places in this file
  // which look at `input_[index_ - 1]`.)
  return std::string_view(input_.data() + index_, count);
}

std::optional<char> JSONParser::PeekChar() {
  std::optional<std::string_view> chars = PeekChars(1);
  if (chars)
    return (*chars)[0];
  return std::nullopt;
}

std::optional<std::string_view> JSONParser::ConsumeChars(size_t count) {
  std::optional<std::string_view> chars = PeekChars(count);
  if (chars)
    index_ += count;
  return chars;
}

std::optional<char> JSONParser::ConsumeChar() {
  std::optional<std::string_view> chars = ConsumeChars(1);
  if (chars)
    return (*chars)[0];
  return std::nullopt;
}

const char* JSONParser::pos() {
  CHECK_LE(index_, input_.length());
  return input_.data() + index_;
}

JSONParser::Token JSONParser::GetNextToken() {
  EatWhitespaceAndComments();

  std::optional<char> c = PeekChar();
  if (!c)
    return T_END_OF_INPUT;

  switch (*c) {
    case '{':
      return T_OBJECT_BEGIN;
    case '}':
      return T_OBJECT_END;
    case '[':
      return T_ARRAY_BEGIN;
    case ']':
      return T_ARRAY_END;
    case '"':
      return T_STRING;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
      return T_NUMBER;
    case 't':
      return T_BOOL_TRUE;
    case 'f':
      return T_BOOL_FALSE;
    case 'n':
      return T_NULL;
    case ',':
      return T_LIST_SEPARATOR;
    case ':':
      return T_OBJECT_PAIR_SEPARATOR;
    default:
      return T_INVALID_TOKEN;
  }
}

void JSONParser::EatWhitespaceAndComments() {
  while (std::optional<char> c = PeekChar()) {
    switch (*c) {
      case '\r':
      case '\n':
        index_last_line_ = index_;
        // Don't increment line_number_ twice for "\r\n".
        if (!(c == '\n' && index_ > 0 && input_[index_ - 1] == '\r')) {
          ++line_number_;
        }
        [[fallthrough]];
      case ' ':
      case '\t':
        ConsumeChar();
        break;
      case '/':
        if (!EatComment())
          return;
        break;
      default:
        return;
    }
  }
}

bool JSONParser::EatComment() {
  std::optional<std::string_view> comment_start = PeekChars(2);
  if (!comment_start)
    return false;

  const bool comments_allowed = options_ & JSON_ALLOW_COMMENTS;

  if (comment_start == "//") {
    UmaHistogramEnumeration(kExtensionHistogramName,
                            ChromiumJsonExtension::kCppComment);
    if (!comments_allowed) {
      ReportError(JSON_UNEXPECTED_TOKEN, 0);
      return false;
    }

    ConsumeChars(2);
    // Single line comment, read to newline.
    while (std::optional<char> c = PeekChar()) {
      if (c == '\n' || c == '\r')
        return true;
      ConsumeChar();
    }
  } else if (comment_start == "/*") {
    UmaHistogramEnumeration(kExtensionHistogramName,
                            ChromiumJsonExtension::kCComment);
    if (!comments_allowed) {
      ReportError(JSON_UNEXPECTED_TOKEN, 0);
      return false;
    }

    ConsumeChars(2);
    char previous_char = '\0';
    // Block comment, read until end marker.
    while (std::optional<char> c = PeekChar()) {
      if (previous_char == '*' && c == '/') {
        // EatWhitespaceAndComments will inspect pos(), which will still be on
        // the last / of the comment, so advance once more (which may also be
        // end of input).
        ConsumeChar();
        return true;
      }
      previous_char = *ConsumeChar();
    }

    // If the comment is unterminated, GetNextToken will report T_END_OF_INPUT.
  }

  return false;
}

std::optional<Value> JSONParser::ParseNextToken() {
  return ParseToken(GetNextToken());
}

std::optional<Value> JSONParser::ParseToken(Token token) {
  switch (token) {
    case T_OBJECT_BEGIN:
      return ConsumeDictionary();
    case T_ARRAY_BEGIN:
      return ConsumeList();
    case T_STRING:
      return ConsumeString();
    case T_NUMBER:
      return ConsumeNumber();
    case T_BOOL_TRUE:
    case T_BOOL_FALSE:
    case T_NULL:
      return ConsumeLiteral();
    default:
      ReportError(JSON_UNEXPECTED_TOKEN, 0);
      return std::nullopt;
  }
}

std::optional<Value> JSONParser::ConsumeDictionary() {
  if (ConsumeChar() != '{') {
    ReportError(JSON_UNEXPECTED_TOKEN, 0);
    return std::nullopt;
  }

  StackMarker depth_check(max_depth_, &stack_depth_);
  if (depth_check.IsTooDeep()) {
    ReportError(JSON_TOO_MUCH_NESTING, -1);
    return std::nullopt;
  }

  std::vector<std::pair<std::string, Value>> values;

  Token token = GetNextToken();
  while (token != T_OBJECT_END) {
    if (token != T_STRING) {
      ReportError(JSON_UNQUOTED_DICTIONARY_KEY, 0);
      return std::nullopt;
    }

    // First consume the key.
    std::optional<std::string> key = ConsumeStringRaw();
    if (!key) {
      return std::nullopt;
    }

    // Read the separator.
    token = GetNextToken();
    if (token != T_OBJECT_PAIR_SEPARATOR) {
      ReportError(JSON_SYNTAX_ERROR, 0);
      return std::nullopt;
    }

    // The next token is the value. Ownership transfers to |dict|.
    ConsumeChar();
    std::optional<Value> value = ParseNextToken();
    if (!value) {
      // ReportError from deeper level.
      return std::nullopt;
    }

    values.emplace_back(std::move(*key), std::move(*value));

    token = GetNextToken();
    if (token == T_LIST_SEPARATOR) {
      ConsumeChar();
      token = GetNextToken();
      if (token == T_OBJECT_END && !(options_ & JSON_ALLOW_TRAILING_COMMAS)) {
        ReportError(JSON_TRAILING_COMMA, 0);
        return std::nullopt;
      }
    } else if (token != T_OBJECT_END) {
      ReportError(JSON_SYNTAX_ERROR, 0);
      return std::nullopt;
    }
  }

  ConsumeChar();  // Closing '}'.
  // Reverse |dict_storage| to keep the last of elements with the same key in
  // the input.
  ranges::reverse(values);
  return Value(Value::Dict(std::make_move_iterator(values.begin()),
                           std::make_move_iterator(values.end())));
}

std::optional<Value> JSONParser::ConsumeList() {
  if (ConsumeChar() != '[') {
    ReportError(JSON_UNEXPECTED_TOKEN, 0);
    return std::nullopt;
  }

  StackMarker depth_check(max_depth_, &stack_depth_);
  if (depth_check.IsTooDeep()) {
    ReportError(JSON_TOO_MUCH_NESTING, -1);
    return std::nullopt;
  }

  Value::List list;

  Token token = GetNextToken();
  while (token != T_ARRAY_END) {
    std::optional<Value> item = ParseToken(token);
    if (!item) {
      // ReportError from deeper level.
      return std::nullopt;
    }

    list.Append(std::move(*item));

    token = GetNextToken();
    if (token == T_LIST_SEPARATOR) {
      ConsumeChar();
      token = GetNextToken();
      if (token == T_ARRAY_END && !(options_ & JSON_ALLOW_TRAILING_COMMAS)) {
        ReportError(JSON_TRAILING_COMMA, 0);
        return std::nullopt;
      }
    } else if (token != T_ARRAY_END) {
      ReportError(JSON_SYNTAX_ERROR, 0);
      return std::nullopt;
    }
  }

  ConsumeChar();  // Closing ']'.

  return Value(std::move(list));
}

std::optional<Value> JSONParser::ConsumeString() {
  std::optional<std::string> string = ConsumeStringRaw();
  if (!string) {
    return std::nullopt;
  }
  return Value(std::move(*string));
}

std::optional<std::string> JSONParser::ConsumeStringRaw() {
  if (ConsumeChar() != '"') {
    ReportError(JSON_UNEXPECTED_TOKEN, 0);
    return std::nullopt;
  }

  std::string string;
  for (;;) {
    auto [result, consumed] = ConsumeStringPart();
    switch (result) {
      case StringResult::kError:
        return std::nullopt;

      case StringResult::kDone:
        // This is the last time we're appending, so pre-reserve the desired
        // size, to prevent `+=` from overallocating. (In other cases, the
        // overallocating is desirable for amortization.) In particular,
        // the common case is that `string` is empty and we return in one step.
        string.reserve(string.size() + consumed.size());
        string += consumed;
        return std::move(string);

      case StringResult::kReplacementCharacter:
        string += consumed;
        string += kUnicodeReplacementString;
        break;  // Keep parsing.

      case StringResult::kEscape:
        string += consumed;
        std::optional<char> escape_char = ConsumeChar();
        if (!escape_char) {
          ReportError(JSON_INVALID_ESCAPE, -1);
          return std::nullopt;
        }

        switch (*escape_char) {
          // Allowed esape sequences:
          case 'x': {  // UTF-8 sequence.
            // UTF-8 \x escape sequences are not allowed in the spec, but they
            // are supported here for backwards-compatiblity with the old
            // parser.
            UmaHistogramEnumeration(kExtensionHistogramName,
                                    ChromiumJsonExtension::kXEscape);
            if (!(options_ & JSON_ALLOW_X_ESCAPES)) {
              ReportError(JSON_INVALID_ESCAPE, -1);
              return std::nullopt;
            }

            std::optional<std::string_view> escape_sequence = ConsumeChars(2);
            if (!escape_sequence) {
              ReportError(JSON_INVALID_ESCAPE, -3);
              return std::nullopt;
            }

            int hex_digit = 0;
            if (!UnprefixedHexStringToInt(*escape_sequence, &hex_digit)) {
              ReportError(JSON_INVALID_ESCAPE, -3);
              return std::nullopt;
            }

            // A two-character hex sequence is at most 0xff and all codepoints
            // up to 0xff are valid.
            DCHECK_LE(hex_digit, 0xff);
            DCHECK(IsValidCharacter(hex_digit));
            WriteUnicodeCharacter(hex_digit, &string);
            break;
          }
          case 'u': {  // UTF-16 sequence.
            // UTF units are of the form \uXXXX.
            base_icu::UChar32 code_point;
            if (!DecodeUTF16(&code_point)) {
              ReportError(JSON_INVALID_ESCAPE, -1);
              return std::nullopt;
            }
            WriteUnicodeCharacter(code_point, &string);
            break;
          }
          case '"':
            string.push_back('"');
            break;
          case '\\':
            string.push_back('\\');
            break;
          case '/':
            string.push_back('/');
            break;
          case 'b':
            string.push_back('\b');
            break;
          case 'f':
            string.push_back('\f');
            break;
          case 'n':
            string.push_back('\n');
            break;
          case 'r':
            string.push_back('\r');
            break;
          case 't':
            string.push_back('\t');
            break;
          case 'v':  // Not listed as valid escape sequence in the RFC.
            UmaHistogramEnumeration(kExtensionHistogramName,
                                    ChromiumJsonExtension::kVerticalTabEscape);
            if (!(options_ & JSON_ALLOW_VERT_TAB)) {
              ReportError(JSON_INVALID_ESCAPE, -1);
              return std::nullopt;
            }
            string.push_back('\v');
            break;
          // All other escape squences are illegal.
          default:
            ReportError(JSON_INVALID_ESCAPE, -1);
            return std::nullopt;
        }
        break;  // Keep parsing.
    }
  }
}

std::pair<JSONParser::StringResult, std::string_view>
JSONParser::ConsumeStringPart() {
  const size_t start_index = index_;
  while (std::optional<char> c = PeekChar()) {
    // Handle non-ASCII characters, which never trigger any special handling
    // beyond needing to be valid UTF-8. ASCII characters will be handled
    // separately below.
    if (static_cast<unsigned char>(*c) >= kExtendedASCIIStart) {
      base_icu::UChar32 next_char = 0;
      size_t last_index = index_;
      if (!ReadUnicodeCharacter(input_.data(), input_.length(), &index_,
                                &next_char)) {
        if ((options_ & JSON_REPLACE_INVALID_CHARACTERS) == 0) {
          ReportError(JSON_UNSUPPORTED_ENCODING, 0);
          // No need to return consumed data.
          return {StringResult::kError, {}};
        }
        ConsumeChar();
        return {StringResult::kReplacementCharacter,
                input_.substr(start_index, last_index - start_index)};
      }

      // Valid UTF-8 will be copied as-is into the output, so keep processing.
      DCHECK_GE(next_char, kExtendedASCIIStart);
      ConsumeChar();
      continue;
    }

    if (*c == '"') {
      std::string_view ret = input_.substr(start_index, index_ - start_index);
      ConsumeChar();
      return {StringResult::kDone, ret};
    }
    if (*c == '\\') {
      std::string_view ret = input_.substr(start_index, index_ - start_index);
      ConsumeChar();
      return {StringResult::kEscape, ret};
    }

    // Per Section 7, "All Unicode characters may be placed within the
    // quotation marks, except for the characters that MUST be escaped:
    // quotation mark, reverse solidus, and the control characters (U+0000
    // through U+001F)".
    if (*c == '\n' || *c == '\r') {
      UmaHistogramEnumeration(kExtensionHistogramName,
                              ChromiumJsonExtension::kNewlineInString);
      if (!(options_ &
            (JSON_ALLOW_NEWLINES_IN_STRINGS | JSON_ALLOW_CONTROL_CHARS))) {
        ReportError(JSON_UNSUPPORTED_ENCODING, -1);
        return {StringResult::kError, {}};  // No need to return consumed data.
      }
    } else if (*c <= 0x1F) {
      UmaHistogramEnumeration(kExtensionHistogramName,
                              ChromiumJsonExtension::kControlCharacter);
      if (!(options_ & JSON_ALLOW_CONTROL_CHARS)) {
        ReportError(JSON_UNSUPPORTED_ENCODING, -1);
        return {StringResult::kError, {}};  // No need to return consumed data.
      }
    }

    // If this character is not an escape sequence, track any line breaks and
    // keep parsing. The JSON spec forbids unescaped ASCII control characters
    // within a string, including '\r' and '\n', but this implementation is more
    // lenient.
    if (*c == '\r' || *c == '\n') {
      index_last_line_ = index_;
      // Don't increment line_number_ twice for "\r\n". We are guaranteed that
      // (index_ > 0) because we are consuming a string, so we must have seen an
      // opening '"' quote character.
      if ((*c == '\r') || (input_[index_ - 1] != '\r')) {
        ++line_number_;
      }
    }
    ConsumeChar();
  }

  ReportError(JSON_SYNTAX_ERROR, -1);
  return {StringResult::kError, {}};  // No need to return consumed data.
}

// Entry is at the first X in \uXXXX.
bool JSONParser::DecodeUTF16(base_icu::UChar32* out_code_point) {
  std::optional<std::string_view> escape_sequence = ConsumeChars(4);
  if (!escape_sequence)
    return false;

  // Consume the UTF-16 code unit, which may be a high surrogate.
  int code_unit16_high = 0;
  if (!UnprefixedHexStringToInt(*escape_sequence, &code_unit16_high))
    return false;

  // If this is a high surrogate, consume the next code unit to get the
  // low surrogate.
  if (CBU16_IS_SURROGATE(code_unit16_high)) {
    // Make sure this is the high surrogate.
    if (!CBU16_IS_SURROGATE_LEAD(code_unit16_high)) {
      if ((options_ & JSON_REPLACE_INVALID_CHARACTERS) == 0)
        return false;
      *out_code_point = kUnicodeReplacementPoint;
      return true;
    }

    // Make sure that the token has more characters to consume the
    // lower surrogate.
    if (!ConsumeIfMatch("\\u")) {
      if ((options_ & JSON_REPLACE_INVALID_CHARACTERS) == 0)
        return false;
      *out_code_point = kUnicodeReplacementPoint;
      return true;
    }

    escape_sequence = ConsumeChars(4);
    if (!escape_sequence)
      return false;

    int code_unit16_low = 0;
    if (!UnprefixedHexStringToInt(*escape_sequence, &code_unit16_low))
      return false;

    if (!CBU16_IS_TRAIL(code_unit16_low)) {
      if ((options_ & JSON_REPLACE_INVALID_CHARACTERS) == 0)
        return false;
      *out_code_point = kUnicodeReplacementPoint;
      return true;
    }

    base_icu::UChar32 code_point =
        CBU16_GET_SUPPLEMENTARY(code_unit16_high, code_unit16_low);

    *out_code_point = code_point;
  } else {
    // Not a surrogate.
    DCHECK(CBU16_IS_SINGLE(code_unit16_high));

    *out_code_point = code_unit16_high;
  }

  return true;
}

std::optional<Value> JSONParser::ConsumeNumber() {
  const char* num_start = pos();
  const size_t start_index = index_;
  size_t end_index = start_index;

  if (PeekChar() == '-')
    ConsumeChar();

  if (!ReadInt(false)) {
    ReportError(JSON_SYNTAX_ERROR, 0);
    return std::nullopt;
  }
  end_index = index_;

  // The optional fraction part.
  if (PeekChar() == '.') {
    ConsumeChar();
    if (!ReadInt(true)) {
      ReportError(JSON_SYNTAX_ERROR, 0);
      return std::nullopt;
    }
    end_index = index_;
  }

  // Optional exponent part.
  std::optional<char> c = PeekChar();
  if (c == 'e' || c == 'E') {
    ConsumeChar();
    if (PeekChar() == '-' || PeekChar() == '+') {
      ConsumeChar();
    }
    if (!ReadInt(true)) {
      ReportError(JSON_SYNTAX_ERROR, 0);
      return std::nullopt;
    }
    end_index = index_;
  }

  std::string_view num_string(num_start, end_index - start_index);

  int num_int;
  if (StringToInt(num_string, &num_int)) {
    // StringToInt will treat `-0` as zero, losing the significance of the
    // negation.
    if (num_int == 0 && num_string.starts_with('-')) {
      return Value(-0.0);
    }
    return Value(num_int);
  }

  double num_double;
  if (StringToDouble(num_string, &num_double) && std::isfinite(num_double)) {
    return Value(num_double);
  }

  ReportError(JSON_UNREPRESENTABLE_NUMBER, 0);
  return std::nullopt;
}

bool JSONParser::ReadInt(bool allow_leading_zeros) {
  size_t len = 0;
  char first = 0;

  while (std::optional<char> c = PeekChar()) {
    if (!IsAsciiDigit(c))
      break;

    if (len == 0)
      first = *c;

    ++len;
    ConsumeChar();
  }

  if (len == 0)
    return false;

  if (!allow_leading_zeros && len > 1 && first == '0')
    return false;

  return true;
}

std::optional<Value> JSONParser::ConsumeLiteral() {
  if (ConsumeIfMatch("true"))
    return Value(true);
  if (ConsumeIfMatch("false"))
    return Value(false);
  if (ConsumeIfMatch("null"))
    return Value(Value::Type::NONE);
  ReportError(JSON_SYNTAX_ERROR, 0);
  return std::nullopt;
}

bool JSONParser::ConsumeIfMatch(std::string_view match) {
  if (match == PeekChars(match.size())) {
    ConsumeChars(match.size());
    return true;
  }
  return false;
}

void JSONParser::ReportError(JsonParseError code, int column_adjust) {
  error_code_ = code;
  error_line_ = line_number_;
  error_column_ = static_cast<int>(index_ - index_last_line_) + column_adjust;

  // For a final blank line ('\n' and then EOF), a negative column_adjust may
  // put us below 1, which doesn't really make sense for 1-based columns.
  if (error_column_ < 1) {
    error_column_ = 1;
  }
}

// static
std::string JSONParser::FormatErrorMessage(int line, int column,
                                           const std::string& description) {
  if (line || column) {
    return StringPrintf("Line: %i, column: %i, %s",
        line, column, description.c_str());
  }
  return description;
}

}  // namespace internal
}  // namespace base
