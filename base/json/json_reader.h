// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A JSON parser, converting from a base::StringPiece to a base::Value.
//
// The JSON spec is:
// https://tools.ietf.org/rfc/rfc8259.txt
// which obsoletes the earlier RFCs 4627, 7158 and 7159.
//
// This RFC should be equivalent to the informal spec:
// https://www.json.org/json-en.html
//
// Implementation choices permitted by the RFC:
// - Nesting is limited (to a configurable depth, 200 by default).
// - Numbers are limited to those representable by a finite double. The
//   conversion from a JSON number (in the base::StringPiece input) to a
//   double-flavored base::Value may also be lossy.
// - The input (which must be UTF-8) may begin with a BOM (Byte Order Mark).
// - Duplicate object keys (strings) are silently allowed. Last key-value pair
//   wins. Previous pairs are discarded.
//
// Configurable (see the JSONParserOptions type) deviations from the RFC:
// - Allow trailing commas: "[1,2,]".
// - Replace invalid Unicode with U+FFFD REPLACEMENT CHARACTER.
//
// Non-configurable deviations from the RFC:
// - Allow "// etc\n" and "/* etc */" C-style comments.
// - Allow ASCII control characters, including literal (not escaped) NUL bytes
//   and new lines, within a JSON string.
// - Allow "\\v" escapes within a JSON string, producing a vertical tab.
// - Allow "\\x23" escapes within a JSON string. Subtly, the 2-digit hex value
//   is a Unicode code point, not a UTF-8 byte. For example, "\\xFF" in the
//   JSON source decodes to a base::Value whose string contains "\xC3\xBF", the
//   UTF-8 encoding of U+00FF LATIN SMALL LETTER Y WITH DIAERESIS. Converting
//   from UTF-8 to UTF-16, e.g. via UTF8ToWide, will recover a 16-bit 0x00FF.

#ifndef BASE_JSON_JSON_READER_H_
#define BASE_JSON_JSON_READER_H_

#include <memory>
#include <string>

#include "base/base_export.h"
#include "base/json/json_common.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/values.h"

namespace base {

enum JSONParserOptions {
  // Parses the input strictly according to RFC 8259, except for where noted
  // above.
  JSON_PARSE_RFC = 0,

  // Allows commas to exist after the last element in structures.
  JSON_ALLOW_TRAILING_COMMAS = 1 << 0,

  // If set the parser replaces invalid code points (i.e. lone
  // surrogates) with the Unicode replacement character (U+FFFD). If
  // not set, invalid code points trigger a hard error and parsing
  // fails.
  JSON_REPLACE_INVALID_CHARACTERS = 1 << 1,
};

class BASE_EXPORT JSONReader {
 public:
  struct BASE_EXPORT ValueWithError {
    ValueWithError();
    ValueWithError(ValueWithError&& other);
    ValueWithError& operator=(ValueWithError&& other);
    ~ValueWithError();

    Optional<Value> value;

    // Contains default values if |value| exists, or the error status if |value|
    // is base::nullopt.
    std::string error_message;
    int error_line = 0;
    int error_column = 0;

    DISALLOW_COPY_AND_ASSIGN(ValueWithError);
  };

  // Reads and parses |json|, returning a Value.
  // If |json| is not a properly formed JSON string, returns base::nullopt.
  static Optional<Value> Read(StringPiece json,
                              int options = JSON_PARSE_RFC,
                              size_t max_depth = internal::kAbsoluteMaxDepth);

  // Deprecated. Use the Read() method above.
  // Reads and parses |json|, returning a Value.
  // If |json| is not a properly formed JSON string, returns nullptr.
  // Wrap this in base::FooValue::From() to check the Value is of type Foo and
  // convert to a FooValue at the same time.
  static std::unique_ptr<Value> ReadDeprecated(
      StringPiece json,
      int options = JSON_PARSE_RFC,
      size_t max_depth = internal::kAbsoluteMaxDepth);

  // Reads and parses |json| like Read(). Returns a ValueWithError, which on
  // error, will be populated with a formatted error message, an error code, and
  // the error location if appropriate.
  static ValueWithError ReadAndReturnValueWithError(
      StringPiece json,
      int options = JSON_PARSE_RFC);

  // This class contains only static methods.
  JSONReader() = delete;
  DISALLOW_COPY_AND_ASSIGN(JSONReader);
};

}  // namespace base

#endif  // BASE_JSON_JSON_READER_H_
